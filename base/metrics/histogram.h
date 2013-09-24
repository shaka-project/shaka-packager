// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Histogram is an object that aggregates statistics, and can summarize them in
// various forms, including ASCII graphical, HTML, and numerically (as a
// vector of numbers corresponding to each of the aggregating buckets).

// It supports calls to accumulate either time intervals (which are processed
// as integral number of milliseconds), or arbitrary integral units.

// For Histogram(exponential histogram), LinearHistogram and CustomHistogram,
// the minimum for a declared range is 1 (instead of 0), while the maximum is
// (HistogramBase::kSampleType_MAX - 1). Currently you can declare histograms
// with ranges exceeding those limits (e.g. 0 as minimal or
// HistogramBase::kSampleType_MAX as maximal), but those excesses will be
// silently clamped to those limits (for backwards compatibility with existing
// code). Best practice is to not exceed the limits.

// Each use of a histogram with the same name will reference the same underlying
// data, so it is safe to record to the same histogram from multiple locations
// in the code. It is a runtime error if all uses of the same histogram do not
// agree exactly in type, bucket size and range.

// For Histogram and LinearHistogram, the maximum for a declared range should
// always be larger (not equal) than minmal range. Zero and
// HistogramBase::kSampleType_MAX are implicitly added as first and last ranges,
// so the smallest legal bucket_count is 3. However CustomHistogram can have
// bucket count as 2 (when you give a custom ranges vector containing only 1
// range).
// For these 3 kinds of histograms, the max bucket count is always
// (Histogram::kBucketCount_MAX - 1).

// The buckets layout of class Histogram is exponential. For example, buckets
// might contain (sequentially) the count of values in the following intervals:
// [0,1), [1,2), [2,4), [4,8), [8,16), [16,32), [32,64), [64,infinity)
// That bucket allocation would actually result from construction of a histogram
// for values between 1 and 64, with 8 buckets, such as:
// Histogram count("some name", 1, 64, 8);
// Note that the underflow bucket [0,1) and the overflow bucket [64,infinity)
// are also counted by the constructor in the user supplied "bucket_count"
// argument.
// The above example has an exponential ratio of 2 (doubling the bucket width
// in each consecutive bucket.  The Histogram class automatically calculates
// the smallest ratio that it can use to construct the number of buckets
// selected in the constructor.  An another example, if you had 50 buckets,
// and millisecond time values from 1 to 10000, then the ratio between
// consecutive bucket widths will be approximately somewhere around the 50th
// root of 10000.  This approach provides very fine grain (narrow) buckets
// at the low end of the histogram scale, but allows the histogram to cover a
// gigantic range with the addition of very few buckets.

// Usually we use macros to define and use a histogram. These macros use a
// pattern involving a function static variable, that is a pointer to a
// histogram.  This static is explicitly initialized on any thread
// that detects a uninitialized (NULL) pointer.  The potentially racy
// initialization is not a problem as it is always set to point to the same
// value (i.e., the FactoryGet always returns the same value).  FactoryGet
// is also completely thread safe, which results in a completely thread safe,
// and relatively fast, set of counters.  To avoid races at shutdown, the static
// pointer is NOT deleted, and we leak the histograms at process termination.

#ifndef BASE_METRICS_HISTOGRAM_H_
#define BASE_METRICS_HISTOGRAM_H_

#include <map>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/time/time.h"

class Pickle;
class PickleIterator;

namespace base {

class Lock;
//------------------------------------------------------------------------------
// Histograms are often put in areas where they are called many many times, and
// performance is critical.  As a result, they are designed to have a very low
// recurring cost of executing (adding additional samples).  Toward that end,
// the macros declare a static pointer to the histogram in question, and only
// take a "slow path" to construct (or find) the histogram on the first run
// through the macro.  We leak the histograms at shutdown time so that we don't
// have to validate using the pointers at any time during the running of the
// process.

// The following code is generally what a thread-safe static pointer
// initializaion looks like for a histogram (after a macro is expanded).  This
// sample is an expansion (with comments) of the code for
// HISTOGRAM_CUSTOM_COUNTS().

/*
  do {
    // The pointer's presence indicates the initialization is complete.
    // Initialization is idempotent, so it can safely be atomically repeated.
    static base::subtle::AtomicWord atomic_histogram_pointer = 0;

    // Acquire_Load() ensures that we acquire visibility to the pointed-to data
    // in the histogrom.
    base::Histogram* histogram_pointer(reinterpret_cast<base::Histogram*>(
        base::subtle::Acquire_Load(&atomic_histogram_pointer)));

    if (!histogram_pointer) {
      // This is the slow path, which will construct OR find the matching
      // histogram.  FactoryGet includes locks on a global histogram name map
      // and is completely thread safe.
      histogram_pointer = base::Histogram::FactoryGet(
          name, min, max, bucket_count, base::HistogramBase::kNoFlags);

      // Use Release_Store to ensure that the histogram data is made available
      // globally before we make the pointer visible.
      // Several threads may perform this store, but the same value will be
      // stored in all cases (for a given named/spec'ed histogram).
      // We could do this without any barrier, since FactoryGet entered and
      // exited a lock after construction, but this barrier makes things clear.
      base::subtle::Release_Store(&atomic_histogram_pointer,
          reinterpret_cast<base::subtle::AtomicWord>(histogram_pointer));
    }

    // Ensure calling contract is upheld, and the name does NOT vary.
    DCHECK(histogram_pointer->histogram_name() == constant_histogram_name);

    histogram_pointer->Add(sample);
  } while (0);
*/

// The above pattern is repeated in several macros.  The only elements that
// vary are the invocation of the Add(sample) vs AddTime(sample), and the choice
// of which FactoryGet method to use.  The different FactoryGet methods have
// various argument lists, so the function with its argument list is provided as
// a macro argument here.  The name is only used in a DCHECK, to assure that
// callers don't try to vary the name of the histogram (which would tend to be
// ignored by the one-time initialization of the histogtram_pointer).
#define STATIC_HISTOGRAM_POINTER_BLOCK(constant_histogram_name, \
                                       histogram_add_method_invocation, \
                                       histogram_factory_get_invocation) \
  do { \
    static base::subtle::AtomicWord atomic_histogram_pointer = 0; \
    base::HistogramBase* histogram_pointer( \
        reinterpret_cast<base::HistogramBase*>( \
            base::subtle::Acquire_Load(&atomic_histogram_pointer))); \
    if (!histogram_pointer) { \
      histogram_pointer = histogram_factory_get_invocation; \
      base::subtle::Release_Store(&atomic_histogram_pointer, \
          reinterpret_cast<base::subtle::AtomicWord>(histogram_pointer)); \
    } \
    DCHECK_EQ(histogram_pointer->histogram_name(), \
              std::string(constant_histogram_name)); \
    histogram_pointer->histogram_add_method_invocation; \
  } while (0)


//------------------------------------------------------------------------------
// Provide easy general purpose histogram in a macro, just like stats counters.
// The first four macros use 50 buckets.

#define HISTOGRAM_TIMES(name, sample) HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromSeconds(10), 50)

// For folks that need real specific times, use this to select a precise range
// of times you want plotted, and the number of buckets you want used.
#define HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddTime(sample), \
        base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
                                        base::HistogramBase::kNoFlags))

#define HISTOGRAM_COUNTS(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define HISTOGRAM_COUNTS_100(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 100, 50)

#define HISTOGRAM_COUNTS_10000(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 10000, 50)

#define HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::Histogram::FactoryGet(name, min, max, bucket_count, \
                                    base::HistogramBase::kNoFlags))

#define HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

#define HISTOGRAM_BOOLEAN(name, sample) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddBoolean(sample), \
        base::BooleanHistogram::FactoryGet(name, base::Histogram::kNoFlags))

// Support histograming of an enumerated value.  The samples should always be
// strictly less than |boundary_value| -- this prevents you from running into
// problems down the line if you add additional buckets to the histogram.  Note
// also that, despite explicitly setting the minimum bucket value to |1| below,
// it is fine for enumerated histograms to be 0-indexed -- this is because
// enumerated histograms should never have underflow.
#define HISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
            boundary_value + 1, base::HistogramBase::kNoFlags))

// Support histograming of an enumerated value. Samples should be one of the
// std::vector<int> list provided via |custom_ranges|. See comments above
// CustomRanges::FactoryGet about the requirement of |custom_ranges|.
// You can use the helper function CustomHistogram::ArrayToCustomRanges to
// transform a C-style array of valid sample values to a std::vector<int>.
#define HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::CustomHistogram::FactoryGet(name, custom_ranges, \
                                          base::HistogramBase::kNoFlags))

#define HISTOGRAM_MEMORY_KB(name, sample) HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1000, 500000, 50)

//------------------------------------------------------------------------------
// Define Debug vs non-debug flavors of macros.
#ifndef NDEBUG

#define DHISTOGRAM_TIMES(name, sample) HISTOGRAM_TIMES(name, sample)
#define DHISTOGRAM_COUNTS(name, sample) HISTOGRAM_COUNTS(name, sample)
#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) HISTOGRAM_PERCENTAGE(\
    name, under_one_hundred)
#define DHISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count)
#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    HISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count)
#define DHISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count)
#define DHISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    HISTOGRAM_ENUMERATION(name, sample, boundary_value)
#define DHISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges)

#else  // NDEBUG
// Keep a mention of passed variables to avoid unused variable warnings in
// release build if these variables are only used in macros.
#define DISCARD_2_ARGUMENTS(a, b) \
  while (0) { \
    static_cast<void>(a); \
    static_cast<void>(b); \
 }
#define DISCARD_3_ARGUMENTS(a, b, c) \
  while (0) { \
    static_cast<void>(a); \
    static_cast<void>(b); \
    static_cast<void>(c); \
 }
#define DISCARD_5_ARGUMENTS(a, b, c, d ,e) \
  while (0) { \
    static_cast<void>(a); \
    static_cast<void>(b); \
    static_cast<void>(c); \
    static_cast<void>(d); \
    static_cast<void>(e); \
 }
#define DHISTOGRAM_TIMES(name, sample) \
    DISCARD_2_ARGUMENTS(name, sample)

#define DHISTOGRAM_COUNTS(name, sample) \
    DISCARD_2_ARGUMENTS(name, sample)

#define DHISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    DISCARD_2_ARGUMENTS(name, under_one_hundred)

#define DHISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    DISCARD_5_ARGUMENTS(name, sample, min, max, bucket_count)

#define DHISTOGRAM_CLIPPED_TIMES(name, sample, min, max, bucket_count) \
    DISCARD_5_ARGUMENTS(name, sample, min, max, bucket_count)

#define DHISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    DISCARD_5_ARGUMENTS(name, sample, min, max, bucket_count)

#define DHISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    DISCARD_3_ARGUMENTS(name, sample, boundary_value)

#define DHISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    DISCARD_3_ARGUMENTS(name, sample, custom_ranges)

#endif  // NDEBUG

//------------------------------------------------------------------------------
// The following macros provide typical usage scenarios for callers that wish
// to record histogram data, and have the data submitted/uploaded via UMA.
// Not all systems support such UMA, but if they do, the following macros
// should work with the service.

#define UMA_HISTOGRAM_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromSeconds(10), 50)

#define UMA_HISTOGRAM_MEDIUM_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(10), \
    base::TimeDelta::FromMinutes(3), 50)

// Use this macro when times can routinely be much longer than 10 seconds.
#define UMA_HISTOGRAM_LONG_TIMES(name, sample) UMA_HISTOGRAM_CUSTOM_TIMES( \
    name, sample, base::TimeDelta::FromMilliseconds(1), \
    base::TimeDelta::FromHours(1), 50)

#define UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddTime(sample), \
        base::Histogram::FactoryTimeGet(name, min, max, bucket_count, \
            base::HistogramBase::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_COUNTS(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000000, 50)

#define UMA_HISTOGRAM_COUNTS_100(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 100, 50)

#define UMA_HISTOGRAM_COUNTS_10000(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 10000, 50)

#define UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample, min, max, bucket_count) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::Histogram::FactoryGet(name, min, max, bucket_count, \
            base::HistogramBase::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_MEMORY_KB(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1000, 500000, 50)

#define UMA_HISTOGRAM_MEMORY_MB(name, sample) UMA_HISTOGRAM_CUSTOM_COUNTS( \
    name, sample, 1, 1000, 50)

#define UMA_HISTOGRAM_PERCENTAGE(name, under_one_hundred) \
    UMA_HISTOGRAM_ENUMERATION(name, under_one_hundred, 101)

#define UMA_HISTOGRAM_BOOLEAN(name, sample) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, AddBoolean(sample), \
        base::BooleanHistogram::FactoryGet(name, \
            base::HistogramBase::kUmaTargetedHistogramFlag))

// The samples should always be strictly less than |boundary_value|.  For more
// details, see the comment for the |HISTOGRAM_ENUMERATION| macro, above.
#define UMA_HISTOGRAM_ENUMERATION(name, sample, boundary_value) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::LinearHistogram::FactoryGet(name, 1, boundary_value, \
            boundary_value + 1, base::HistogramBase::kUmaTargetedHistogramFlag))

#define UMA_HISTOGRAM_CUSTOM_ENUMERATION(name, sample, custom_ranges) \
    STATIC_HISTOGRAM_POINTER_BLOCK(name, Add(sample), \
        base::CustomHistogram::FactoryGet(name, custom_ranges, \
            base::HistogramBase::kUmaTargetedHistogramFlag))

//------------------------------------------------------------------------------

class BucketRanges;
class SampleVector;

class BooleanHistogram;
class CustomHistogram;
class Histogram;
class LinearHistogram;

class BASE_EXPORT Histogram : public HistogramBase {
 public:
  // Initialize maximum number of buckets in histograms as 16,384.
  static const size_t kBucketCount_MAX;

  typedef std::vector<Count> Counts;

  //----------------------------------------------------------------------------
  // For a valid histogram, input should follow these restrictions:
  // minimum > 0 (if a minimum below 1 is specified, it will implicitly be
  //              normalized up to 1)
  // maximum > minimum
  // buckets > 2 [minimum buckets needed: underflow, overflow and the range]
  // Additionally,
  // buckets <= (maximum - minimum + 2) - this is to ensure that we don't have
  // more buckets than the range of numbers; having more buckets than 1 per
  // value in the range would be nonsensical.
  static HistogramBase* FactoryGet(const std::string& name,
                                   Sample minimum,
                                   Sample maximum,
                                   size_t bucket_count,
                                   int32 flags);
  static HistogramBase* FactoryTimeGet(const std::string& name,
                                       base::TimeDelta minimum,
                                       base::TimeDelta maximum,
                                       size_t bucket_count,
                                       int32 flags);

  // Time call for use with DHISTOGRAM*.
  // Returns TimeTicks::Now() in debug and TimeTicks() in release build.
  static TimeTicks DebugNow();

  static void InitializeBucketRanges(Sample minimum,
                                     Sample maximum,
                                     BucketRanges* ranges);

  // This constant if for FindCorruption. Since snapshots of histograms are
  // taken asynchronously relative to sampling, and our counting code currently
  // does not prevent race conditions, it is pretty likely that we'll catch a
  // redundant count that doesn't match the sample count.  We allow for a
  // certain amount of slop before flagging this as an inconsistency. Even with
  // an inconsistency, we'll snapshot it again (for UMA in about a half hour),
  // so we'll eventually get the data, if it was not the result of a corruption.
  static const int kCommonRaceBasedCountMismatch;

  // Check to see if bucket ranges, counts and tallies in the snapshot are
  // consistent with the bucket ranges and checksums in our histogram.  This can
  // produce a false-alarm if a race occurred in the reading of the data during
  // a SnapShot process, but should otherwise be false at all times (unless we
  // have memory over-writes, or DRAM failures).
  virtual int FindCorruption(const HistogramSamples& samples) const OVERRIDE;

  //----------------------------------------------------------------------------
  // Accessors for factory constuction, serialization and testing.
  //----------------------------------------------------------------------------
  Sample declared_min() const { return declared_min_; }
  Sample declared_max() const { return declared_max_; }
  virtual Sample ranges(size_t i) const;
  virtual size_t bucket_count() const;
  const BucketRanges* bucket_ranges() const { return bucket_ranges_; }

  // This function validates histogram construction arguments. It returns false
  // if some of the arguments are totally bad.
  // Note. Currently it allow some bad input, e.g. 0 as minimum, but silently
  // converts it to good input: 1.
  // TODO(kaiwang): Be more restrict and return false for any bad input, and
  // make this a readonly validating function.
  static bool InspectConstructionArguments(const std::string& name,
                                           Sample* minimum,
                                           Sample* maximum,
                                           size_t* bucket_count);

  // HistogramBase implementation:
  virtual HistogramType GetHistogramType() const OVERRIDE;
  virtual bool HasConstructionArguments(
      Sample expected_minimum,
      Sample expected_maximum,
      size_t expected_bucket_count) const OVERRIDE;
  virtual void Add(Sample value) OVERRIDE;
  virtual scoped_ptr<HistogramSamples> SnapshotSamples() const OVERRIDE;
  virtual void AddSamples(const HistogramSamples& samples) OVERRIDE;
  virtual bool AddSamplesFromPickle(PickleIterator* iter) OVERRIDE;
  virtual void WriteHTMLGraph(std::string* output) const OVERRIDE;
  virtual void WriteAscii(std::string* output) const OVERRIDE;

 protected:
  // |ranges| should contain the underflow and overflow buckets. See top
  // comments for example.
  Histogram(const std::string& name,
            Sample minimum,
            Sample maximum,
            const BucketRanges* ranges);

  virtual ~Histogram();

  // HistogramBase implementation:
  virtual bool SerializeInfoImpl(Pickle* pickle) const OVERRIDE;

  // Method to override to skip the display of the i'th bucket if it's empty.
  virtual bool PrintEmptyBucket(size_t index) const;

  // Get normalized size, relative to the ranges(i).
  virtual double GetBucketSize(Count current, size_t i) const;

  // Return a string description of what goes in a given bucket.
  // Most commonly this is the numeric value, but in derived classes it may
  // be a name (or string description) given to the bucket.
  virtual const std::string GetAsciiBucketRange(size_t it) const;

 private:
  // Allow tests to corrupt our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, BoundsTest);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, BucketPlacementTest);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptBucketBounds);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, CorruptSampleCounts);
  FRIEND_TEST_ALL_PREFIXES(HistogramTest, NameMatchTest);

  friend class StatisticsRecorder;  // To allow it to delete duplicates.
  friend class StatisticsRecorderTest;

  friend BASE_EXPORT_PRIVATE HistogramBase* DeserializeHistogramInfo(
      PickleIterator* iter);
  static HistogramBase* DeserializeInfoImpl(PickleIterator* iter);

  // Implementation of SnapshotSamples function.
  scoped_ptr<SampleVector> SnapshotSampleVector() const;

  //----------------------------------------------------------------------------
  // Helpers for emitting Ascii graphic.  Each method appends data to output.

  void WriteAsciiImpl(bool graph_it,
                      const std::string& newline,
                      std::string* output) const;

  // Find out how large (graphically) the largest bucket will appear to be.
  double GetPeakBucketSize(const SampleVector& samples) const;

  // Write a common header message describing this histogram.
  void WriteAsciiHeader(const SampleVector& samples,
                        Count sample_count,
                        std::string* output) const;

  // Write information about previous, current, and next buckets.
  // Information such as cumulative percentage, etc.
  void WriteAsciiBucketContext(const int64 past, const Count current,
                               const int64 remaining, const size_t i,
                               std::string* output) const;

  // WriteJSON calls these.
  virtual void GetParameters(DictionaryValue* params) const OVERRIDE;

  virtual void GetCountAndBucketData(Count* count,
                                     int64* sum,
                                     ListValue* buckets) const OVERRIDE;

  // Does not own this object. Should get from StatisticsRecorder.
  const BucketRanges* bucket_ranges_;

  Sample declared_min_;  // Less than this goes into the first bucket.
  Sample declared_max_;  // Over this goes into the last bucket.

  // Finally, provide the state that changes with the addition of each new
  // sample.
  scoped_ptr<SampleVector> samples_;

  DISALLOW_COPY_AND_ASSIGN(Histogram);
};

//------------------------------------------------------------------------------

// LinearHistogram is a more traditional histogram, with evenly spaced
// buckets.
class BASE_EXPORT LinearHistogram : public Histogram {
 public:
  virtual ~LinearHistogram();

  /* minimum should start from 1. 0 is as minimum is invalid. 0 is an implicit
     default underflow bucket. */
  static HistogramBase* FactoryGet(const std::string& name,
                                   Sample minimum,
                                   Sample maximum,
                                   size_t bucket_count,
                                   int32 flags);
  static HistogramBase* FactoryTimeGet(const std::string& name,
                                       TimeDelta minimum,
                                       TimeDelta maximum,
                                       size_t bucket_count,
                                       int32 flags);

  struct DescriptionPair {
    Sample sample;
    const char* description;  // Null means end of a list of pairs.
  };

  // Create a LinearHistogram and store a list of number/text values for use in
  // writing the histogram graph.
  // |descriptions| can be NULL, which means no special descriptions to set. If
  // it's not NULL, the last element in the array must has a NULL in its
  // "description" field.
  static HistogramBase* FactoryGetWithRangeDescription(
      const std::string& name,
      Sample minimum,
      Sample maximum,
      size_t bucket_count,
      int32 flags,
      const DescriptionPair descriptions[]);

  static void InitializeBucketRanges(Sample minimum,
                                     Sample maximum,
                                     BucketRanges* ranges);

  // Overridden from Histogram:
  virtual HistogramType GetHistogramType() const OVERRIDE;

 protected:
  LinearHistogram(const std::string& name,
                  Sample minimum,
                  Sample maximum,
                  const BucketRanges* ranges);

  virtual double GetBucketSize(Count current, size_t i) const OVERRIDE;

  // If we have a description for a bucket, then return that.  Otherwise
  // let parent class provide a (numeric) description.
  virtual const std::string GetAsciiBucketRange(size_t i) const OVERRIDE;

  // Skip printing of name for numeric range if we have a name (and if this is
  // an empty bucket).
  virtual bool PrintEmptyBucket(size_t index) const OVERRIDE;

 private:
  friend BASE_EXPORT_PRIVATE HistogramBase* DeserializeHistogramInfo(
      PickleIterator* iter);
  static HistogramBase* DeserializeInfoImpl(PickleIterator* iter);

  // For some ranges, we store a printable description of a bucket range.
  // If there is no desciption, then GetAsciiBucketRange() uses parent class
  // to provide a description.
  typedef std::map<Sample, std::string> BucketDescriptionMap;
  BucketDescriptionMap bucket_description_;

  DISALLOW_COPY_AND_ASSIGN(LinearHistogram);
};

//------------------------------------------------------------------------------

// BooleanHistogram is a histogram for booleans.
class BASE_EXPORT BooleanHistogram : public LinearHistogram {
 public:
  static HistogramBase* FactoryGet(const std::string& name, int32 flags);

  virtual HistogramType GetHistogramType() const OVERRIDE;

 private:
  BooleanHistogram(const std::string& name, const BucketRanges* ranges);

  friend BASE_EXPORT_PRIVATE HistogramBase* DeserializeHistogramInfo(
      PickleIterator* iter);
  static HistogramBase* DeserializeInfoImpl(PickleIterator* iter);

  DISALLOW_COPY_AND_ASSIGN(BooleanHistogram);
};

//------------------------------------------------------------------------------

// CustomHistogram is a histogram for a set of custom integers.
class BASE_EXPORT CustomHistogram : public Histogram {
 public:
  // |custom_ranges| contains a vector of limits on ranges. Each limit should be
  // > 0 and < kSampleType_MAX. (Currently 0 is still accepted for backward
  // compatibility). The limits can be unordered or contain duplication, but
  // client should not depend on this.
  static HistogramBase* FactoryGet(const std::string& name,
                                   const std::vector<Sample>& custom_ranges,
                                   int32 flags);

  // Overridden from Histogram:
  virtual HistogramType GetHistogramType() const OVERRIDE;

  // Helper method for transforming an array of valid enumeration values
  // to the std::vector<int> expected by HISTOGRAM_CUSTOM_ENUMERATION.
  // This function ensures that a guard bucket exists right after any
  // valid sample value (unless the next higher sample is also a valid value),
  // so that invalid samples never fall into the same bucket as valid samples.
  // TODO(kaiwang): Change name to ArrayToCustomEnumRanges.
  static std::vector<Sample> ArrayToCustomRanges(const Sample* values,
                                                 size_t num_values);
 protected:
  CustomHistogram(const std::string& name,
                  const BucketRanges* ranges);

  // HistogramBase implementation:
  virtual bool SerializeInfoImpl(Pickle* pickle) const OVERRIDE;

  virtual double GetBucketSize(Count current, size_t i) const OVERRIDE;

 private:
  friend BASE_EXPORT_PRIVATE HistogramBase* DeserializeHistogramInfo(
      PickleIterator* iter);
  static HistogramBase* DeserializeInfoImpl(PickleIterator* iter);

  static bool ValidateCustomRanges(const std::vector<Sample>& custom_ranges);
  static BucketRanges* CreateBucketRangesFromCustomRanges(
      const std::vector<Sample>& custom_ranges);

  DISALLOW_COPY_AND_ASSIGN(CustomHistogram);
};

}  // namespace base

#endif  // BASE_METRICS_HISTOGRAM_H_
