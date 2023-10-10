// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/test/test_web_server.h>

#include <chrono>
#include <random>
#include <string_view>

#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>
#include <mongoose.h>
#include <nlohmann/json.hpp>

// A full replacement for our former use of httpbin.org in tests.  This
// embedded web server can:
//
// 1. Reflect the request method, body, and headers
// 2. Return a requested status code
// 3. Delay a response by a requested amount of time

namespace {

// A random HTTP port will be chosen, and if there is a collision, we will try
// again up to |kMaxPortTries| times.
const int kMinPortNumber = 58000;
const int kMaxPortNumber = 58999;
const int kMaxPortTries = 10;

// Get a string_view on mongoose's mg_string, which may not be nul-terminated.
std::string_view MongooseStringView(const mg_str& mg_string) {
  return std::string_view(mg_string.ptr, mg_string.len);
}

bool IsMongooseStringNull(const mg_str& mg_string) {
  return mg_string.ptr == NULL;
}

bool IsMongooseStringNullOrBlank(const mg_str& mg_string) {
  return mg_string.ptr == NULL || mg_string.len == 0;
}

// Get a string query parameter from a mongoose HTTP message.
bool GetStringQueryParameter(struct mg_http_message* message,
                             const char* name,
                             std::string_view* str) {
  struct mg_str value_mg_str = mg_http_var(message->query, mg_str(name));

  if (!IsMongooseStringNull(value_mg_str)) {
    *str = MongooseStringView(value_mg_str);
    return true;
  }

  return false;
}

// Get an integer query parameter from a mongoose HTTP message.
bool GetIntQueryParameter(struct mg_http_message* message,
                          const char* name,
                          int* value) {
  std::string_view str;

  if (GetStringQueryParameter(message, name, &str)) {
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

bool TestWebServer::Start() {
  thread_.reset(new std::thread(&TestWebServer::ThreadCallback, this));

  absl::MutexLock lock(&mutex_);
  while (status_ == kNew) {
    started_.Wait(&mutex_);
  }

  return status_ == kStarted;
}

bool TestWebServer::TryListenOnPort(struct mg_mgr* manager, int port) {
  // Mongoose needs an HTTP server address in string format.
  // "127.0.0.1" is "localhost", and is not visible to other machines on the
  // network.
  base_url_ = absl::StrFormat("http://127.0.0.1:%d", port);

  auto connection =
      mg_http_listen(manager, base_url_.c_str(), &TestWebServer::HandleEvent,
                     this /* callback_data */);
  return connection != NULL;
}

void TestWebServer::ThreadCallback() {
  // Set up the manager structure to be automatically cleaned up when it leaves
  // scope.
  std::unique_ptr<struct mg_mgr, decltype(&mg_mgr_free)> manager(
      new struct mg_mgr, mg_mgr_free);
  // Then initialize it.
  mg_mgr_init(manager.get());

  // Prepare to choose a random port.
  std::random_device r;
  std::default_random_engine e1(r());
  std::uniform_int_distribution<int> uniform_dist(kMinPortNumber,
                                                  kMaxPortNumber);

  bool ok = false;
  for (int i = 0; !ok && i < kMaxPortTries; ++i) {
    // Choose a random port in the range.
    int port = uniform_dist(e1);
    ok = TryListenOnPort(manager.get(), port);
  }

  {
    absl::MutexLock lock(&mutex_);
    if (!ok) {
      // Failed to find a port to listen on.  Mongoose has already printed an
      // error message.
      status_ = kFailed;
      started_.Signal();
      return;
    }

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
      if (stopped)
        status_ = kStopped;
    }
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

  if (GetIntQueryParameter(message, "code", &code)) {
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
  if (message && GetIntQueryParameter(message, "seconds", &seconds)) {
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
  reply["method"] = MongooseStringView(message->method);
  if (!IsMongooseStringNull(message->body)) {
    reply["body"] = MongooseStringView(message->body);
  }

  nlohmann::json headers;
  for (int i = 0; i < MG_MAX_HTTP_HEADERS; ++i) {
    struct mg_http_header header = message->headers[i];
    if (IsMongooseStringNullOrBlank(header.name)) {
      break;
    }

    headers[MongooseStringView(header.name)] = MongooseStringView(header.value);
  }
  reply["headers"] = headers;

  mg_http_reply(connection, 200 /* OK */, NULL /* headers */, "%s\n",
                reply.dump().c_str());
  return true;
}

}  // namespace media
}  // namespace shaka
