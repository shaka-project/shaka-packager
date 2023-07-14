// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_TEST_TEST_WEB_SERVER_H_
#define PACKAGER_MEDIA_TEST_TEST_WEB_SERVER_H_

#include <map>
#include <memory>
#include <thread>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

// Forward declare mongoose struct types, used as pointers below.
struct mg_connection;
struct mg_http_message;

namespace shaka {
namespace media {

class TestWebServer {
 public:
  TestWebServer();
  ~TestWebServer();

  bool Start(int port);

 private:
  enum TestWebServerStatus {
    kNew,
    kFailed,
    kStarted,
    kStopped,
  };

  absl::Mutex mutex_;
  TestWebServerStatus status_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar started_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar stop_ ABSL_GUARDED_BY(mutex_);
  bool stopped_ ABSL_GUARDED_BY(mutex_);

  // Connections to be handled again later, mapped to the time at which we
  // should handle them again.  We can't block the server thread directly to
  // simulate delays.  Only ever accessed from |thread_|.
  std::map<struct mg_connection*, absl::Time> delayed_connections_;

  std::unique_ptr<std::thread> thread_;

  void ThreadCallback(int port);

  static void HandleEvent(struct mg_connection* connection,
                          int event,
                          void* event_data,
                          void* callback_data);

  bool HandleStatus(struct mg_http_message* message,
                    struct mg_connection* connection);
  bool HandleDelay(struct mg_http_message* message,
                   struct mg_connection* connection);
  bool HandleReflect(struct mg_http_message* message,
                     struct mg_connection* connection);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_TEST_TEST_WEB_SERVER_H_
