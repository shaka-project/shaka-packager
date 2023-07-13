// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/test/test_web_server.h"

#include <chrono>
#include <string_view>

#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "mongoose.h"
#include "nlohmann/json.hpp"

// A full replacement for our former use of httpbin.org in tests.  This
// embedded web server can:
//
// 1. Reflect the request method, body, and headers
// 2. Return a requested status code
// 3. Delay a response by a requested amount of time

namespace {

// Get a string_view on mongoose's mg_string, which may not be nul-terminated.
std::string_view view_mg_str(const mg_str& mg_string) {
  return std::string_view(mg_string.ptr, mg_string.len);
}

bool is_mg_str_null(const mg_str& mg_string) {
  return mg_string.ptr == NULL;
}

bool is_mg_str_null_or_blank(const mg_str& mg_string) {
  return mg_string.ptr == NULL || mg_string.len == 0;
}

// Get a string query parameter from a mongoose HTTP message.
bool get_string_query_parameter(struct mg_http_message* message,
                                const char* name,
                                std::string_view* str) {
  struct mg_str value_mg_str = mg_http_var(message->query, mg_str(name));

  if (!is_mg_str_null(value_mg_str)) {
    *str = view_mg_str(value_mg_str);
    return true;
  }

  return false;
}

// Get an integer query parameter from a mongoose HTTP message.
bool get_int_query_parameter(struct mg_http_message* message,
                             const char* name,
                             int* value) {
  std::string_view str;

  if (get_string_query_parameter(message, name, &str)) {
    return absl::SimpleAtoi(str, value);
  }

  return false;
}

}  // namespace

namespace shaka {
namespace media {

TestWebServer::TestWebServer() : status_(kNew), stopped_(false) {}

TestWebServer::~TestWebServer() {
  {
    absl::MutexLock lock(&mutex_);
    stop_.Signal();
    stopped_ = true;
  }
  if (thread_) {
    thread_->join();
  }
  thread_.reset();
}

bool TestWebServer::Start(int port) {
  thread_.reset(new std::thread(&TestWebServer::ThreadCallback, this, port));

  absl::MutexLock lock(&mutex_);
  while (status_ == kNew) {
    started_.Wait(&mutex_);
  }

  return status_ == kStarted;
}

void TestWebServer::ThreadCallback(int port) {
  // Mongoose needs an HTTP server address in string format.
  // "127.0.0.1" is "localhost", and is not visible to other machines on the
  // network.
  std::string http_address = absl::StrFormat("http://127.0.0.1:%d", port);

  // Set up the manager structure to be automatically cleaned up when it leaves
  // scope.
  std::unique_ptr<struct mg_mgr, decltype(&mg_mgr_free)> manager(
      new struct mg_mgr, mg_mgr_free);
  // Then initialize it.
  mg_mgr_init(manager.get());

  auto connection =
      mg_http_listen(manager.get(), http_address.c_str(),
                     &TestWebServer::HandleEvent, this /* callback_data */);
  if (connection == NULL) {
    // Failed to listen to the requested port.  Mongoose has already printed an
    // error message.
    absl::MutexLock lock(&mutex_);
    status_ = kFailed;
    started_.Signal();
    return;
  }

  {
    absl::MutexLock lock(&mutex_);
    status_ = kStarted;
    started_.Signal();
  }

  bool stopped = false;
  while (!stopped) {
    // Let Mongoose poll the sockets for 100ms.
    mg_mgr_poll(manager.get(), 100);

    // Check for a stop signal from the test.
    {
      absl::MutexLock lock(&mutex_);
      stopped = stopped_;
    }
  }

  {
    absl::MutexLock lock(&mutex_);
    status_ = kStopped;
  }
}

// static
void TestWebServer::HandleEvent(struct mg_connection* connection,
                                int event,
                                void* event_data,
                                void* callback_data) {
  TestWebServer* instance = static_cast<TestWebServer*>(callback_data);

  if (event == MG_EV_POLL) {
    std::vector<struct mg_connection*> to_delete;

    // Check if it's time to re-handle any delayed connections.
    for (const auto& pair : instance->delayed_connections_) {
      const auto delayed_connection = pair.first;
      const auto deadline = pair.second;
      if (deadline <= absl::Now()) {
        to_delete.push_back(delayed_connection);
        instance->HandleDelay(NULL, delayed_connection);
      }
    }

    // Now that we're done iterating the map, delete any connections we are done
    // responding to.
    for (const auto& delayed_connection : to_delete) {
      instance->delayed_connections_.erase(delayed_connection);
    }
  } else if (event == MG_EV_CLOSE) {
    if (instance->delayed_connections_.count(connection)) {
      // The client hung up before our delay expired.  Remove this from our map.
      instance->delayed_connections_.erase(connection);
    }
  }

  if (event != MG_EV_HTTP_MSG)
    return;

  struct mg_http_message* message =
      static_cast<struct mg_http_message*>(event_data);
  if (mg_http_match_uri(message, "/reflect")) {
    if (instance->HandleReflect(message, connection))
      return;
  } else if (mg_http_match_uri(message, "/status")) {
    if (instance->HandleStatus(message, connection))
      return;
  } else if (mg_http_match_uri(message, "/delay")) {
    if (instance->HandleDelay(message, connection))
      return;
  }

  mg_http_reply(connection, 400 /* bad request */, NULL /* headers */,
                "Bad request!");
}

bool TestWebServer::HandleStatus(struct mg_http_message* message,
                                 struct mg_connection* connection) {
  int code = 0;

  if (get_int_query_parameter(message, "code", &code)) {
    // Reply with the requested status code.
    mg_http_reply(connection, code, NULL /* headers */, "%s", "{}");
    return true;
  }

  return false;
}

bool TestWebServer::HandleDelay(struct mg_http_message* message,
                                struct mg_connection* connection) {
  if (delayed_connections_.count(connection)) {
    // We're being called back after a delay has elapsed.
    // Respond now.
    mg_http_reply(connection, 200 /* OK */, NULL /* headers */, "%s", "{}");
    return true;
  }

  int seconds = 0;
  // Checking |message| here is a small safety measure, since we call this
  // method back a second time with message set to NULL.  That is supposed to
  // be handled above, but this is defense in depth against a crash.
  if (message && get_int_query_parameter(message, "seconds", &seconds)) {
    // We can't block this thread, so compute the deadline and add the
    // connection to a map.  The main handler will call us back later if the
    // client doesn't hang up first.
    absl::Time deadline = absl::Now() + absl::Seconds(seconds);
    delayed_connections_[connection] = deadline;
    return true;
  }

  return false;
}

bool TestWebServer::HandleReflect(struct mg_http_message* message,
                                  struct mg_connection* connection) {
  // Serialize a reply in JSON that reflects the request method, body, and
  // headers.
  nlohmann::json reply;
  reply["method"] = view_mg_str(message->method);
  if (!is_mg_str_null(message->body)) {
    reply["body"] = view_mg_str(message->body);
  }

  nlohmann::json headers;
  for (int i = 0; i < MG_MAX_HTTP_HEADERS; ++i) {
    struct mg_http_header header = message->headers[i];
    if (is_mg_str_null_or_blank(header.name)) {
      break;
    }

    headers[view_mg_str(header.name)] = view_mg_str(header.value);
  }
  reply["headers"] = headers;

  mg_http_reply(connection, 200 /* OK */, NULL /* headers */, "%s\n",
                reply.dump().c_str());
  return true;
}

}  // namespace media
}  // namespace shaka
