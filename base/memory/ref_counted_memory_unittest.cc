// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(RefCountedMemoryUnitTest, RefCountedStaticMemory) {
  scoped_refptr<RefCountedMemory> mem = new RefCountedStaticMemory(
      reinterpret_cast<const uint8*>("static mem00"), 10);

  EXPECT_EQ(10U, mem->size());
  EXPECT_EQ("static mem",
            std::string(reinterpret_cast<const char*>(mem->front()),
                        mem->size()));
}

TEST(RefCountedMemoryUnitTest, RefCountedBytes) {
  std::vector<uint8> data;
  data.push_back(45);
  data.push_back(99);
  scoped_refptr<RefCountedMemory> mem = RefCountedBytes::TakeVector(&data);

  EXPECT_EQ(0U, data.size());

  EXPECT_EQ(2U, mem->size());
  EXPECT_EQ(45U, mem->front()[0]);
  EXPECT_EQ(99U, mem->front()[1]);
}

TEST(RefCountedMemoryUnitTest, RefCountedString) {
  std::string s("destroy me");
  scoped_refptr<RefCountedMemory> mem = RefCountedString::TakeString(&s);

  EXPECT_EQ(0U, s.size());

  EXPECT_EQ(10U, mem->size());
  EXPECT_EQ('d', mem->front()[0]);
  EXPECT_EQ('e', mem->front()[1]);
}

TEST(RefCountedMemoryUnitTest, Equals) {
  std::string s1("same");
  scoped_refptr<RefCountedMemory> mem1 = RefCountedString::TakeString(&s1);

  std::vector<unsigned char> d2;
  d2.push_back('s');
  d2.push_back('a');
  d2.push_back('m');
  d2.push_back('e');
  scoped_refptr<RefCountedMemory> mem2 = RefCountedBytes::TakeVector(&d2);

  EXPECT_TRUE(mem1->Equals(mem2));

  std::string s3("diff");
  scoped_refptr<RefCountedMemory> mem3 = RefCountedString::TakeString(&s3);

  EXPECT_FALSE(mem1->Equals(mem3));
  EXPECT_FALSE(mem2->Equals(mem3));
}

TEST(RefCountedMemoryUnitTest, EqualsNull) {
  std::string s("str");
  scoped_refptr<RefCountedMemory> mem = RefCountedString::TakeString(&s);
  EXPECT_FALSE(mem->Equals(NULL));
}

}  //  namespace base
