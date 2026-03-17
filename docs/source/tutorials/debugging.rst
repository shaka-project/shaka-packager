Debugging
=========

Logging System Overview
------------------------

Shaka Packager uses the Google logging library (glog) which provides three types
of logging mechanisms:

1. **LOG()** - Standard severity-based logging (INFO, WARNING, ERROR, FATAL)
2. **VLOG()** - Global verbose logging with numeric levels
3. **DVLOG()** - Per-module debug verbose logging with numeric levels

Understanding the differences between these logging types is essential for
effective debugging without being overwhelmed by log output.

Logging Types Comparison
~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 15 20 20 45

   * - Type
     - Control Flag
     - Default Behavior
     - Purpose
   * - LOG(INFO/WARNING/ERROR)
     - ``--minloglevel``
     - Always shown
     - Standard production logging
   * - VLOG(N)
     - ``--v=N``
     - Hidden (off)
     - Global verbose debugging
   * - DVLOG(N)
     - ``--vmodule=module=N``
     - Hidden (off)
     - Per-module verbose debugging

Standard Logging (LOG)
----------------------

Standard LOG statements are used for production logging and are always enabled
by default. They use severity levels to indicate the importance of messages.

Severity Levels
~~~~~~~~~~~~~~~

- **LOG(INFO)** - Informational messages (routine operations, status updates)
- **LOG(WARNING)** - Warning messages (potential issues, degraded functionality)
- **LOG(ERROR)** - Error messages (recoverable errors, failed operations)
- **LOG(FATAL)** - Fatal errors (unrecoverable errors, terminates program)

Controlling LOG Output
~~~~~~~~~~~~~~~~~~~~~~

Use ``--minloglevel=N`` to set the minimum severity level to display::

    # Show only WARNING and above (hide INFO messages)
    $ packager input.ts --minloglevel=1

    # Show only ERROR and above (hide INFO and WARNING)
    $ packager input.ts --minloglevel=2

    # Show everything (default)
    $ packager input.ts --minloglevel=0

Severity level values:

- 0 = INFO and above (default)
- 1 = WARNING and above
- 2 = ERROR and above
- 3 = FATAL only

Other useful LOG flags::

    # Send logs to stderr in addition to log files
    $ packager input.ts --alsologtostderr

    # Set which logs go to stderr (0=INFO, 1=WARNING, 2=ERROR)
    $ packager input.ts --stderrthreshold=1

Verbose Logging (VLOG and DVLOG)
---------------------------------

VLOG and DVLOG provide detailed diagnostic logging that is silent by default.
These are essential for debugging specific components without cluttering
production output.

Verbosity Levels
~~~~~~~~~~~~~~~~

Both VLOG and DVLOG use numeric levels to control detail:

- **Level 1**: High-level events (initialization, major state changes)
- **Level 2**: Medium detail (sample/packet processing, typical debugging)
- **Level 3**: Detailed tracing (internal operations, verbose debugging)
- **Level 4**: Very detailed tracing (fine-grained internal operations)

Higher levels include all messages from lower levels (e.g., level 2 includes
level 1 messages).

VLOG - Global Verbose Logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VLOG affects ALL modules in the entire application. Use this when you want
general verbose output from the whole system::

    # Enable all VLOG(1) and VLOG(2) statements everywhere
    $ packager input.ts --v=2

    # Enable only VLOG(1) statements everywhere
    $ packager input.ts --v=1

**Warning**: Using high VLOG levels (--v=3 or --v=4) can produce very large
amounts of output and may impact performance.

DVLOG - Per-Module Debug Logging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

DVLOG (Debug Verbose LOG) allows selective enabling for specific modules only.
This is the **recommended approach** for targeted debugging::

    # Enable logging for teletext parser only
    $ packager input.ts --vmodule=es_parser_teletext=2

    # Enable logging for multiple modules
    $ packager input.ts --vmodule=es_parser_teletext=2,text_chunker=1

    # Combine global and module-specific levels
    $ packager input.ts --v=1 --vmodule=mp2t_media_parser=3

Module names correspond to source file names without extensions (e.g.,
``es_parser_teletext`` for ``es_parser_teletext.cc``). Glob patterns with
``*`` and ``?`` are supported.

Practical Examples
------------------

Debugging Teletext Heartbeat Feature
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To debug the teletext heartbeat mechanism with detailed logging from only the
relevant modules::

    $ packager \
      'in=input.ts,stream=text,cc_index=888,output=out.vtt' \
      --vmodule=es_parser_teletext=2,text_chunker=2,mp2t_media_parser=2

This enables:

- **es_parser_teletext=2**: Shows heartbeat emission (kTextHeartBeat, kCueStart, kCueEnd)
- **text_chunker=2**: Shows heartbeat processing and segment dispatch
- **mp2t_media_parser=2**: Shows media heartbeat generation from video PTS

Clean Output with Minimal Noise
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To see only warnings/errors plus specific debug output::

    $ packager input.ts \
      --minloglevel=1 \
      --vmodule=text_chunker=2

This hides routine INFO messages while showing WARNING/ERROR messages and
detailed text_chunker debug output.

Debugging Segment Output
~~~~~~~~~~~~~~~~~~~~~~~~~

To see when segments are created and written to disk::

    $ packager input.ts \
      --vmodule=text_chunker=2,mp4_muxer=2 \
      --v=1

- ``text_chunker=2`` shows segment dispatch (DVLOG)
- ``--v=1`` enables mp4_muxer segment finalization logs (VLOG)

Full Debug Output
~~~~~~~~~~~~~~~~~

For comprehensive debugging (use cautiously, generates large output)::

    $ packager input.ts --v=3

This enables all VLOG statements at level 3 across the entire application.

Best Practices
--------------

1. **Start with DVLOG**: Use ``--vmodule`` to target specific modules rather
   than enabling global verbose logging with ``--v``.

2. **Suppress routine INFO**: Add ``--minloglevel=1`` to hide routine INFO
   messages when debugging, making it easier to spot relevant debug output.

3. **Use appropriate levels**: Start with level 1 or 2 for most debugging.
   Only use level 3+ when you need very detailed internal tracing.

4. **Combine strategically**: You can combine LOG control with VLOG/DVLOG::

       $ packager input.ts --minloglevel=1 --vmodule=my_module=2

   This shows only warnings/errors globally, plus detailed debug from my_module.

5. **Save output to file**: When using verbose logging, redirect to a file for
   easier analysis::

       $ packager input.ts --vmodule=text_chunker=3 2>&1 | tee debug.log

Useful Modules for Debugging
-----------------------------

Common modules that are useful to debug:

- ``es_parser_teletext`` - Teletext parsing and subtitle extraction
- ``text_chunker`` - Text segmentation and cue processing
- ``mp2t_media_parser`` - MPEG-2 TS parsing and PID handling
- ``mp4_muxer`` - MP4 segment creation and finalization
- ``chunking_handler`` - General media chunking/segmentation
- ``webvtt_to_mp4_handler`` - WebVTT to MP4 conversion
- ``ts_muxer`` - MPEG-2 TS muxing

Use ``--vmodule=module_name=2`` to enable debugging for any of these modules.
