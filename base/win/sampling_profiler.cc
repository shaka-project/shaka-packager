// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/sampling_profiler.h"

#include <winternl.h>  // for NTSTATUS.

#include "base/lazy_instance.h"

// Copied from wdm.h in the WDK as we don't want to take
// a dependency on the WDK.
typedef enum _KPROFILE_SOURCE {
    ProfileTime,
    ProfileAlignmentFixup,
    ProfileTotalIssues,
    ProfilePipelineDry,
    ProfileLoadInstructions,
    ProfilePipelineFrozen,
    ProfileBranchInstructions,
    ProfileTotalNonissues,
    ProfileDcacheMisses,
    ProfileIcacheMisses,
    ProfileCacheMisses,
    ProfileBranchMispredictions,
    ProfileStoreInstructions,
    ProfileFpInstructions,
    ProfileIntegerInstructions,
    Profile2Issue,
    Profile3Issue,
    Profile4Issue,
    ProfileSpecialInstructions,
    ProfileTotalCycles,
    ProfileIcacheIssues,
    ProfileDcacheAccesses,
    ProfileMemoryBarrierCycles,
    ProfileLoadLinkedIssues,
    ProfileMaximum
} KPROFILE_SOURCE;


namespace {

// Signatures for the native functions we need to access the sampling profiler.
typedef NTSTATUS (NTAPI *ZwSetIntervalProfileFunc)(ULONG, KPROFILE_SOURCE);
typedef NTSTATUS (NTAPI *ZwQueryIntervalProfileFunc)(KPROFILE_SOURCE, PULONG);

typedef NTSTATUS (NTAPI *ZwCreateProfileFunc)(PHANDLE profile,
                                              HANDLE process,
                                              PVOID code_start,
                                              ULONG code_size,
                                              ULONG eip_bucket_shift,
                                              PULONG buckets,
                                              ULONG buckets_byte_size,
                                              KPROFILE_SOURCE source,
                                              DWORD_PTR processor_mask);

typedef NTSTATUS (NTAPI *ZwStartProfileFunc)(HANDLE);
typedef NTSTATUS (NTAPI *ZwStopProfileFunc)(HANDLE);

// This class is used to lazy-initialize pointers to the native
// functions we need to access.
class ProfilerFuncs {
 public:
  ProfilerFuncs();

  ZwSetIntervalProfileFunc ZwSetIntervalProfile;
  ZwQueryIntervalProfileFunc ZwQueryIntervalProfile;
  ZwCreateProfileFunc ZwCreateProfile;
  ZwStartProfileFunc ZwStartProfile;
  ZwStopProfileFunc ZwStopProfile;

  // True iff all of the function pointers above were successfully initialized.
  bool initialized_;
};

ProfilerFuncs::ProfilerFuncs()
    : ZwSetIntervalProfile(NULL),
      ZwQueryIntervalProfile(NULL),
      ZwCreateProfile(NULL),
      ZwStartProfile(NULL),
      ZwStopProfile(NULL),
      initialized_(false) {
  HMODULE ntdll = ::GetModuleHandle(L"ntdll.dll");
  if (ntdll != NULL) {
    ZwSetIntervalProfile = reinterpret_cast<ZwSetIntervalProfileFunc>(
        ::GetProcAddress(ntdll, "ZwSetIntervalProfile"));
    ZwQueryIntervalProfile = reinterpret_cast<ZwQueryIntervalProfileFunc>(
        ::GetProcAddress(ntdll, "ZwQueryIntervalProfile"));
    ZwCreateProfile = reinterpret_cast<ZwCreateProfileFunc>(
        ::GetProcAddress(ntdll, "ZwCreateProfile"));
    ZwStartProfile = reinterpret_cast<ZwStartProfileFunc>(
        ::GetProcAddress(ntdll, "ZwStartProfile"));
    ZwStopProfile = reinterpret_cast<ZwStopProfileFunc>(
        ::GetProcAddress(ntdll, "ZwStopProfile"));

    if (ZwSetIntervalProfile &&
        ZwQueryIntervalProfile &&
        ZwCreateProfile &&
        ZwStartProfile &&
        ZwStopProfile) {
      initialized_ = true;
    }
  }
}

base::LazyInstance<ProfilerFuncs>::Leaky funcs = LAZY_INSTANCE_INITIALIZER;

}  // namespace


namespace base {
namespace win {

SamplingProfiler::SamplingProfiler() : is_started_(false) {
}

SamplingProfiler::~SamplingProfiler() {
  if (is_started_) {
    CHECK(Stop()) <<
        "Unable to stop sampling profiler, this will cause memory corruption.";
  }
}

bool SamplingProfiler::Initialize(HANDLE process,
                                  void* start,
                                  size_t size,
                                  size_t log2_bucket_size) {
  // You only get to initialize each instance once.
  DCHECK(!profile_handle_.IsValid());
  DCHECK(!is_started_);
  DCHECK(start != NULL);
  DCHECK_NE(0U, size);
  DCHECK_LE(2, log2_bucket_size);
  DCHECK_GE(32, log2_bucket_size);

  // Bail if the native functions weren't found.
  if (!funcs.Get().initialized_)
    return false;

  size_t bucket_size = 1 << log2_bucket_size;
  size_t num_buckets = (size + bucket_size - 1) / bucket_size;
  DCHECK(num_buckets != 0);
  buckets_.resize(num_buckets);

  // Get our affinity mask for the call below.
  DWORD_PTR process_affinity = 0;
  DWORD_PTR system_affinity = 0;
  if (!::GetProcessAffinityMask(process, &process_affinity, &system_affinity)) {
    LOG(ERROR) << "Failed to get process affinity mask.";
    return false;
  }

  HANDLE profile = NULL;
  NTSTATUS status =
      funcs.Get().ZwCreateProfile(&profile,
                                  process,
                                  start,
                                  static_cast<ULONG>(size),
                                  static_cast<ULONG>(log2_bucket_size),
                                  &buckets_[0],
                                  static_cast<ULONG>(
                                      sizeof(buckets_[0]) * num_buckets),
                                  ProfileTime,
                                  process_affinity);

  if (!NT_SUCCESS(status)) {
    // Might as well deallocate the buckets.
    buckets_.resize(0);
    LOG(ERROR) << "Failed to create profile, error 0x" << std::hex << status;
    return false;
  }

  DCHECK(profile != NULL);
  profile_handle_.Set(profile);

  return true;
}

bool SamplingProfiler::Start() {
  DCHECK(profile_handle_.IsValid());
  DCHECK(!is_started_);
  DCHECK(funcs.Get().initialized_);

  NTSTATUS status = funcs.Get().ZwStartProfile(profile_handle_.Get());
  if (!NT_SUCCESS(status))
    return false;

  is_started_ = true;

  return true;
}

bool SamplingProfiler::Stop() {
  DCHECK(profile_handle_.IsValid());
  DCHECK(is_started_);
  DCHECK(funcs.Get().initialized_);

  NTSTATUS status = funcs.Get().ZwStopProfile(profile_handle_.Get());
  if (!NT_SUCCESS(status))
    return false;
  is_started_ = false;

  return true;
}

bool SamplingProfiler::SetSamplingInterval(base::TimeDelta sampling_interval) {
  if (!funcs.Get().initialized_)
    return false;

  // According to Nebbet, the sampling interval is in units of 100ns.
  ULONG interval = sampling_interval.InMicroseconds() * 10;
  NTSTATUS status = funcs.Get().ZwSetIntervalProfile(interval, ProfileTime);
  if (!NT_SUCCESS(status))
    return false;

  return true;
}

bool SamplingProfiler::GetSamplingInterval(base::TimeDelta* sampling_interval) {
  DCHECK(sampling_interval != NULL);

  if (!funcs.Get().initialized_)
    return false;

  ULONG interval = 0;
  NTSTATUS status = funcs.Get().ZwQueryIntervalProfile(ProfileTime, &interval);
  if (!NT_SUCCESS(status))
    return false;

  // According to Nebbet, the sampling interval is in units of 100ns.
  *sampling_interval = base::TimeDelta::FromMicroseconds(interval / 10);

  return true;
}

}  // namespace win
}  // namespace base
