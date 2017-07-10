// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/file.h"

#include <gflags/gflags.h>
#include <algorithm>
#include <memory>
#include "packager/base/files/important_file_writer.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_piece.h"
#include "packager/file/local_file.h"
#include "packager/file/memory_file.h"
#include "packager/file/threaded_io_file.h"
#include "packager/file/udp_file.h"

DEFINE_uint64(io_cache_size,
              32ULL << 20,
              "Size of the threaded I/O cache, in bytes. Specify 0 to disable "
              "threaded I/O.");
DEFINE_uint64(io_block_size,
              2ULL << 20,
              "Size of the block size used for threaded I/O, in bytes.");

// Needed for Windows weirdness which somewhere defines CopyFile as CopyFileW.
#ifdef CopyFile
#undef CopyFile
#endif  // CopyFile

namespace shaka {

const char* kLocalFilePrefix = "file://";
const char* kUdpFilePrefix = "udp://";
const char* kMemoryFilePrefix = "memory://";

namespace {

typedef File* (*FileFactoryFunction)(const char* file_name, const char* mode);
typedef bool (*FileDeleteFunction)(const char* file_name);
typedef bool (*FileAtomicWriteFunction)(const char* file_name,
                                        const std::string& contents);

struct FileTypeInfo {
  const char* type;
  size_t type_length;
  const FileFactoryFunction factory_function;
  const FileDeleteFunction delete_function;
  const FileAtomicWriteFunction atomic_write_function;
};

File* CreateLocalFile(const char* file_name, const char* mode) {
  return new LocalFile(file_name, mode);
}

bool DeleteLocalFile(const char* file_name) {
  return LocalFile::Delete(file_name);
}

bool WriteLocalFileAtomically(const char* file_name,
                              const std::string& contents) {
  return base::ImportantFileWriter::WriteFileAtomically(
      base::FilePath::FromUTF8Unsafe(file_name), contents);
}

File* CreateUdpFile(const char* file_name, const char* mode) {
  if (strcmp(mode, "r")) {
    NOTIMPLEMENTED() << "UdpFile only supports read (receive) mode.";
    return NULL;
  }
  return new UdpFile(file_name);
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
        strlen(kLocalFilePrefix),
        &CreateLocalFile,
        &DeleteLocalFile,
        &WriteLocalFileAtomically,
    },
    {kUdpFilePrefix, strlen(kUdpFilePrefix), &CreateUdpFile, nullptr, nullptr},
    {kMemoryFilePrefix, strlen(kMemoryFilePrefix), &CreateMemoryFile,
     &DeleteMemoryFile, nullptr},
};

const FileTypeInfo* GetFileTypeInfo(base::StringPiece file_name,
                                    base::StringPiece* real_file_name) {
  for (const FileTypeInfo& file_type : kFileTypeInfo) {
    if (strncmp(file_type.type, file_name.data(), file_type.type_length) == 0) {
      *real_file_name = file_name.substr(file_type.type_length);
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

  if (!strncmp(file_name, kMemoryFilePrefix, strlen(kMemoryFilePrefix))) {
    // Disable caching for memory files.
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
  base::StringPiece real_file_name;
  const FileTypeInfo* file_type = GetFileTypeInfo(file_name, &real_file_name);
  DCHECK(file_type);
  return file_type->delete_function
             ? file_type->delete_function(real_file_name.data())
             : false;
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

bool File::WriteFileAtomically(const char* file_name,
                               const std::string& contents) {
  base::StringPiece real_file_name;
  const FileTypeInfo* file_type = GetFileTypeInfo(file_name, &real_file_name);
  DCHECK(file_type);
  if (file_type->atomic_write_function)
    return file_type->atomic_write_function(real_file_name.data(), contents);

  // Provide a default implementation which may not be atomic unfortunately.

  // Skip the warning message for memory files, which is meant for testing
  // anyway..
  if (strncmp(file_name, kMemoryFilePrefix, strlen(kMemoryFilePrefix)) != 0) {
    LOG(WARNING) << "Writing to " << file_name
                 << " is not guaranteed to be atomic.";
  }

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
  return true;
}

bool File::Copy(const char* from_file_name, const char* to_file_name) {
  std::string content;
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

}  // namespace shaka
