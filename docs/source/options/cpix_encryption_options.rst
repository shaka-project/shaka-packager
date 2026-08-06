CPIX encryption options
^^^^^^^^^^^^^^^^^^^^^^^

--enable_cpix_encryption

    Enable encryption with keys from a CPIX (DASH-IF Content Protection
    Information Exchange) document. Keys, DRM signaling (PSSH) and the key to
    stream mapping are read from the document. DRM signaling comes from the
    document's DRMSystemList and is authoritative; --protection_systems may
    be used to generate signaling for additional protection systems.

--enable_cpix_decryption

    Enable decryption with keys from a CPIX document. Keys are looked up by
    key ID, so the document's usage rules are not used.

--cpix <path or url>

    Path or URL to the CPIX document. URLs are fetched with GET, unless
    --cpix_request_file is also set, in which case the request document is
    POSTed to the URL and the response is used as the CPIX document.

--cpix_request_file <path>

    Optional path to a CPIX request document. If set, --cpix must be an
    HTTP(S) URL to POST it to (SPEKE style request/response exchange).

--cpix_headers <headers>

    Optional semicolon separated list of HTTP headers in 'Name: value' form
    to send when fetching the CPIX document, e.g. for authentication.

--cpix_private_key <path>

    Optional path to the recipient RSA private key (PEM or DER), used to
    decrypt encrypted CPIX documents. Required when the document's content
    keys are encrypted.
