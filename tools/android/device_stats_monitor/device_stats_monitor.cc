// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Dumps CPU and IO stats to a file at a regular interval.
//
// Output may be post processed by host to get top/iotop style information.

#include <signal.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/strings/string_split.h"

namespace {

const char kIOStatsPath[] = "/proc/diskstats";
const char kCPUStatsPath[] = "/proc/stat";

class DeviceStatsMonitor {
 public:
  explicit DeviceStatsMonitor(const std::string& out_path)
      : out_path_(out_path),
        record_(true) {
    CHECK(!out_path_.empty());
    samples_.reserve(1024 * 1024);
  }

  // Records stats continuously at |hz| cycles per second util
  // StopRecordingAndDumpStats() is called.
  //
  // Yes, this buffers everything in memory, so it cannot be used for extended
  // durations without OOM. But that beats writing during the trace which
  // would affect the results.
  void Start(int hz) {
    const int sample_interval = 1000000 / hz;
    const base::FilePath io_stats_path(kIOStatsPath);
    const base::FilePath cpu_stats_path(kCPUStatsPath);
    std::string out;
    while (record_) {
      out.clear();
      CHECK(file_util::ReadFileToString(io_stats_path, &out));
      CHECK(file_util::ReadFileToString(cpu_stats_path, &out));
      samples_.push_back(out);
      usleep(sample_interval);
    }
  }

  // Stops recording and saves samples to file.
  void StopAndDumpStats() {
    record_ = false;
    usleep(250 * 1000);
    std::ofstream out_stream;
    out_stream.open(out_path_.value().c_str(), std::ios::out);
    for (std::vector<std::string>::const_iterator i = samples_.begin();
         i != samples_.end(); ++i) {
      out_stream << i->c_str() << std::endl;
    }
    out_stream.close();
  }

 private:
  const base::FilePath out_path_;
  std::vector<std::string> samples_;
  bool record_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatsMonitor);
};

DeviceStatsMonitor* g_device_stats_monitor = NULL;

void SigTermHandler(int unused) {
  printf("Stopping device stats monitor\n");
  g_device_stats_monitor->StopAndDumpStats();
}

}  // namespace

int main(int argc, char** argv) {
  const int kDefaultHz = 20;

  CommandLine command_line(argc, argv);
  CommandLine::StringVector args = command_line.GetArgs();
  if (command_line.HasSwitch("h") || command_line.HasSwitch("help") ||
      args.size() != 1) {
    printf("Usage: %s OUTPUT_FILE\n"
           "  --hz=HZ              Number of samples/second. default=%d\n",
           argv[0], kDefaultHz);
    return 1;
  }

  int hz = command_line.HasSwitch("hz") ?
      atoi(command_line.GetSwitchValueNative("hz").c_str()) :
      kDefaultHz;

  printf("Starting device stats monitor\n");
  g_device_stats_monitor = new DeviceStatsMonitor(args[0]);
  signal(SIGTERM, SigTermHandler);
  g_device_stats_monitor->Start(hz);
  delete g_device_stats_monitor;

  return 0;
}
