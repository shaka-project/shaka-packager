// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_BASE_FOURCCS_H_
#define PACKAGER_MEDIA_BASE_FOURCCS_H_

#include <string>

namespace shaka {
namespace media {

enum FourCC : uint32_t {
  FOURCC_NULL = 0,

  FOURCC_ID32 = 0x49443332,
  FOURCC_Head = 0x48656164,
  FOURCC_Opus = 0x4f707573,
  FOURCC_PRIV = 0x50524956,

  FOURCC_aacd = 0x61616364,
  FOURCC_ac_3 = 0x61632d33,  // "ac-3"
  FOURCC_ac3d = 0x61633364,
  FOURCC_apad = 0x61706164,
  FOURCC_av01 = 0x61763031,
  FOURCC_av1C = 0x61763143,
  FOURCC_avc1 = 0x61766331,
  FOURCC_avc3 = 0x61766333,
  FOURCC_avcC = 0x61766343,
  FOURCC_bloc = 0x626C6F63,
  FOURCC_cbc1 = 0x63626331,
  // This is a fake protection scheme fourcc code to indicate Apple Sample AES.
  FOURCC_cbca = 0x63626361,
  FOURCC_cbcs = 0x63626373,
  FOURCC_cenc = 0x63656e63,
  FOURCC_cens = 0x63656e73,
  FOURCC_co64 = 0x636f3634,
  FOURCC_cmfc = 0x636d6663,
  FOURCC_cmfs = 0x636d6673,
  FOURCC_ctim = 0x6374696d,
  FOURCC_ctts = 0x63747473,
  FOURCC_dOps = 0x644f7073,
  FOURCC_dac3 = 0x64616333,
  FOURCC_dash = 0x64617368,
  FOURCC_ddts = 0x64647473,
  FOURCC_dec3 = 0x64656333,
  FOURCC_dfLa = 0x64664c61,
  FOURCC_dinf = 0x64696e66,
  FOURCC_dref = 0x64726566,
  FOURCC_dtsc = 0x64747363,
  FOURCC_dtse = 0x64747365,
  FOURCC_dtsh = 0x64747368,
  FOURCC_dtsl = 0x6474736c,
  FOURCC_dtsm = 0x6474732d,  // "dts-"
  FOURCC_dtsp = 0x6474732b,  // "dts+"
  FOURCC_ec_3 = 0x65632d33,  // "ec-3"
  FOURCC_ec3d = 0x65633364,
  FOURCC_edts = 0x65647473,
  FOURCC_elst = 0x656c7374,
  FOURCC_enca = 0x656e6361,
  FOURCC_encv = 0x656e6376,
  FOURCC_esds = 0x65736473,
  FOURCC_fLaC = 0x664c6143,
  FOURCC_free = 0x66726565,
  FOURCC_frma = 0x66726d61,
  FOURCC_ftyp = 0x66747970,
  FOURCC_hdlr = 0x68646c72,
  FOURCC_hev1 = 0x68657631,
  FOURCC_hint = 0x68696e74,
  FOURCC_hvc1 = 0x68766331,
  FOURCC_hvcC = 0x68766343,
  FOURCC_iden = 0x6964656e,
  FOURCC_iso6 = 0x69736f36,
  FOURCC_iso8 = 0x69736f38,
  FOURCC_isom = 0x69736f6d,
  FOURCC_iods = 0x696f6473,
  FOURCC_mdat = 0x6d646174,
  FOURCC_mdhd = 0x6d646864,
  FOURCC_mdia = 0x6d646961,
  FOURCC_meco = 0x6d65636f,
  FOURCC_mehd = 0x6d656864,
  FOURCC_meta = 0x6d657461,
  FOURCC_mfhd = 0x6d666864,
  FOURCC_mfra = 0x6d667261,
  FOURCC_minf = 0x6d696e66,
  FOURCC_moof = 0x6d6f6f66,
  FOURCC_moov = 0x6d6f6f76,
  FOURCC_mp41 = 0x6d703431,
  FOURCC_mp4a = 0x6d703461,
  FOURCC_mp4v = 0x6d703476,
  FOURCC_mvex = 0x6d766578,
  FOURCC_mvhd = 0x6d766864,
  FOURCC_pasp = 0x70617370,
  FOURCC_payl = 0x7061796c,
  FOURCC_pdin = 0x7064696e,
  FOURCC_prft = 0x70726674,
  FOURCC_pssh = 0x70737368,
  FOURCC_roll = 0x726f6c6c,
  FOURCC_saio = 0x7361696f,
  FOURCC_saiz = 0x7361697a,
  FOURCC_sbgp = 0x73626770,
  FOURCC_schi = 0x73636869,
  FOURCC_schm = 0x7363686d,
  FOURCC_sdtp = 0x73647470,
  FOURCC_seig = 0x73656967,
  FOURCC_senc = 0x73656e63,
  FOURCC_sgpd = 0x73677064,
  FOURCC_sidx = 0x73696478,
  FOURCC_sinf = 0x73696e66,
  FOURCC_skip = 0x736b6970,
  FOURCC_smhd = 0x736d6864,
  FOURCC_soun = 0x736f756e,
  FOURCC_ssix = 0x73736978,
  FOURCC_stbl = 0x7374626c,
  FOURCC_stco = 0x7374636f,
  FOURCC_sthd = 0x73746864,
  FOURCC_stsc = 0x73747363,
  FOURCC_stsd = 0x73747364,
  FOURCC_stss = 0x73747373,
  FOURCC_stsz = 0x7374737a,
  FOURCC_sttg = 0x73747467,
  FOURCC_stts = 0x73747473,
  FOURCC_styp = 0x73747970,
  FOURCC_stz2 = 0x73747a32,
  FOURCC_subt = 0x73756274,
  FOURCC_tenc = 0x74656e63,
  FOURCC_text = 0x74657874,
  FOURCC_tfdt = 0x74666474,
  FOURCC_tfhd = 0x74666864,
  FOURCC_tkhd = 0x746b6864,
  FOURCC_traf = 0x74726166,
  FOURCC_trak = 0x7472616b,
  FOURCC_trex = 0x74726578,
  FOURCC_trun = 0x7472756e,
  FOURCC_udta = 0x75647461,
  FOURCC_url = 0x75726c20,  // "url "
  FOURCC_urn = 0x75726e20,  // "urn "
  FOURCC_uuid = 0x75756964,
  FOURCC_vide = 0x76696465,
  FOURCC_vlab = 0x766c6162,
  FOURCC_vmhd = 0x766d6864,
  FOURCC_vp08 = 0x76703038,
  FOURCC_vp09 = 0x76703039,
  FOURCC_vpcC = 0x76706343,
  FOURCC_vsid = 0x76736964,
  FOURCC_vttC = 0x76747443,
  FOURCC_vtta = 0x76747461,
  FOURCC_vttc = 0x76747463,
  FOURCC_vtte = 0x76747465,
  FOURCC_wide = 0x77696465,
  FOURCC_wvtt = 0x77767474,
  FOURCC_zaac = 0x7A616163,
  FOURCC_zac3 = 0x7A616333,
  FOURCC_zach = 0x7A616368,
  FOURCC_zacp = 0x7A616370,
  FOURCC_zavc = 0x7A617663,
  FOURCC_zec3 = 0x7A656333,
};

const FourCC kAppleSampleAesProtectionScheme = FOURCC_cbca;

const inline std::string FourCCToString(FourCC fourcc) {
  char buf[5];
  buf[0] = (fourcc >> 24) & 0xff;
  buf[1] = (fourcc >> 16) & 0xff;
  buf[2] = (fourcc >> 8) & 0xff;
  buf[3] = (fourcc) & 0xff;
  buf[4] = 0;
  return std::string(buf);
}

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_FOURCCS_H_
