
#ifndef APP_PLAYREADY_ENCRYPTION_FLAGS_H_
#define APP_PLAYREADY_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_bool(enable_playready_encryption);
DECLARE_string(pr_key_id);
DECLARE_string(pr_key);
DECLARE_string(pr_iv);
DECLARE_string(pr_additiona_key_id_list);
DECLARE_string(pr_la_url);
DECLARE_string(pr_lui_url);
DECLARE_bool(pr_ondemand);
DECLARE_bool(pr_include_empty_license_store);

namespace shaka {

/// Validate fixed encryption/decryption flags.
/// @return true on success, false otherwise.
bool ValidatePlayreadyCryptoFlags();

}  // namespace shaka

#endif  // APP_PLAYREADY_ENCRYPTION_FLAGS_H_
