// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"

#import <Cocoa/Cocoa.h>
#import <IOKit/IOKitLib.h>

#include <errno.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/xattr.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"

namespace base {
namespace mac {

// Replicate specific 10.7 SDK declarations for building with prior SDKs.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

enum {
  NSApplicationPresentationFullScreen = 1 << 10
};

#endif  // MAC_OS_X_VERSION_10_7

namespace {

// The current count of outstanding requests for full screen mode from browser
// windows, plugins, etc.
int g_full_screen_requests[kNumFullScreenModes] = { 0 };

// Sets the appropriate application presentation option based on the current
// full screen requests.  Since only one presentation option can be active at a
// given time, full screen requests are ordered by priority.  If there are no
// outstanding full screen requests, reverts to normal mode.  If the correct
// presentation option is already set, does nothing.
void SetUIMode() {
  NSApplicationPresentationOptions current_options =
      [NSApp presentationOptions];

  // Determine which mode should be active, based on which requests are
  // currently outstanding.  More permissive requests take precedence.  For
  // example, plugins request |kFullScreenModeAutoHideAll|, while browser
  // windows request |kFullScreenModeHideDock| when the fullscreen overlay is
  // down.  Precedence goes to plugins in this case, so AutoHideAll wins over
  // HideDock.
  NSApplicationPresentationOptions desired_options =
      NSApplicationPresentationDefault;
  if (g_full_screen_requests[kFullScreenModeAutoHideAll] > 0) {
    desired_options = NSApplicationPresentationHideDock |
                      NSApplicationPresentationAutoHideMenuBar;
  } else if (g_full_screen_requests[kFullScreenModeHideDock] > 0) {
    desired_options = NSApplicationPresentationHideDock;
  } else if (g_full_screen_requests[kFullScreenModeHideAll] > 0) {
    desired_options = NSApplicationPresentationHideDock |
                      NSApplicationPresentationHideMenuBar;
  }

  // Mac OS X bug: if the window is fullscreened (Lion-style) and
  // NSApplicationPresentationDefault is requested, the result is that the menu
  // bar doesn't auto-hide. rdar://13576498 http://www.openradar.me/13576498
  //
  // As a workaround, in that case, explicitly set the presentation options to
  // the ones that are set by the system as it fullscreens a window.
  if (desired_options == NSApplicationPresentationDefault &&
      current_options & NSApplicationPresentationFullScreen) {
    desired_options |= NSApplicationPresentationFullScreen |
                       NSApplicationPresentationAutoHideMenuBar |
                       NSApplicationPresentationAutoHideDock;
  }

  if (current_options != desired_options)
    [NSApp setPresentationOptions:desired_options];
}

// Looks into Shared File Lists corresponding to Login Items for the item
// representing the current application.  If such an item is found, returns a
// retained reference to it. Caller is responsible for releasing the reference.
LSSharedFileListItemRef GetLoginItemForApp() {
  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    DLOG(ERROR) << "Couldn't get a Login Items list.";
    return NULL;
  }

  base::scoped_nsobject<NSArray> login_items_array(
      CFToNSCast(LSSharedFileListCopySnapshot(login_items, NULL)));

  NSURL* url = [NSURL fileURLWithPath:[base::mac::MainBundle() bundlePath]];

  for(NSUInteger i = 0; i < [login_items_array count]; ++i) {
    LSSharedFileListItemRef item = reinterpret_cast<LSSharedFileListItemRef>(
        [login_items_array objectAtIndex:i]);
    CFURLRef item_url_ref = NULL;

    if (LSSharedFileListItemResolve(item, 0, &item_url_ref, NULL) == noErr) {
      ScopedCFTypeRef<CFURLRef> item_url(item_url_ref);
      if (CFEqual(item_url, url)) {
        CFRetain(item);
        return item;
      }
    }
  }

  return NULL;
}

bool IsHiddenLoginItem(LSSharedFileListItemRef item) {
  ScopedCFTypeRef<CFBooleanRef> hidden(reinterpret_cast<CFBooleanRef>(
      LSSharedFileListItemCopyProperty(item,
          reinterpret_cast<CFStringRef>(kLSSharedFileListLoginItemHidden))));

  return hidden && hidden == kCFBooleanTrue;
}

}  // namespace

std::string PathFromFSRef(const FSRef& ref) {
  ScopedCFTypeRef<CFURLRef> url(
      CFURLCreateFromFSRef(kCFAllocatorDefault, &ref));
  NSString *path_string = [(NSURL *)url.get() path];
  return [path_string fileSystemRepresentation];
}

bool FSRefFromPath(const std::string& path, FSRef* ref) {
  OSStatus status = FSPathMakeRef((const UInt8*)path.c_str(),
                                  ref, nil);
  return status == noErr;
}

CGColorSpaceRef GetGenericRGBColorSpace() {
  // Leaked. That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_generic_rgb(
      CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB));
  DLOG_IF(ERROR, !g_color_space_generic_rgb) <<
      "Couldn't get the generic RGB color space";
  return g_color_space_generic_rgb;
}

CGColorSpaceRef GetSRGBColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  static CGColorSpaceRef g_color_space_sRGB =
      CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  DLOG_IF(ERROR, !g_color_space_sRGB) << "Couldn't get the sRGB color space";
  return g_color_space_sRGB;
}

CGColorSpaceRef GetSystemColorSpace() {
  // Leaked.  That's OK, it's scoped to the lifetime of the application.
  // Try to get the main display's color space.
  static CGColorSpaceRef g_system_color_space =
      CGDisplayCopyColorSpace(CGMainDisplayID());

  if (!g_system_color_space) {
    // Use a generic RGB color space.  This is better than nothing.
    g_system_color_space = CGColorSpaceCreateDeviceRGB();

    if (g_system_color_space) {
      DLOG(WARNING) <<
          "Couldn't get the main display's color space, using generic";
    } else {
      DLOG(ERROR) << "Couldn't get any color space";
    }
  }

  return g_system_color_space;
}

// Add a request for full screen mode.  Must be called on the main thread.
void RequestFullScreen(FullScreenMode mode) {
  DCHECK_LT(mode, kNumFullScreenModes);
  if (mode >= kNumFullScreenModes)
    return;

  DCHECK_GE(g_full_screen_requests[mode], 0);
  if (mode < 0)
    return;

  g_full_screen_requests[mode] = std::max(g_full_screen_requests[mode] + 1, 1);
  SetUIMode();
}

// Release a request for full screen mode.  Must be called on the main thread.
void ReleaseFullScreen(FullScreenMode mode) {
  DCHECK_LT(mode, kNumFullScreenModes);
  if (mode >= kNumFullScreenModes)
    return;

  DCHECK_GE(g_full_screen_requests[mode], 0);
  if (mode < 0)
    return;

  g_full_screen_requests[mode] = std::max(g_full_screen_requests[mode] - 1, 0);
  SetUIMode();
}

// Switches full screen modes.  Releases a request for |from_mode| and adds a
// new request for |to_mode|.  Must be called on the main thread.
void SwitchFullScreenModes(FullScreenMode from_mode, FullScreenMode to_mode) {
  DCHECK_LT(from_mode, kNumFullScreenModes);
  DCHECK_LT(to_mode, kNumFullScreenModes);
  if (from_mode >= kNumFullScreenModes || to_mode >= kNumFullScreenModes)
    return;

  DCHECK_GT(g_full_screen_requests[from_mode], 0);
  DCHECK_GE(g_full_screen_requests[to_mode], 0);
  g_full_screen_requests[from_mode] =
      std::max(g_full_screen_requests[from_mode] - 1, 0);
  g_full_screen_requests[to_mode] =
      std::max(g_full_screen_requests[to_mode] + 1, 1);
  SetUIMode();
}

void SetCursorVisibility(bool visible) {
  if (visible)
    [NSCursor unhide];
  else
    [NSCursor hide];
}

bool ShouldWindowsMiniaturizeOnDoubleClick() {
  // We use an undocumented method in Cocoa; if it doesn't exist, default to
  // |true|. If it ever goes away, we can do (using an undocumented pref key):
  //   NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  //   return ![defaults objectForKey:@"AppleMiniaturizeOnDoubleClick"] ||
  //          [defaults boolForKey:@"AppleMiniaturizeOnDoubleClick"];
  BOOL methodImplemented =
      [NSWindow respondsToSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
  DCHECK(methodImplemented);
  return !methodImplemented ||
      [NSWindow performSelector:@selector(_shouldMiniaturizeOnDoubleClick)];
}

void ActivateProcess(pid_t pid) {
  ProcessSerialNumber process;
  OSStatus status = GetProcessForPID(pid, &process);
  if (status == noErr) {
    SetFrontProcess(&process);
  } else {
    OSSTATUS_DLOG(WARNING, status) << "Unable to get process for pid " << pid;
  }
}

bool AmIForeground() {
  ProcessSerialNumber foreground_psn = { 0 };
  OSErr err = GetFrontProcess(&foreground_psn);
  if (err != noErr) {
    OSSTATUS_DLOG(WARNING, err) << "GetFrontProcess";
    return false;
  }

  ProcessSerialNumber my_psn = { 0, kCurrentProcess };

  Boolean result = FALSE;
  err = SameProcess(&foreground_psn, &my_psn, &result);
  if (err != noErr) {
    OSSTATUS_DLOG(WARNING, err) << "SameProcess";
    return false;
  }

  return result;
}

bool SetFileBackupExclusion(const FilePath& file_path) {
  NSString* file_path_ns =
      [NSString stringWithUTF8String:file_path.value().c_str()];
  NSURL* file_url = [NSURL fileURLWithPath:file_path_ns];

  // When excludeByPath is true the application must be running with root
  // privileges (admin for 10.6 and earlier) but the URL does not have to
  // already exist. When excludeByPath is false the URL must already exist but
  // can be used in non-root (or admin as above) mode. We use false so that
  // non-root (or admin) users don't get their TimeMachine drive filled up with
  // unnecessary backups.
  OSStatus os_err =
      CSBackupSetItemExcluded(base::mac::NSToCFCast(file_url), TRUE, FALSE);
  if (os_err != noErr) {
    OSSTATUS_DLOG(WARNING, os_err)
        << "Failed to set backup exclusion for file '"
        << file_path.value().c_str() << "'";
  }
  return os_err == noErr;
}

void SetProcessName(CFStringRef process_name) {
  if (!process_name || CFStringGetLength(process_name) == 0) {
    NOTREACHED() << "SetProcessName given bad name.";
    return;
  }

  if (![NSThread isMainThread]) {
    NOTREACHED() << "Should only set process name from main thread.";
    return;
  }

  // Warning: here be dragons! This is SPI reverse-engineered from WebKit's
  // plugin host, and could break at any time (although realistically it's only
  // likely to break in a new major release).
  // When 10.7 is available, check that this still works, and update this
  // comment for 10.8.

  // Private CFType used in these LaunchServices calls.
  typedef CFTypeRef PrivateLSASN;
  typedef PrivateLSASN (*LSGetCurrentApplicationASNType)();
  typedef OSStatus (*LSSetApplicationInformationItemType)(int, PrivateLSASN,
                                                          CFStringRef,
                                                          CFStringRef,
                                                          CFDictionaryRef*);

  static LSGetCurrentApplicationASNType ls_get_current_application_asn_func =
      NULL;
  static LSSetApplicationInformationItemType
      ls_set_application_information_item_func = NULL;
  static CFStringRef ls_display_name_key = NULL;

  static bool did_symbol_lookup = false;
  if (!did_symbol_lookup) {
    did_symbol_lookup = true;
    CFBundleRef launch_services_bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.LaunchServices"));
    if (!launch_services_bundle) {
      DLOG(ERROR) << "Failed to look up LaunchServices bundle";
      return;
    }

    ls_get_current_application_asn_func =
        reinterpret_cast<LSGetCurrentApplicationASNType>(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle, CFSTR("_LSGetCurrentApplicationASN")));
    if (!ls_get_current_application_asn_func)
      DLOG(ERROR) << "Could not find _LSGetCurrentApplicationASN";

    ls_set_application_information_item_func =
        reinterpret_cast<LSSetApplicationInformationItemType>(
            CFBundleGetFunctionPointerForName(
                launch_services_bundle,
                CFSTR("_LSSetApplicationInformationItem")));
    if (!ls_set_application_information_item_func)
      DLOG(ERROR) << "Could not find _LSSetApplicationInformationItem";

    CFStringRef* key_pointer = reinterpret_cast<CFStringRef*>(
        CFBundleGetDataPointerForName(launch_services_bundle,
                                      CFSTR("_kLSDisplayNameKey")));
    ls_display_name_key = key_pointer ? *key_pointer : NULL;
    if (!ls_display_name_key)
      DLOG(ERROR) << "Could not find _kLSDisplayNameKey";

    // Internally, this call relies on the Mach ports that are started up by the
    // Carbon Process Manager.  In debug builds this usually happens due to how
    // the logging layers are started up; but in release, it isn't started in as
    // much of a defined order.  So if the symbols had to be loaded, go ahead
    // and force a call to make sure the manager has been initialized and hence
    // the ports are opened.
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
  }
  if (!ls_get_current_application_asn_func ||
      !ls_set_application_information_item_func ||
      !ls_display_name_key) {
    return;
  }

  PrivateLSASN asn = ls_get_current_application_asn_func();
  // Constant used by WebKit; what exactly it means is unknown.
  const int magic_session_constant = -2;
  OSErr err =
      ls_set_application_information_item_func(magic_session_constant, asn,
                                               ls_display_name_key,
                                               process_name,
                                               NULL /* optional out param */);
  OSSTATUS_DLOG_IF(ERROR, err != noErr, err)
      << "Call to set process name failed";
}

// Converts a NSImage to a CGImageRef.  Normally, the system frameworks can do
// this fine, especially on 10.6.  On 10.5, however, CGImage cannot handle
// converting a PDF-backed NSImage into a CGImageRef.  This function will
// rasterize the PDF into a bitmap CGImage.  The caller is responsible for
// releasing the return value.
CGImageRef CopyNSImageToCGImage(NSImage* image) {
  // This is based loosely on http://www.cocoadev.com/index.pl?CGImageRef .
  NSSize size = [image size];
  ScopedCFTypeRef<CGContextRef> context(
      CGBitmapContextCreate(NULL,  // Allow CG to allocate memory.
                            size.width,
                            size.height,
                            8,  // bitsPerComponent
                            0,  // bytesPerRow - CG will calculate by default.
                            [[NSColorSpace genericRGBColorSpace] CGColorSpace],
                            kCGBitmapByteOrder32Host |
                                kCGImageAlphaPremultipliedFirst));
  if (!context.get())
    return NULL;

  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:
      [NSGraphicsContext graphicsContextWithGraphicsPort:context.get()
                                                 flipped:NO]];
  [image drawInRect:NSMakeRect(0,0, size.width, size.height)
           fromRect:NSZeroRect
          operation:NSCompositeCopy
           fraction:1.0];
  [NSGraphicsContext restoreGraphicsState];

  return CGBitmapContextCreateImage(context);
}

bool CheckLoginItemStatus(bool* is_hidden) {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get())
    return false;

  if (is_hidden)
    *is_hidden = IsHiddenLoginItem(item);

  return true;
}

void AddToLoginItems(bool hide_on_startup) {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (item.get() && (IsHiddenLoginItem(item) == hide_on_startup)) {
    return;  // Already is a login item with required hide flag.
  }

  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    DLOG(ERROR) << "Couldn't get a Login Items list.";
    return;
  }

  // Remove the old item, it has wrong hide flag, we'll create a new one.
  if (item.get()) {
    LSSharedFileListItemRemove(login_items, item);
  }

  NSURL* url = [NSURL fileURLWithPath:[base::mac::MainBundle() bundlePath]];

  BOOL hide = hide_on_startup ? YES : NO;
  NSDictionary* properties =
      [NSDictionary
        dictionaryWithObject:[NSNumber numberWithBool:hide]
                      forKey:(NSString*)kLSSharedFileListLoginItemHidden];

  ScopedCFTypeRef<LSSharedFileListItemRef> new_item;
  new_item.reset(LSSharedFileListInsertItemURL(
      login_items, kLSSharedFileListItemLast, NULL, NULL,
      reinterpret_cast<CFURLRef>(url),
      reinterpret_cast<CFDictionaryRef>(properties), NULL));

  if (!new_item.get()) {
    DLOG(ERROR) << "Couldn't insert current app into Login Items list.";
  }
}

void RemoveFromLoginItems() {
  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get())
    return;

  ScopedCFTypeRef<LSSharedFileListRef> login_items(LSSharedFileListCreate(
      NULL, kLSSharedFileListSessionLoginItems, NULL));

  if (!login_items.get()) {
    DLOG(ERROR) << "Couldn't get a Login Items list.";
    return;
  }

  LSSharedFileListItemRemove(login_items, item);
}

bool WasLaunchedAsLoginOrResumeItem() {
  ProcessSerialNumber psn = { 0, kCurrentProcess };

  base::scoped_nsobject<NSDictionary> process_info(
      CFToNSCast(ProcessInformationCopyDictionary(
          &psn, kProcessDictionaryIncludeAllInformationMask)));

  long long temp = [[process_info objectForKey:@"ParentPSN"] longLongValue];
  ProcessSerialNumber parent_psn =
      { (temp >> 32) & 0x00000000FFFFFFFFLL, temp & 0x00000000FFFFFFFFLL };

  base::scoped_nsobject<NSDictionary> parent_info(
      CFToNSCast(ProcessInformationCopyDictionary(
          &parent_psn, kProcessDictionaryIncludeAllInformationMask)));

  // Check that creator process code is that of loginwindow.
  BOOL result =
      [[parent_info objectForKey:@"FileCreator"] isEqualToString:@"lgnw"];

  return result == YES;
}

bool WasLaunchedAsHiddenLoginItem() {
  if (!WasLaunchedAsLoginOrResumeItem())
    return false;

  ScopedCFTypeRef<LSSharedFileListItemRef> item(GetLoginItemForApp());
  if (!item.get()) {
    // Lion can launch items for the resume feature.  So log an error only for
    // Snow Leopard or earlier.
    if (IsOSSnowLeopard())
      DLOG(ERROR) <<
          "Process launched at Login but can't access Login Item List.";

    return false;
  }
  return IsHiddenLoginItem(item);
}

bool RemoveQuarantineAttribute(const FilePath& file_path) {
  const char kQuarantineAttrName[] = "com.apple.quarantine";
  int status = removexattr(file_path.value().c_str(), kQuarantineAttrName, 0);
  return status == 0 || errno == ENOATTR;
}

namespace {

// Returns the running system's Darwin major version. Don't call this, it's
// an implementation detail and its result is meant to be cached by
// MacOSXMinorVersion.
int DarwinMajorVersionInternal() {
  // base::OperatingSystemVersionNumbers calls Gestalt, which is a
  // higher-level operation than is needed. It might perform unnecessary
  // operations. On 10.6, it was observed to be able to spawn threads (see
  // http://crbug.com/53200). It might also read files or perform other
  // blocking operations. Actually, nobody really knows for sure just what
  // Gestalt might do, or what it might be taught to do in the future.
  //
  // uname, on the other hand, is implemented as a simple series of sysctl
  // system calls to obtain the relevant data from the kernel. The data is
  // compiled right into the kernel, so no threads or blocking or other
  // funny business is necessary.

  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    DPLOG(ERROR) << "uname";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0) {
    DLOG(ERROR) << "unexpected uname sysname " << uname_info.sysname;
    return 0;
  }

  int darwin_major_version = 0;
  char* dot = strchr(uname_info.release, '.');
  if (dot) {
    if (!base::StringToInt(base::StringPiece(uname_info.release,
                                             dot - uname_info.release),
                           &darwin_major_version)) {
      dot = NULL;
    }
  }

  if (!dot) {
    DLOG(ERROR) << "could not parse uname release " << uname_info.release;
    return 0;
  }

  return darwin_major_version;
}

// Returns the running system's Mac OS X minor version. This is the |y| value
// in 10.y or 10.y.z. Don't call this, it's an implementation detail and the
// result is meant to be cached by MacOSXMinorVersion.
int MacOSXMinorVersionInternal() {
  int darwin_major_version = DarwinMajorVersionInternal();

  // The Darwin major version is always 4 greater than the Mac OS X minor
  // version for Darwin versions beginning with 6, corresponding to Mac OS X
  // 10.2. Since this correspondence may change in the future, warn when
  // encountering a version higher than anything seen before. Older Darwin
  // versions, or versions that can't be determined, result in
  // immediate death.
  CHECK(darwin_major_version >= 6);
  int mac_os_x_minor_version = darwin_major_version - 4;
  DLOG_IF(WARNING, darwin_major_version > 12) << "Assuming Darwin "
      << base::IntToString(darwin_major_version) << " is Mac OS X 10."
      << base::IntToString(mac_os_x_minor_version);

  return mac_os_x_minor_version;
}

// Returns the running system's Mac OS X minor version. This is the |y| value
// in 10.y or 10.y.z.
int MacOSXMinorVersion() {
  static int mac_os_x_minor_version = MacOSXMinorVersionInternal();
  return mac_os_x_minor_version;
}

enum {
  SNOW_LEOPARD_MINOR_VERSION = 6,
  LION_MINOR_VERSION = 7,
  MOUNTAIN_LION_MINOR_VERSION = 8,
};

}  // namespace

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GE_10_7)
bool IsOSSnowLeopard() {
  return MacOSXMinorVersion() == SNOW_LEOPARD_MINOR_VERSION;
}
#endif

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GT_10_7)
bool IsOSLion() {
  return MacOSXMinorVersion() == LION_MINOR_VERSION;
}
#endif

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GT_10_7)
bool IsOSLionOrEarlier() {
  return MacOSXMinorVersion() <= LION_MINOR_VERSION;
}
#endif

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GE_10_7)
bool IsOSLionOrLater() {
  return MacOSXMinorVersion() >= LION_MINOR_VERSION;
}
#endif

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GT_10_8)
bool IsOSMountainLion() {
  return MacOSXMinorVersion() == MOUNTAIN_LION_MINOR_VERSION;
}
#endif

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GE_10_8)
bool IsOSMountainLionOrLater() {
  return MacOSXMinorVersion() >= MOUNTAIN_LION_MINOR_VERSION;
}
#endif

#if !defined(BASE_MAC_MAC_UTIL_H_INLINED_GT_10_8)
bool IsOSLaterThanMountainLion_DontCallThis() {
  return MacOSXMinorVersion() > MOUNTAIN_LION_MINOR_VERSION;
}
#endif

std::string GetModelIdentifier() {
  std::string return_string;
  ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (platform_expert) {
    ScopedCFTypeRef<CFDataRef> model_data(
        static_cast<CFDataRef>(IORegistryEntryCreateCFProperty(
            platform_expert,
            CFSTR("model"),
            kCFAllocatorDefault,
            0)));
    if (model_data) {
      return_string =
          reinterpret_cast<const char*>(CFDataGetBytePtr(model_data));
    }
  }
  return return_string;
}

bool ParseModelIdentifier(const std::string& ident,
                          std::string* type,
                          int32* major,
                          int32* minor) {
  size_t number_loc = ident.find_first_of("0123456789");
  if (number_loc == std::string::npos)
    return false;
  size_t comma_loc = ident.find(',', number_loc);
  if (comma_loc == std::string::npos)
    return false;
  int32 major_tmp, minor_tmp;
  std::string::const_iterator begin = ident.begin();
  if (!StringToInt(
          StringPiece(begin + number_loc, begin + comma_loc), &major_tmp) ||
      !StringToInt(
          StringPiece(begin + comma_loc + 1, ident.end()), &minor_tmp))
    return false;
  *type = ident.substr(0, number_loc);
  *major = major_tmp;
  *minor = minor_tmp;
  return true;
}

}  // namespace mac
}  // namespace base
