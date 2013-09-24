// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shortcut.h"

#include <shellapi.h>
#include <shlobj.h>
#include <propkey.h>

#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/scoped_comptr.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

// Initializes |i_shell_link| and |i_persist_file| (releasing them first if they
// are already initialized).
// If |shortcut| is not NULL, loads |shortcut| into |i_persist_file|.
// If any of the above steps fail, both |i_shell_link| and |i_persist_file| will
// be released.
void InitializeShortcutInterfaces(
    const wchar_t* shortcut,
    ScopedComPtr<IShellLink>* i_shell_link,
    ScopedComPtr<IPersistFile>* i_persist_file) {
  i_shell_link->Release();
  i_persist_file->Release();
  if (FAILED(i_shell_link->CreateInstance(CLSID_ShellLink, NULL,
                                          CLSCTX_INPROC_SERVER)) ||
      FAILED(i_persist_file->QueryFrom(*i_shell_link)) ||
      (shortcut && FAILED((*i_persist_file)->Load(shortcut, STGM_READWRITE)))) {
    i_shell_link->Release();
    i_persist_file->Release();
  }
}

}  // namespace

bool CreateOrUpdateShortcutLink(const FilePath& shortcut_path,
                                const ShortcutProperties& properties,
                                ShortcutOperation operation) {
  base::ThreadRestrictions::AssertIOAllowed();

  // A target is required unless |operation| is SHORTCUT_UPDATE_EXISTING.
  if (operation != SHORTCUT_UPDATE_EXISTING &&
      !(properties.options & ShortcutProperties::PROPERTIES_TARGET)) {
    NOTREACHED();
    return false;
  }

  bool shortcut_existed = PathExists(shortcut_path);

  // Interfaces to the old shortcut when replacing an existing shortcut.
  ScopedComPtr<IShellLink> old_i_shell_link;
  ScopedComPtr<IPersistFile> old_i_persist_file;

  // Interfaces to the shortcut being created/updated.
  ScopedComPtr<IShellLink> i_shell_link;
  ScopedComPtr<IPersistFile> i_persist_file;
  switch (operation) {
    case SHORTCUT_CREATE_ALWAYS:
      InitializeShortcutInterfaces(NULL, &i_shell_link, &i_persist_file);
      break;
    case SHORTCUT_UPDATE_EXISTING:
      InitializeShortcutInterfaces(shortcut_path.value().c_str(), &i_shell_link,
                                   &i_persist_file);
      break;
    case SHORTCUT_REPLACE_EXISTING:
      InitializeShortcutInterfaces(shortcut_path.value().c_str(),
                                   &old_i_shell_link, &old_i_persist_file);
      // Confirm |shortcut_path| exists and is a shortcut by verifying
      // |old_i_persist_file| was successfully initialized in the call above. If
      // so, initialize the interfaces to begin writing a new shortcut (to
      // overwrite the current one if successful).
      if (old_i_persist_file.get())
        InitializeShortcutInterfaces(NULL, &i_shell_link, &i_persist_file);
      break;
    default:
      NOTREACHED();
  }

  // Return false immediately upon failure to initialize shortcut interfaces.
  if (!i_persist_file.get())
    return false;

  if ((properties.options & ShortcutProperties::PROPERTIES_TARGET) &&
      FAILED(i_shell_link->SetPath(properties.target.value().c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) &&
      FAILED(i_shell_link->SetWorkingDirectory(
          properties.working_dir.value().c_str()))) {
    return false;
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    if (FAILED(i_shell_link->SetArguments(properties.arguments.c_str())))
      return false;
  } else if (old_i_persist_file.get()) {
    wchar_t current_arguments[MAX_PATH] = {0};
    if (SUCCEEDED(old_i_shell_link->GetArguments(current_arguments,
                                                 MAX_PATH))) {
      i_shell_link->SetArguments(current_arguments);
    }
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) &&
      FAILED(i_shell_link->SetDescription(properties.description.c_str()))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ICON) &&
      FAILED(i_shell_link->SetIconLocation(properties.icon.value().c_str(),
                                           properties.icon_index))) {
    return false;
  }

  bool has_app_id =
      (properties.options & ShortcutProperties::PROPERTIES_APP_ID) != 0;
  bool has_dual_mode =
      (properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE) != 0;
  if ((has_app_id || has_dual_mode) &&
      GetVersion() >= VERSION_WIN7) {
    ScopedComPtr<IPropertyStore> property_store;
    if (FAILED(property_store.QueryFrom(i_shell_link)) || !property_store.get())
      return false;

    if (has_app_id &&
        !SetAppIdForPropertyStore(property_store, properties.app_id.c_str())) {
      return false;
    }
    if (has_dual_mode &&
        !SetBooleanValueForPropertyStore(property_store,
                                         PKEY_AppUserModel_IsDualMode,
                                         properties.dual_mode)) {
      return false;
    }
  }

  // Release the interfaces to the old shortcut to make sure it doesn't prevent
  // overwriting it if needed.
  old_i_persist_file.Release();
  old_i_shell_link.Release();

  HRESULT result = i_persist_file->Save(shortcut_path.value().c_str(), TRUE);

  // Release the interfaces in case the SHChangeNotify call below depends on
  // the operations above being fully completed.
  i_persist_file.Release();
  i_shell_link.Release();

  // If we successfully created/updated the icon, notify the shell that we have
  // done so.
  const bool succeeded = SUCCEEDED(result);
  if (succeeded) {
    if (shortcut_existed) {
      // TODO(gab): SHCNE_UPDATEITEM might be sufficient here; further testing
      // required.
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    } else {
      SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, shortcut_path.value().c_str(),
                     NULL);
    }
  }

  return succeeded;
}

bool ResolveShortcut(const FilePath& shortcut_path,
                     FilePath* target_path,
                     string16* args) {
  base::ThreadRestrictions::AssertIOAllowed();

  HRESULT result;
  ScopedComPtr<IShellLink> i_shell_link;

  // Get pointer to the IShellLink interface.
  result = i_shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                       CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;

  ScopedComPtr<IPersistFile> persist;
  // Query IShellLink for the IPersistFile interface.
  result = persist.QueryFrom(i_shell_link);
  if (FAILED(result))
    return false;

  // Load the shell link.
  result = persist->Load(shortcut_path.value().c_str(), STGM_READ);
  if (FAILED(result))
    return false;

  WCHAR temp[MAX_PATH];
  if (target_path) {
    // Try to find the target of a shortcut.
    result = i_shell_link->Resolve(0, SLR_NO_UI | SLR_NOSEARCH);
    if (FAILED(result))
      return false;

    result = i_shell_link->GetPath(temp, MAX_PATH, NULL, SLGP_UNCPRIORITY);
    if (FAILED(result))
      return false;

    *target_path = FilePath(temp);
  }

  if (args) {
    result = i_shell_link->GetArguments(temp, MAX_PATH);
    if (FAILED(result))
      return false;

    *args = string16(temp);
  }
  return true;
}

bool TaskbarPinShortcutLink(const wchar_t* shortcut) {
  base::ThreadRestrictions::AssertIOAllowed();

  // "Pin to taskbar" is only supported after Win7.
  if (GetVersion() < VERSION_WIN7)
    return false;

  int result = reinterpret_cast<int>(ShellExecute(NULL, L"taskbarpin", shortcut,
      NULL, NULL, 0));
  return result > 32;
}

bool TaskbarUnpinShortcutLink(const wchar_t* shortcut) {
  base::ThreadRestrictions::AssertIOAllowed();

  // "Unpin from taskbar" is only supported after Win7.
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return false;

  int result = reinterpret_cast<int>(ShellExecute(NULL, L"taskbarunpin",
      shortcut, NULL, NULL, 0));
  return result > 32;
}

}  // namespace win
}  // namespace base
