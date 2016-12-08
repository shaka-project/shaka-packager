// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_PLAYREADY_PSSH_DATA_H_
#define MEDIA_BASE_PLAYREADY_PSSH_DATA_H_

#include <string>
#include <vector>

namespace shaka {
namespace media {


class PlayReadyPsshData {
 public:
    PlayReadyPsshData();

    bool add_kid_hex(const std::string& key_id_hex);
    void set_la_url(const ::std::string& value);
    void set_lui_url(const ::std::string& value);
    void set_decrypto_setup(bool on_demand);
    void set_include_empty_license_store(bool include);
    
    void serialize_to_vector(::std::vector<uint8_t>& output) const;
    
 private:
    ::std::vector<::std::u16string> kids_;
    ::std::u16string la_url_;
    ::std::u16string lui_url_;
    bool on_demand_;
    bool include_empty_license_store_;
};
   
} //namespace media
} // namespace shaka

#endif //MEDIA_BASE_PLAYREADY_PSSH_DATA_H_
