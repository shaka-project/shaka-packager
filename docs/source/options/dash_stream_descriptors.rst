DASH specific stream descriptor fields
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:dash_accessibilities (accessibilities):

    Optional semicolon separated list of values for DASH Accessibility element.
    The value should be in the format: scheme_id_uri=value, which propagates
    to the Accessibility element in the result DASH manifest. See DASH
    (ISO/IEC 23009-1) specification for details.

:dash_roles (roles):

    Optional semicolon separated list of values for DASH Role element. The
    value should be one of: **caption**, **subtitle**, **main**, **alternate**,
    **supplementary**, **commentary**, **description** and **dub**. See 
    DASH (ISO/IEC 23009-1) specification for details.
