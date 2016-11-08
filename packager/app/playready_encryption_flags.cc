
#include "packager/app/fixed_key_encryption_flags.h"

#include "packager/app/validate_flag.h"
#include "packager/base/strings/stringprintf.h"

DEFINE_bool(enable_playready_encryption,
            false,
            "Enable encryption with PlayReady. If enabled User should "
            "provide at least encryption key and encryption key id(--pr_key, "
            "pr_key_id).");
DEFINE_string(pr_key_id, "", "Encryption key Key id in hex string format.");
DEFINE_string(pr_key, "", "Encryption key in hex string format.");
DEFINE_string(pr_iv,
              "",
              "Optional iv in hex string format. If not specified, a random iv will be "
              "generated. This flag should only be used for testing.");
DEFINE_string(pr_additiona_key_id_list,
              "",
              "Optional comma separated list of additional hex string format key ids "
              "added to the PlayReady protection system specific data");
DEFINE_string(pr_la_url,
              "",
              "Optional license acquisition web service URL.");
DEFINE_string(pr_lui_url,
              "",
              "Optional non-silent license acquisition web page URL.");
DEFINE_bool(pr_ondemand,
            false,
            "If true indicates to an application that is should not expect "
            "the full license chain for the content to be available for "
            "acquisition, or already present on the media, prior to setting up "
            "the media graph.");
DEFINE_bool(pr_include_empty_license_store,
            false,
            "Is an empty license store in included in the PlayReady pssh data.");

namespace shaka {

bool ValidatePlayreadyCryptoFlags() {
  bool success = true;

 
  const char playready_crypto_label[] = "--enable_playready_encryption";
  if (!ValidateFlag(
          "pr_key_id", FLAGS_pr_key_id, FLAGS_enable_playready_encryption,
          false, playready_crypto_label)) {
    success = false;
  }

  if (FLAGS_pr_key_id.size() != 2 * 16) {
       PrintError(
          "--pr_kid should be either 16 bytes (32 hex digits).");
       success = false;
  }
  
  if (!ValidateFlag(
          "pr_key", FLAGS_pr_key, FLAGS_enable_playready_encryption,
          false, playready_crypto_label)) {
    success = false;
  }
  if (!ValidateFlag("pr_iv", FLAGS_pr_iv, FLAGS_enable_playready_encryption, true,
                    playready_crypto_label)) {
    success = false;
  }

  if (!FLAGS_pr_iv.empty()) {
    if (FLAGS_pr_iv.size() != 8 * 2 && FLAGS_pr_iv.size() != 16 * 2) {
      PrintError(
          "--pr_iv should be either 8 bytes (16 hex digits) or 16 bytes (32 hex "
          "digits).");
      success = false;
    }
  }

  if (!ValidateFlag(
          "pr_additiona_key_id_list", FLAGS_pr_additiona_key_id_list,
          FLAGS_enable_playready_encryption,
          true, playready_crypto_label)) {
    success = false;
  }  

  if (!ValidateFlag(
          "pr_la_url", FLAGS_pr_la_url,
          FLAGS_enable_playready_encryption,
          true, playready_crypto_label)) {
    success = false;
  }    

  if (!ValidateFlag(
          "pr_lui_url", FLAGS_pr_lui_url,
          FLAGS_enable_playready_encryption,
          true, playready_crypto_label)) {
    success = false;
  }    

  if (!FLAGS_enable_playready_encryption && FLAGS_pr_ondemand) {
      PrintError(base::StringPrintf(
                     "--pr_ondemand should be specified only if %s",
                     playready_crypto_label));
      success = false;
  }

  if (!FLAGS_enable_playready_encryption &&
      FLAGS_pr_include_empty_license_store) {
      PrintError(base::StringPrintf(
                     "--pr_include_empty_license_store should be specified "
                     " only if %s", playready_crypto_label));
      success = false;
  }
  
  return success;
}

}  // namespace shaka
