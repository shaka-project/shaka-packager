#include "packager/media/emsg/pluto_emsg/hasher.h"

#include <openssl/md5.h>
#include <cstdint>
#include <iostream>
#include <list>
#include <vector>

using namespace std;

const int MULT = 37;
const int RESERVED_SPACE = 100000;

namespace shaka {
namespace media {
namespace hasher {

uint32_t hashTo32(uint8_t* p_input, size_t length) {
  uint32_t retval = 0;
  const uint32_t maxUint32 = 0xFFFFFFFF;

  for (int i = 0; size_t(i) < length; ++i) {
    retval = MULT * retval + static_cast<uint32_t>((*p_input + i));
  }

  while (retval > maxUint32 - RESERVED_SPACE) {
    retval += RESERVED_SPACE;
  }
  return retval;
}

uint32_t Hasher32(string input) {
  const size_t lenInput = input.length();
  const uint8_t* pInput_bytes = reinterpret_cast<const uint8_t*>(input.c_str());
  uint8_t outArray[MD5_DIGEST_LENGTH] = {0};
  uint8_t* p_lMD5 = MD5(pInput_bytes, lenInput, outArray);
  return hashTo32(p_lMD5, MD5_DIGEST_LENGTH);
}
}  // namespace hasher
}  // namespace media
}  // namespace shaka