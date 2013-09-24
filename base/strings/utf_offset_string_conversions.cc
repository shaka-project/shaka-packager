// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_offset_string_conversions.h"

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversion_utils.h"

namespace base {

// Converts the given source Unicode character type to the given destination
// Unicode character type as a STL string. The given input buffer and size
// determine the source, and the given output STL string will be replaced by
// the result.
template<typename SrcChar, typename DestStdString>
bool ConvertUnicode(const SrcChar* src,
                    size_t src_len,
                    DestStdString* output,
                    std::vector<size_t>* offsets_for_adjustment) {
  if (offsets_for_adjustment) {
    std::for_each(offsets_for_adjustment->begin(),
                  offsets_for_adjustment->end(),
                  LimitOffset<DestStdString>(src_len));
  }

  // ICU requires 32-bit numbers.
  bool success = true;
  OffsetAdjuster offset_adjuster(offsets_for_adjustment);
  int32 src_len32 = static_cast<int32>(src_len);
  for (int32 i = 0; i < src_len32; i++) {
    uint32 code_point;
    size_t original_i = i;
    size_t chars_written = 0;
    if (ReadUnicodeCharacter(src, src_len32, &i, &code_point)) {
      chars_written = WriteUnicodeCharacter(code_point, output);
    } else {
      chars_written = WriteUnicodeCharacter(0xFFFD, output);
      success = false;
    }
    if (offsets_for_adjustment) {
      // NOTE: ReadUnicodeCharacter() adjusts |i| to point _at_ the last
      // character read, not after it (so that incrementing it in the loop
      // increment will place it at the right location), so we need to account
      // for that in determining the amount that was read.
      offset_adjuster.Add(OffsetAdjuster::Adjustment(original_i,
          i - original_i + 1, chars_written));
    }
  }
  return success;
}

bool UTF8ToUTF16AndAdjustOffset(const char* src,
                                size_t src_len,
                                string16* output,
                                size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  PrepareForUTF16Or32Output(src, src_len, output);
  bool ret = ConvertUnicode(src, src_len, output, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return ret;
}

bool UTF8ToUTF16AndAdjustOffsets(const char* src,
                                 size_t src_len,
                                 string16* output,
                                 std::vector<size_t>* offsets_for_adjustment) {
  PrepareForUTF16Or32Output(src, src_len, output);
  return ConvertUnicode(src, src_len, output, offsets_for_adjustment);
}

string16 UTF8ToUTF16AndAdjustOffset(const base::StringPiece& utf8,
                                        size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  string16 result;
  UTF8ToUTF16AndAdjustOffsets(utf8.data(), utf8.length(), &result,
                              &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

string16 UTF8ToUTF16AndAdjustOffsets(
    const base::StringPiece& utf8,
    std::vector<size_t>* offsets_for_adjustment) {
  string16 result;
  UTF8ToUTF16AndAdjustOffsets(utf8.data(), utf8.length(), &result,
                              offsets_for_adjustment);
  return result;
}

std::string UTF16ToUTF8AndAdjustOffset(
    const base::StringPiece16& utf16,
    size_t* offset_for_adjustment) {
  std::vector<size_t> offsets;
  if (offset_for_adjustment)
    offsets.push_back(*offset_for_adjustment);
  std::string result = UTF16ToUTF8AndAdjustOffsets(utf16, &offsets);
  if (offset_for_adjustment)
    *offset_for_adjustment = offsets[0];
  return result;
}

std::string UTF16ToUTF8AndAdjustOffsets(
    const base::StringPiece16& utf16,
    std::vector<size_t>* offsets_for_adjustment) {
  std::string result;
  PrepareForUTF8Output(utf16.data(), utf16.length(), &result);
  ConvertUnicode(utf16.data(), utf16.length(), &result, offsets_for_adjustment);
  return result;
}

OffsetAdjuster::Adjustment::Adjustment(size_t original_offset,
                                       size_t original_length,
                                       size_t output_length)
    : original_offset(original_offset),
      original_length(original_length),
      output_length(output_length) {
}

OffsetAdjuster::OffsetAdjuster(std::vector<size_t>* offsets_for_adjustment)
    : offsets_for_adjustment_(offsets_for_adjustment) {
}

OffsetAdjuster::~OffsetAdjuster() {
  if (!offsets_for_adjustment_ || adjustments_.empty())
    return;
  for (std::vector<size_t>::iterator i(offsets_for_adjustment_->begin());
       i != offsets_for_adjustment_->end(); ++i)
    AdjustOffset(i);
}

void OffsetAdjuster::Add(const Adjustment& adjustment) {
  adjustments_.push_back(adjustment);
}

void OffsetAdjuster::AdjustOffset(std::vector<size_t>::iterator offset) {
  if (*offset == string16::npos)
    return;
  size_t adjustment = 0;
  for (std::vector<Adjustment>::const_iterator i = adjustments_.begin();
       i != adjustments_.end(); ++i) {
    if (*offset == i->original_offset && i->output_length == 0) {
      *offset = string16::npos;
      return;
    }
    if (*offset <= i->original_offset)
      break;
    if (*offset < (i->original_offset + i->original_length)) {
      *offset = string16::npos;
      return;
    }
    adjustment += (i->original_length - i->output_length);
  }
  *offset -= adjustment;
}

}  // namespace base
