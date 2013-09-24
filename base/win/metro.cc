// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/metro.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {
bool g_should_tsf_aware_required = false;
}

HMODULE GetMetroModule() {
  const HMODULE kUninitialized = reinterpret_cast<HMODULE>(1);
  static HMODULE metro_module = kUninitialized;

  if (metro_module == kUninitialized) {
    // Initialize the cache, note that the initialization is idempotent
    // under the assumption that metro_driver is never unloaded, so the
    // race to this assignment is safe.
    metro_module = GetModuleHandleA("metro_driver.dll");
    if (metro_module != NULL) {
      // This must be a metro process if the metro_driver is loaded.
      DCHECK(IsMetroProcess());
    }
  }

  DCHECK(metro_module != kUninitialized);
  return metro_module;
}

bool IsMetroProcess() {
  enum ImmersiveState {
    kImmersiveUnknown,
    kImmersiveTrue,
    kImmersiveFalse
  };
  // The immersive state of a process can never change.
  // Look it up once and cache it here.
  static ImmersiveState state = kImmersiveUnknown;

  if (state == kImmersiveUnknown) {
    if (IsProcessImmersive(::GetCurrentProcess())) {
      state = kImmersiveTrue;
    } else {
      state = kImmersiveFalse;
    }
  }
  DCHECK_NE(kImmersiveUnknown, state);
  return state == kImmersiveTrue;
}

bool IsProcessImmersive(HANDLE process) {
  typedef BOOL (WINAPI* IsImmersiveProcessFunc)(HANDLE process);
  HMODULE user32 = ::GetModuleHandleA("user32.dll");
  DCHECK(user32 != NULL);

  IsImmersiveProcessFunc is_immersive_process =
      reinterpret_cast<IsImmersiveProcessFunc>(
          ::GetProcAddress(user32, "IsImmersiveProcess"));

  if (is_immersive_process)
    return is_immersive_process(process) ? true: false;
  return false;
}

bool IsTSFAwareRequired() {
#if defined(USE_AURA)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8)
    return true;
#endif
  // Although this function is equal to IsMetroProcess at this moment,
  // Chrome for Win7 and Vista may support TSF in the future.
  return g_should_tsf_aware_required || IsMetroProcess();
}

void SetForceToUseTSF() {
  g_should_tsf_aware_required = true;

  // Since Windows 8 Metro mode disables CUAS (Cicero Unaware Application
  // Support) via ImmDisableLegacyIME API, Chrome must be fully TSF-aware on
  // Metro mode. For debugging purposes, explicitly call ImmDisableLegacyIME so
  // that one can test TSF functionality even on Windows 8 desktop mode. Note
  // that CUAS cannot be disabled on Windows Vista/7 where ImmDisableLegacyIME
  // is not available.
  typedef BOOL (* ImmDisableLegacyIMEFunc)();
  HMODULE imm32 = ::GetModuleHandleA("imm32.dll");
  if (imm32 == NULL)
    return;

  ImmDisableLegacyIMEFunc imm_disable_legacy_ime =
      reinterpret_cast<ImmDisableLegacyIMEFunc>(
          ::GetProcAddress(imm32, "ImmDisableLegacyIME"));

  if (imm_disable_legacy_ime == NULL) {
    // Unsupported API, just do nothing.
    return;
  }

  if (!imm_disable_legacy_ime()) {
    DVLOG(1) << "Failed to disable legacy IME.";
  }
}

wchar_t* LocalAllocAndCopyString(const string16& src) {
  size_t dest_size = (src.length() + 1) * sizeof(wchar_t);
  wchar_t* dest = reinterpret_cast<wchar_t*>(LocalAlloc(LPTR, dest_size));
  base::wcslcpy(dest, src.c_str(), dest_size);
  return dest;
}

bool IsParentalControlActivityLoggingOn() {
  // Query this info on Windows Vista and above.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  static bool parental_control_logging_required = false;
  static bool parental_control_status_determined = false;

  if (parental_control_status_determined)
    return parental_control_logging_required;

  parental_control_status_determined = true;

  ScopedComPtr<IWindowsParentalControlsCore> parent_controls;
  HRESULT hr = parent_controls.CreateInstance(
      __uuidof(WindowsParentalControls));
  if (FAILED(hr))
    return false;

  ScopedComPtr<IWPCSettings> settings;
  hr = parent_controls->GetUserSettings(NULL, settings.Receive());
  if (FAILED(hr))
    return false;

  unsigned long restrictions = 0;
  settings->GetRestrictions(&restrictions);

  parental_control_logging_required =
      (restrictions & WPCFLAG_LOGGING_REQUIRED) == WPCFLAG_LOGGING_REQUIRED;
  return parental_control_logging_required;
}

// Metro driver exports for getting the launch type, initial url, initial
// search term, etc.
extern "C" {
typedef const wchar_t* (*GetInitialUrl)();
typedef const wchar_t* (*GetInitialSearchString)();
typedef base::win::MetroLaunchType (*GetLaunchType)(
    base::win::MetroPreviousExecutionState* previous_state);
}

MetroLaunchType GetMetroLaunchParams(string16* params) {
  HMODULE metro = base::win::GetMetroModule();
  if (!metro)
    return base::win::METRO_LAUNCH_ERROR;

  GetLaunchType get_launch_type = reinterpret_cast<GetLaunchType>(
      ::GetProcAddress(metro, "GetLaunchType"));
  DCHECK(get_launch_type);

  base::win::MetroLaunchType launch_type = get_launch_type(NULL);

  if ((launch_type == base::win::METRO_PROTOCOL) ||
      (launch_type == base::win::METRO_LAUNCH)) {
    GetInitialUrl initial_metro_url = reinterpret_cast<GetInitialUrl>(
        ::GetProcAddress(metro, "GetInitialUrl"));
    DCHECK(initial_metro_url);
    *params = initial_metro_url();
  } else if (launch_type == base::win::METRO_SEARCH) {
    GetInitialSearchString initial_search_string =
        reinterpret_cast<GetInitialSearchString>(
            ::GetProcAddress(metro, "GetInitialSearchString"));
    DCHECK(initial_search_string);
    *params = initial_search_string();
  }
  return launch_type;
}

}  // namespace win
}  // namespace base
