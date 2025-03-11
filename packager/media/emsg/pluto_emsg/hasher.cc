#include "packager/media/emsg/pluto_emsg/hasher.h"

#include <cstdint>
#include <iostream>
#include <list>
#include <vector>

using namespace std;

const int MULT = 37;
const int RESERVED_SPACE = 100000;

const uint32_t s[64] = {7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,
                        12, 17, 22, 5,  9,  14, 20, 5,  9,  14, 20, 5,  9,
                        14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16,
                        23, 4,  11, 16, 23, 4,  11, 16, 23, 6,  10, 15, 21,
                        6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21};

const uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
    0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
    0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
    0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
    0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
    0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

namespace shaka {
namespace media {
namespace hasher {

uint32_t leftRotate(uint32_t value, uint32_t shift) {
  return (value << shift) | (value >> (32 - shift));
}

uint32_t flipEndian(uint32_t in) {
  uint32_t reverse = 0;
  reverse |= (in & 0xFF000000) >> 24;
  reverse |= (in & 0xFF0000) >> 8;
  reverse |= (in & 0xFF00) << 8;
  reverse |= (in & 0xFF) << 24;
  return reverse;
}

void setByte32(uint32_t& input, uint8_t additive, int position) {
  switch (position) {
    case 3:
      input |= static_cast<uint32_t>(additive);
      break;
    case 2:
      input |= static_cast<uint32_t>(additive) << 8;
      break;
    case 1:
      input |= static_cast<uint32_t>(additive) << 16;
      break;
    case 0:
      input |= static_cast<uint32_t>(additive) << 24;
      break;

    default:
      break;
  }
}

uint8_t extractByte(uint32_t input, int byteIndex) {
  if (byteIndex > 3 || byteIndex < 0) {
    return 0;
  }
  return (input >> ((3 - byteIndex) * 8)) & 0xFF;
}

void addToByteList(list<uint8_t>& theList, uint32_t input) {
  for (int i = 0; i < 4; ++i) {
    theList.push_back(extractByte(input, i));
  }
}

template <typename... u32s>
void addToByteList(list<uint8_t>& theList, uint32_t input, u32s... inputs) {
  addToByteList(theList, input);
  addToByteList(theList, inputs...);
}

uint32_t combine32(list<uint8_t>& input) {
  uint32_t retval = 0;
  auto it = input.begin();
  for (int i = 0; it != input.end() && i < 4; ++i) {
    retval |= (*it << ((3 - i) * 8));
    ++it;
  }
  return retval;
}

void addFirstBytesToByteList(list<uint8_t>& theList, uint32_t input) {
  theList.push_back(extractByte(input, 0));
}

template <typename... u32s>
void addFirstBytesToByteList(list<uint8_t>& theList,
                             uint32_t input,
                             u32s... inputs) {
  addFirstBytesToByteList(theList, input);
  addFirstBytesToByteList(theList, inputs...);
}

void stringToByteList(list<uint8_t>& theList, string theString) {
  for (uint8_t c : theString) {
    theList.push_back(c);
  }
}

uint32_t hashTo32(list<uint8_t>& input) {
  uint32_t retval = 0;
  const uint32_t maxUint32 = 0xFFFFFFFF;

  auto it = input.begin();
  for (int i = 0; it != input.end(); ++i) {
    retval = MULT * retval + static_cast<uint32_t>(*it);
    ++it;
  }

  while (retval > maxUint32 - RESERVED_SPACE) {
    retval += RESERVED_SPACE;
  }
  return retval;
}

uint32_t Hasher32(string input) {
  list<uint8_t> M;
  stringToByteList(M, input);
  int lenInput = M.size();
  uint64_t numInputBits = lenInput * 8;
  list<uint8_t> numInputBitCountBytes;
  numInputBitCountBytes.clear();
  for (int j = 7; j >= 0; j--) {
    numInputBitCountBytes.push_back(uint8_t((numInputBits >> (j * 8)) & 0xFF));
  }

  if (M.size() < 56) {
    M.push_back(0x80);
  }
  while (M.size() < 56) {
    M.push_back(0);
  }
  for (uint8_t b : numInputBitCountBytes) {
    M.push_back(b);
  }
  numInputBitCountBytes.clear();

  uint32_t a0 = 0x67452301;  // A
  uint32_t b0 = 0xefcdab89;  // B
  uint32_t c0 = 0x98badcfe;  // C
  uint32_t d0 = 0x10325476;  // D

  // Convert into 16 4 byte words (16 * 4 = 64)
  while (M.size() > 0) {
    vector<uint32_t> m16;
    m16.reserve(16);
    while (m16.size() < 16) {
      uint32_t word = 0;
      for (int bytes = 0; bytes < 4; ++bytes) {
        if (M.size() > 0) {
          setByte32(word, M.front(), bytes);
          M.pop_front();
        } else {
          setByte32(word, 0, bytes);
        }
      }
      m16.push_back(word);
    }

    uint32_t A = a0;
    uint32_t B = b0;
    uint32_t C = c0;
    uint32_t D = d0;

    for (int i = 0; i < 64; ++i) {
      uint32_t F = 0;
      uint32_t g = 0;
      if (i < 16) {
        F = D ^ (B & (C ^ D));
        g = i;
      } else if (i < 32) {
        F = C ^ (D & (B ^ C));
        g = ((5 * i) + 1) % 16;
      } else if (i < 48) {
        F = B ^ C ^ D;
        g = ((3 * i) + 5) % 16;
      } else if (i < 64) {
        F = C ^ (B | (~D));
        g = (7 * i) % 16;
      }
      F += A + K[i] + m16[g];
      A = D;
      D = C;
      C = B;
      B += leftRotate(F, s[i]);
    }

    a0 += A;
    b0 += B;
    c0 += C;
    d0 += D;
  }

  a0 = flipEndian(a0);
  b0 = flipEndian(b0);
  c0 = flipEndian(c0);
  d0 = flipEndian(d0);

  list<uint8_t> resultList;
  addToByteList(resultList, a0, b0, c0, d0);
  return hashTo32(resultList);
}
}  // namespace hasher
}  // namespace media
}  // namespace shaka