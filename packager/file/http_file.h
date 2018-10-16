// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_HTTP_H_
#define PACKAGER_FILE_HTTP_H_

#include <curl/curl.h>

#include "packager/base/compiler_specific.h"
#include "packager/file/file.h"
#include "packager/status.h"

namespace shaka {

/// HttpFile delegates read calls to HTTP GET requests and
/// write calls to HTTP PATCH requests by following the
/// sabre/dav WebDAV implementation [1].
///
/// For running this on your workbench, please visit the
/// corresponding documentation [2,3].
///
/// [1] http://sabre.io/dav/http-patch/
/// [2] https://google.github.io/shaka-packager/html/tutorials/http_upload.html
/// [3] https://github.com/3QSDN/shaka-packager/blob/http-upload/docs/source/tutorials/http_upload.rst
///
class HttpFile : public File {

    public:

        /// Create a HTTP client with no timeout
        /// @param file_name C string containing the url of the resource to be accessed.
        ///        Note that the file type prefix should be stripped off already.
        /// @param mode C string containing a file access mode, refer to fopen for
        ///        the available modes.
        HttpFile(const char* file_name, const char* mode);

        /// Create a HTTP client with timeout
        /// @param file_name C string containing the url of the resource to be accessed.
        ///        Note that the file type prefix should be stripped off already.
        /// @param mode C string containing a file access mode, refer to fopen for
        ///        the available modes.
        /// @param timeout_in_seconds specifies the timeout in seconds.
        HttpFile(const char* file_name, const char* mode, uint32_t timeout_in_seconds);

        /// @name File implementation overrides.
        /// @{
        bool Close() override;
        int64_t Read(void* buffer, uint64_t length) override;
        int64_t Write(const void* buffer, uint64_t length) override;
        int64_t Size() override;
        bool Flush() override;
        bool Seek(uint64_t position) override;
        bool Tell(uint64_t* position) override;
        /// @}

        /// @return The full resource url
        const std::string& resource_url() const { return resource_url_; }

    protected:
        // Destructor
        ~HttpFile() override;

        bool Open() override;

    private:

        enum HttpMethod {
            GET,
            POST,
            PUT,
            PATCH,
        };

        HttpFile(const HttpFile&) = delete;
        HttpFile& operator=(const HttpFile&) = delete;

        // Internal implementation of HTTP functions, e.g. Get and Post.
        bool SetupRequest(CURL* curl,
                          std::string* response,
                          HttpMethod method,
                          const std::string& url,
                          const std::string& data,
                          const bool append_data);

        Status Request(std::string* response,
                       HttpMethod method,
                       const std::string& url,
                       const std::string& data,
                       const bool append_data);

        std::string method_as_text(HttpMethod method);

        std::string resource_url_;
        std::string file_mode_;
        const uint32_t timeout_in_seconds_;

    };

}  // namespace shaka

#endif  // PACKAGER_FILE_HTTP_H_
