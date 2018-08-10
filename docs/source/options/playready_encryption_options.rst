PlayReady encryption options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

--enable_playready_encryption

    Enable encryption with PlayReady key. This generates PlayReady protection
    system if --protection_systems is not specified. Use --protection_system to
    generate multiple protection systems.

--playready_server_url <url>

    PlayReady packaging server url.

--program_identifier <program_identifier>

    Program identifier for packaging request.

--ca_file <file path>

    Absolute path to the certificate authority file for the server cert.
    PEM format. Optional, depends on server configuration.

--client_cert_file <file path>

    Absolute path to client certificate file. Optional, depends on server
    configuration.

--client_cert_private_key_file <file path>

    Absolute path to the private key file. Optional, depends on server
    configuration.

--client_cert_private_key_password <string>

    Password to the private key file. Optional, depends on server configuration.
