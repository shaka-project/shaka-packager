UDP file options
^^^^^^^^^^^^^^^^

UDP file is of the form::

    udp://<ip>:<port>[?<option>[&<option>]...]

Here is the list of supported options:

:buffer_size=<size_in_bytes>:

    UDP maximum receive buffer size in bytes. Note that although it can be set
    to any value, the actual value is capped by maximum allowed size defined by
    the underlying operating system. On linux, the maximum size allowed can be
    retrieved using `sysctl net.core.rmem_max` and configured using
    `sysctl -w net.core.rmem_max=<size_in_bytes>`.

:interface=<addr>:

    Multicast group interface address. Only the packets sent to this address are
    received. Default to "0.0.0.0" if not specified.

:reuse=0|1:

    Allow or disallow reusing UDP sockets.

:source=<addr>:

    Multicast source ip address. Only the packets sent from this source address
    are received. Enables Source Specific Multicast (SSM) if set.

:timeout=<microseconds>:

    UDP timeout in microseconds.

Example::

    udp://224.1.2.30:88?interface=10.11.12.13&reuse=1

.. note::

    UDP is by definition unreliable. There could be packets dropped.

    UDP packets do not get lost magically. There are things you can do to
    minimize the packet loss. A common cause of packet loss is buffer overrun,
    either in send buffer or receive buffer.

    On Linux, you can check UDP errors by monitoring the output from
    `netstat -suna` command.

    If there is an increase in `send buffer errors` from the `netstat` output,
    then try increasing `buffer_size` in
    [FFmpeg](https://ffmpeg.org/ffmpeg-protocols.html#udp).

    If there is an increase in `receive buffer errors`, then try increasing
    `buffer_size` in UDP options (See above) or increasing `--io_cache_size`.
    `buffer_size` in UDP options defines the UDP buffer size of the underlying
    system while `io_cache_size` defines the size of the internal circular
    buffer managed by `Shaka Packager`.
