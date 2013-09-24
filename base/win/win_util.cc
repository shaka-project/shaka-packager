// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/win_util.h"

#include <aclapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>  // Must be before propkey.
#include <initguid.h>
#include <propkey.h>
#include <propvarutil.h>
#include <sddl.h>
#include <signal.h>
#include <stdlib.h>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"

namespace {

// Sets the value of |property_key| to |property_value| in |property_store|.
bool SetPropVariantValueForPropertyStore(
    IPropertyStore* property_store,
    const PROPERTYKEY& property_key,
    const base::win::ScopedPropVariant& property_value) {
  DCHECK(property_store);

  HRESULT result = property_store->SetValue(property_key, property_value.get());
  if (result == S_OK)
    result = property_store->Commit();
  return SUCCEEDED(result);
}

void __cdecl ForceCrashOnSigAbort(int) {
  *((int*)0) = 0x1337;
}

const wchar_t kWindows8OSKRegPath[] =
    L"Software\\Classes\\CLSID\\{054AAE20-4BEA-4347-8A35-64A533254A9D}"
    L"\\LocalServer32";

}  // namespace

namespace base {
namespace win {

static bool g_crash_on_process_detach = false;

#define NONCLIENTMETRICS_SIZE_PRE_VISTA \
    SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(NONCLIENTMETRICS, lfMessageFont)

void GetNonClientMetrics(NONCLIENTMETRICS* metrics) {
  DCHECK(metrics);

  static const UINT SIZEOF_NONCLIENTMETRICS =
      (base::win::GetVersion() >= base::win::VERSION_VISTA) ?
      sizeof(NONCLIENTMETRICS) : NONCLIENTMETRICS_SIZE_PRE_VISTA;
  metrics->cbSize = SIZEOF_NONCLIENTMETRICS;
  const bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                              SIZEOF_NONCLIENTMETRICS, metrics,
                                              0);
  DCHECK(success);
}

bool GetUserSidString(std::wstring* user_sid) {
  // Get the current token.
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  base::win::ScopedHandle token_scoped(token);

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  scoped_ptr<BYTE[]> user_bytes(new BYTE[size]);
  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

  if (!::GetTokenInformation(token, TokenUser, user, size, &size))
    return false;

  if (!user->User.Sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!::ConvertSidToStringSid(user->User.Sid, &sid_string))
    return false;

  *user_sid = sid_string;

  ::LocalFree(sid_string);

  return true;
}

bool IsShiftPressed() {
  return (::GetKeyState(VK_SHIFT) & 0x8000) == 0x8000;
}

bool IsCtrlPressed() {
  return (::GetKeyState(VK_CONTROL) & 0x8000) == 0x8000;
}

bool IsAltPressed() {
  return (::GetKeyState(VK_MENU) & 0x8000) == 0x8000;
}

bool UserAccountControlIsEnabled() {
  // This can be slow if Windows ends up going to disk.  Should watch this key
  // for changes and only read it once, preferably on the file thread.
  //   http://code.google.com/p/chromium/issues/detail?id=61644
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  base::win::RegKey key(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
      KEY_READ);
  DWORD uac_enabled;
  if (key.ReadValueDW(L"EnableLUA", &uac_enabled) != ERROR_SUCCESS)
    return true;
  // Users can set the EnableLUA value to something arbitrary, like 2, which
  // Vista will treat as UAC enabled, so we make sure it is not set to 0.
  return (uac_enabled != 0);
}

bool SetBooleanValueForPropertyStore(IPropertyStore* property_store,
                                     const PROPERTYKEY& property_key,
                                     bool property_bool_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromBoolean(property_bool_value,
                                        property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store,
                                             property_key,
                                             property_value);
}

bool SetStringValueForPropertyStore(IPropertyStore* property_store,
                                    const PROPERTYKEY& property_key,
                                    const wchar_t* property_string_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromString(property_string_value,
                                       property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store,
                                             property_key,
                                             property_value);
}

bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                              const wchar_t* app_id) {
  // App id should be less than 64 chars and contain no space. And recommended
  // format is CompanyName.ProductName[.SubProduct.ProductNumber].
  // See http://msdn.microsoft.com/en-us/library/dd378459%28VS.85%29.aspx
  DCHECK(lstrlen(app_id) < 64 && wcschr(app_id, L' ') == NULL);

  return SetStringValueForPropertyStore(property_store,
                                        PKEY_AppUserModel_ID,
                                        app_id);
}

static const char16 kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool AddCommandToAutoRun(HKEY root_key, const string16& name,
                         const string16& command) {
  base::win::RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.WriteValue(name.c_str(), command.c_str()) ==
      ERROR_SUCCESS);
}

bool RemoveCommandFromAutoRun(HKEY root_key, const string16& name) {
  base::win::RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.DeleteValue(name.c_str()) == ERROR_SUCCESS);
}

bool ReadCommandFromAutoRun(HKEY root_key,
                            const string16& name,
                            string16* command) {
  base::win::RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_QUERY_VALUE);
  return (autorun_key.ReadValue(name.c_str(), command) == ERROR_SUCCESS);
}

void SetShouldCrashOnProcessDetach(bool crash) {
  g_crash_on_process_detach = crash;
}

bool ShouldCrashOnProcessDetach() {
  return g_crash_on_process_detach;
}

void SetAbortBehaviorForCrashReporting() {
  // Prevent CRT's abort code from prompting a dialog or trying to "report" it.
  // Disabling the _CALL_REPORTFAULT behavior is important since otherwise it
  // has the sideffect of clearing our exception filter, which means we
  // don't get any crash.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

  // Set a SIGABRT handler for good measure. We will crash even if the default
  // is left in place, however this allows us to crash earlier. And it also
  // lets us crash in response to code which might directly call raise(SIGABRT)
  signal(SIGABRT, ForceCrashOnSigAbort);
}

bool IsTouchEnabledDevice() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;
  const int kMultiTouch = NID_INTEGRATED_TOUCH | NID_MULTI_INPUT | NID_READY;
  int sm = GetSystemMetrics(SM_DIGITIZER);
  if ((sm & kMultiTouch) == kMultiTouch) {
    return true;
  }
  return false;
}

bool DisplayVirtualKeyboard() {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;

  static base::LazyInstance<string16>::Leaky osk_path =
      LAZY_INSTANCE_INITIALIZER;

  if (osk_path.Get().empty()) {
    // We need to launch TabTip.exe from the location specified under the
    // LocalServer32 key for the {{054AAE20-4BEA-4347-8A35-64A533254A9D}}
    // CLSID.
    // TabTip.exe is typically found at
    // c:\program files\common files\microsoft shared\ink on English Windows.
    // We don't want to launch TabTip.exe from
    // c:\program files (x86)\common files\microsoft shared\ink. This path is
    // normally found on 64 bit Windows.
    base::win::RegKey key(HKEY_LOCAL_MACHINE,
                          kWindows8OSKRegPath,
                          KEY_READ | KEY_WOW64_64KEY);
    DWORD osk_path_length = 1024;
    if (key.ReadValue(NULL,
                      WriteInto(&osk_path.Get(), osk_path_length),
                      &osk_path_length,
                      NULL) != ERROR_SUCCESS) {
      DLOG(WARNING) << "Failed to read on screen keyboard path from registry";
      return false;
    }
    size_t common_program_files_offset =
        osk_path.Get().find(L"%CommonProgramFiles%");
    // Typically the path to TabTip.exe read from the registry will start with
    // %CommonProgramFiles% which needs to be replaced with the corrsponding
    // expanded string.
    // If the path does not begin with %CommonProgramFiles% we use it as is.
    if (common_program_files_offset != string16::npos) {
      // Preserve the beginning quote in the path.
      osk_path.Get().erase(common_program_files_offset,
                           wcslen(L"%CommonProgramFiles%"));
      // The path read from the registry contains the %CommonProgramFiles%
      // environment variable prefix. On 64 bit Windows the SHGetKnownFolderPath
      // function returns the common program files path with the X86 suffix for
      // the FOLDERID_ProgramFilesCommon value.
      // To get the correct path to TabTip.exe we first read the environment
      // variable CommonProgramW6432 which points to the desired common
      // files path. Failing that we fallback to the SHGetKnownFolderPath API.

      // We then replace the %CommonProgramFiles% value with the actual common
      // files path found in the process.
      string16 common_program_files_path;
      scoped_ptr<wchar_t[]> common_program_files_wow6432;
      DWORD buffer_size =
          GetEnvironmentVariable(L"CommonProgramW6432", NULL, 0);
      if (buffer_size) {
        common_program_files_wow6432.reset(new wchar_t[buffer_size]);
        GetEnvironmentVariable(L"CommonProgramW6432",
                               common_program_files_wow6432.get(),
                               buffer_size);
        common_program_files_path = common_program_files_wow6432.get();
        DCHECK(!common_program_files_path.empty());
      } else {
        base::win::ScopedCoMem<wchar_t> common_program_files;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL,
                                        &common_program_files))) {
          return false;
        }
        common_program_files_path = common_program_files;
      }

      osk_path.Get().insert(1, common_program_files_path);
    }
  }

  HINSTANCE ret = ::ShellExecuteW(NULL,
                                  L"",
                                  osk_path.Get().c_str(),
                                  NULL,
                                  NULL,
                                  SW_SHOW);
  return reinterpret_cast<int>(ret) > 32;
}

bool DismissVirtualKeyboard() {
  if (base::win::GetVersion() < base::win::VERSION_WIN8)
    return false;

  // We dismiss the virtual keyboard by generating the ESC keystroke
  // programmatically.
  const wchar_t kOSKClassName[] = L"IPTip_Main_Window";
  HWND osk = ::FindWindow(kOSKClassName, NULL);
  if (::IsWindow(osk) && ::IsWindowEnabled(osk)) {
    PostMessage(osk, WM_SYSCOMMAND, SC_CLOSE, 0);
    return true;
  }
  return false;
}

}  // namespace win
}  // namespace base

#ifdef _MSC_VER

// There are optimizer bugs in x86 VS2012 pre-Update 1.
#if _MSC_VER == 1700 && defined _M_IX86 && _MSC_FULL_VER < 170051106

#pragma message("Relevant defines:")
#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __PPOUT__(x) "#define " #x " " __STR1__(x)
#if defined(_M_IX86)
  #pragma message(__PPOUT__(_M_IX86))
#endif
#if defined(_M_X64)
  #pragma message(__PPOUT__(_M_X64))
#endif
#if defined(_MSC_FULL_VER)
  #pragma message(__PPOUT__(_MSC_FULL_VER))
#endif

#pragma message("Visual Studio 2012 x86 must be updated to at least Update 1")
#error Must install Update 1 to Visual Studio 2012.
#endif

#endif  // _MSC_VER

