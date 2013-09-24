// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int testint = 2093847192;
const std::string teststr("Hello world");  // note non-aligned string length
const std::wstring testwstr(L"Hello, world");
const char testdata[] = "AAA\0BBB\0";
const int testdatalen = arraysize(testdata) - 1;
const bool testbool1 = false;
const bool testbool2 = true;
const uint16 testuint16 = 32123;
const float testfloat = 3.1415926935f;

// checks that the result
void VerifyResult(const Pickle& pickle) {
  PickleIterator iter(pickle);

  int outint;
  EXPECT_TRUE(pickle.ReadInt(&iter, &outint));
  EXPECT_EQ(testint, outint);

  std::string outstr;
  EXPECT_TRUE(pickle.ReadString(&iter, &outstr));
  EXPECT_EQ(teststr, outstr);

  std::wstring outwstr;
  EXPECT_TRUE(pickle.ReadWString(&iter, &outwstr));
  EXPECT_EQ(testwstr, outwstr);

  bool outbool;
  EXPECT_TRUE(pickle.ReadBool(&iter, &outbool));
  EXPECT_FALSE(outbool);
  EXPECT_TRUE(pickle.ReadBool(&iter, &outbool));
  EXPECT_TRUE(outbool);

  uint16 outuint16;
  EXPECT_TRUE(pickle.ReadUInt16(&iter, &outuint16));
  EXPECT_EQ(testuint16, outuint16);

  float outfloat;
  EXPECT_TRUE(pickle.ReadFloat(&iter, &outfloat));
  EXPECT_EQ(testfloat, outfloat);

  const char* outdata;
  int outdatalen;
  EXPECT_TRUE(pickle.ReadData(&iter, &outdata, &outdatalen));
  EXPECT_EQ(testdatalen, outdatalen);
  EXPECT_EQ(memcmp(testdata, outdata, outdatalen), 0);

  EXPECT_TRUE(pickle.ReadData(&iter, &outdata, &outdatalen));
  EXPECT_EQ(testdatalen, outdatalen);
  EXPECT_EQ(memcmp(testdata, outdata, outdatalen), 0);

  // reads past the end should fail
  EXPECT_FALSE(pickle.ReadInt(&iter, &outint));
}

}  // namespace

TEST(PickleTest, EncodeDecode) {
  Pickle pickle;

  EXPECT_TRUE(pickle.WriteInt(testint));
  EXPECT_TRUE(pickle.WriteString(teststr));
  EXPECT_TRUE(pickle.WriteWString(testwstr));
  EXPECT_TRUE(pickle.WriteBool(testbool1));
  EXPECT_TRUE(pickle.WriteBool(testbool2));
  EXPECT_TRUE(pickle.WriteUInt16(testuint16));
  EXPECT_TRUE(pickle.WriteFloat(testfloat));
  EXPECT_TRUE(pickle.WriteData(testdata, testdatalen));

  // Over allocate BeginWriteData so we can test TrimWriteData.
  char* dest = pickle.BeginWriteData(testdatalen + 100);
  EXPECT_TRUE(dest);
  memcpy(dest, testdata, testdatalen);

  pickle.TrimWriteData(testdatalen);

  VerifyResult(pickle);

  // test copy constructor
  Pickle pickle2(pickle);
  VerifyResult(pickle2);

  // test operator=
  Pickle pickle3;
  pickle3 = pickle;
  VerifyResult(pickle3);
}

// Tests that we can handle really small buffers.
TEST(PickleTest, SmallBuffer) {
  scoped_ptr<char[]> buffer(new char[1]);

  // We should not touch the buffer.
  Pickle pickle(buffer.get(), 1);

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(pickle.ReadInt(&iter, &data));
}

// Tests that we can handle improper headers.
TEST(PickleTest, BigSize) {
  int buffer[] = { 0x56035200, 25, 40, 50 };

  Pickle pickle(reinterpret_cast<char*>(buffer), sizeof(buffer));

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(pickle.ReadInt(&iter, &data));
}

TEST(PickleTest, UnalignedSize) {
  int buffer[] = { 10, 25, 40, 50 };

  Pickle pickle(reinterpret_cast<char*>(buffer), sizeof(buffer));

  PickleIterator iter(pickle);
  int data;
  EXPECT_FALSE(pickle.ReadInt(&iter, &data));
}

TEST(PickleTest, ZeroLenStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteString(std::string()));

  PickleIterator iter(pickle);
  std::string outstr;
  EXPECT_TRUE(pickle.ReadString(&iter, &outstr));
  EXPECT_EQ("", outstr);
}

TEST(PickleTest, ZeroLenWStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteWString(std::wstring()));

  PickleIterator iter(pickle);
  std::string outstr;
  EXPECT_TRUE(pickle.ReadString(&iter, &outstr));
  EXPECT_EQ("", outstr);
}

TEST(PickleTest, BadLenStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(-2));

  PickleIterator iter(pickle);
  std::string outstr;
  EXPECT_FALSE(pickle.ReadString(&iter, &outstr));
}

TEST(PickleTest, BadLenWStr) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(-1));

  PickleIterator iter(pickle);
  std::wstring woutstr;
  EXPECT_FALSE(pickle.ReadWString(&iter, &woutstr));
}

TEST(PickleTest, FindNext) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteInt(1));
  EXPECT_TRUE(pickle.WriteString("Domo"));

  const char* start = reinterpret_cast<const char*>(pickle.data());
  const char* end = start + pickle.size();

  EXPECT_TRUE(end == Pickle::FindNext(pickle.header_size_, start, end));
  EXPECT_TRUE(NULL == Pickle::FindNext(pickle.header_size_, start, end - 1));
  EXPECT_TRUE(end == Pickle::FindNext(pickle.header_size_, start, end + 1));
}

TEST(PickleTest, FindNextWithIncompleteHeader) {
  size_t header_size = sizeof(Pickle::Header);
  scoped_ptr<char[]> buffer(new char[header_size - 1]);
  memset(buffer.get(), 0x1, header_size - 1);

  const char* start = buffer.get();
  const char* end = start + header_size - 1;

  EXPECT_TRUE(NULL == Pickle::FindNext(header_size, start, end));
}

TEST(PickleTest, GetReadPointerAndAdvance) {
  Pickle pickle;

  PickleIterator iter(pickle);
  EXPECT_FALSE(iter.GetReadPointerAndAdvance(1));

  EXPECT_TRUE(pickle.WriteInt(1));
  EXPECT_TRUE(pickle.WriteInt(2));
  int bytes = sizeof(int) * 2;

  EXPECT_TRUE(PickleIterator(pickle).GetReadPointerAndAdvance(0));
  EXPECT_TRUE(PickleIterator(pickle).GetReadPointerAndAdvance(1));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(-1));
  EXPECT_TRUE(PickleIterator(pickle).GetReadPointerAndAdvance(bytes));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(bytes + 1));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(INT_MAX));
  EXPECT_FALSE(PickleIterator(pickle).GetReadPointerAndAdvance(INT_MIN));
}

TEST(PickleTest, Resize) {
  size_t unit = Pickle::kPayloadUnit;
  scoped_ptr<char[]> data(new char[unit]);
  char* data_ptr = data.get();
  for (size_t i = 0; i < unit; i++)
    data_ptr[i] = 'G';

  // construct a message that will be exactly the size of one payload unit,
  // note that any data will have a 4-byte header indicating the size
  const size_t payload_size_after_header = unit - sizeof(uint32);
  Pickle pickle;
  pickle.WriteData(data_ptr,
      static_cast<int>(payload_size_after_header - sizeof(uint32)));
  size_t cur_payload = payload_size_after_header;

  // note: we assume 'unit' is a power of 2
  EXPECT_EQ(unit, pickle.capacity());
  EXPECT_EQ(pickle.payload_size(), payload_size_after_header);

  // fill out a full page (noting data header)
  pickle.WriteData(data_ptr, static_cast<int>(unit - sizeof(uint32)));
  cur_payload += unit;
  EXPECT_EQ(unit * 2, pickle.capacity());
  EXPECT_EQ(cur_payload, pickle.payload_size());

  // one more byte should double the capacity
  pickle.WriteData(data_ptr, 1);
  cur_payload += 5;
  EXPECT_EQ(unit * 4, pickle.capacity());
  EXPECT_EQ(cur_payload, pickle.payload_size());
}

namespace {

struct CustomHeader : Pickle::Header {
  int blah;
};

}  // namespace

TEST(PickleTest, HeaderPadding) {
  const uint32 kMagic = 0x12345678;

  Pickle pickle(sizeof(CustomHeader));
  pickle.WriteInt(kMagic);

  // this should not overwrite the 'int' payload
  pickle.headerT<CustomHeader>()->blah = 10;

  PickleIterator iter(pickle);
  int result;
  ASSERT_TRUE(pickle.ReadInt(&iter, &result));

  EXPECT_EQ(static_cast<uint32>(result), kMagic);
}

TEST(PickleTest, EqualsOperator) {
  Pickle source;
  source.WriteInt(1);

  Pickle copy_refs_source_buffer(static_cast<const char*>(source.data()),
                                 source.size());
  Pickle copy;
  copy = copy_refs_source_buffer;
  ASSERT_EQ(source.size(), copy.size());
}

TEST(PickleTest, EvilLengths) {
  Pickle source;
  std::string str(100000, 'A');
  EXPECT_TRUE(source.WriteData(str.c_str(), 100000));
  // ReadString16 used to have its read buffer length calculation wrong leading
  // to out-of-bounds reading.
  PickleIterator iter(source);
  string16 str16;
  EXPECT_FALSE(source.ReadString16(&iter, &str16));

  // And check we didn't break ReadString16.
  str16 = (wchar_t) 'A';
  Pickle str16_pickle;
  EXPECT_TRUE(str16_pickle.WriteString16(str16));
  iter = PickleIterator(str16_pickle);
  EXPECT_TRUE(str16_pickle.ReadString16(&iter, &str16));
  EXPECT_EQ(1U, str16.length());

  // Check we don't fail in a length check with invalid String16 size.
  // (1<<31) * sizeof(char16) == 0, so this is particularly evil.
  Pickle bad_len;
  EXPECT_TRUE(bad_len.WriteInt(1 << 31));
  iter = PickleIterator(bad_len);
  EXPECT_FALSE(bad_len.ReadString16(&iter, &str16));

  // Check we don't fail in a length check with large WStrings.
  Pickle big_len;
  EXPECT_TRUE(big_len.WriteInt(1 << 30));
  iter = PickleIterator(big_len);
  std::wstring wstr;
  EXPECT_FALSE(big_len.ReadWString(&iter, &wstr));
}

// Check we can write zero bytes of data and 'data' can be NULL.
TEST(PickleTest, ZeroLength) {
  Pickle pickle;
  EXPECT_TRUE(pickle.WriteData(NULL, 0));

  PickleIterator iter(pickle);
  const char* outdata;
  int outdatalen;
  EXPECT_TRUE(pickle.ReadData(&iter, &outdata, &outdatalen));
  EXPECT_EQ(0, outdatalen);
  // We can't assert that outdata is NULL.
}

// Check that ReadBytes works properly with an iterator initialized to NULL.
TEST(PickleTest, ReadBytes) {
  Pickle pickle;
  int data = 0x7abcd;
  EXPECT_TRUE(pickle.WriteBytes(&data, sizeof(data)));

  PickleIterator iter(pickle);
  const char* outdata_char = NULL;
  EXPECT_TRUE(pickle.ReadBytes(&iter, &outdata_char, sizeof(data)));

  int outdata;
  memcpy(&outdata, outdata_char, sizeof(outdata));
  EXPECT_EQ(data, outdata);
}
