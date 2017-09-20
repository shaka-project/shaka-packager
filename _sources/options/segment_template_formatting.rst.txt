Segment template formatting
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The implementation is based on *Template-based Segment URL construction*
described in ISO/IEC 23009-1:2014.

.. table:: Supported identifiers

    ============== ============================== ==============================
    $<Identifier>$ Substitution parameter         Format
    ============== ============================== ==============================
    $$             is an escape sequence, i.e.    Not applicable.
                   "$$" is replaced with a single
                   "$".
    $Number$       This identifier is substitued  The format tag may be present.
                   with the *number* of the
                   corresponding Segment.         If no format tag is present, a
                                                  default format tag with
                                                  *width*\=1 shall be used.
    $Time$         This identifier is substituted The format tag may be present.
                   with the value of the
                   **SegmentTimeline@t**          If no format tag is present, a
                   attribute for the Segment      default format tag with
                   being accessed. Either         *width*\=1 shall be used.
                   $Number$ or $Time$ may be used
                   but not both at the same time.
    ============== ============================== ==============================

.. note::

   Identifiers $RepresentationID$ and $Bandwidth$ are not supported in this
   version. Please file an `issue
   <https://github.com/google/shaka-packager/issues>`_ if you want it to be
   supported.

In each URL, the identifiers shall be replaced by the substitution parameter
per the definition in the above table. Identifier matching is case-sensitive.

Each identifier may be suffixed, within the enclosing '$' characters, with an
additional format tag aligned with the *printf* format tag as defined in IEEE
1003.1-2008 following this prototype::

    %0[width]d

The *width* parameter is an unsigned integer that provides the minimum number
of characters to be printed. If the value to be printed is shorter than this
number, the result shall be padded with zeros. The value is not truncated even
if the result is larger.

Strings outside identifiers shall only contain characters that are permitted
within URLs according to RFC 3986.
