// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/mp4/box_buffer.h"
#include "media/mp4/rcheck.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace mp4 {

static const uint8 kSkipBox[] = {
    // Top-level test box containing three children.
    0x00, 0x00, 0x00, 0x40, 's',  'k',  'i',  'p',  0x01, 0x02, 0x03, 0x04,
    0x05, 0x06, 0x07, 0x08, 0xf9, 0x0a, 0x0b, 0x0c, 0xfd, 0x0e, 0x0f, 0x10,
    // Ordinary (8-byte header) child box.
    0x00, 0x00, 0x00, 0x0c, 'p',  's',  's',  'h',  0xde, 0xad, 0xbe, 0xef,
    // Extended-size header child box.
    0x00, 0x00, 0x00, 0x01, 'p',  's',  's',  'h',  0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x14, 0xfa, 0xce, 0xca, 0xfe,
    // Empty free box.
    0x00, 0x00, 0x00, 0x08, 'f',  'r',  'e',  'e',
    // Trailing garbage.
    0x00};

struct FreeBox : Box {
  virtual bool ReadWrite(BoxBuffer* buffer) OVERRIDE {
    return true;
  }
  virtual FourCC BoxType() const OVERRIDE { return FOURCC_FREE; }
  virtual uint32 ComputeSize() OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }
};

struct PsshBox : Box {
  virtual bool ReadWrite(BoxBuffer* buffer) OVERRIDE {
    return buffer->ReadWriteUInt32(&val);
  }
  virtual FourCC BoxType() const OVERRIDE { return FOURCC_PSSH; }
  virtual uint32 ComputeSize() OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }

  uint32 val;
};

struct SkipBox : FullBox {
  virtual bool ReadWrite(BoxBuffer* buffer) OVERRIDE {
    RCHECK(FullBox::ReadWrite(buffer) &&
           buffer->ReadWriteUInt8(&a) &&
           buffer->ReadWriteUInt8(&b) &&
           buffer->ReadWriteUInt16(&c) &&
           buffer->ReadWriteInt32(&d) &&
           buffer->ReadWriteInt64NBytes(&e, sizeof(uint32)));
    RCHECK(buffer->PrepareChildren());
    if (buffer->Reading()) {
      DCHECK(buffer->reader());
      RCHECK(buffer->reader()->ReadChildren(&kids));
    } else {
      NOTIMPLEMENTED();
    }
    return buffer->TryReadWriteChild(&empty);
  }
  virtual FourCC BoxType() const OVERRIDE { return FOURCC_SKIP; }
  virtual uint32 ComputeSize() OVERRIDE {
    NOTIMPLEMENTED();
    return 0;
  }

  uint8 a, b;
  uint16 c;
  int32 d;
  int64 e;

  std::vector<PsshBox> kids;
  FreeBox empty;
};

class BoxReaderTest : public testing::Test {
 protected:
  std::vector<uint8> GetBuf() {
    return std::vector<uint8>(kSkipBox, kSkipBox + sizeof(kSkipBox));
  }
};

TEST_F(BoxReaderTest, ExpectedOperationTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));
  EXPECT_FALSE(err);
  EXPECT_TRUE(reader.get());

  SkipBox box;
  EXPECT_TRUE(box.Parse(reader.get()));
  EXPECT_EQ(0x01, box.version);
  EXPECT_EQ(0x020304u, box.flags);
  EXPECT_EQ(0x05, box.a);
  EXPECT_EQ(0x06, box.b);
  EXPECT_EQ(0x0708, box.c);
  EXPECT_EQ(static_cast<int32>(0xf90a0b0c), box.d);
  EXPECT_EQ(static_cast<int32>(0xfd0e0f10), box.e);

  EXPECT_EQ(2u, box.kids.size());
  EXPECT_EQ(0xdeadbeef, box.kids[0].val);
  EXPECT_EQ(0xfacecafe, box.kids[1].val);

  // Accounting for the extra byte outside of the box above.
  EXPECT_EQ(buf.size(), static_cast<uint64>(reader->size() + 1));
}

TEST_F(BoxReaderTest, OuterTooShortTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;

  // Create a soft failure by truncating the outer box.
  scoped_ptr<BoxReader> r(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size() - 2, &err));

  EXPECT_FALSE(err);
  EXPECT_FALSE(r.get());
}

TEST_F(BoxReaderTest, InnerTooLongTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;

  // Make an inner box too big for its outer box.
  buf[25] = 1;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  SkipBox box;
  EXPECT_FALSE(box.Parse(reader.get()));
}

TEST_F(BoxReaderTest, WrongFourCCTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;

  // Set an unrecognized top-level FourCC.
  buf[5] = 1;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));
  EXPECT_FALSE(reader.get());
  EXPECT_TRUE(err);
}

TEST_F(BoxReaderTest, ScanChildrenTest) {
  std::vector<uint8> buf = GetBuf();
  bool err;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  EXPECT_TRUE(reader->SkipBytes(16) && reader->ScanChildren());

  FreeBox free;
  EXPECT_TRUE(reader->ReadChild(&free));
  EXPECT_FALSE(reader->ReadChild(&free));
  EXPECT_TRUE(reader->TryReadChild(&free));

  std::vector<PsshBox> kids;

  EXPECT_TRUE(reader->ReadChildren(&kids));
  EXPECT_EQ(2u, kids.size());
  kids.clear();
  EXPECT_FALSE(reader->ReadChildren(&kids));
  EXPECT_TRUE(reader->TryReadChildren(&kids));
}

TEST_F(BoxReaderTest, ReadAllChildrenTest) {
  std::vector<uint8> buf = GetBuf();
  // Modify buffer to exclude its last 'free' box.
  buf[3] = 0x38;
  bool err;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  std::vector<PsshBox> kids;
  EXPECT_TRUE(reader->SkipBytes(16) && reader->ReadAllChildren(&kids));
  EXPECT_EQ(2u, kids.size());
  EXPECT_EQ(kids[0].val, 0xdeadbeef);   // Ensure order is preserved.
}

TEST_F(BoxReaderTest, SkippingBloc) {
  static const uint8 kData[] = {0x00, 0x00, 0x00, 0x09,  // Box size.
                                'b',  'l',  'o',  'c',   // FourCC.
                                0x00};                   // Reserved byte.

  std::vector<uint8> buf(kData, kData + sizeof(kData));

  bool err;
  scoped_ptr<BoxReader> reader(
      BoxReader::ReadTopLevelBox(&buf[0], buf.size(), &err));

  EXPECT_FALSE(err);
  EXPECT_TRUE(reader);
  EXPECT_EQ(FOURCC_BLOC, reader->type());
}

}  // namespace mp4
}  // namespace media
