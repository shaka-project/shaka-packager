// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <codecvt>
#include <openssl/aes.h>

#include "packager/media/base/playready_pssh_data.h"

#include "packager/base/logging.h"
#include "packager/base/base64.h"
#include "packager/base/strings/string_number_conversions.h"

namespace shaka {    
namespace media {

namespace {

#define USE_WRMHEADER_4_0

#if defined(USE_WRMHEADER_4_0)
    
const char16_t WRMHEADER_START_TAG[] =
    u"<WRMHEADER " 
     "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
     "version=\"4.0.0.0\">";
    const char16_t PROTECT_INFO_TAG[] =
        u"<PROTECTINFO><KEYLEN>16</KEYLEN><ALGID>AESCTR</ALGID></PROTECTINFO>";

    const char16_t KID_START_TAG[] =
        u"<KID>";

    const char16_t KID_END_TAG[] =
        u"</KID>";

    const char16_t CHECKSUM_START_TAG[] =
        u"<CHECKSUM>";
    
    const char16_t CHECKSUM_END_TAG[] =
        u"</CHECKSUM>";

    
#else

const char16_t WRMHEADER_START_TAG[] =
    u"<WRMHEADER " 
     "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\" "
     "version=\"4.0.0.0\">";

//const char16_t WRMHEADER_START_TAG[] =
//    u"<WRMHEADER version=\"4.2.0.0\" "
//     "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\">";

const char16_t PROTECT_INFO_KIDS_START_TAG[] =
    u"<PROTECTINFO><KIDS>";
const char16_t PROTECT_INFO_KIDS_END_TAG[] =
    u"</KIDS></PROTECTINFO>";

//PlayReady Header Object specification specifies that
//KID attribute should be value. However, pr porting kit
//uses capital attribute name VALUE.
const char16_t KID_START_TAG[] = u"<KID VALUE=\"";
const char16_t KID_END_TAG[] = u"\" ALGID=\"AESCTR\"></KID>";

const char16_t DECRYPTOR_SETUP_TAG[] = u"<DECRYPTORSETUP>ONDEMAND</DECRYPTORSETUP>";
#endif //if defined(USE_WRMHEADER_4_0)

const char16_t WRMHEADER_END_TAG[] =
    u"</WRMHEADER>";

const char16_t DATA_START_TAG[] = u"<DATA>";
const char16_t DATA_END_TAG[] = u"</DATA>";

const char16_t LA_URL_START_TAG[] = u"<LA_URL>";
const char16_t LA_URL_END_TAG[] = u"</LA_URL>";

const char16_t LUI_URL_START_TAG[] = u"<LUI_URL>";
const char16_t LUI_URL_END_TAG[] = u"</LUI_URL>";
    
    
const uint16_t PR_RIGHT_MGMT_RECORD_TYPE = 0x0001;
const uint16_t PR_EMBEDDED_LICENSE_STORE_RECORD_TYPE = 0x0003;
const uint16_t PR_EMBEDDED_LICENSE_STORE_SIZE = 10 * 1024;

const size_t PR_KID_CHECKSUM_LENGTH = 8;

} 

PlayReadyPsshData::PlayReadyPsshData()
    :on_demand_(false),
     include_empty_license_store_(false)
{
}

bool PlayReadyPsshData::add_key_info(const EncryptionKey& encryption_key)
{
    //In playready KID has to be in GUID format:
    //(DWORD, WORD, WORD, 8-BYTE array) in little endian.
    const size_t GUID_LENGTH = sizeof(uint32_t) + sizeof(uint16_t) +
        sizeof(uint16_t) + 8;
    
    if (encryption_key.key_id.size() != GUID_LENGTH) {
        LOG(ERROR) << "Invalid key id length " << encryption_key.key_id.size()
                   << ". Expecting " << GUID_LENGTH;
        return false;
    }

    const std::string kid(
        reinterpret_cast<const char*>(encryption_key.key_id.data()),
        encryption_key.key_id.size());
    std::string kidChecksum;
    std::string base64Kid;
    std::string base64KidChecksum;
    
    //base64 encode kid
    base::Base64Encode(kid, &base64Kid);
    
    //Calculate KID checksum
    if (!PlayReadyPsshData::kid_check_sum(encryption_key.key_id,
                                          encryption_key.key,
                                          kidChecksum)) {
        LOG(ERROR) << "Key id checksum calculation failed.";
        return false;

    }
    //base64 encode kid checksum
    base::Base64Encode(kidChecksum, &base64KidChecksum);
    
    //Convert to UTF16
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conversion;
    kids_.push_back(
        ::std::pair<::std::u16string, ::std::u16string>(
            conversion.from_bytes(base64Kid), conversion.from_bytes(base64KidChecksum)));
    
    return true;
}
    
void PlayReadyPsshData::set_la_url(const ::std::string& value)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conversion;
    la_url_ = conversion.from_bytes(value);
}

void PlayReadyPsshData::set_lui_url(const ::std::string& value)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conversion;
    lui_url_ = conversion.from_bytes(value);    
}

void PlayReadyPsshData::set_decryptor_setup(bool on_demand)
{
    on_demand_ = on_demand;
}

void PlayReadyPsshData::set_include_empty_license_store(bool include)
{
    include_empty_license_store_ = include;
}
    
void PlayReadyPsshData::serialize_to_vector(::std::vector<uint8_t>& output) const
{
    /* PSSH data format is specified in Microsoft Playready Header Object document.
       This implementation implements Rigths Management Header V4.2.0.0 */
    
    //Generate the XML content;
    ::std::u16string xmlContent = WRMHEADER_START_TAG;
    xmlContent.append(DATA_START_TAG);

#if defined(USE_WRMHEADER_4_0)
     xmlContent.append(PROTECT_INFO_TAG);

     if (kids_.size()) {
         //In WRMHEADER 4.0 there can be only one KID.
         xmlContent.append(KID_START_TAG);
         xmlContent.append(kids_[0].first);
         xmlContent.append(KID_END_TAG);
     }

     if (la_url_.length() > 0) {
         xmlContent.append(LA_URL_START_TAG);
         xmlContent.append(la_url_);
         xmlContent.append(LA_URL_END_TAG);
     }

     if (lui_url_.length() > 0) {
         xmlContent.append(LUI_URL_START_TAG);
         xmlContent.append(lui_url_);
         xmlContent.append(LUI_URL_END_TAG);
    }

     if (kids_.size()) {
         //In WRMHEADER 4.0 there can be only one KID.
         xmlContent.append(CHECKSUM_START_TAG);
         xmlContent.append(kids_[0].second);
         xmlContent.append(CHECKSUM_END_TAG);
     }

#else
    
    if (kids_.size()) {
        xmlContent.append(PROTECT_INFO_KIDS_START_TAG);

        ::std::vector<::std::pair<::std::u16string, ::std::u16string>>::
        const_iterator it;
        for (it = kids_.cbegin(); it != kids_.cend(); it++) {
            xmlContent.append(KID_START_TAG);
            xmlContent.append((*it).first);
            xmlContent.append(KID_END_TAG);
        }
        
        xmlContent.append(PROTECT_INFO_KIDS_END_TAG);
    }
    
    if (la_url_.length() > 0) {
        xmlContent.append(LA_URL_START_TAG);
        xmlContent.append(la_url_);
        xmlContent.append(LA_URL_END_TAG);
    }

    if (lui_url_.length() > 0) {
        xmlContent.append(LUI_URL_START_TAG);
        xmlContent.append(lui_url_);
        xmlContent.append(LUI_URL_END_TAG);
    }

    if (on_demand_) {
        xmlContent.append(DECRYPTOR_SETUP_TAG);
    }
    
#endif //if defined(USE_WRMHEADER_4_0)
    
    xmlContent.append(DATA_END_TAG); 
    xmlContent.append(WRMHEADER_END_TAG);
    output.clear();
    
    uint16_t xmlDataSize = static_cast<uint16_t>(xmlContent.size() * 2);
    //If we do not pack in empty license store we will have only 1 record.
    uint16_t PrRecordCount = (!include_empty_license_store_)? 1 : 2;

    //RightManager header length + Playready header object size.
    //PR Header object: Length (uint32_t), PR Record count (uint16_t),
    //PR Record: Record type (uint16_t), Record length (uint16_t),
    //           Record value.(the xml content)
    uint32_t prHeaderObjSize = xmlDataSize + 3 * sizeof(uint16_t) + sizeof(uint32_t);
   
    if (include_empty_license_store_) {
        //Add empty license store size to total length.
        //Empty license store size is 10KB. In addition
        //2*sizeof(uint16_t) is required for PlayReady Record header.
        prHeaderObjSize += 2 * sizeof(uint16_t) + PR_EMBEDDED_LICENSE_STORE_SIZE;
    }
    
    output.insert(output.end(),
                  reinterpret_cast<uint8_t*>(&prHeaderObjSize),
                  reinterpret_cast<uint8_t*>(&prHeaderObjSize) +
                  sizeof(prHeaderObjSize));
    output.insert(output.end(),
                  reinterpret_cast<uint8_t*>(&PrRecordCount),
                  reinterpret_cast<uint8_t*>(&PrRecordCount) +
                  sizeof(PrRecordCount));
    output.insert(output.end(),
                  reinterpret_cast<const uint8_t*>(&PR_RIGHT_MGMT_RECORD_TYPE ),
                  reinterpret_cast<const uint8_t*>(&PR_RIGHT_MGMT_RECORD_TYPE ) +
                  sizeof(PR_RIGHT_MGMT_RECORD_TYPE ));
    output.insert(output.end(),
                  reinterpret_cast<uint8_t*>(&xmlDataSize),
                  reinterpret_cast<uint8_t*>(&xmlDataSize) +
                  sizeof(xmlDataSize));
    output.insert(output.end(),
                  reinterpret_cast<const uint8_t*>(xmlContent.data()),
                  reinterpret_cast<const uint8_t*>(xmlContent.data()) +
                  (xmlDataSize));

    if (include_empty_license_store_) {
        output.insert(output.end(),
                      reinterpret_cast<const uint8_t*>(&PR_EMBEDDED_LICENSE_STORE_RECORD_TYPE),
                      reinterpret_cast<const uint8_t*>(&PR_EMBEDDED_LICENSE_STORE_RECORD_TYPE) +
                      sizeof(PR_EMBEDDED_LICENSE_STORE_RECORD_TYPE));
        output.insert(output.end(),
                      reinterpret_cast<const uint8_t*>(&PR_EMBEDDED_LICENSE_STORE_SIZE),
                      reinterpret_cast<const uint8_t*>(&PR_EMBEDDED_LICENSE_STORE_SIZE) +
                      sizeof(PR_EMBEDDED_LICENSE_STORE_SIZE));
        output.resize(output.size() + PR_EMBEDDED_LICENSE_STORE_SIZE, 0x00);
    }
}


bool PlayReadyPsshData::kid_check_sum(const ::std::vector<uint8_t>& KID,
                                      const ::std::vector<uint8_t>& content_key,
                                      ::std::string& check_sum) const
{
    AES_KEY aeskey;
    uint8_t cipher_text[AES_BLOCK_SIZE];
    
    if (KID.size() != AES_BLOCK_SIZE || content_key.size() != 16) {
        return false;
    }

    if (AES_set_encrypt_key(content_key.data(),
                            128, &aeskey) < 0) {
        return false;
    }
    
    AES_encrypt(KID.data(), cipher_text, &aeskey);

    memset(&aeskey, 0, sizeof(aeskey));

    check_sum.clear();
    check_sum.assign(reinterpret_cast<char*>(cipher_text),
                     PR_KID_CHECKSUM_LENGTH);
    
    return true;
}
    
}  // namespace media
}  // namespace shaka
