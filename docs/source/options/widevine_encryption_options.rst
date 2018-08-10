Widevine encryption options
^^^^^^^^^^^^^^^^^^^^^^^^^^^

--enable_widevine_encryption

    Enable encryption with Widevine key server. User should provide either
    AES signing key (--aes_signing_key, --aes_signing_iv) or RSA signing key
    (--rsa_signing_key_path). This generates Widevine protection system if
    --protection_systems is not specified. Use --protection_systems to generate
    multiple protection systems.

--enable_widevine_decryption

    Enable decryption with Widevine key server. User should provide either
    AES signing key (--aes_signing_key, --aes_signing_iv) or RSA signing key
    (--rsa_signing_key_path).

--key_server_url <url>

    Key server url. Required for Widevine encryption and decryption.

--content_id <hex>

    Content identifier that uniquely identifies the content.

--policy <policy>

    The name of a stored policy, which specifies DRM content rights.

--max_sd_pixels <pixels>

    The video track is considered SD if its max pixels per frame is no higher
    than *max_sd_pixels*. Default: 442368 (768 x 576).

--max_hd_pixels <pixels>

    The video track is considered HD if its max pixels per frame is higher than
    *max_sd_pixels*, but no higher than *max_hd_pixels*. Default: 2073600
    (1920 x 1080).

--max_uhd1_pixels <pixels>

    The video track is considered UHD1 if its max pixels per frame is higher
    than *max_hd_pixels*, but no higher than *max_uhd1_pixels*. Otherwise it is
    UHD2. Default: 8847360 (4096 x 2160).

--signer <signer>

    The name of the signer.

--aes_signing_key <hex>

    AES signing key in hex string. *aes_signing_iv* is required if
    *aes_signing_key* is specified. This option is exclusive with
    *rsa_signing_key_path*.

--aes_signing_iv <hex>

    AES signing iv in hex string.

--rsa_signing_key_path <file path>

    Path to the file containing PKCS#1 RSA private key for request signing.
    This option is exclusive with *aes_signing_key*.

--crypto_period_duration <seconds>

    Defines how often key rotates. If it is non-zero, key rotation is enabled.

--group_id <hex>

    Identifier for a group of licenses.
