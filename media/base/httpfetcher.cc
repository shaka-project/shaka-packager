// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/httpfetcher.h"

#ifdef WIN32
#include <winsock2.h>
#endif  // WIN32

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/happyhttp/src/happyhttp.h"

namespace {

struct HTTPResult {
  int status_code;
  std::string status_message;
  std::string response;
};

bool ExtractUrlParams(const std::string& url, std::string* host,
                      std::string* path, int* port) {
  DCHECK(host && path && port);

  static const char kHttp[] = "http://";
  // arraysize counts the last null character, which needs to be removed.
  const char kHttpSize = arraysize(kHttp) - 1;
  static const char kHttps[] = "https://";
  const char kHttpsSize = arraysize(kHttps) - 1;
  size_t host_start_pos;
  if (StartsWithASCII(url, kHttp, false)) {
    host_start_pos = kHttpSize;
  } else if (StartsWithASCII(url, kHttps, false)) {
    host_start_pos = kHttpsSize;
    NOTIMPLEMENTED() << "Secure HTTP is not implemented yet.";
    return false;
  } else {
    host_start_pos = 0;
  }

  const size_t npos = std::string::npos;
  const size_t port_start_pos = url.find(':', host_start_pos);
  const size_t path_start_pos = url.find('/', host_start_pos);

  size_t host_size;
  if (port_start_pos == npos) {
    const int kStandardHttpPort = 80;
    *port = kStandardHttpPort;

    host_size = path_start_pos == npos ? npos : path_start_pos - host_start_pos;
  } else {
    if (port_start_pos >= path_start_pos)
      return false;
    const size_t port_size =
        path_start_pos == npos ? npos : path_start_pos - port_start_pos - 1;
    if (!base::StringToInt(url.substr(port_start_pos + 1, port_size), port))
      return false;

    host_size = port_start_pos - host_start_pos;
  }

  *host = url.substr(host_start_pos, host_size);
  *path = path_start_pos == npos ? "/" : url.substr(path_start_pos);
  return true;
}

// happyhttp event callbacks.
void OnBegin(const happyhttp::Response* response, void* userdata) {
  DCHECK(response && userdata);
  DLOG(INFO) << "BEGIN (" << response->getstatus() << ", "
             << response->getreason() << ").";

  HTTPResult* result = static_cast<HTTPResult*>(userdata);
  result->status_code = response->getstatus();
  result->status_message = response->getreason();
  result->response.clear();
}

void OnData(const happyhttp::Response* response,
            void* userdata,
            const unsigned char* data,
            int num_bytes) {
  DCHECK(response && userdata && data);
  HTTPResult* result = static_cast<HTTPResult*>(userdata);
  result->response.append(reinterpret_cast<const char*>(data), num_bytes);
}

void OnComplete(const happyhttp::Response* response, void* userdata) {
  DCHECK(response && userdata);
  HTTPResult* result = static_cast<HTTPResult*>(userdata);
  DLOG(INFO) << "COMPLETE (" << result->response.size() << " bytes).";
}

const int kHttpOK = 200;

}  // namespace

namespace media {

HTTPFetcher::HTTPFetcher() {
#ifdef WIN32
  WSAData wsa_data;
  int code = WSAStartup(MAKEWORD(1, 1), &wsa_data);
  wsa_startup_succeeded_ = (code == 0);
  if (!wsa_startup_succeeded_)
    LOG(ERROR) << "WSAStartup failed with code " << code;
#endif  // WIN32
}

HTTPFetcher::~HTTPFetcher() {
#ifdef WIN32
  if (wsa_startup_succeeded_)
    WSACleanup();
#endif  // WIN32
}

Status HTTPFetcher::Get(const std::string& path, std::string* response) {
  return FetchInternal("GET", path, "", response);
}

Status HTTPFetcher::Post(const std::string& path, const std::string& data,
                         std::string* response) {
  return FetchInternal("POST", path, data, response);
}

Status HTTPFetcher::FetchInternal(const std::string& method,
                                  const std::string& url,
                                  const std::string& data,
                                  std::string* response) {
  DCHECK(response);

  int status_code = 0;

  std::string host;
  std::string path;
  int port = 0;
  if (!ExtractUrlParams(url, &host, &path, &port)) {
    std::string error_message = "Cannot extract url parameters from " + url;
    LOG(ERROR) << error_message;
    return Status(error::INVALID_ARGUMENT, error_message);
  }

  try {
    HTTPResult result;
    happyhttp::Connection connection(host.data(), port);
    connection.setcallbacks(OnBegin, OnData, OnComplete, &result);

    VLOG(1) << "Send " << method << " request to " << url << ": " << data;

    static const char* kHeaders[] = {
        "Connection",   "close",
        "Content-type", "application/x-www-form-urlencoded",
        "Accept", "text/plain",
        0};
    connection.request(
        method.data(), path.data(), kHeaders,
        data.empty() ? NULL : reinterpret_cast<const uint8*>(data.data()),
        data.size());

    while (connection.outstanding())
      connection.pump();

    status_code = result.status_code;
    *response = result.response;

    VLOG(1) << "Response: " << result.response;
  } catch (happyhttp::Wobbly& exception) {
    std::string error_message =
        std::string("HTTP fetcher failed: ") + exception.what();
    LOG(ERROR) << error_message;
    return Status(error::HTTP_FAILURE, error_message);
  }

  if (status_code != kHttpOK) {
    std::string error_message = "HTTP returns status " + base::IntToString(status_code);
    LOG(ERROR) << error_message;
    return Status(error::HTTP_FAILURE, error_message);
  }
  return Status::OK;
}

}  // namespace media
