// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_launcher.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace base {

// See https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ .
using ::operator<<;

// The environment variable name for the total number of test shards.
const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard index.
const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

// The default output file for XML output.
const FilePath::CharType kDefaultOutputFile[] = FILE_PATH_LITERAL(
    "test_detail.xml");

namespace {

// Parses the environment variable var as an Int32.  If it is unset, returns
// default_val.  If it is set, unsets it then converts it to Int32 before
// returning it.  If unsetting or converting to an Int32 fails, print an
// error and exit with failure.
int32 Int32FromEnvOrDie(const char* const var, int32 default_val) {
  scoped_ptr<Environment> env(Environment::Create());
  std::string str_val;
  int32 result;
  if (!env->GetVar(var, &str_val))
    return default_val;
  if (!env->UnSetVar(var)) {
    LOG(ERROR) << "Invalid environment: we could not unset " << var << ".\n";
    exit(EXIT_FAILURE);
  }
  if (!StringToInt(str_val, &result)) {
    LOG(ERROR) << "Invalid environment: " << var << " is not an integer.\n";
    exit(EXIT_FAILURE);
  }
  return result;
}

// Checks whether sharding is enabled by examining the relevant
// environment variable values.  If the variables are present,
// but inconsistent (i.e., shard_index >= total_shards), prints
// an error and exits.
void InitSharding(int32* total_shards, int32* shard_index) {
  *total_shards = Int32FromEnvOrDie(kTestTotalShards, 1);
  *shard_index = Int32FromEnvOrDie(kTestShardIndex, 0);

  if (*total_shards == -1 && *shard_index != -1) {
    LOG(ERROR) << "Invalid environment variables: you have "
               << kTestShardIndex << " = " << *shard_index
               << ", but have left " << kTestTotalShards << " unset.\n";
    exit(EXIT_FAILURE);
  } else if (*total_shards != -1 && *shard_index == -1) {
    LOG(ERROR) << "Invalid environment variables: you have "
               << kTestTotalShards << " = " << *total_shards
               << ", but have left " << kTestShardIndex << " unset.\n";
    exit(EXIT_FAILURE);
  } else if (*shard_index < 0 || *shard_index >= *total_shards) {
    LOG(ERROR) << "Invalid environment variables: we require 0 <= "
               << kTestShardIndex << " < " << kTestTotalShards
               << ", but you have " << kTestShardIndex << "=" << *shard_index
               << ", " << kTestTotalShards << "=" << *total_shards << ".\n";
    exit(EXIT_FAILURE);
  }
}

// Given the total number of shards, the shard index, and the test id, returns
// true iff the test should be run on this shard.  The test id is some arbitrary
// but unique non-negative integer assigned by this launcher to each test
// method.  Assumes that 0 <= shard_index < total_shards, which is first
// verified in ShouldShard().
bool ShouldRunTestOnShard(int total_shards, int shard_index, int test_id) {
  return (test_id % total_shards) == shard_index;
}

// A helper class to output results.
// Note: as currently XML is the only supported format by gtest, we don't
// check output format (e.g. "xml:" prefix) here and output an XML file
// unconditionally.
// Note: we don't output per-test-case or total summary info like
// total failed_test_count, disabled_test_count, elapsed_time and so on.
// Only each test (testcase element in the XML) will have the correct
// failed/disabled/elapsed_time information. Each test won't include
// detailed failure messages either.
class ResultsPrinter {
 public:
  explicit ResultsPrinter(const CommandLine& command_line);
  ~ResultsPrinter();

  // Adds |result| to the stored test results.
  void AddTestResult(const TestResult& result);

  // Returns list of full names of failed tests.
  const std::vector<std::string>& failed_tests() const { return failed_tests_; }

  // Returns total number of tests run.
  size_t test_run_count() const { return test_run_count_; }

 private:
  // Test results grouped by test case name.
  typedef std::map<std::string, std::vector<TestResult> > ResultsMap;
  ResultsMap results_;

  // List of full names of failed tests.
  std::vector<std::string> failed_tests_;

  // Total number of tests run.
  size_t test_run_count_;

  // File handle of output file (can be NULL if no file).
  FILE* out_;

  DISALLOW_COPY_AND_ASSIGN(ResultsPrinter);
};

ResultsPrinter::ResultsPrinter(const CommandLine& command_line)
    : test_run_count_(0),
      out_(NULL) {
  if (!command_line.HasSwitch(kGTestOutputFlag))
    return;
  std::string flag = command_line.GetSwitchValueASCII(kGTestOutputFlag);
  size_t colon_pos = flag.find(':');
  FilePath path;
  if (colon_pos != std::string::npos) {
    FilePath flag_path =
        command_line.GetSwitchValuePath(kGTestOutputFlag);
    FilePath::StringType path_string = flag_path.value();
    path = FilePath(path_string.substr(colon_pos + 1));
    // If the given path ends with '/', consider it is a directory.
    // Note: This does NOT check that a directory (or file) actually exists
    // (the behavior is same as what gtest does).
    if (path.EndsWithSeparator()) {
      FilePath executable = command_line.GetProgram().BaseName();
      path = path.Append(executable.ReplaceExtension(
          FilePath::StringType(FILE_PATH_LITERAL("xml"))));
    }
  }
  if (path.value().empty())
    path = FilePath(kDefaultOutputFile);
  FilePath dir_name = path.DirName();
  if (!DirectoryExists(dir_name)) {
    LOG(WARNING) << "The output directory does not exist. "
                 << "Creating the directory: " << dir_name.value();
    // Create the directory if necessary (because the gtest does the same).
    file_util::CreateDirectory(dir_name);
  }
  out_ = file_util::OpenFile(path, "w");
  if (!out_) {
    LOG(ERROR) << "Cannot open output file: "
               << path.value() << ".";
    return;
  }
}

ResultsPrinter::~ResultsPrinter() {
  if (!out_)
    return;
  fprintf(out_, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(out_, "<testsuites name=\"AllTests\" tests=\"\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n");
  for (ResultsMap::iterator i = results_.begin(); i != results_.end(); ++i) {
    fprintf(out_, "  <testsuite name=\"%s\" tests=\"%" PRIuS "\" failures=\"\""
            " disabled=\"\" errors=\"\" time=\"\">\n",
            i->first.c_str(), i->second.size());
    for (size_t j = 0; j < i->second.size(); ++j) {
      const TestResult& result = i->second[j];
      fprintf(out_, "    <testcase name=\"%s\" status=\"run\" time=\"%.3f\""
              " classname=\"%s\">\n",
              result.test_name.c_str(),
              result.elapsed_time.InSecondsF(),
              result.test_case_name.c_str());
      if (!result.success)
        fprintf(out_, "      <failure message=\"\" type=\"\"></failure>\n");
      fprintf(out_, "    </testcase>\n");
    }
    fprintf(out_, "  </testsuite>\n");
  }
  fprintf(out_, "</testsuites>\n");
  fclose(out_);
}

void ResultsPrinter::AddTestResult(const TestResult& result) {
  ++test_run_count_;
  results_[result.test_case_name].push_back(result);

  if (!result.success) {
    failed_tests_.push_back(
        std::string(result.test_case_name) + "." + result.test_name);
  }
}

// For a basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc, see the comment below and http://crbug.com/44497)
bool PatternMatchesString(const char* pattern, const char* str) {
  switch (*pattern) {
    case '\0':
    case ':':  // Either ':' or '\0' marks the end of the pattern.
      return *str == '\0';
    case '?':  // Matches any single character.
      return *str != '\0' && PatternMatchesString(pattern + 1, str + 1);
    case '*':  // Matches any string (possibly empty) of characters.
      return (*str != '\0' && PatternMatchesString(pattern, str + 1)) ||
          PatternMatchesString(pattern + 1, str);
    default:  // Non-special character.  Matches itself.
      return *pattern == *str &&
          PatternMatchesString(pattern + 1, str + 1);
  }
}

// TODO(phajdan.jr): Avoid duplicating gtest code. (http://crbug.com/44497)
// For basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc)
bool MatchesFilter(const std::string& name, const std::string& filter) {
  const char *cur_pattern = filter.c_str();
  for (;;) {
    if (PatternMatchesString(cur_pattern, name.c_str())) {
      return true;
    }

    // Finds the next pattern in the filter.
    cur_pattern = strchr(cur_pattern, ':');

    // Returns if no more pattern can be found.
    if (cur_pattern == NULL) {
      return false;
    }

    // Skips the pattern separater (the ':' character).
    cur_pattern++;
  }
}

bool RunTests(TestLauncherDelegate* launcher_delegate,
              int total_shards,
              int shard_index) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  DCHECK(!command_line->HasSwitch(kGTestListTestsFlag));

  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  std::string filter = command_line->GetSwitchValueASCII(kGTestFilterFlag);

  // Split --gtest_filter at '-', if there is one, to separate into
  // positive filter and negative filter portions.
  std::string positive_filter = filter;
  std::string negative_filter;
  size_t dash_pos = filter.find('-');
  if (dash_pos != std::string::npos) {
    positive_filter = filter.substr(0, dash_pos);  // Everything up to the dash.
    negative_filter = filter.substr(dash_pos + 1); // Everything after the dash.
  }

  int num_runnable_tests = 0;

  ResultsPrinter printer(*command_line);
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      std::string test_name = test_info->test_case_name();
      test_name.append(".");
      test_name.append(test_info->name());

      // Skip disabled tests.
      if (test_name.find("DISABLED") != std::string::npos &&
          !command_line->HasSwitch(kGTestRunDisabledTestsFlag)) {
        continue;
      }

      // Skip the test that doesn't match the filter string (if given).
      if ((!positive_filter.empty() &&
           !MatchesFilter(test_name, positive_filter)) ||
          MatchesFilter(test_name, negative_filter)) {
        continue;
      }

      if (!launcher_delegate->ShouldRunTest(test_case, test_info))
        continue;

      bool should_run = ShouldRunTestOnShard(total_shards, shard_index,
                                             num_runnable_tests);
      num_runnable_tests += 1;
      if (!should_run)
        continue;

      launcher_delegate->RunTest(test_case,
                                 test_info,
                                 base::Bind(
                                     &ResultsPrinter::AddTestResult,
                                     base::Unretained(&printer)));
    }
  }

  launcher_delegate->RunRemainingTests();

  printf("%" PRIuS " test%s run\n",
         printer.test_run_count(),
         printer.test_run_count() > 1 ? "s" : "");
  printf("%" PRIuS " test%s failed\n",
         printer.failed_tests().size(),
         printer.failed_tests().size() != 1 ? "s" : "");
  if (printer.failed_tests().empty())
    return true;

  printf("Failing tests:\n");
  for (size_t i = 0; i < printer.failed_tests().size(); ++i)
    printf("%s\n", printer.failed_tests()[i].c_str());

  return false;
}

}  // namespace

const char kGTestFilterFlag[] = "gtest_filter";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";

const char kHelpFlag[]   = "help";

TestResult::TestResult() {
}

TestLauncherDelegate::~TestLauncherDelegate() {
}

int LaunchChildGTestProcess(const CommandLine& command_line,
                            const std::string& wrapper,
                            base::TimeDelta timeout,
                            bool* was_timeout) {
  CommandLine new_command_line(command_line.GetProgram());
  CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the other tests.
  switches.erase(kGTestOutputFlag);

  // Strip out gtest_repeat flag - this is handled by the launcher process.
  switches.erase(kGTestRepeatFlag);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  // Prepend wrapper after last CommandLine quasi-copy operation. CommandLine
  // does not really support removing switches well, and trying to do that
  // on a CommandLine with a wrapper is known to break.
  // TODO(phajdan.jr): Give it a try to support CommandLine removing switches.
#if defined(OS_WIN)
  new_command_line.PrependWrapper(ASCIIToWide(wrapper));
#elif defined(OS_POSIX)
  new_command_line.PrependWrapper(wrapper);
#endif

  base::ProcessHandle process_handle;
  base::LaunchOptions options;

#if defined(OS_POSIX)
  // On POSIX, we launch the test in a new process group with pgid equal to
  // its pid. Any child processes that the test may create will inherit the
  // same pgid. This way, if the test is abruptly terminated, we can clean up
  // any orphaned child processes it may have left behind.
  options.new_process_group = true;
#endif

  if (!base::LaunchProcess(new_command_line, options, &process_handle))
    return -1;

  int exit_code = 0;
  if (!base::WaitForExitCodeWithTimeout(process_handle,
                                        &exit_code,
                                        timeout)) {
    *was_timeout = true;
    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    // Ensure that the process terminates.
    base::KillProcess(process_handle, -1, true);
  }

#if defined(OS_POSIX)
  if (exit_code != 0) {
    // On POSIX, in case the test does not exit cleanly, either due to a crash
    // or due to it timing out, we need to clean up any child processes that
    // it might have created. On Windows, child processes are automatically
    // cleaned up using JobObjects.
    base::KillProcessGroup(process_handle);
  }
#endif

  base::CloseProcessHandle(process_handle);

  return exit_code;
}

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int argc,
                char** argv) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  int32 total_shards;
  int32 shard_index;
  InitSharding(&total_shards, &shard_index);

  int cycles = 1;
  if (command_line->HasSwitch(kGTestRepeatFlag))
    StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag), &cycles);

  int exit_code = 0;
  while (cycles != 0) {
    if (!RunTests(launcher_delegate, total_shards, shard_index)) {
      exit_code = 1;
      break;
    }

    // Special value "-1" means "repeat indefinitely".
    if (cycles != -1)
      cycles--;
  }

  return exit_code;
}

}  // namespace base
