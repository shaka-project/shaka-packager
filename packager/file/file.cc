// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/file.h"

#include <gflags/gflags.h>
#include <inttypes.h>
#include <algorithm>
#include <memory>
#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_piece.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/file/callback_file.h"
#include "packager/file/file_util.h"
#include "packager/file/local_file.h"
#include "packager/file/memory_file.h"
#include "packager/file/threaded_io_file.h"
#include "packager/file/udp_file.h"
#include "packager/file/http_file.h"

DEFINE_uint64(io_cache_size,
              32ULL << 20,
              "Size of the threaded I/O cache, in bytes. Specify 0 to disable "
              "threaded I/O.");
DEFINE_uint64(io_block_size,
              1ULL << 16,
              "Size of the block size used for threaded I/O, in bytes.");

// Needed for Windows weirdness which somewhere defines CopyFile as CopyFileW.
#ifdef CopyFile
#undef CopyFile
#endif  // CopyFile

namespace shaka {

const char* kCallbackFilePrefix = "callback://";
const char* kLocalFilePrefix = "file://";
const char* kMemoryFilePrefix = "memory://";
const char* kUdpFilePrefix = "udp://";
const char* kHttpFilePrefix = "http://";
const char* kHttpsFilePrefix = "https://";


namespace {

typedef File* (*FileFactoryFunction)(const char* file_name, const char* mode);
typedef bool (*FileDeleteFunction)(const char* file_name);
typedef bool (*FileAtomicWriteFunction)(const char* file_name,
                                        const std::string& contents);

struct FileTypeInfo {
  const char* type;
  const FileFactoryFunction factory_function;
  const FileDeleteFunction delete_function;
  const FileAtomicWriteFunction atomic_write_function;
};

File* CreateCallbackFile(const char* file_name, const char* mode) {
  return new CallbackFile(file_name, mode);
}

File* CreateLocalFile(const char* file_name, const char* mode) {
  return new LocalFile(file_name, mode);
}

bool DeleteLocalFile(const char* file_name) {
  return LocalFile::Delete(file_name);
}

bool WriteLocalFileAtomically(const char* file_name,
                              const std::string& contents) {
  const base::FilePath file_path = base::FilePath::FromUTF8Unsafe(file_name);
  const std::string dir_name = file_path.DirName().AsUTF8Unsafe();
  std::string temp_file_name;
  if (!TempFilePath(dir_name, &temp_file_name))
    return false;
  if (!File::WriteStringToFile(temp_file_name.c_str(), contents))
    return false;
  base::File::Error replace_file_error = base::File::FILE_OK;
  if (!base::ReplaceFile(base::FilePath::FromUTF8Unsafe(temp_file_name),
                         file_path, &replace_file_error)) {
    LOG(ERROR) << "Failed to replace file '" << file_name << "' with '"
               << temp_file_name << "', error: " << replace_file_error;
    return false;
  }
  return true;
}

File* CreateUdpFile(const char* file_name, const char* mode) {
  if (strcmp(mode, "r")) {
    NOTIMPLEMENTED() << "UdpFile only supports read (receive) mode.";
    return NULL;
  }
  return new UdpFile(file_name);
}

File* CreateHttpsFile(const char* file_name, const char* mode) {
  return new HttpFile(HttpMethod::kPut, std::string("https://") + file_name);
}

File* CreateHttpFile(const char* file_name, const char* mode) {
  return new HttpFile(HttpMethod::kPut, std::string("http://") + file_name);
}

File* CreateMemoryFile(const char* file_name, const char* mode) {
  return new MemoryFile(file_name, mode);
}

bool DeleteMemoryFile(const char* file_name) {
  MemoryFile::Delete(file_name);
  return true;
}

static const FileTypeInfo kFileTypeInfo[] = {
    {
        kLocalFilePrefix,
        &CreateLocalFile,
        &DeleteLocalFile,
        &WriteLocalFileAtomically,
    },
    {kUdpFilePrefix, &CreateUdpFile, nullptr, nullptr},
    {kMemoryFilePrefix, &CreateMemoryFile, &DeleteMemoryFile, nullptr},
    {kCallbackFilePrefix, &CreateCallbackFile, nullptr, nullptr},
    {kHttpFilePrefix, &CreateHttpFile, nullptr, nullptr},
    {kHttpsFilePrefix, &CreateHttpsFile, nullptr, nullptr},
};

base::StringPiece GetFileTypePrefix(base::StringPiece file_name) {
  size_t pos = file_name.find("://");
  return (pos == std::string::npos) ? "" : file_name.substr(0, pos + 3);
}

const FileTypeInfo* GetFileTypeInfo(base::StringPiece file_name,
                                    base::StringPiece* real_file_name) {
  base::StringPiece file_type_prefix = GetFileTypePrefix(file_name);
  for (const FileTypeInfo& file_type : kFileTypeInfo) {
    if (file_type_prefix == file_type.type) {
      *real_file_name = file_name.substr(file_type_prefix.size());
      return &file_type;
    }
  }
  // Otherwise we default to the first file type, which is LocalFile.
  *real_file_name = file_name;
  return &kFileTypeInfo[0];
}

}  // namespace

File* File::Create(const char* file_name, const char* mode) {
  std::unique_ptr<File, FileCloser> internal_file(
      CreateInternalFile(file_name, mode));

  base::StringPiece file_type_prefix = GetFileTypePrefix(file_name);
  if (file_type_prefix == kMemoryFilePrefix ||
      file_type_prefix == kCallbackFilePrefix) {
    // Disable caching for memory and callback files.
    return internal_file.release();
  }

  if (FLAGS_io_cache_size) {
    // Enable threaded I/O for "r", "w", and "a" modes only.
    if (!strcmp(mode, "r")) {
      return new ThreadedIoFile(std::move(internal_file),
                                ThreadedIoFile::kInputMode, FLAGS_io_cache_size,
                                FLAGS_io_block_size);
    } else if (!strcmp(mode, "w") || !strcmp(mode, "a")) {
      return new ThreadedIoFile(std::move(internal_file),
                                ThreadedIoFile::kOutputMode,
                                FLAGS_io_cache_size, FLAGS_io_block_size);
    }
  }

  // Threaded I/O is disabled.
  DLOG(WARNING) << "Threaded I/O is disabled. Performance may be decreased.";
  return internal_file.release();
}

File* File::CreateInternalFile(const char* file_name, const char* mode) {
  base::StringPiece real_file_name;
  const FileTypeInfo* file_type = GetFileTypeInfo(file_name, &real_file_name);
  DCHECK(file_type);
  return file_type->factory_function(real_file_name.data(), mode);
}

File* File::Open(const char* file_name, const char* mode) {
  File* file = File::Create(file_name, mode);
  if (!file)
    return NULL;
  if (!file->Open()) {
    delete file;
    return NULL;
  }
  return file;
}

File* File::OpenWithNoBuffering(const char* file_name, const char* mode) {
  File* file = File::CreateInternalFile(file_name, mode);
  if (!file)
    return NULL;
  if (!file->Open()) {
    delete file;
    return NULL;
  }
  return file;
}

bool File::Delete(const char* file_name) {
  static bool logged = false;
  base::StringPiece real_file_name;
  const FileTypeInfo* file_type = GetFileTypeInfo(file_name, &real_file_name);
  DCHECK(file_type);
  if (file_type->delete_function) {
    return file_type->delete_function(real_file_name.data());
  } else {
    if (!logged) {
      logged = true;
      LOG(WARNING) << "File::Delete: file type for "
            << file_name
            << " ('" << file_type->type << "') "
            << "has no 'delete' function.";
    }
    return true;
  }
}

int64_t File::GetFileSize(const char* file_name) {
  File* file = File::Open(file_name, "r");
  if (!file)
    return -1;
  int64_t res = file->Size();
  file->Close();
  return res;
}

bool File::ReadFileToString(const char* file_name, std::string* contents) {
  DCHECK(contents);

  File* file = File::Open(file_name, "r");
  if (!file)
    return false;

  const size_t kBufferSize = 0x40000;  // 256KB.
  std::unique_ptr<char[]> buf(new char[kBufferSize]);

  int64_t len;
  while ((len = file->Read(buf.get(), kBufferSize)) > 0)
    contents->append(buf.get(), len);

  file->Close();
  return len == 0;
}

bool File::WriteStringToFile(const char* file_name,
                             const std::string& contents) {
  VLOG(2) << "File::WriteStringToFile: " << file_name;
  std::unique_ptr<File, FileCloser> file(File::Open(file_name, "w"));
  if (!file) {
    LOG(ERROR) << "Failed to open file " << file_name;
    return false;
  }
  int64_t bytes_written = file->Write(contents.data(), contents.size());
  if (bytes_written < 0) {
    LOG(ERROR) << "Failed to write to file '" << file_name << "' ("
               << bytes_written << ").";
    return false;
  }
  if (static_cast<size_t>(bytes_written) != contents.size()) {
    LOG(ERROR) << "Failed to write the whole file to " << file_name
               << ". Wrote " << bytes_written << " but expecting "
               << contents.size() << " bytes.";
    return false;
  }
  if (!file.release()->Close()) {
    LOG(ERROR)
        << "Failed to close file '" << file_name
        << "', possibly file permission issue or running out of disk space.";
    return false;
  }
  return true;
}

bool File::WriteFileAtomically(const char* file_name,
                               const std::string& contents) {
  VLOG(2) << "File::WriteFileAtomically: " << file_name;
  base::StringPiece real_file_name;
  const FileTypeInfo* file_type = GetFileTypeInfo(file_name, &real_file_name);
  DCHECK(file_type);
  if (file_type->atomic_write_function)
    return file_type->atomic_write_function(real_file_name.data(), contents);

  // Provide a default implementation which may not be atomic unfortunately.

  // Skip the warning message for memory files, which is meant for testing
  // anyway..
  // Also check for http files, as they can't do atomic writes.
  if (strncmp(file_name, kMemoryFilePrefix, strlen(kMemoryFilePrefix)) != 0
      && strncmp(file_name, kHttpFilePrefix, strlen(kHttpFilePrefix)) != 0
      && strncmp(file_name, kHttpsFilePrefix, strlen(kHttpsFilePrefix)) != 0) {
    LOG(WARNING) << "Writing to " << file_name
                 << " is not guaranteed to be atomic.";
  }
  return WriteStringToFile(file_name, contents);
}

bool File::Copy(const char* from_file_name, const char* to_file_name) {
  std::string content;
  VLOG(2) << "File::Copy from " << from_file_name << " to " << to_file_name;
  if (!ReadFileToString(from_file_name, &content)) {
    LOG(ERROR) << "Failed to open file " << from_file_name;
    return false;
  }

  std::unique_ptr<File, FileCloser> output_file(File::Open(to_file_name, "w"));
  if (!output_file) {
    LOG(ERROR) << "Failed to write to " << to_file_name;
    return false;
  }

  uint64_t bytes_left = content.size();
  uint64_t total_bytes_written = 0;
  const char* content_cstr = content.c_str();
  while (bytes_left > total_bytes_written) {
    const int64_t bytes_written =
        output_file->Write(content_cstr + total_bytes_written, bytes_left);
    if (bytes_written < 0) {
      LOG(ERROR) << "Failure while writing to " << to_file_name;
      return false;
    }

    total_bytes_written += bytes_written;
  }
  if (!output_file.release()->Close()) {
    LOG(ERROR)
        << "Failed to close file '" << to_file_name
        << "', possibly file permission issue or running out of disk space.";
    return false;
  }
  return true;
}

int64_t File::CopyFile(File* source, File* destination) {
  return CopyFile(source, destination, kWholeFile);
}

int64_t File::CopyFile(File* source, File* destination, int64_t max_copy) {
  DCHECK(source);
  DCHECK(destination);
  if (max_copy < 0)
    max_copy = std::numeric_limits<int64_t>::max();

  VLOG(2) << "File::CopyFile from " << source->file_name() << " to "
          << destination->file_name();

  const int64_t kBufferSize = 0x40000;  // 256KB.
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[kBufferSize]);
  int64_t bytes_copied = 0;
  while (bytes_copied < max_copy) {
    const int64_t size = std::min(kBufferSize, max_copy - bytes_copied);
    const int64_t bytes_read = source->Read(buffer.get(), size);
    if (bytes_read < 0)
      return bytes_read;
    if (bytes_read == 0)
      break;

    int64_t total_bytes_written = 0;
    while (total_bytes_written < bytes_read) {
      const int64_t bytes_written = destination->Write(
          buffer.get() + total_bytes_written, bytes_read - total_bytes_written);
      if (bytes_written < 0)
        return bytes_written;

      total_bytes_written += bytes_written;
    }

    DCHECK_EQ(total_bytes_written, bytes_read);
    bytes_copied += bytes_read;
  }

  return bytes_copied;
}

bool File::IsLocalRegularFile(const char* file_name) {
  base::StringPiece real_file_name;
  const FileTypeInfo* file_type = GetFileTypeInfo(file_name, &real_file_name);
  DCHECK(file_type);
  if (file_type->type != kLocalFilePrefix)
    return false;
#if defined(OS_WIN)
  const base::FilePath file_path(
      base::FilePath::FromUTF8Unsafe(real_file_name));
  const DWORD fileattr = GetFileAttributes(file_path.value().c_str());
  if (fileattr == INVALID_FILE_ATTRIBUTES) {
    LOG(ERROR) << "Failed to GetFileAttributes of " << file_path.value();
    return false;
  }
  return (fileattr & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  struct stat info;
  if (stat(real_file_name.data(), &info) != 0) {
    LOG(ERROR) << "Failed to run stat on " << real_file_name;
    return false;
  }
  return S_ISREG(info.st_mode);
#endif
}

std::string File::MakeCallbackFileName(
    const BufferCallbackParams& callback_params,
    const std::string& name) {
  if (name.empty())
    return "";
  return base::StringPrintf("%s%" PRIdPTR "/%s", kCallbackFilePrefix,
                            reinterpret_cast<intptr_t>(&callback_params),
                            name.c_str());
}

bool File::ParseCallbackFileName(const std::string& callback_file_name,
                                 const BufferCallbackParams** callback_params,
                                 std::string* name) {
  size_t pos = callback_file_name.find("/");
  int64_t callback_address = 0;
  if (pos == std::string::npos ||
      !base::StringToInt64(callback_file_name.substr(0, pos),
                           &callback_address)) {
    LOG(ERROR) << "Expecting CallbackFile with name like "
                  "'<callback address>/<entity name>', but seeing "
               << callback_file_name;
    return false;
  }
  *callback_params = reinterpret_cast<BufferCallbackParams*>(callback_address);
  *name = callback_file_name.substr(pos + 1);
  return true;
}

}  // namespace shaka
