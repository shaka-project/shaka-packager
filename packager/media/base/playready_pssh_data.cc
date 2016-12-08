// Copyright 2016 Inside Secure Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <codecvt>

#include "packager/media/base/playready_pssh_data.h"

#include "packager/base/logging.h"
#include "packager/base/base64.h"
#include "packager/base/strings/string_number_conversions.h"

namespace shaka {
namespace media {

namespace prPssh {
static const char16_t WRMHEADER_START_TAG[] =
    u"<WRMHEADER version=\"4.2.0.0\" "
     "xmlns=\"http://schemas.microsoft.com/DRM/2007/03/PlayReadyHeader\">";
static const char16_t WRMHEADER_END_TAG[] =
    u"</WRMHEADER>";

    static const char16_t DATA_START_TAG[] = u"<DATA>";
    static const char16_t DATA_END_TAG[] = u"</DATA>";

    static const char16_t PROTECT_INFO_KIDS_START_TAG[] =
        u"<PROTECTINFO><KIDS>";
    static const char16_t PROTECT_INFO_KIDS_END_TAG[] =
        u"</KIDS></PROTECTINFO>";

    //PlayReady Header Object specification specifies that
    //KID attribute should be value. However, pr porting kit
    //uses capital attribute name VALUE.
    static const char16_t KID_START_TAG[] = u"<KID VALUE=\"";
    static const char16_t KID_END_TAG[] = u"\" ALGID=\"AESCTR\" />";
        
    static const char16_t LA_URL_START_TAG[] = u"<LA_URL>";
    static const char16_t LA_URL_END_TAG[] = u"</LA_URL>";

    static const char16_t LUI_URL_START_TAG[] = u"<LUI_URL>";
    static const char16_t LUI_URL_END_TAG[] = u"</LUI_URL>";

    static const char16_t DECRYPTO_SETUP_TAG[] = u"<DECRYPTOSETUP>ONDEMAND</DECRYPTOSETUP>";
    
    static const uint16_t PR_RIGHT_MGMT_RECORD_TYPE = 0x0001;
    static const uint16_t PR_EMBEDDED_LICENSE_STORE_RECORD_TYPE = 0x0003;
    static const uint16_t PR_EMBEDDED_LICENSE_STORE_SIZE = 10 * 1024;
} //namespaece prPSSH

using namespace prPssh;

PlayReadyPsshData::PlayReadyPsshData()
    :on_demand_(false),
     include_empty_license_store_(false)
{
}

bool PlayReadyPsshData::add_kid_hex(const std::string& key_id_hex)
{
    //In playready KID has to be in GUID format:
    //(DWORD, WORD, WORD, 8-BYTE array) in little endian.
    const size_t GUID_LENGTH = sizeof(uint32_t) + sizeof(uint16_t) +
        sizeof(uint16_t) + 8;
    
    //Convert to binary vector.
    std::vector<uint8_t> kid;
    if( !base::HexStringToBytes(key_id_hex, &kid) )
    {
        LOG(ERROR) << "Unable to parse key id: " << key_id_hex;
        return false;
    }

    if (kid.size() != GUID_LENGTH)
    {
        LOG(ERROR) << "Invalid key id " << key_id_hex << ". Length " << key_id_hex.size()
                   << ". Expecting " << GUID_LENGTH;
        return false;
    }
   
    //Convert to MS GUID format
    std::string guid;
    //dword
    guid.push_back(kid[3]);
    guid.push_back(kid[2]);
    guid.push_back(kid[1]);
    guid.push_back(kid[0]);

    //first word
    guid.push_back(kid[5]);
    guid.push_back(kid[4]);

    //second word
    guid.push_back(kid[7]);
    guid.push_back(kid[6]);

    //rest is just byte data
    guid.insert(guid.end(), kid.begin() + 8, kid.end());
    
    //base64 encode
    base::Base64Encode(guid, &guid);

    //Convert to UTF16
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conversion;
    kids_.push_back(conversion.from_bytes(guid));
    
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

void PlayReadyPsshData::set_decrypto_setup(bool on_demand)
{
    on_demand_ = on_demand;
}

void PlayReadyPsshData::set_include_empty_license_store(bool include)
{
    include_empty_license_store_ = include;
}
    
void PlayReadyPsshData::SerializeToVector(::std::vector<uint8_t>& output) const
{
    /* PSSH data format is specified in Microsoft Playready Header Object document.
       This implementation implements Rigths Management Header V4.2.0.0 */
    
    //Generate the XML content;
    ::std::u16string xmlContent = WRMHEADER_START_TAG;
    xmlContent.append(DATA_START_TAG);

    if (kids_.size()) {
        xmlContent.append(PROTECT_INFO_KIDS_START_TAG);

        ::std::vector<::std::u16string>::const_iterator it;
        for (it = kids_.begin(); it != kids_.end(); it++) {
            xmlContent.append(KID_START_TAG);
            xmlContent.append(*it);
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
        xmlContent.append(DECRYPTO_SETUP_TAG);
    }
    
    xmlContent.append(DATA_END_TAG);
    xmlContent.append(WRMHEADER_END_TAG);

    output.clear();
    //PR Header object: Length (uint32_t), PR Record count (uint16_t),
    //PR Record: Record type (uint16_t), Record length uitn16_t,
    //           Record value.(the xml content)
    uint16_t xmlDataSize = static_cast<uint16_t>(xmlContent.size() * 2);
    uint16_t PrRecordCount = (!include_empty_license_store_)? 1 : 2;

    //RightManager header length + Playready header object size.
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


}  // namespace media
}  // namespace shaka
