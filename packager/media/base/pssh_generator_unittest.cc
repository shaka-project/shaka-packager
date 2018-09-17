// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/common_pssh_generator.h"
#include "packager/media/base/playready_pssh_generator.h"
#include "packager/media/base/widevine_pssh_generator.h"
#include "packager/status_test_util.h"

using ::testing::ElementsAreArray;

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

const char kExpectedPlayReadyPssh[] = {
    '\x0',  '\x0',  '\x2',  '\x3A', 'p',    's',    's',    'h',    '\x1',
    '\x0',  '\x0',  '\x0',  '\x9A', '\x04', '\xf0', '\x79', '\x98', '\x40',
    '\x42', '\x86', '\xab', '\x92', '\xe6', '\x5b', '\xe0', '\x88', '\x5f',
    '\x95', '\x0',  '\x0',  '\x0',  '\x1',  'k',    'e',    'y',    'i',
    'd',    '1',    '~',    '~',    '~',    '~',    '~',    '~',    '~',
    '~',    '~',    '~',    '\x0',  '\x0',  '\x2',  '\x6',  '\x6',  '\x2',
    '\x0',  '\x0',  '\x1',  '\x0',  '\x1',  '\x0',  '\xfc', '\x1',  '<',
    '\x0',  'W',    '\x0',  'R',    '\x0',  'M',    '\x0',  'H',    '\x0',
    'E',    '\x0',  'A',    '\x0',  'D',    '\x0',  'E',    '\x0',  'R',
    '\x0',  ' ',    '\x0',  'x',    '\x0',  'm',    '\x0',  'l',    '\x0',
    'n',    '\x0',  's',    '\x0',  '=',    '\x0',  '"',    '\x0',  'h',
    '\x0',  't',    '\x0',  't',    '\x0',  'p',    '\x0',  ':',    '\x0',
    '/',    '\x0',  '/',    '\x0',  's',    '\x0',  'c',    '\x0',  'h',
    '\x0',  'e',    '\x0',  'm',    '\x0',  'a',    '\x0',  's',    '\x0',
    '.',    '\x0',  'm',    '\x0',  'i',    '\x0',  'c',    '\x0',  'r',
    '\x0',  'o',    '\x0',  's',    '\x0',  'o',    '\x0',  'f',    '\x0',
    't',    '\x0',  '.',    '\x0',  'c',    '\x0',  'o',    '\x0',  'm',
    '\x0',  '/',    '\x0',  'D',    '\x0',  'R',    '\x0',  'M',    '\x0',
    '/',    '\x0',  '2',    '\x0',  '0',    '\x0',  '0',    '\x0',  '7',
    '\x0',  '/',    '\x0',  '0',    '\x0',  '3',    '\x0',  '/',    '\x0',
    'P',    '\x0',  'l',    '\x0',  'a',    '\x0',  'y',    '\x0',  'R',
    '\x0',  'e',    '\x0',  'a',    '\x0',  'd',    '\x0',  'y',    '\x0',
    'H',    '\x0',  'e',    '\x0',  'a',    '\x0',  'd',    '\x0',  'e',
    '\x0',  'r',    '\x0',  '"',    '\x0',  ' ',    '\x0',  'v',    '\x0',
    'e',    '\x0',  'r',    '\x0',  's',    '\x0',  'i',    '\x0',  'o',
    '\x0',  'n',    '\x0',  '=',    '\x0',  '"',    '\x0',  '4',    '\x0',
    '.',    '\x0',  '0',    '\x0',  '.',    '\x0',  '0',    '\x0',  '.',
    '\x0',  '0',    '\x0',  '"',    '\x0',  '>',    '\x0',  '<',    '\x0',
    'D',    '\x0',  'A',    '\x0',  'T',    '\x0',  'A',    '\x0',  '>',
    '\x0',  '<',    '\x0',  'P',    '\x0',  'R',    '\x0',  'O',    '\x0',
    'T',    '\x0',  'E',    '\x0',  'C',    '\x0',  'T',    '\x0',  'I',
    '\x0',  'N',    '\x0',  'F',    '\x0',  'O',    '\x0',  '>',    '\x0',
    '<',    '\x0',  'K',    '\x0',  'E',    '\x0',  'Y',    '\x0',  'L',
    '\x0',  'E',    '\x0',  'N',    '\x0',  '>',    '\x0',  '1',    '\x0',
    '6',    '\x0',  '<',    '\x0',  '/',    '\x0',  'K',    '\x0',  'E',
    '\x0',  'Y',    '\x0',  'L',    '\x0',  'E',    '\x0',  'N',    '\x0',
    '>',    '\x0',  '<',    '\x0',  'A',    '\x0',  'L',    '\x0',  'G',
    '\x0',  'I',    '\x0',  'D',    '\x0',  '>',    '\x0',  'A',    '\x0',
    'E',    '\x0',  'S',    '\x0',  'C',    '\x0',  'T',    '\x0',  'R',
    '\x0',  '<',    '\x0',  '/',    '\x0',  'A',    '\x0',  'L',    '\x0',
    'G',    '\x0',  'I',    '\x0',  'D',    '\x0',  '>',    '\x0',  '<',
    '\x0',  '/',    '\x0',  'P',    '\x0',  'R',    '\x0',  'O',    '\x0',
    'T',    '\x0',  'E',    '\x0',  'C',    '\x0',  'T',    '\x0',  'I',
    '\x0',  'N',    '\x0',  'F',    '\x0',  'O',    '\x0',  '>',    '\x0',
    '<',    '\x0',  'K',    '\x0',  'I',    '\x0',  'D',    '\x0',  '>',
    '\x0',  'a',    '\x0',  'X',    '\x0',  'l',    '\x0',  'l',    '\x0',
    'a',    '\x0',  'z',    '\x0',  'F',    '\x0',  'k',    '\x0',  'f',
    '\x0',  'n',    '\x0',  '5',    '\x0',  '+',    '\x0',  'f',    '\x0',
    'n',    '\x0',  '5',    '\x0',  '+',    '\x0',  'f',    '\x0',  'n',
    '\x0',  '5',    '\x0',  '+',    '\x0',  'f',    '\x0',  'g',    '\x0',
    '=',    '\x0',  '=',    '\x0',  '<',    '\x0',  '/',    '\x0',  'K',
    '\x0',  'I',    '\x0',  'D',    '\x0',  '>',    '\x0',  '<',    '\x0',
    'C',    '\x0',  'H',    '\x0',  'E',    '\x0',  'C',    '\x0',  'K',
    '\x0',  'S',    '\x0',  'U',    '\x0',  'M',    '\x0',  '>',    '\x0',
    'u',    '\x0',  'F',    '\x0',  'Y',    '\x0',  '/',    '\x0',  'O',
    '\x0',  'i',    '\x0',  'r',    '\x0',  'Q',    '\x0',  'j',    '\x0',
    '/',    '\x0',  'U',    '\x0',  '=',    '\x0',  '<',    '\x0',  '/',
    '\x0',  'C',    '\x0',  'H',    '\x0',  'E',    '\x0',  'C',    '\x0',
    'K',    '\x0',  'S',    '\x0',  'U',    '\x0',  'M',    '\x0',  '>',
    '\x0',  '<',    '\x0',  '/',    '\x0',  'D',    '\x0',  'A',    '\x0',
    'T',    '\x0',  'A',    '\x0',  '>',    '\x0',  '<',    '\x0',  '/',
    '\x0',  'W',    '\x0',  'R',    '\x0',  'M',    '\x0',  'H',    '\x0',
    'E',    '\x0',  'A',    '\x0',  'D',    '\x0',  'E',    '\x0',  'R',
    '\x0',  '>',    '\x0',
};

const char kExpectedCommonPssh[] = {
    '\x0',  '\x0',  '\x0',  '\x44', 'p',    's',    's',    'h',    '\x1',
    '\x0',  '\x0',  '\x0',  '\x10', '\x77', '\xEF', '\xEC', '\xC0', '\xB2',
    '\x4D', '\x02', '\xAC', '\xE3', '\x3C', '\x1E', '\x52', '\xE2', '\xFB',
    '\x4B', '\x0',  '\x0',  '\x0',  '\x2',  'k',    'e',    'y',    'i',
    'd',    '1',    '~',    '~',    '~',    '~',    '~',    '~',    '~',
    '~',    '~',    '~',    'k',    'e',    'y',    'i',    'd',    '2',
    '~',    '~',    '~',    '~',    '~',    '~',    '~',    '~',    '~',
    '~',    '\x0',  '\x0',  '\x0',  '\x0',

};

const char kExpectedWidevinePssh[] = {
    '\x0',  '\x0',  '\x0',  '\x44', 'p',    's',    's',    'h',    '\x0',
    '\x0',  '\x0',  '\x0',  '\xED', '\xEF', '\x8B', '\xA9', '\x79', '\xD6',
    '\x4A', '\xCE', '\xA3', '\xC8', '\x27', '\xDC', '\xD5', '\x1D', '\x21',
    '\xED', '\x0',  '\x0',  '\x0',  '\x24', '\x12', '\x10', 'k',    'e',
    'y',    'i',    'd',    '1',    '~',    '~',    '~',    '~',    '~',
    '~',    '~',    '~',    '~',    '~',    '\x12', '\x10', 'k',    'e',
    'y',    'i',    'd',    '2',    '~',    '~',    '~',    '~',    '~',
    '~',    '~',    '~',    '~',    '~',
};

const char kExpectedWidevinePsshCbcs[] = {
    '\x0',  '\x0',  '\x0',  '\x4A', 'p',    's',    's',    'h',    '\x0',
    '\x0',  '\x0',  '\x0',  '\xED', '\xEF', '\x8B', '\xA9', '\x79', '\xD6',
    '\x4A', '\xCE', '\xA3', '\xC8', '\x27', '\xDC', '\xD5', '\x1D', '\x21',
    '\xED', '\x0',  '\x0',  '\x0',  '\x2A', '\x12', '\x10', 'k',    'e',
    'y',    'i',    'd',    '1',    '~',    '~',    '~',    '~',    '~',
    '~',    '~',    '~',    '~',    '~',    '\x12', '\x10', 'k',    'e',
    'y',    'i',    'd',    '2',    '~',    '~',    '~',    '~',    '~',
    '~',    '~',    '~',    '~',    '~',    '\x48', '\xF3', '\xC6', '\x89',
    '\x9B', '\x06',
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
}  // namespace

// TODO(hmchen): move each PsshGenerateTest for each specific key system
// to each individual files (e.g., playready_pssh_generate_unittest.cc).
TEST(PsshGeneratorTest, GeneratePlayReadyPsshFromKeyIds) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<PlayReadyPsshGenerator> playready_pssh_generator(
      new PlayReadyPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_NOT_OK(
      playready_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));
}

TEST(PsshGeneratorTest, GeneratePlayReadyPsshFromKeyIdAndKey) {
  const std::vector<uint8_t> kTestKeyId = GetTestKeyId1();
  const std::vector<uint8_t> kTestKey = GetTestKey1();
  std::unique_ptr<PlayReadyPsshGenerator> playready_pssh_generator(
      new PlayReadyPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_OK(playready_pssh_generator->GeneratePsshFromKeyIdAndKey(
      kTestKeyId, kTestKey, &info));

  EXPECT_THAT(info.psshs, ElementsAreArray(std::begin(kExpectedPlayReadyPssh),
                                           std::end(kExpectedPlayReadyPssh)));
}

TEST(PsshGeneratorTest, GenerateCommonPsshFromKeyIds) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<CommonPsshGenerator> common_pssh_generator(
      new CommonPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_OK(common_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));
  EXPECT_THAT(info.psshs, ElementsAreArray(std::begin(kExpectedCommonPssh),
                                           std::end(kExpectedCommonPssh)));
}

TEST(PsshGeneratorTest, GenerateCommonPsshFromKeyIdAndKey) {
  const std::vector<uint8_t> kTestKeyId = GetTestKeyId1();
  const std::vector<uint8_t> kTestKey = GetTestKey1();
  std::unique_ptr<CommonPsshGenerator> common_pssh_generator(
      new CommonPsshGenerator());
  ProtectionSystemSpecificInfo info;
  EXPECT_NOT_OK(common_pssh_generator->GeneratePsshFromKeyIdAndKey(
      kTestKeyId, kTestKey, &info));
}

TEST(PsshGeneratorTest, GenerateWidevinePsshFromKeyIds) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<WidevinePsshGenerator> widevine_pssh_generator(
      new WidevinePsshGenerator(FOURCC_NULL));
  ProtectionSystemSpecificInfo info;
  ASSERT_OK(
      widevine_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));

  EXPECT_THAT(info.psshs, ElementsAreArray(std::begin(kExpectedWidevinePssh),
                                           std::end(kExpectedWidevinePssh)));
}

TEST(PsshGeneratorTest, GenerateWidevinePsshFromKeyIdsWithProtectionScheme) {
  const std::vector<std::vector<uint8_t>> kTestKeyIds = {GetTestKeyId1(),
                                                         GetTestKeyId2()};
  std::unique_ptr<WidevinePsshGenerator> widevine_pssh_generator(
      new WidevinePsshGenerator(FOURCC_cbcs));
  ProtectionSystemSpecificInfo info;
  ASSERT_OK(
      widevine_pssh_generator->GeneratePsshFromKeyIds(kTestKeyIds, &info));

  EXPECT_THAT(info.psshs,
              ElementsAreArray(std::begin(kExpectedWidevinePsshCbcs),
                               std::end(kExpectedWidevinePsshCbcs)));
}

TEST(PsshGeneratorTest, GenerateWidevinyPsshFromKeyIdAndKey) {
  const std::vector<uint8_t> kTestKeyId = GetTestKeyId1();
  const std::vector<uint8_t> kTestKey = GetTestKey1();
  std::unique_ptr<WidevinePsshGenerator> widevine_pssh_generator(
      new WidevinePsshGenerator(FOURCC_NULL));
  ProtectionSystemSpecificInfo info;
  EXPECT_NOT_OK(widevine_pssh_generator->GeneratePsshFromKeyIdAndKey(
      kTestKeyId, kTestKey, &info));
}

}  // namespace media
}  // namespace shaka
