Raw key encryption options
^^^^^^^^^^^^^^^^^^^^^^^^^^

--enable_fixed_key_encryption

    Enable encryption with fixed key.

--enable_fixed_key_decryption

    Enable decryption with fixed key.

--key_id <32-digit hex string>

    The key id in hex string format.
    HEX.

--key <32-digit hex string>

    The key in hex string format.

--iv <16-digit or 32-digit hex string>

    IV in hex string format. If not specified, a random IV will be generated.
    This flag should only be used for testing. IV must be either 8 bytes
    (16 digits HEX) or 16 bytes (32 digits in HEX).

--pssh <hex string>

    One or more concatenated PSSH boxes in hex string format. If not specified,
    a `v1 common PSSH box <https://goo.gl/s8RIhr>`_ will be generated.
