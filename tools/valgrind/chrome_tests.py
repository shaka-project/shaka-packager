#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Runs various chrome tests through valgrind_test.py.'''

import glob
import logging
import multiprocessing
import optparse
import os
import stat
import sys

import logging_utils
import path_utils

import common
import valgrind_test

class TestNotFound(Exception): pass

class MultipleGTestFiltersSpecified(Exception): pass

class BuildDirNotFound(Exception): pass

class BuildDirAmbiguous(Exception): pass

class ChromeTests:
  SLOW_TOOLS = ["memcheck", "tsan", "tsan_rv", "drmemory"]
  LAYOUT_TESTS_DEFAULT_CHUNK_SIZE = 500

  def __init__(self, options, args, test):
    if ':' in test:
      (self._test, self._gtest_filter) = test.split(':', 1)
    else:
      self._test = test
      self._gtest_filter = options.gtest_filter

    if self._test not in self._test_list:
      raise TestNotFound("Unknown test: %s" % test)

    if options.gtest_filter and options.gtest_filter != self._gtest_filter:
      raise MultipleGTestFiltersSpecified("Can not specify both --gtest_filter "
                                          "and --test %s" % test)

    self._options = options
    self._args = args

    script_dir = path_utils.ScriptDir()
    # Compute the top of the tree (the "source dir") from the script dir (where
    # this script lives).  We assume that the script dir is in tools/valgrind/
    # relative to the top of the tree.
    self._source_dir = os.path.dirname(os.path.dirname(script_dir))
    # since this path is used for string matching, make sure it's always
    # an absolute Unix-style path
    self._source_dir = os.path.abspath(self._source_dir).replace('\\', '/')
    valgrind_test_script = os.path.join(script_dir, "valgrind_test.py")
    self._command_preamble = ["--source_dir=%s" % (self._source_dir)]

    if not self._options.build_dir:
      dirs = [
        os.path.join(self._source_dir, "xcodebuild", "Debug"),
        os.path.join(self._source_dir, "out", "Debug"),
        os.path.join(self._source_dir, "build", "Debug"),
      ]
      build_dir = [d for d in dirs if os.path.isdir(d)]
      if len(build_dir) > 1:
        raise BuildDirAmbiguous("Found more than one suitable build dir:\n"
                                "%s\nPlease specify just one "
                                "using --build_dir" % ", ".join(build_dir))
      elif build_dir:
        self._options.build_dir = build_dir[0]
      else:
        self._options.build_dir = None

    if self._options.build_dir:
      build_dir = os.path.abspath(self._options.build_dir)
      self._command_preamble += ["--build_dir=%s" % (self._options.build_dir)]

  def _EnsureBuildDirFound(self):
    if not self._options.build_dir:
      raise BuildDirNotFound("Oops, couldn't find a build dir, please "
                             "specify it manually using --build_dir")

  def _DefaultCommand(self, tool, exe=None, valgrind_test_args=None):
    '''Generates the default command array that most tests will use.'''
    if exe and common.IsWindows():
      exe += '.exe'

    cmd = list(self._command_preamble)

    # Find all suppressions matching the following pattern:
    # tools/valgrind/TOOL/suppressions[_PLATFORM].txt
    # and list them with --suppressions= prefix.
    script_dir = path_utils.ScriptDir()
    tool_name = tool.ToolName();
    suppression_file = os.path.join(script_dir, tool_name, "suppressions.txt")
    if os.path.exists(suppression_file):
      cmd.append("--suppressions=%s" % suppression_file)
    # Platform-specific suppression
    for platform in common.PlatformNames():
      platform_suppression_file = \
          os.path.join(script_dir, tool_name, 'suppressions_%s.txt' % platform)
      if os.path.exists(platform_suppression_file):
        cmd.append("--suppressions=%s" % platform_suppression_file)

    if self._options.valgrind_tool_flags:
      cmd += self._options.valgrind_tool_flags.split(" ")
    if self._options.keep_logs:
      cmd += ["--keep_logs"]
    if valgrind_test_args != None:
      for arg in valgrind_test_args:
        cmd.append(arg)
    if exe:
      self._EnsureBuildDirFound()
      cmd.append(os.path.join(self._options.build_dir, exe))
      # Valgrind runs tests slowly, so slow tests hurt more; show elapased time
      # so we can find the slowpokes.
      cmd.append("--gtest_print_time")
    if self._options.gtest_repeat:
      cmd.append("--gtest_repeat=%s" % self._options.gtest_repeat)
    return cmd

  def Run(self):
    ''' Runs the test specified by command-line argument --test '''
    logging.info("running test %s" % (self._test))
    return self._test_list[self._test](self)

  def _AppendGtestFilter(self, tool, name, cmd):
    '''Append an appropriate --gtest_filter flag to the googletest binary
       invocation.
       If the user passed his own filter mentioning only one test, just use it.
       Othewise, filter out tests listed in the appropriate gtest_exclude files.
    '''
    if (self._gtest_filter and
        ":" not in self._gtest_filter and
        "?" not in self._gtest_filter and
        "*" not in self._gtest_filter):
      cmd.append("--gtest_filter=%s" % self._gtest_filter)
      return

    filters = []
    gtest_files_dir = os.path.join(path_utils.ScriptDir(), "gtest_exclude")

    gtest_filter_files = [
        os.path.join(gtest_files_dir, name + ".gtest-%s.txt" % tool.ToolName())]
    # Use ".gtest.txt" files only for slow tools, as they now contain
    # Valgrind- and Dr.Memory-specific filters.
    # TODO(glider): rename the files to ".gtest_slow.txt"
    if tool.ToolName() in ChromeTests.SLOW_TOOLS:
      gtest_filter_files += [os.path.join(gtest_files_dir, name + ".gtest.txt")]
    for platform_suffix in common.PlatformNames():
      gtest_filter_files += [
        os.path.join(gtest_files_dir, name + ".gtest_%s.txt" % platform_suffix),
        os.path.join(gtest_files_dir, name + ".gtest-%s_%s.txt" % \
            (tool.ToolName(), platform_suffix))]
    logging.info("Reading gtest exclude filter files:")
    for filename in gtest_filter_files:
      # strip the leading absolute path (may be very long on the bot)
      # and the following / or \.
      readable_filename = filename.replace("\\", "/")  # '\' on Windows
      readable_filename = readable_filename.replace(self._source_dir, "")[1:]
      if not os.path.exists(filename):
        logging.info("  \"%s\" - not found" % readable_filename)
        continue
      logging.info("  \"%s\" - OK" % readable_filename)
      f = open(filename, 'r')
      for line in f.readlines():
        if line.startswith("#") or line.startswith("//") or line.isspace():
          continue
        line = line.rstrip()
        test_prefixes = ["FLAKY", "FAILS"]
        for p in test_prefixes:
          # Strip prefixes from the test names.
          line = line.replace(".%s_" % p, ".")
        # Exclude the original test name.
        filters.append(line)
        if line[-2:] != ".*":
          # List all possible prefixes if line doesn't end with ".*".
          for p in test_prefixes:
            filters.append(line.replace(".", ".%s_" % p))
    # Get rid of duplicates.
    filters = set(filters)
    gtest_filter = self._gtest_filter
    if len(filters):
      if gtest_filter:
        gtest_filter += ":"
        if gtest_filter.find("-") < 0:
          gtest_filter += "-"
      else:
        gtest_filter = "-"
      gtest_filter += ":".join(filters)
    if gtest_filter:
      cmd.append("--gtest_filter=%s" % gtest_filter)

  @staticmethod
  def ShowTests():
    test_to_names = {}
    for name, test_function in ChromeTests._test_list.iteritems():
      test_to_names.setdefault(test_function, []).append(name)

    name_to_aliases = {}
    for names in test_to_names.itervalues():
      names.sort(key=lambda name: len(name))
      name_to_aliases[names[0]] = names[1:]

    print
    print "Available tests:"
    print "----------------"
    for name, aliases in sorted(name_to_aliases.iteritems()):
      if aliases:
        print "   {} (aka {})".format(name, ', '.join(aliases))
      else:
        print "   {}".format(name)

  def SetupLdPath(self, requires_build_dir):
    if requires_build_dir:
      self._EnsureBuildDirFound()
    elif not self._options.build_dir:
      return

    # Append build_dir to LD_LIBRARY_PATH so external libraries can be loaded.
    if (os.getenv("LD_LIBRARY_PATH")):
      os.putenv("LD_LIBRARY_PATH", "%s:%s" % (os.getenv("LD_LIBRARY_PATH"),
                                              self._options.build_dir))
    else:
      os.putenv("LD_LIBRARY_PATH", self._options.build_dir)

  def SimpleTest(self, module, name, valgrind_test_args=None, cmd_args=None):
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool, name, valgrind_test_args)
    self._AppendGtestFilter(tool, name, cmd)
    cmd.extend(['--test-tiny-timeout=1000'])
    if cmd_args:
      cmd.extend(cmd_args)

    self.SetupLdPath(True)
    return tool.Run(cmd, module)

  def RunCmdLine(self):
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool, None, self._args)
    self.SetupLdPath(False)
    return tool.Run(cmd, None)

  def TestAppList(self):
    return self.SimpleTest("app_list", "app_list_unittests")

  def TestAsh(self):
    return self.SimpleTest("ash", "ash_unittests")

  def TestAura(self):
    return self.SimpleTest("aura", "aura_unittests")

  def TestBase(self):
    return self.SimpleTest("base", "base_unittests")

  def TestChromeOS(self):
    return self.SimpleTest("chromeos", "chromeos_unittests")

  def TestComponents(self):
    return self.SimpleTest("components", "components_unittests")

  def TestCompositor(self):
    return self.SimpleTest("compositor", "compositor_unittests")

  def TestContent(self):
    return self.SimpleTest("content", "content_unittests")

  def TestContentBrowser(self):
    return self.SimpleTest("content", "content_browsertests")

  def TestCourgette(self):
    return self.SimpleTest("courgette", "courgette_unittests")

  def TestCrypto(self):
    return self.SimpleTest("crypto", "crypto_unittests")

  def TestDevice(self):
    return self.SimpleTest("device", "device_unittests")

  def TestFFmpeg(self):
    return self.SimpleTest("chrome", "ffmpeg_unittests")

  def TestFFmpegRegressions(self):
    return self.SimpleTest("chrome", "ffmpeg_regression_tests")

  def TestGPU(self):
    return self.SimpleTest("gpu", "gpu_unittests")

  def TestIpc(self):
    return self.SimpleTest("ipc", "ipc_tests",
                           valgrind_test_args=["--trace_children"])

  def TestJingle(self):
    return self.SimpleTest("chrome", "jingle_unittests")

  def TestMedia(self):
    return self.SimpleTest("chrome", "media_unittests")

  def TestMessageCenter(self):
    return self.SimpleTest("message_center", "message_center_unittests")

  def TestNet(self):
    return self.SimpleTest("net", "net_unittests")

  def TestNetPerf(self):
    return self.SimpleTest("net", "net_perftests")

  def TestPPAPI(self):
    return self.SimpleTest("chrome", "ppapi_unittests")

  def TestPrinting(self):
    return self.SimpleTest("chrome", "printing_unittests")

  def TestRemoting(self):
    return self.SimpleTest("chrome", "remoting_unittests",
                           cmd_args=[
                               "--ui-test-action-timeout=60000",
                               "--ui-test-action-max-timeout=150000"])

  def TestSql(self):
    return self.SimpleTest("chrome", "sql_unittests")

  def TestSync(self):
    return self.SimpleTest("chrome", "sync_unit_tests")

  def TestLinuxSandbox(self):
    return self.SimpleTest("sandbox", "sandbox_linux_unittests")

  def TestUnit(self):
    # http://crbug.com/51716
    # Disabling all unit tests
    # Problems reappeared after r119922
    if common.IsMac() and (self._options.valgrind_tool == "memcheck"):
      logging.warning("unit_tests are disabled for memcheck on MacOS.")
      return 0;
    return self.SimpleTest("chrome", "unit_tests")

  def TestUIUnit(self):
    return self.SimpleTest("chrome", "ui_unittests")

  def TestURL(self):
    return self.SimpleTest("chrome", "url_unittests")

  def TestViews(self):
    return self.SimpleTest("views", "views_unittests")

  # Valgrind timeouts are in seconds.
  UI_VALGRIND_ARGS = ["--timeout=14400", "--trace_children", "--indirect"]
  # UI test timeouts are in milliseconds.
  UI_TEST_ARGS = ["--ui-test-action-timeout=60000",
                  "--ui-test-action-max-timeout=150000",
                  "--no-sandbox"]

  # TODO(thestig) fine-tune these values.
  # Valgrind timeouts are in seconds.
  BROWSER_VALGRIND_ARGS = ["--timeout=50000", "--trace_children", "--indirect"]
  # Browser test timeouts are in milliseconds.
  BROWSER_TEST_ARGS = ["--ui-test-action-timeout=400000",
                       "--ui-test-action-max-timeout=800000",
                       "--no-sandbox"]

  def TestAutomatedUI(self):
    return self.SimpleTest("chrome", "automated_ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestBrowser(self):
    return self.SimpleTest("chrome", "browser_tests",
                           valgrind_test_args=self.BROWSER_VALGRIND_ARGS,
                           cmd_args=self.BROWSER_TEST_ARGS)

  def TestInteractiveUI(self):
    return self.SimpleTest("chrome", "interactive_ui_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=self.UI_TEST_ARGS)

  def TestReliability(self):
    script_dir = path_utils.ScriptDir()
    url_list_file = os.path.join(script_dir, "reliability", "url_list.txt")
    return self.SimpleTest("chrome", "reliability_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(self.UI_TEST_ARGS +
                                     ["--list=%s" % url_list_file]))

  def TestSafeBrowsing(self):
    return self.SimpleTest("chrome", "safe_browsing_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(["--ui-test-action-max-timeout=450000"]))

  def TestSyncIntegration(self):
    return self.SimpleTest("chrome", "sync_integration_tests",
                           valgrind_test_args=self.UI_VALGRIND_ARGS,
                           cmd_args=(["--ui-test-action-max-timeout=450000"]))

  def TestLayoutChunk(self, chunk_num, chunk_size):
    # Run tests [chunk_num*chunk_size .. (chunk_num+1)*chunk_size) from the
    # list of tests.  Wrap around to beginning of list at end.
    # If chunk_size is zero, run all tests in the list once.
    # If a text file is given as argument, it is used as the list of tests.
    #
    # Build the ginormous commandline in 'cmd'.
    # It's going to be roughly
    #  python valgrind_test.py ... python run_webkit_tests.py ...
    # but we'll use the --indirect flag to valgrind_test.py
    # to avoid valgrinding python.
    # Start by building the valgrind_test.py commandline.
    tool = valgrind_test.CreateTool(self._options.valgrind_tool)
    cmd = self._DefaultCommand(tool)
    cmd.append("--trace_children")
    cmd.append("--indirect_webkit_layout")
    cmd.append("--ignore_exit_code")
    # Now build script_cmd, the run_webkits_tests.py commandline
    # Store each chunk in its own directory so that we can find the data later
    chunk_dir = os.path.join("layout", "chunk_%05d" % chunk_num)
    out_dir = os.path.join(path_utils.ScriptDir(), "latest")
    out_dir = os.path.join(out_dir, chunk_dir)
    if os.path.exists(out_dir):
      old_files = glob.glob(os.path.join(out_dir, "*.txt"))
      for f in old_files:
        os.remove(f)
    else:
      os.makedirs(out_dir)
    script = os.path.join(self._source_dir, "webkit", "tools", "layout_tests",
                          "run_webkit_tests.py")
    # http://crbug.com/260627: After the switch to content_shell from DRT, each
    # test now brings up 3 processes.  Under Valgrind, they become memory bound
    # and can eventually OOM if we don't reduce the total count.
    jobs = int(multiprocessing.cpu_count() * 0.3)
    script_cmd = ["python", script, "-v",
                  "--run-singly",  # run a separate DumpRenderTree for each test
                  "--fully-parallel",
                  "--child-processes=%d" % jobs,
                  "--time-out-ms=200000",
                  "--no-retry-failures",  # retrying takes too much time
                  # http://crbug.com/176908: Don't launch a browser when done.
                  "--no-show-results",
                  "--nocheck-sys-deps"]
    # Pass build mode to run_webkit_tests.py.  We aren't passed it directly,
    # so parse it out of build_dir.  run_webkit_tests.py can only handle
    # the two values "Release" and "Debug".
    # TODO(Hercules): unify how all our scripts pass around build mode
    # (--mode / --target / --build_dir / --debug)
    if self._options.build_dir.endswith("Debug"):
      script_cmd.append("--debug");
    if (chunk_size > 0):
      script_cmd.append("--run-chunk=%d:%d" % (chunk_num, chunk_size))
    if len(self._args):
      # if the arg is a txt file, then treat it as a list of tests
      if os.path.isfile(self._args[0]) and self._args[0][-4:] == ".txt":
        script_cmd.append("--test-list=%s" % self._args[0])
      else:
        script_cmd.extend(self._args)
    self._AppendGtestFilter(tool, "layout", script_cmd)
    # Now run script_cmd with the wrapper in cmd
    cmd.extend(["--"])
    cmd.extend(script_cmd)

    # Layout tests often times fail quickly, but the buildbot remains green.
    # Detect this situation when running with the default chunk size.
    if chunk_size == self.LAYOUT_TESTS_DEFAULT_CHUNK_SIZE:
      min_runtime_in_seconds=120
    else:
      min_runtime_in_seconds=0
    ret = tool.Run(cmd, "layout", min_runtime_in_seconds=min_runtime_in_seconds)
    return ret


  def TestLayout(self):
    # A "chunk file" is maintained in the local directory so that each test
    # runs a slice of the layout tests of size chunk_size that increments with
    # each run.  Since tests can be added and removed from the layout tests at
    # any time, this is not going to give exact coverage, but it will allow us
    # to continuously run small slices of the layout tests under valgrind rather
    # than having to run all of them in one shot.
    chunk_size = self._options.num_tests
    if (chunk_size == 0):
      return self.TestLayoutChunk(0, 0)
    chunk_num = 0
    chunk_file = os.path.join("valgrind_layout_chunk.txt")
    logging.info("Reading state from " + chunk_file)
    try:
      f = open(chunk_file)
      if f:
        str = f.read()
        if len(str):
          chunk_num = int(str)
        # This should be enough so that we have a couple of complete runs
        # of test data stored in the archive (although note that when we loop
        # that we almost guaranteed won't be at the end of the test list)
        if chunk_num > 10000:
          chunk_num = 0
        f.close()
    except IOError, (errno, strerror):
      logging.error("error reading from file %s (%d, %s)" % (chunk_file,
                    errno, strerror))
    # Save the new chunk size before running the tests. Otherwise if a
    # particular chunk hangs the bot, the chunk number will never get
    # incremented and the bot will be wedged.
    logging.info("Saving state to " + chunk_file)
    try:
      f = open(chunk_file, "w")
      chunk_num += 1
      f.write("%d" % chunk_num)
      f.close()
    except IOError, (errno, strerror):
      logging.error("error writing to file %s (%d, %s)" % (chunk_file, errno,
                    strerror))
    # Since we're running small chunks of the layout tests, it's important to
    # mark the ones that have errors in them.  These won't be visible in the
    # summary list for long, but will be useful for someone reviewing this bot.
    return self.TestLayoutChunk(chunk_num, chunk_size)

  # The known list of tests.
  # Recognise the original abbreviations as well as full executable names.
  _test_list = {
    "cmdline" : RunCmdLine,
    "app_list": TestAppList,     "app_list_unittests": TestAppList,
    "ash": TestAsh,              "ash_unittests": TestAsh,
    "aura": TestAura,            "aura_unittests": TestAura,
    "automated_ui" : TestAutomatedUI,
    "base": TestBase,            "base_unittests": TestBase,
    "browser": TestBrowser,      "browser_tests": TestBrowser,
    "chromeos": TestChromeOS,    "chromeos_unittests": TestChromeOS,
    "components": TestComponents,"components_unittests": TestComponents,
    "compositor": TestCompositor,"compositor_unittests": TestCompositor,
    "content": TestContent,      "content_unittests": TestContent,
    "content_browsertests": TestContentBrowser,
    "courgette": TestCourgette,  "courgette_unittests": TestCourgette,
    "crypto": TestCrypto,        "crypto_unittests": TestCrypto,
    "device": TestDevice,        "device_unittests": TestDevice,
    "ffmpeg": TestFFmpeg,        "ffmpeg_unittests": TestFFmpeg,
    "ffmpeg_regression_tests": TestFFmpegRegressions,
    "gpu": TestGPU,              "gpu_unittests": TestGPU,
    "ipc": TestIpc,              "ipc_tests": TestIpc,
    "interactive_ui": TestInteractiveUI,
    "jingle": TestJingle,        "jingle_unittests": TestJingle,
    "layout": TestLayout,        "layout_tests": TestLayout,
    "webkit": TestLayout,
    "media": TestMedia,          "media_unittests": TestMedia,
    "message_center": TestMessageCenter,
    "message_center_unittests" : TestMessageCenter,
    "net": TestNet,              "net_unittests": TestNet,
    "net_perf": TestNetPerf,     "net_perftests": TestNetPerf,
    "ppapi": TestPPAPI,          "ppapi_unittests": TestPPAPI,
    "printing": TestPrinting,    "printing_unittests": TestPrinting,
    "reliability": TestReliability, "reliability_tests": TestReliability,
    "remoting": TestRemoting,    "remoting_unittests": TestRemoting,
    "safe_browsing": TestSafeBrowsing, "safe_browsing_tests": TestSafeBrowsing,
    "sandbox": TestLinuxSandbox, "sandbox_linux_unittests": TestLinuxSandbox,
    "sql": TestSql,              "sql_unittests": TestSql,
    "sync": TestSync,            "sync_unit_tests": TestSync,
    "sync_integration_tests": TestSyncIntegration,
    "sync_integration": TestSyncIntegration,
    "ui_unit": TestUIUnit,       "ui_unittests": TestUIUnit,
    "unit": TestUnit,            "unit_tests": TestUnit,
    "url": TestURL,              "url_unittests": TestURL,
    "views": TestViews,          "views_unittests": TestViews,
  }


def _main():
  parser = optparse.OptionParser("usage: %prog -b <dir> -t <test> "
                                 "[-t <test> ...]")
  parser.disable_interspersed_args()

  parser.add_option("", "--help-tests", dest="help_tests", action="store_true",
                    default=False, help="List all available tests")
  parser.add_option("-b", "--build_dir",
                    help="the location of the compiler output")
  parser.add_option("-t", "--test", action="append", default=[],
                    help="which test to run, supports test:gtest_filter format "
                         "as well.")
  parser.add_option("", "--baseline", action="store_true", default=False,
                    help="generate baseline data instead of validating")
  parser.add_option("", "--gtest_filter",
                    help="additional arguments to --gtest_filter")
  parser.add_option("", "--gtest_repeat",
                    help="argument for --gtest_repeat")
  parser.add_option("-v", "--verbose", action="store_true", default=False,
                    help="verbose output - enable debug log messages")
  parser.add_option("", "--tool", dest="valgrind_tool", default="memcheck",
                    help="specify a valgrind tool to run the tests under")
  parser.add_option("", "--tool_flags", dest="valgrind_tool_flags", default="",
                    help="specify custom flags for the selected valgrind tool")
  parser.add_option("", "--keep_logs", action="store_true", default=False,
                    help="store memory tool logs in the <tool>.logs directory "
                         "instead of /tmp.\nThis can be useful for tool "
                         "developers/maintainers.\nPlease note that the <tool>"
                         ".logs directory will be clobbered on tool startup.")
  parser.add_option("-n", "--num_tests", type="int",
                    default=ChromeTests.LAYOUT_TESTS_DEFAULT_CHUNK_SIZE,
                    help="for layout tests: # of subtests per run.  0 for all.")
  # TODO(thestig) Remove this if we can.
  parser.add_option("", "--gtest_color", dest="gtest_color", default="no",
                    help="dummy compatibility flag for sharding_supervisor.")

  options, args = parser.parse_args()

  if options.verbose:
    logging_utils.config_root(logging.DEBUG)
  else:
    logging_utils.config_root()

  if options.help_tests:
    ChromeTests.ShowTests()
    return 0

  if not options.test:
    parser.error("--test not specified")

  if len(options.test) != 1 and options.gtest_filter:
    parser.error("--gtest_filter and multiple tests don't make sense together")

  for t in options.test:
    tests = ChromeTests(options, args, t)
    ret = tests.Run()
    if ret: return ret
  return 0


if __name__ == "__main__":
  sys.exit(_main())
