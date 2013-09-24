// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Static class for hooking Win32 API routines.

// Some notes about how to hook Memory Allocation Routines in Windows.
//
// For our purposes we do not hook the libc routines.  There are two
// reasons for this.  First, the libc routines all go through HeapAlloc
// anyway.  So, it's redundant to log both HeapAlloc and malloc.
// Second, it can be tricky to hook in both static and dynamic linkages
// of libc.

#include <windows.h>

#include "memory_hook.h"
#include "memory_watcher.h"
#include "preamble_patcher.h"

// Calls GetProcAddress, but casts to the correct type.
#define GET_PROC_ADDRESS(hmodule, name) \
  ( (Type_##name)(::GetProcAddress(hmodule, #name)) )

// Macro to declare Patch functions.
#define DECLARE_PATCH(name) Patch<Type_##name> patch_##name

// Macro to install Patch functions.
#define INSTALL_PATCH(name)  do {                                       \
  patch_##name.set_original(GET_PROC_ADDRESS(hkernel32, ##name));       \
  patch_##name.Install(&Perftools_##name);                              \
} while (0)

// Macro to install Patch functions.
#define INSTALL_NTDLLPATCH(name)  do {                                  \
  patch_##name.set_original(GET_PROC_ADDRESS(hntdll, ##name));          \
  patch_##name.Install(&Perftools_##name);                              \
} while (0)

// Macro to uninstall Patch functions.
#define UNINSTALL_PATCH(name) patch_##name.Uninstall();



// Windows APIs to be hooked

// HeapAlloc routines
typedef HANDLE (WINAPI *Type_HeapCreate)(DWORD flOptions,
                                         SIZE_T dwInitialSize,
                                         SIZE_T dwMaximumSize);
typedef BOOL (WINAPI *Type_HeapDestroy)(HANDLE hHeap);
typedef LPVOID (WINAPI *Type_HeapAlloc)(HANDLE hHeap, DWORD dwFlags,
                                        DWORD_PTR dwBytes);
typedef LPVOID (WINAPI *Type_HeapReAlloc)(HANDLE hHeap, DWORD dwFlags,
                                          LPVOID lpMem, SIZE_T dwBytes);
typedef BOOL (WINAPI *Type_HeapFree)(HANDLE hHeap, DWORD dwFlags,
                                     LPVOID lpMem);

// GlobalAlloc routines
typedef HGLOBAL (WINAPI *Type_GlobalAlloc)(UINT uFlags, SIZE_T dwBytes);
typedef HGLOBAL (WINAPI *Type_GlobalReAlloc)(HGLOBAL hMem, SIZE_T dwBytes,
                                             UINT uFlags);
typedef HGLOBAL (WINAPI *Type_GlobalFree)(HGLOBAL hMem);

// LocalAlloc routines
typedef HLOCAL (WINAPI *Type_LocalAlloc)(UINT uFlags, SIZE_T uBytes);
typedef HLOCAL (WINAPI *Type_LocalReAlloc)(HLOCAL hMem, SIZE_T uBytes,
                                           UINT uFlags);
typedef HLOCAL (WINAPI *Type_LocalFree)(HLOCAL hMem);

// A Windows-API equivalent of mmap and munmap, for "anonymous regions"
typedef LPVOID (WINAPI *Type_VirtualAllocEx)(HANDLE process, LPVOID address,
                                             SIZE_T size, DWORD type,
                                             DWORD protect);
typedef BOOL (WINAPI *Type_VirtualFreeEx)(HANDLE process, LPVOID address,
                                          SIZE_T size, DWORD type);

// A Windows-API equivalent of mmap and munmap, for actual files
typedef LPVOID (WINAPI *Type_MapViewOfFile)(HANDLE hFileMappingObject,
                                            DWORD dwDesiredAccess,
                                            DWORD dwFileOffsetHigh,
                                            DWORD dwFileOffsetLow,
                                            SIZE_T dwNumberOfBytesToMap);
typedef LPVOID (WINAPI *Type_MapViewOfFileEx)(HANDLE hFileMappingObject,
                                              DWORD dwDesiredAccess,
                                              DWORD dwFileOffsetHigh,
                                              DWORD dwFileOffsetLow,
                                              SIZE_T dwNumberOfBytesToMap,
                                              LPVOID lpBaseAddress);
typedef BOOL (WINAPI *Type_UnmapViewOfFile)(LPVOID lpBaseAddress);

typedef DWORD (WINAPI *Type_NtUnmapViewOfSection)(HANDLE process,
                                                  LPVOID lpBaseAddress);


// Patch is a template for keeping the pointer to the original
// hooked routine, the function to call when hooked, and the
// stub routine which is patched.
template<class T>
class Patch {
 public:
  // Constructor.  Does not hook the function yet.
  Patch<T>()
    : original_function_(NULL),
      patch_function_(NULL),
      stub_function_(NULL) {
  }

  // Destructor.  Unhooks the function if it has been hooked.
  ~Patch<T>() {
    Uninstall();
  }

  // Patches original function with func.
  // Must have called set_original to set the original function.
  void Install(T func) {
    patch_function_ = func;
    CHECK(patch_function_ != NULL);
    CHECK(original_function_ != NULL);
    CHECK(stub_function_ == NULL);
    CHECK(sidestep::SIDESTEP_SUCCESS ==
          sidestep::PreamblePatcher::Patch(original_function_,
                                           patch_function_, &stub_function_));
  }

  // Un-patches the function.
  void Uninstall() {
    if (stub_function_)
      sidestep::PreamblePatcher::Unpatch(original_function_,
                                         patch_function_, stub_function_);
    stub_function_ = NULL;
  }

  // Set the function to be patched.
  void set_original(T original) { original_function_ = original; }

  // Get the original function being patched.
  T original() { return original_function_; }

  // Get the patched function.  (e.g. the replacement function)
  T patched() { return patch_function_; }

  // Access to the stub for calling the original function
  // while it is patched.
  T operator()() {
    DCHECK(stub_function_);
    return stub_function_;
  }

 private:
  // The function that we plan to patch.
  T original_function_;
  // The function to replace the original with.
  T patch_function_;
  // To unpatch, we also need to keep around a "stub" that points to the
  // pre-patched Windows function.
  T stub_function_;
};


// All Windows memory-allocation routines call through to one of these.
DECLARE_PATCH(HeapCreate);
DECLARE_PATCH(HeapDestroy);
DECLARE_PATCH(HeapAlloc);
DECLARE_PATCH(HeapReAlloc);
DECLARE_PATCH(HeapFree);
DECLARE_PATCH(VirtualAllocEx);
DECLARE_PATCH(VirtualFreeEx);
DECLARE_PATCH(MapViewOfFile);
DECLARE_PATCH(MapViewOfFileEx);
DECLARE_PATCH(UnmapViewOfFile);
DECLARE_PATCH(GlobalAlloc);
DECLARE_PATCH(GlobalReAlloc);
DECLARE_PATCH(GlobalFree);
DECLARE_PATCH(LocalAlloc);
DECLARE_PATCH(LocalReAlloc);
DECLARE_PATCH(LocalFree);
DECLARE_PATCH(NtUnmapViewOfSection);

// Our replacement functions.

static HANDLE WINAPI Perftools_HeapCreate(DWORD flOptions,
                                          SIZE_T dwInitialSize,
                                          SIZE_T dwMaximumSize) {
  if (dwInitialSize > 4096)
    dwInitialSize = 4096;
  return patch_HeapCreate()(flOptions, dwInitialSize, dwMaximumSize);
}

static BOOL WINAPI Perftools_HeapDestroy(HANDLE hHeap) {
  return patch_HeapDestroy()(hHeap);
}

static LPVOID WINAPI Perftools_HeapAlloc(HANDLE hHeap, DWORD dwFlags,
                                         DWORD_PTR dwBytes) {
  LPVOID rv = patch_HeapAlloc()(hHeap, dwFlags, dwBytes);
  MemoryHook::hook()->OnTrack(hHeap, reinterpret_cast<int32>(rv), dwBytes);
  return rv;
}

static BOOL WINAPI Perftools_HeapFree(HANDLE hHeap, DWORD dwFlags,
                                      LPVOID lpMem) {
  size_t size = 0;
  if (lpMem != 0) {
    size = HeapSize(hHeap, 0, lpMem);  // Will crash if lpMem is 0.
    // Note: size could be 0; HeapAlloc does allocate 0 length buffers.
  }
  MemoryHook::hook()->OnUntrack(hHeap, reinterpret_cast<int32>(lpMem), size);
  return patch_HeapFree()(hHeap, dwFlags, lpMem);
}

static LPVOID WINAPI Perftools_HeapReAlloc(HANDLE hHeap, DWORD dwFlags,
                                           LPVOID lpMem, SIZE_T dwBytes) {
  // Don't call realloc, but instead do a free/malloc.  The problem is that
  // the builtin realloc may either expand a buffer, or it may simply
  // just call free/malloc.  If so, we will already have tracked the new
  // block via Perftools_HeapAlloc.

  LPVOID rv = Perftools_HeapAlloc(hHeap, dwFlags, dwBytes);
  DCHECK_EQ((HEAP_REALLOC_IN_PLACE_ONLY & dwFlags), 0);

  // If there was an old buffer, now copy the data to the new buffer.
  if (lpMem != 0) {
    size_t size = HeapSize(hHeap, 0, lpMem);
    if (size > dwBytes)
      size = dwBytes;
    // Note: size could be 0; HeapAlloc does allocate 0 length buffers.
    memcpy(rv, lpMem, size);
    Perftools_HeapFree(hHeap, dwFlags, lpMem);
  }
  return rv;
}

static LPVOID WINAPI Perftools_VirtualAllocEx(HANDLE process, LPVOID address,
                                              SIZE_T size, DWORD type,
                                              DWORD protect) {
  bool already_committed = false;
  if (address != NULL) {
    MEMORY_BASIC_INFORMATION info;
    CHECK(VirtualQuery(address, &info, sizeof(info)));
    if (info.State & MEM_COMMIT) {
      already_committed = true;
      CHECK(size >= info.RegionSize);
    }
  }
  bool reserving = (address == NULL) || (type & MEM_RESERVE);
  bool committing = !already_committed && (type & MEM_COMMIT);


  LPVOID result = patch_VirtualAllocEx()(process, address, size, type,
                                         protect);
  MEMORY_BASIC_INFORMATION info;
  CHECK(VirtualQuery(result, &info, sizeof(info)));
  size = info.RegionSize;

  if (committing)
    MemoryHook::hook()->OnTrack(0, reinterpret_cast<int32>(result), size);

  return result;
}

static BOOL WINAPI Perftools_VirtualFreeEx(HANDLE process, LPVOID address,
                                           SIZE_T size, DWORD type) {
  int chunk_size = size;
  MEMORY_BASIC_INFORMATION info;
  CHECK(VirtualQuery(address, &info, sizeof(info)));
  if (chunk_size == 0)
    chunk_size = info.RegionSize;
  bool decommit = (info.State & MEM_COMMIT);

  if (decommit)
      MemoryHook::hook()->OnUntrack(0, reinterpret_cast<int32>(address),
                                     chunk_size);

  return patch_VirtualFreeEx()(process, address, size, type);
}

static base::Lock known_maps_lock;
static std::map<void*, int> known_maps;

static LPVOID WINAPI Perftools_MapViewOfFileEx(HANDLE hFileMappingObject,
                                               DWORD dwDesiredAccess,
                                               DWORD dwFileOffsetHigh,
                                               DWORD dwFileOffsetLow,
                                               SIZE_T dwNumberOfBytesToMap,
                                               LPVOID lpBaseAddress) {
  // For this function pair, you always deallocate the full block of
  // data that you allocate, so NewHook/DeleteHook is the right API.
  LPVOID result = patch_MapViewOfFileEx()(hFileMappingObject, dwDesiredAccess,
                                           dwFileOffsetHigh, dwFileOffsetLow,
                                           dwNumberOfBytesToMap, lpBaseAddress);
  {
    base::AutoLock lock(known_maps_lock);
    MEMORY_BASIC_INFORMATION info;
    if (known_maps.find(result) == known_maps.end()) {
      CHECK(VirtualQuery(result, &info, sizeof(info)));
      // TODO(mbelshe):  THIS map uses the standard heap!!!!
      known_maps[result] = 1;
      MemoryHook::hook()->OnTrack(0, reinterpret_cast<int32>(result),
                                  info.RegionSize);
    } else {
      known_maps[result] = known_maps[result] + 1;
    }
  }
  return result;
}

static LPVOID WINAPI Perftools_MapViewOfFile(HANDLE hFileMappingObject,
                                               DWORD dwDesiredAccess,
                                               DWORD dwFileOffsetHigh,
                                               DWORD dwFileOffsetLow,
                                               SIZE_T dwNumberOfBytesToMap) {
  return Perftools_MapViewOfFileEx(hFileMappingObject, dwDesiredAccess,
                                   dwFileOffsetHigh, dwFileOffsetLow,
                                   dwNumberOfBytesToMap, 0);
}

static BOOL WINAPI Perftools_UnmapViewOfFile(LPVOID lpBaseAddress) {
  // This will call into NtUnmapViewOfSection().
  return patch_UnmapViewOfFile()(lpBaseAddress);
}

static DWORD WINAPI Perftools_NtUnmapViewOfSection(HANDLE process,
                                                   LPVOID lpBaseAddress) {
  // Some windows APIs call directly into this routine rather
  // than calling UnmapViewOfFile.  If we didn't trap this function,
  // then we appear to have bogus leaks.
  {
    base::AutoLock lock(known_maps_lock);
    MEMORY_BASIC_INFORMATION info;
    CHECK(VirtualQuery(lpBaseAddress, &info, sizeof(info)));
    if (known_maps.find(lpBaseAddress) != known_maps.end()) {
      if (known_maps[lpBaseAddress] == 1) {
        MemoryHook::hook()->OnUntrack(0, reinterpret_cast<int32>(lpBaseAddress),
                                       info.RegionSize);
        known_maps.erase(lpBaseAddress);
      } else {
        known_maps[lpBaseAddress] = known_maps[lpBaseAddress] - 1;
      }
    }
  }
  return patch_NtUnmapViewOfSection()(process, lpBaseAddress);
}

static HGLOBAL WINAPI Perftools_GlobalAlloc(UINT uFlags, SIZE_T dwBytes) {
  // GlobalAlloc is built atop HeapAlloc anyway.  So we don't track these.
  // GlobalAlloc will internally call into HeapAlloc and we track there.

  // Force all memory to be fixed.
  uFlags &= ~GMEM_MOVEABLE;
  HGLOBAL rv = patch_GlobalAlloc()(uFlags, dwBytes);
  return rv;
}

static HGLOBAL WINAPI Perftools_GlobalFree(HGLOBAL hMem) {
  return patch_GlobalFree()(hMem);
}

static HGLOBAL WINAPI Perftools_GlobalReAlloc(HGLOBAL hMem, SIZE_T dwBytes,
                                              UINT uFlags) {
  // TODO(jar): [The following looks like a copy/paste typo from LocalRealloc.]
  // GlobalDiscard is a macro which calls LocalReAlloc with size 0.
  if (dwBytes == 0) {
    return patch_GlobalReAlloc()(hMem, dwBytes, uFlags);
  }

  HGLOBAL rv = Perftools_GlobalAlloc(uFlags, dwBytes);
  if (hMem != 0) {
    size_t size = GlobalSize(hMem);
    if (size > dwBytes)
      size = dwBytes;
    // Note: size could be 0; HeapAlloc does allocate 0 length buffers.
    memcpy(rv, hMem, size);
    Perftools_GlobalFree(hMem);
  }

  return rv;
}

static HLOCAL WINAPI Perftools_LocalAlloc(UINT uFlags, SIZE_T dwBytes) {
  // LocalAlloc is built atop HeapAlloc anyway.  So we don't track these.
  // LocalAlloc will internally call into HeapAlloc and we track there.

  // Force all memory to be fixed.
  uFlags &= ~LMEM_MOVEABLE;
  HLOCAL rv = patch_LocalAlloc()(uFlags, dwBytes);
  return rv;
}

static HLOCAL WINAPI Perftools_LocalFree(HLOCAL hMem) {
  return patch_LocalFree()(hMem);
}

static HLOCAL WINAPI Perftools_LocalReAlloc(HLOCAL hMem, SIZE_T dwBytes,
                                            UINT uFlags) {
  // LocalDiscard is a macro which calls LocalReAlloc with size 0.
  if (dwBytes == 0) {
    return patch_LocalReAlloc()(hMem, dwBytes, uFlags);
  }

  HGLOBAL rv = Perftools_LocalAlloc(uFlags, dwBytes);
  if (hMem != 0) {
    size_t size = LocalSize(hMem);
    if (size > dwBytes)
      size = dwBytes;
    // Note: size could be 0; HeapAlloc does allocate 0 length buffers.
    memcpy(rv, hMem, size);
    Perftools_LocalFree(hMem);
  }

  return rv;
}

bool MemoryHook::hooked_ = false;
MemoryHook* MemoryHook::global_hook_ = NULL;

MemoryHook::MemoryHook()
  : watcher_(NULL),
    heap_(NULL) {
  CreateHeap();
}

MemoryHook::~MemoryHook() {
  // It's a bit dangerous to ever close this heap; MemoryWatchers may have
  // used this heap for their tracking data.  Closing the heap while any
  // MemoryWatchers still exist is pretty dangerous.
  CloseHeap();
}

bool MemoryHook::Initialize() {
  if (global_hook_ == NULL)
    global_hook_ = new MemoryHook();
  return true;
}

bool MemoryHook::Hook() {
  DCHECK(!hooked_);
  if (!hooked_) {
    DCHECK(global_hook_);

    // Luckily, Patch() doesn't call malloc or windows alloc routines
    // itself -- though it does call new (we can use PatchWithStub to
    // get around that, and will need to if we need to patch new).

    HMODULE hkernel32 = ::GetModuleHandle(L"kernel32");
    CHECK(hkernel32 != NULL);

    HMODULE hntdll = ::GetModuleHandle(L"ntdll");
    CHECK(hntdll != NULL);

    // Now that we've found all the functions, patch them
    INSTALL_PATCH(HeapCreate);
    INSTALL_PATCH(HeapDestroy);
    INSTALL_PATCH(HeapAlloc);
    INSTALL_PATCH(HeapReAlloc);
    INSTALL_PATCH(HeapFree);
    INSTALL_PATCH(VirtualAllocEx);
    INSTALL_PATCH(VirtualFreeEx);
    INSTALL_PATCH(MapViewOfFileEx);
    INSTALL_PATCH(MapViewOfFile);
    INSTALL_PATCH(UnmapViewOfFile);
    INSTALL_NTDLLPATCH(NtUnmapViewOfSection);
    INSTALL_PATCH(GlobalAlloc);
    INSTALL_PATCH(GlobalReAlloc);
    INSTALL_PATCH(GlobalFree);
    INSTALL_PATCH(LocalAlloc);
    INSTALL_PATCH(LocalReAlloc);
    INSTALL_PATCH(LocalFree);

    // We are finally completely hooked.
    hooked_ = true;
  }
  return true;
}

bool MemoryHook::Unhook() {
  if (hooked_) {
    // We need to go back to the system malloc/etc at global destruct time,
    // so objects that were constructed before tcmalloc, using the system
    // malloc, can destroy themselves using the system free.  This depends
    // on DLLs unloading in the reverse order in which they load!
    //
    // We also go back to the default HeapAlloc/etc, just for consistency.
    // Who knows, it may help avoid weird bugs in some situations.
    UNINSTALL_PATCH(HeapCreate);
    UNINSTALL_PATCH(HeapDestroy);
    UNINSTALL_PATCH(HeapAlloc);
    UNINSTALL_PATCH(HeapReAlloc);
    UNINSTALL_PATCH(HeapFree);
    UNINSTALL_PATCH(VirtualAllocEx);
    UNINSTALL_PATCH(VirtualFreeEx);
    UNINSTALL_PATCH(MapViewOfFile);
    UNINSTALL_PATCH(MapViewOfFileEx);
    UNINSTALL_PATCH(UnmapViewOfFile);
    UNINSTALL_PATCH(NtUnmapViewOfSection);
    UNINSTALL_PATCH(GlobalAlloc);
    UNINSTALL_PATCH(GlobalReAlloc);
    UNINSTALL_PATCH(GlobalFree);
    UNINSTALL_PATCH(LocalAlloc);
    UNINSTALL_PATCH(LocalReAlloc);
    UNINSTALL_PATCH(LocalFree);

    hooked_ = false;
  }
  return true;
}

bool MemoryHook::RegisterWatcher(MemoryObserver* watcher) {
  DCHECK(global_hook_->watcher_ == NULL);

  if (!hooked_)
    Hook();

  DCHECK(global_hook_);
  global_hook_->watcher_ = watcher;
  return true;
}

bool MemoryHook::UnregisterWatcher(MemoryObserver* watcher) {
  DCHECK(hooked_);
  DCHECK(global_hook_->watcher_ == watcher);
  // TODO(jar): changing watcher_ here is very racy.  Other threads may (without
  // a lock) testing, and then calling through this value.  We probably can't
  // remove this until we are single threaded.
  global_hook_->watcher_ = NULL;

  // For now, since there are no more watchers, unhook memory.
  return Unhook();
}

bool MemoryHook::CreateHeap() {
  // Create a heap for our own memory.
  DCHECK(heap_ == NULL);
  heap_ = HeapCreate(0, 0, 0);
  DCHECK(heap_ != NULL);
  return heap_ != NULL;
}

bool MemoryHook::CloseHeap() {
  DCHECK(heap_ != NULL);
  HeapDestroy(heap_);
  heap_ = NULL;
  return true;
}

void MemoryHook::OnTrack(HANDLE heap, int32 id, int32 size) {
  // Don't notify about allocations to our internal heap.
  if (heap == heap_)
    return;

  if (watcher_)
    watcher_->OnTrack(heap, id, size);
}

void MemoryHook::OnUntrack(HANDLE heap, int32 id, int32 size) {
  // Don't notify about allocations to our internal heap.
  if (heap == heap_)
    return;

  if (watcher_)
    watcher_->OnUntrack(heap, id, size);
}
