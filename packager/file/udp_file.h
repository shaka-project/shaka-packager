// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_UDP_FILE_H_
#define MEDIA_FILE_UDP_FILE_H_

#include <cstdint>
#include <string>

#if defined(OS_WIN)
#include <windows.h>
#include <winsock2.h>
#else
typedef int SOCKET;
#endif  // defined(OS_WIN)

#include <packager/file.h>
#include <packager/macros/classes.h>

namespace shaka {

/// Implements UdpFile, which receives UDP unicast and multicast streams.
class UdpFile : public File {
 public:
  /// @param file_name C string containing the address of the stream to receive.
  ///        It should be of the form "<ip_address>:<port>".
  explicit UdpFile(const char* address_and_port);

  /// @name File implementation overrides.
  /// @{
  bool Close() override;
  int64_t Read(void* buffer, uint64_t length) override;
  int64_t Write(const void* buffer, uint64_t length) override;
  void CloseForWriting() override;
  int64_t Size() override;
  bool Flush() override;
  bool Seek(uint64_t position) override;
  bool Tell(uint64_t* position) override;
  /// @}

 protected:
  ~UdpFile() override;

  bool Open() override;

 private:
  SOCKET socket_;
#if defined(OS_WIN)
  // For Winsock in Windows.
  bool wsa_started_ = false;
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(UdpFile);
};

}  // namespace shaka

#endif  // MEDIA_FILE_UDP_FILE_H_
