// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_PLAYREADY_PSSH_DATA_H_
#define MEDIA_BASE_PLAYREADY_PSSH_DATA_H_

#include <string>
#include <vector>
#include <utility>

#include "key_source.h"

namespace shaka {
namespace media {

class PlayReadyPsshData {
 public:
    PlayReadyPsshData();
    
    //add_kid_hex requires the key_hex to calculate KID checksum
    bool add_key_info(const EncryptionKey& encryption_key);
    void set_la_url(const ::std::string& value);
    void set_lui_url(const ::std::string& value);
    void set_decryptor_setup(bool on_demand);
    void set_include_empty_license_store(bool include);
    
    void serialize_to_vector(::std::vector<uint8_t>& output) const;
    
 private:
    
    bool kid_check_sum(const ::std::vector<uint8_t>& KID,
                       const ::std::vector<uint8_t>& content_key,
                       ::std::string& check_sum) const;
    
    //::std::vector<::std::u16string> kids_;
    //First is the actual KID. Second is the checksum
    ::std::vector<::std::pair<::std::u16string, ::std::u16string>> kids_;
    
    ::std::u16string la_url_;
    ::std::u16string lui_url_;
    bool on_demand_;
    bool include_empty_license_store_;
};
   
} //namespace media
} // namespace shaka

#endif //MEDIA_BASE_PLAYREADY_PSSH_DATA_H_
