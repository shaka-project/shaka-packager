// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAFE_NUMERICS_H_
#define BASE_SAFE_NUMERICS_H_

#include <limits>

#include "base/logging.h"

namespace base {
namespace internal {

template <bool SameSize, bool DestLarger,
          bool DestIsSigned, bool SourceIsSigned>
struct IsValidNumericCastImpl;

#define BASE_NUMERIC_CAST_CASE_SPECIALIZATION(A, B, C, D, Code) \
template <> struct IsValidNumericCastImpl<A, B, C, D> { \
  template <class Source, class DestBounds> static inline bool Test( \
      Source source, DestBounds min, DestBounds max) { \
    return Code; \
  } \
}

#define BASE_NUMERIC_CAST_CASE_SAME_SIZE(DestSigned, SourceSigned, Code) \
  BASE_NUMERIC_CAST_CASE_SPECIALIZATION( \
      true, true, DestSigned, SourceSigned, Code); \
  BASE_NUMERIC_CAST_CASE_SPECIALIZATION( \
      true, false, DestSigned, SourceSigned, Code)

#define BASE_NUMERIC_CAST_CASE_SOURCE_LARGER(DestSigned, SourceSigned, Code) \
  BASE_NUMERIC_CAST_CASE_SPECIALIZATION( \
      false, false, DestSigned, SourceSigned, Code); \

#define BASE_NUMERIC_CAST_CASE_DEST_LARGER(DestSigned, SourceSigned, Code) \
  BASE_NUMERIC_CAST_CASE_SPECIALIZATION( \
      false, true, DestSigned, SourceSigned, Code); \

// The three top level cases are:
// - Same size
// - Source larger
// - Dest larger
// And for each of those three cases, we handle the 4 different possibilities
// of signed and unsigned. This gives 12 cases to handle, which we enumerate
// below.
//
// The last argument in each of the macros is the actual comparison code. It
// has three arguments available, source (the value), and min/max which are
// the ranges of the destination.


// These are the cases where both types have the same size.

// Both signed.
BASE_NUMERIC_CAST_CASE_SAME_SIZE(true, true, true);
// Both unsigned.
BASE_NUMERIC_CAST_CASE_SAME_SIZE(false, false, true);
// Dest unsigned, Source signed.
BASE_NUMERIC_CAST_CASE_SAME_SIZE(false, true, source >= 0);
// Dest signed, Source unsigned.
// This cast is OK because Dest's max must be less than Source's.
BASE_NUMERIC_CAST_CASE_SAME_SIZE(true, false,
                                 source <= static_cast<Source>(max));


// These are the cases where Source is larger.

// Both unsigned.
BASE_NUMERIC_CAST_CASE_SOURCE_LARGER(false, false, source <= max);
// Both signed.
BASE_NUMERIC_CAST_CASE_SOURCE_LARGER(true, true,
                                     source >= min && source <= max);
// Dest is unsigned, Source is signed.
BASE_NUMERIC_CAST_CASE_SOURCE_LARGER(false, true,
                                     source >= 0 && source <= max);
// Dest is signed, Source is unsigned.
// This cast is OK because Dest's max must be less than Source's.
BASE_NUMERIC_CAST_CASE_SOURCE_LARGER(true, false,
                                     source <= static_cast<Source>(max));


// These are the cases where Dest is larger.

// Both unsigned.
BASE_NUMERIC_CAST_CASE_DEST_LARGER(false, false, true);
// Both signed.
BASE_NUMERIC_CAST_CASE_DEST_LARGER(true, true, true);
// Dest is unsigned, Source is signed.
BASE_NUMERIC_CAST_CASE_DEST_LARGER(false, true, source >= 0);
// Dest is signed, Source is unsigned.
BASE_NUMERIC_CAST_CASE_DEST_LARGER(true, false, true);

#undef BASE_NUMERIC_CAST_CASE_SPECIALIZATION
#undef BASE_NUMERIC_CAST_CASE_SAME_SIZE
#undef BASE_NUMERIC_CAST_CASE_SOURCE_LARGER
#undef BASE_NUMERIC_CAST_CASE_DEST_LARGER


// The main test for whether the conversion will under or overflow.
template <class Dest, class Source>
inline bool IsValidNumericCast(Source source) {
  typedef std::numeric_limits<Source> SourceLimits;
  typedef std::numeric_limits<Dest> DestLimits;
  COMPILE_ASSERT(SourceLimits::is_specialized, argument_must_be_numeric);
  COMPILE_ASSERT(SourceLimits::is_integer, argument_must_be_integral);
  COMPILE_ASSERT(DestLimits::is_specialized, result_must_be_numeric);
  COMPILE_ASSERT(DestLimits::is_integer, result_must_be_integral);

  return IsValidNumericCastImpl<
      sizeof(Dest) == sizeof(Source),
      (sizeof(Dest) > sizeof(Source)),
      DestLimits::is_signed,
      SourceLimits::is_signed>::Test(
          source,
          DestLimits::min(),
          DestLimits::max());
}

}  // namespace internal

// checked_numeric_cast<> is analogous to static_cast<> for numeric types,
// except that it CHECKs that the specified numeric conversion will not
// overflow or underflow. Floating point arguments are not currently allowed
// (this is COMPILE_ASSERTd), though this could be supported if necessary.
template <class Dest, class Source>
inline Dest checked_numeric_cast(Source source) {
  CHECK(internal::IsValidNumericCast<Dest>(source));
  return static_cast<Dest>(source);
}

}  // namespace base

#endif  // BASE_SAFE_NUMERICS_H_
