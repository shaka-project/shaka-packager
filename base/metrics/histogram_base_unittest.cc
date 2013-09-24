// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class HistogramBaseTest : public testing::Test {
 protected:
  HistogramBaseTest() {
    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    statistics_recorder_ = NULL;
    ResetStatisticsRecorder();
  }

  virtual ~HistogramBaseTest() {
    delete statistics_recorder_;
  }

  void ResetStatisticsRecorder() {
    delete statistics_recorder_;
    statistics_recorder_ = new StatisticsRecorder();
  }

 private:
  StatisticsRecorder* statistics_recorder_;
};

TEST_F(HistogramBaseTest, DeserializeHistogram) {
  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10,
      (HistogramBase::kUmaTargetedHistogramFlag |
      HistogramBase::kIPCSerializationSourceFlag));

  Pickle pickle;
  ASSERT_TRUE(histogram->SerializeInfo(&pickle));

  PickleIterator iter(pickle);
  HistogramBase* deserialized = DeserializeHistogramInfo(&iter);
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = DeserializeHistogramInfo(&iter2);
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(1, 1000, 10));

  // kIPCSerializationSourceFlag will be cleared.
  EXPECT_EQ(HistogramBase::kUmaTargetedHistogramFlag, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeHistogramAndAddSamples) {
  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kIPCSerializationSourceFlag);
  histogram->Add(1);
  histogram->Add(10);
  histogram->Add(100);
  histogram->Add(1000);

  Pickle pickle;
  ASSERT_TRUE(histogram->SerializeInfo(&pickle));
  histogram->SnapshotSamples()->Serialize(&pickle);

  PickleIterator iter(pickle);
  DeserializeHistogramAndAddSamples(&iter);

  // The histogram has kIPCSerializationSourceFlag. So samples will be ignored.
  scoped_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(1, snapshot->GetCount(1));
  EXPECT_EQ(1, snapshot->GetCount(10));
  EXPECT_EQ(1, snapshot->GetCount(100));
  EXPECT_EQ(1, snapshot->GetCount(1000));

  // Clear kIPCSerializationSourceFlag to emulate multi-process usage.
  histogram->ClearFlags(HistogramBase::kIPCSerializationSourceFlag);
  PickleIterator iter2(pickle);
  DeserializeHistogramAndAddSamples(&iter2);

  scoped_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(2, snapshot2->GetCount(1));
  EXPECT_EQ(2, snapshot2->GetCount(10));
  EXPECT_EQ(2, snapshot2->GetCount(100));
  EXPECT_EQ(2, snapshot2->GetCount(1000));
}

TEST_F(HistogramBaseTest, DeserializeLinearHistogram) {
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      "TestHistogram", 1, 1000, 10,
      HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  ASSERT_TRUE(histogram->SerializeInfo(&pickle));

  PickleIterator iter(pickle);
  HistogramBase* deserialized = DeserializeHistogramInfo(&iter);
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = DeserializeHistogramInfo(&iter2);
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(1, 1000, 10));
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeBooleanHistogram) {
  HistogramBase* histogram = BooleanHistogram::FactoryGet(
      "TestHistogram", HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  ASSERT_TRUE(histogram->SerializeInfo(&pickle));

  PickleIterator iter(pickle);
  HistogramBase* deserialized = DeserializeHistogramInfo(&iter);
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = DeserializeHistogramInfo(&iter2);
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(1, 2, 3));
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeCustomHistogram) {
  std::vector<HistogramBase::Sample> ranges;
  ranges.push_back(13);
  ranges.push_back(5);
  ranges.push_back(9);

  HistogramBase* histogram = CustomHistogram::FactoryGet(
      "TestHistogram", ranges, HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  ASSERT_TRUE(histogram->SerializeInfo(&pickle));

  PickleIterator iter(pickle);
  HistogramBase* deserialized = DeserializeHistogramInfo(&iter);
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = DeserializeHistogramInfo(&iter2);
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_TRUE(deserialized->HasConstructionArguments(5, 13, 4));
  EXPECT_EQ(0, deserialized->flags());
}

TEST_F(HistogramBaseTest, DeserializeSparseHistogram) {
  HistogramBase* histogram = SparseHistogram::FactoryGet(
      "TestHistogram", HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  ASSERT_TRUE(histogram->SerializeInfo(&pickle));

  PickleIterator iter(pickle);
  HistogramBase* deserialized = DeserializeHistogramInfo(&iter);
  EXPECT_EQ(histogram, deserialized);

  ResetStatisticsRecorder();

  PickleIterator iter2(pickle);
  deserialized = DeserializeHistogramInfo(&iter2);
  EXPECT_TRUE(deserialized);
  EXPECT_NE(histogram, deserialized);
  EXPECT_EQ("TestHistogram", deserialized->histogram_name());
  EXPECT_EQ(0, deserialized->flags());
}

}  // namespace base
