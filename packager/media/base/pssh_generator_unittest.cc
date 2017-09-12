// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/base/playready_pssh_generator.h"
#include "packager/media/base/raw_key_pssh_generator.h"
#include "packager/media/base/widevine_pssh_generator.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {
// Key ID and key should be 16 bytes.
const uint8_t kTestKeyId1[] = {'k', 'e', 'y', 'i', 'd', '1', '~', '~',
                               '~', '~', '~', '~', '~', '~', '~', '~'};
const uint8_t kTestKey1[] = {'c', 'o', 'n', 't', 'e', 'n', 't', 'k',
                             'e', 'y', '1', '~', '~', '~', '~', '~'};
const uint8_t kTestKeyId2[] = {'k', 'e', 'y', 'i', 'd', '2', '~', '~',
                               '~', '~', '~', '~', '~', '~', '~', '~'};

const char kExpectedPlayReadyPsshData[] = {
    '\x6', '\x2', '\x0', '\x0', '\x1', '\x0', '\x1', '\x0', '\xfc', '\x1',
    '<',   '\x0', 'W',   '\x0', 'R',   '\x0', 'M',   '\x0', 'H',    '\x0',
    'E',   '\x0', 'A',   '\x0', 'D',   '\x0', 'E',   '\x0', 'R',    '\x0',
    ' ',   '\x0', 'x',   '\x0', 'm',   '\x0', 'l',   '\x0', 'n',    '\x0',
    's',   '\x0', '=',   '\x0', '"',   '\x0', 'h',   '\x0', 't',    '\x0',
    't',   '\x0', 'p',   '\x0', ':',   '\x0', '/',   '\x0', '/',    '\x0',
    's',   '\x0', 'c',   '\x0', 'h',   '\x0', 'e',   '\x0', 'm',    '\x0',
    'a',   '\x0', 's',   '\x0', '.',   '\x0', 'm',   '\x0', 'i',    '\x0',
    'c',   '\x0', 'r',   '\x0', 'o',   '\x0', 's',   '\x0', 'o',    '\x0',
    'f',   '\x0', 't',   '\x0', '.',   '\x0', 'c',   '\x0', 'o',    '\x0',
    'm',   '\x0', '/',   '\x0', 'D',   '\x0', 'R',   '\x0', 'M',    '\x0',
    '/',   '\x0', '2',   '\x0', '0',   '\x0', '0',   '\x0', '7',    '\x0',
    '/',   '\x0', '0',   '\x0', '3',   '\x0', '/',   '\x0', 'P',    '\x0',
    'l',   '\x0', 'a',   '\x0', 'y',   '\x0', 'R',   '\x0', 'e',    '\x0',
    'a',   '\x0', 'd',   '\x0', 'y',   '\x0', 'H',   '\x0', 'e',    '\x0',
    'a',   '\x0', 'd',   '\x0', 'e',   '\x0', 'r',   '\x0', '"',    '\x0',
    ' ',   '\x0', 'v',   '\x0', 'e',   '\x0', 'r',   '\x0', 's',    '\x0',
    'i',   '\x0', 'o',   '\x0', 'n',   '\x0', '=',   '\x0', '"',    '\x0',
    '4',   '\x0', '.',   '\x0', '0',   '\x0', '.',   '\x0', '0',    '\x0',
    '.',   '\x0', '0',   '\x0', '"',   '\x0', '>',   '\x0', '<',    '\x0',
    'D',   '\x0', 'A',   '\x0', 'T',   '\x0', 'A',   '\x0', '>',    '\x0',
    '<',   '\x0', 'P',   '\x0', 'R',   '\x0', 'O',   '\x0', 'T',    '\x0',
    'E',   '\x0', 'C',   '\x0', 'T',   '\x0', 'I',   '\x0', 'N',    '\x0',
    'F',   '\x0', 'O',   '\x0', '>',   '\x0', '<',   '\x0', 'K',    '\x0',
    'E',   '\x0', 'Y',   '\x0', 'L',   '\x0', 'E',   '\x0', 'N',    '\x0',
    '>',   '\x0', '1',   '\x0', '6',   '\x0', '<',   '\x0', '/',    '\x0',
    'K',   '\x0', 'E',   '\x0', 'Y',   '\x0', 'L',   '\x0', 'E',    '\x0',
    'N',   '\x0', '>',   '\x0', '<',   '\x0', 'A',   '\x0', 'L',    '\x0',
    'G',   '\x0', 'I',   '\x0', 'D',   '\x0', '>',   '\x0', 'A',    '\x0',
    'E',   '\x0', 'S',   '\x0', 'C',   '\x0', 'T',   '\x0', 'R',    '\x0',
    '<',   '\x0', '/',   '\x0', 'A',   '\x0', 'L',   '\x0', 'G',    '\x0',
    'I',   '\x0', 'D',   '\x0', '>',   '\x0', '<',   '\x0', '/',    '\x0',
    'P',   '\x0', 'R',   '\x0', 'O',   '\x0', 'T',   '\x0', 'E',    '\x0',
    'C',   '\x0', 'T',   '\x0', 'I',   '\x0', 'N',   '\x0', 'F',    '\x0',
    'O',   '\x0', '>',   '\x0', '<',   '\x0', 'K',   '\x0', 'I',    '\x0',
    'D',   '\x0', '>',   '\x0', 'a',   '\x0', 'X',   '\x0', 'l',    '\x0',
    'l',   '\x0', 'a',   '\x0', 'z',   '\x0', 'F',   '\x0', 'k',    '\x0',
    'f',   '\x0', 'n',   '\x0', '5',   '\x0', '+',   '\x0', 'f',    '\x0',
    'n',   '\x0', '5',   '\x0', '+',   '\x0', 'f',   '\x0', 'n',    '\x0',
    '5',   '\x0', '+',   '\x0', 'f',   '\x0', 'g',   '\x0', '=',    '\x0',
    '=',   '\x0', '<',   '\x0', '/',   '\x0', 'K',   '\x0', 'I',    '\x0',
    'D',   '\x0', '>',   '\x0', '<',   '\x0', 'C',   '\x0', 'H',    '\x0',
    'E',   '\x0', 'C',   '\x0', 'K',   '\x0', 'S',   '\x0', 'U',    '\x0',
    'M',   '\x0', '>',   '\x0', 'u',   '\x0', 'F',   '\x0', 'Y',    '\x0',
    '/',   '\x0', 'O',   '\x0', 'i',   '\x0', 'r',   '\x0', 'Q',    '\x0',
    'j',   '\x0', '/',   '\x0', 'U',   '\x0', '=',   '\x0', '<',    '\x0',
    '/',   '\x0', 'C',   '\x0', 'H',   '\x0', 'E',   '\x0', 'C',    '\x0',
    'K',   '\x0', 'S',   '\x0', 'U',   '\x0', 'M',   '\x0', '>',    '\x0',
    '<',   '\x0', '/',   '\x0', 'D',   '\x0', 'A',   '\x0', 'T',    '\x0',
    'A',   '\x0', '>',   '\x0', '<',   '\x0', '/',   '\x0', 'W',    '\x0',
    'R',   '\x0', 'M',   '\x0', 'H',   '\x0', 'E',   '\x0', 'A',    '\x0',
    'D',   '\x0', 'E',   '\x0', 'R',   '\x0', '>',   '\x0',
};

const char kExpectedWidevinePsshData[] = {
    '\x12', '\x10', 'k', 'e', 'y', 'i', 'd',    '1',    '~', '~', '~', '~',
    '~',    '~',    '~', '~', '~', '~', '\x12', '\x10', 'k', 'e', 'y', 'i',
    'd',    '2',    '~', '~', '~', '~', '~',    '~',    '~', '~', '~', '~',
};

std::vector<uint8_t> GetTestKeyId1() {
  return std::vector<uint8_t>(std::begin(kTestKeyId1), std::end(kTestKeyId1));
}

std::vector<uint8_t> GetTestKey1() {
  return std::vector<uint8_t>(std::begin(kTestKey1), std::end(kTestKey1));
}

std::vector<uint8_t> GetTestKeyId2() {
  return std::vector<uint8_t>(std::begin(kTestKeyId2), std::end(kTestKeyId2));
}

std::vector<uint8_t> GetExpectedPlayReadyPsshData() {
  return std::vector<uint8_t>(std::begin(kExpectedPlayReadyPsshData),
                              std::end(kExpectedPlayReadyPsshData));
}

std::vector<uint8_t> GetExpectedWidevinePsshData() {
  return std::vector<uint8_t>(std::begin(kExpectedWidevinePsshData),
                              std::end(kExpectedWidevinePsshData));
}
}  // namespace

// Folowing tests test PlayReady, RawKey and Widevine PSSH generators. Note
// that for each generator, it can generate PSSH from a pair of key id and key
// or multiple key ids. Since some of generating methods are not implemented yet
// (or not needed), tests make sure those methods return failure.
class PsshGeneratorTest : public ::testing::Test {};

// TODO(hmchen): move each PsshGenerateTest for each specific key system
// to each individual files (e.g., playready_pssh_generate_unittest.cc).
TEST(PsshGeneratorTest, GeneratePlayReadyPsshDataFromKeyIds) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<PlayReadyPsshGenerator> playready_pssh_generator(
      new PlayReadyPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_NOT_OK(
      playready_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));
}

// TODO(hmchen): change the testing interface from
// GeneratePsshDataFromKeyIdAndKey to GeneratePsshFromKeyIdAndKey, after the
// later one is not used as a static function.
TEST(PsshGeneratorTest, GeneratePlayReadyPsshDataFromKeyIdAndKey) {
  const std::vector<uint8_t> kTestKeyId = GetTestKeyId1();
  const std::vector<uint8_t> kTestKey = GetTestKey1();
  std::unique_ptr<PlayReadyPsshGenerator> playready_pssh_generator(
      new PlayReadyPsshGenerator());
  base::Optional<std::vector<uint8_t>> pssh_data =
      playready_pssh_generator->GeneratePsshDataFromKeyIdAndKey(kTestKeyId,
                                                                kTestKey);
  ASSERT_TRUE(pssh_data);

  const std::vector<uint8_t> kExpectedPsshData = GetExpectedPlayReadyPsshData();
  EXPECT_EQ(kExpectedPsshData, pssh_data.value());
}

TEST(PsshGeneratorTest, GenerateRawKeyPsshDataFromKeyIds) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<RawKeyPsshGenerator> raw_key_pssh_generator(
      new RawKeyPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_OK(raw_key_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));
  // Intentionally empty pssh data for raw key.
  EXPECT_TRUE(info.pssh_data().empty());
}

TEST(PsshGeneratorTest, GenerateRawKeyPsshDataFromKeyIdAndKey) {
  const std::vector<uint8_t> kTestKeyId = GetTestKeyId1();
  const std::vector<uint8_t> kTestKey = GetTestKey1();
  std::unique_ptr<RawKeyPsshGenerator> raw_key_pssh_generator(
      new RawKeyPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_NOT_OK(raw_key_pssh_generator->GeneratePsshFromKeyIdAndKey(
      kTestKeyId, kTestKey, &info));
}

TEST(PsshGeneratorTest, GenerateWidevinePsshDataFromKeyIds) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<WidevinePsshGenerator> widevine_pssh_generator(
      new WidevinePsshGenerator());
  ProtectionSystemSpecificInfo info;
  ASSERT_OK(
      widevine_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));

  const std::vector<uint8_t> kExpectedPsshData = GetExpectedWidevinePsshData();
  EXPECT_EQ(kExpectedPsshData, info.pssh_data());
}

TEST(PsshGeneratorTest, GenerateWidevinyPsshDataFromKeyIdAndKey) {
  const std::vector<uint8_t> kTestKeyId = GetTestKeyId1();
  const std::vector<uint8_t> kTestKey = GetTestKey1();
  std::unique_ptr<WidevinePsshGenerator> widevine_pssh_generator(
      new WidevinePsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_NOT_OK(widevine_pssh_generator->GeneratePsshFromKeyIdAndKey(
      kTestKeyId, kTestKey, &info));
}

}  // namespace media
}  // namespace shaka
