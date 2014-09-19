// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_UDP_FILE_H_
#define MEDIA_FILE_UDP_FILE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/file/file.h"

namespace edash_packager {
namespace media {

/// Implements UdpFile, which receives UDP unicast and multicast streams.
class UdpFile : public File {
 public:
  /// @param file_name C string containing the address of the stream to receive.
  ///        It should be of the form "<ip_address>:<port>".
  explicit UdpFile(const char* address_and_port);

  /// @name File implementation overrides.
  /// @{
  virtual bool Close() OVERRIDE;
  virtual int64 Read(void* buffer, uint64 length) OVERRIDE;
  virtual int64 Write(const void* buffer, uint64 length) OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool Flush() OVERRIDE;
  virtual bool Eof() OVERRIDE;
  /// @}

 protected:
  virtual ~UdpFile();

  virtual bool Open() OVERRIDE;

 private:
  int socket_;

  DISALLOW_COPY_AND_ASSIGN(UdpFile);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FILE_UDP_FILE_H_
