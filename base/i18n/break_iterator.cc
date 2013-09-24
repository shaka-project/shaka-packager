// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/break_iterator.h"

#include "base/logging.h"
#include "third_party/icu/source/common/unicode/ubrk.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/ustring.h"

namespace base {
namespace i18n {

const size_t npos = -1;

BreakIterator::BreakIterator(const string16& str, BreakType break_type)
    : iter_(NULL),
      string_(str),
      break_type_(break_type),
      prev_(npos),
      pos_(0) {
}

BreakIterator::~BreakIterator() {
  if (iter_)
    ubrk_close(static_cast<UBreakIterator*>(iter_));
}

bool BreakIterator::Init() {
  UErrorCode status = U_ZERO_ERROR;
  UBreakIteratorType break_type;
  switch (break_type_) {
    case BREAK_CHARACTER:
      break_type = UBRK_CHARACTER;
      break;
    case BREAK_WORD:
      break_type = UBRK_WORD;
      break;
    case BREAK_LINE:
    case BREAK_NEWLINE:
      break_type = UBRK_LINE;
      break;
    default:
      NOTREACHED() << "invalid break_type_";
      return false;
  }
  iter_ = ubrk_open(break_type, NULL,
                    string_.data(), static_cast<int32_t>(string_.size()),
                    &status);
  if (U_FAILURE(status)) {
    NOTREACHED() << "ubrk_open failed";
    return false;
  }
  // Move the iterator to the beginning of the string.
  ubrk_first(static_cast<UBreakIterator*>(iter_));
  return true;
}

bool BreakIterator::Advance() {
  int32_t pos;
  int32_t status;
  prev_ = pos_;
  switch (break_type_) {
    case BREAK_CHARACTER:
    case BREAK_WORD:
    case BREAK_LINE:
      pos = ubrk_next(static_cast<UBreakIterator*>(iter_));
      if (pos == UBRK_DONE) {
        pos_ = npos;
        return false;
      }
      pos_ = static_cast<size_t>(pos);
      return true;
    case BREAK_NEWLINE:
      do {
        pos = ubrk_next(static_cast<UBreakIterator*>(iter_));
        if (pos == UBRK_DONE)
          break;
        pos_ = static_cast<size_t>(pos);
        status = ubrk_getRuleStatus(static_cast<UBreakIterator*>(iter_));
      } while (status >= UBRK_LINE_SOFT && status < UBRK_LINE_SOFT_LIMIT);
      if (pos == UBRK_DONE && prev_ == pos_) {
        pos_ = npos;
        return false;
      }
      return true;
    default:
      NOTREACHED() << "invalid break_type_";
      return false;
  }
}

bool BreakIterator::IsWord() const {
  int32_t status = ubrk_getRuleStatus(static_cast<UBreakIterator*>(iter_));
  return (break_type_ == BREAK_WORD && status != UBRK_WORD_NONE);
}

bool BreakIterator::IsEndOfWord(size_t position) const {
  if (break_type_ != BREAK_WORD)
    return false;

  UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
  UBool boundary = ubrk_isBoundary(iter, static_cast<int32_t>(position));
  int32_t status = ubrk_getRuleStatus(iter);
  return (!!boundary && status != UBRK_WORD_NONE);
}

bool BreakIterator::IsStartOfWord(size_t position) const {
  if (break_type_ != BREAK_WORD)
    return false;

  UBreakIterator* iter = static_cast<UBreakIterator*>(iter_);
  UBool boundary = ubrk_isBoundary(iter, static_cast<int32_t>(position));
  ubrk_next(iter);
  int32_t next_status = ubrk_getRuleStatus(iter);
  return (!!boundary && next_status != UBRK_WORD_NONE);
}

string16 BreakIterator::GetString() const {
  DCHECK(prev_ != npos && pos_ != npos);
  return string_.substr(prev_, pos_ - prev_);
}

}  // namespace i18n
}  // namespace base
