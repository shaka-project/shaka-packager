#include "packager/file/http_file.h"
#include <gtest/gtest.h>
#include <memory>
#include "packager/file/file.h"
#include "packager/file/file_closer.h"

namespace shaka {
namespace {

const uint8_t kWriteBuffer[] = {1, 2, 3, 4, 5, 6, 7, 8};
const int64_t kWriteBufferSize = sizeof(kWriteBuffer);

}  // namespace

class HttpFileTest : public testing::Test {};

TEST_F(HttpFileTest, PutChunkedTranser) {
  std::unique_ptr<File, FileCloser> writer(
      File::Open("http://127.0.0.1:8080/test_out", "w"));
  ASSERT_TRUE(writer);
  ASSERT_EQ(kWriteBufferSize, writer->Write(kWriteBuffer, kWriteBufferSize));
  writer.release()->Close();
}

}  // namespace shaka
