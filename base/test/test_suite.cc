// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debug_on_start_win.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/memory.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#if defined(OS_IOS)
#include "base/test/test_listener_ios.h"
#else
#include "base/test/mock_chrome_application_mac.h"
#endif  // OS_IOS
#endif  // OS_MACOSX

#if defined(OS_ANDROID)
#include "base/test/test_support_android.h"
#endif

#if defined(OS_IOS)
#include "base/test/test_support_ios.h"
#endif

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

namespace {

class MaybeTestDisabler : public testing::EmptyTestEventListener {
 public:
  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    ASSERT_FALSE(TestSuite::IsMarkedMaybe(test_info))
        << "Probably the OS #ifdefs don't include all of the necessary "
           "platforms.\nPlease ensure that no tests have the MAYBE_ prefix "
           "after the code is preprocessed.";
  }
};

class TestClientInitializer : public testing::EmptyTestEventListener {
 public:
  TestClientInitializer()
      : old_command_line_(CommandLine::NO_PROGRAM) {
  }

  virtual void OnTestStart(const testing::TestInfo& test_info) OVERRIDE {
    old_command_line_ = *CommandLine::ForCurrentProcess();
  }

  virtual void OnTestEnd(const testing::TestInfo& test_info) OVERRIDE {
    *CommandLine::ForCurrentProcess() = old_command_line_;
  }

 private:
  CommandLine old_command_line_;

  DISALLOW_COPY_AND_ASSIGN(TestClientInitializer);
};

}  // namespace

TestSuite::TestSuite(int argc, char** argv) : initialized_command_line_(false) {
  PreInitialize(argc, argv, true);
}

TestSuite::TestSuite(int argc, char** argv, bool create_at_exit_manager)
    : initialized_command_line_(false) {
  PreInitialize(argc, argv, create_at_exit_manager);
}

TestSuite::~TestSuite() {
  if (initialized_command_line_)
    CommandLine::Reset();
}

void TestSuite::PreInitialize(int argc, char** argv,
                              bool create_at_exit_manager) {
#if defined(OS_WIN)
  testing::GTEST_FLAG(catch_exceptions) = false;
#endif
  base::EnableTerminationOnHeapCorruption();
  initialized_command_line_ = CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
#if defined(OS_LINUX) && defined(USE_AURA)
  // When calling native char conversion functions (e.g wrctomb) we need to
  // have the locale set. In the absence of such a call the "C" locale is the
  // default. In the gtk code (below) gtk_init() implicitly sets a locale.
  setlocale(LC_ALL, "");
#elif defined(TOOLKIT_GTK)
  gtk_init_check(&argc, &argv);
#endif  // defined(TOOLKIT_GTK)

  // On Android, AtExitManager is created in
  // testing/android/native_test_wrapper.cc before main() is called.
#if !defined(OS_ANDROID)
  if (create_at_exit_manager)
    at_exit_manager_.reset(new base::AtExitManager);
#endif

#if defined(OS_IOS)
  InitIOSRunHook(this, argc, argv);
#endif

  // Don't add additional code to this function.  Instead add it to
  // Initialize().  See bug 6436.
}


// static
bool TestSuite::IsMarkedMaybe(const testing::TestInfo& test) {
  return strncmp(test.name(), "MAYBE_", 6) == 0;
}

void TestSuite::CatchMaybeTests() {
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new MaybeTestDisabler);
}

void TestSuite::ResetCommandLine() {
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new TestClientInitializer);
}

// Don't add additional code to this method.  Instead add it to
// Initialize().  See bug 6436.
int TestSuite::Run() {
#if defined(OS_IOS)
  RunTestsFromIOSApp();
#endif

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool scoped_pool;
#endif

  Initialize();
  std::string client_func =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTestChildProcess);

  // Check to see if we are being run as a client process.
  if (!client_func.empty())
    return multi_process_function_list::InvokeChildProcessTest(client_func);
#if defined(OS_IOS)
  base::test_listener_ios::RegisterTestEndListener();
#endif
  int result = RUN_ALL_TESTS();

#if defined(OS_MACOSX)
  // This MUST happen before Shutdown() since Shutdown() tears down
  // objects (such as NotificationService::current()) that Cocoa
  // objects use to remove themselves as observers.
  scoped_pool.Recycle();
#endif

  Shutdown();

  return result;
}

// static
void TestSuite::UnitTestAssertHandler(const std::string& str) {
  RAW_LOG(FATAL, str.c_str());
}

void TestSuite::SuppressErrorDialogs() {
#if defined(OS_WIN)
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at
  // http://blogs.msdn.com/oldnewthing/archive/2004/07/27/198410.aspx
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);

#if defined(_DEBUG) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 1)
  // Suppress the "Debug Assertion Failed" dialog.
  // TODO(hbono): remove this code when gtest has it.
  // http://groups.google.com/d/topic/googletestframework/OjuwNlXy5ac/discussion
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif  // defined(_DEBUG) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 1)
#endif  // defined(OS_WIN)
}

void TestSuite::Initialize() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Some of the app unit tests spin runloops.
  mock_cr_app::RegisterMockCrApp();
#endif

#if defined(OS_IOS)
  InitIOSTestMessageLoop();
#endif  // OS_IOS

#if defined(OS_ANDROID)
  InitAndroidTest();
#else
  // Initialize logging.
  base::FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  base::FilePath log_filename = exe.ReplaceExtension(FILE_PATH_LITERAL("log"));
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  // We want process and thread IDs because we may have multiple processes.
  // Note: temporarily enabled timestamps in an effort to catch bug 6361.
  logging::SetLogItems(true, true, true, true);
#endif  // else defined(OS_ANDROID)

  CHECK(base::debug::EnableInProcessStackDumping());
#if defined(OS_WIN)
  // Make sure we run with high resolution timer to minimize differences
  // between production code and test code.
  base::Time::EnableHighResolutionTimer(true);
#endif  // defined(OS_WIN)

  // In some cases, we do not want to see standard error dialogs.
  if (!base::debug::BeingDebugged() &&
      !CommandLine::ForCurrentProcess()->HasSwitch("show-error-dialogs")) {
    SuppressErrorDialogs();
    base::debug::SetSuppressDebugUI(true);
    logging::SetLogAssertHandler(UnitTestAssertHandler);
  }

  icu_util::Initialize();

  CatchMaybeTests();
  ResetCommandLine();

  TestTimeouts::Initialize();
}

void TestSuite::Shutdown() {
}
