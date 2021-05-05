// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_HTTP_H_
#define PACKAGER_FILE_HTTP_H_

#include <memory>
#include <string>

#include "packager/base/synchronization/waitable_event.h"
#include "packager/file/file.h"
#include "packager/file/io_cache.h"
#include "packager/status.h"

typedef void CURL;
struct curl_slist;

namespace shaka {

enum class HttpMethod {
  kGet,
  kPost,
  kPut,
};

/// HttpFile reads or writes network requests.
///
/// Note that calling Flush will indicate EOF for the upload and no more can be
/// uploaded.
///
/// About how to use this, please visit the corresponding documentation [1].
///
/// [1] https://google.github.io/shaka-packager/html/tutorials/http_upload.html
///
class HttpFile : public File {
 public:
  HttpFile(HttpMethod method, const std::string& url);
  HttpFile(HttpMethod method,
           const std::string& url,
           const std::string& upload_content_type,
           const std::vector<std::string>& headers,
           uint32_t timeout_in_seconds);

  HttpFile(const HttpFile&) = delete;
  HttpFile& operator=(const HttpFile&) = delete;

  Status CloseWithStatus();

  /// @name File implementation overrides.
  /// @{
  bool Close() override;
  int64_t Read(void* buffer, uint64_t length) override;
  int64_t Write(const void* buffer, uint64_t length) override;
  int64_t Size() override;
  bool Flush() override;
  bool Seek(uint64_t position) override;
  bool Tell(uint64_t* position) override;
  bool Open() override;
  /// @}

 protected:
  ~HttpFile() override;

 private:
  struct CurlDelete {
    void operator()(CURL* curl);
    void operator()(curl_slist* headers);
  };

  void SetupRequest();
  void ThreadMain();

  const std::string url_;
  const std::string upload_content_type_;
  const uint32_t timeout_in_seconds_;
  const HttpMethod method_;
  IoCache download_cache_;
  IoCache upload_cache_;
  std::unique_ptr<CURL, CurlDelete> curl_;
  // The headers need to remain alive for the duration of the request.
  std::unique_ptr<curl_slist, CurlDelete> request_headers_;
  Status status_;
  std::string user_agent_;

  // Signaled when the "curl easy perform" task completes.
  base::WaitableEvent task_exit_event_;
};

}  // namespace shaka

#endif  // PACKAGER_FILE_HTTP_H_
