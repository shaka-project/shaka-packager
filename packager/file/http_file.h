// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_HTTP_H_
#define PACKAGER_FILE_HTTP_H_

#include "packager/base/compiler_specific.h"
#include "packager/file/file.h"
#include "packager/status.h"

namespace shaka {

/// Implements HttpFile, which delegates read calls to HTTP GET requests and
/// write calls to HTTP WebDAV PATCH requests by following http://sabre.io/dav/http-patch/
    class HttpFile : public File {
    public:
        /// @param file_name C string containing the url of the resource to be accessed.
        ///        Note that the file type prefix should be stripped off already.
        /// @param mode C string containing a file access mode, refer to fopen for
        ///        the available modes.
        HttpFile(const char* file_name, const char* mode);

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

    protected:
        ~HttpFile() override;

        bool Open() override;

    private:

        enum HttpMethod {
            GET,
            POST,
            PUT
        };

        HttpFile(const HttpFile&) = delete;
        HttpFile& operator=(const HttpFile&) = delete;

        std::string name_;
        std::string file_mode_;
    };

}  // namespace shaka

#endif  // PACKAGER_FILE_HTTP_H_
