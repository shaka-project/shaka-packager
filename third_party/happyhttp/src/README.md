HappyHTTP
=========

*a simple HTTP library*
 
 Fork of [http://www.scumways.com/happyhttp/happyhttp.html](http://www.scumways.com/happyhttp/happyhttp.html)

Contents
--------

-   [Overview](#overview)
-   [Download](#download)
-   [Usage](#usage)
-   [Example](#example)
-   [TODO](#todo)
-   [License](#license)

* * * * *

Overview
--------

HappyHTTP is a simple C++ library for issuing HTTP requests and
processing responses.

-   Simple to integrate - just drop in the [.h](happyhttp.h) and
    [.cpp](happyhttp.cpp) files
-   Easy-to-use interface ([example](#example))
-   Non-blocking operation, suitable for use in game update loops
-   Supports pipelining. You can issue multiple requests without waiting
    for responses.
-   Licensed under the [zlib/libpng
    license](http://www.opensource.org/licenses/zlib-license.php).
-   Cross-platform (Linux, OSX, Windows)

* * * * *

Download
--------

Latest Version is 0.1:
[happyhttp-0.1.tar.gz](http://www.scumways.com/happyhttp/happyhttp-0.1.tar.gz)

* * * * *

Usage
-----

The interface is based loosely on Python's
[httplib](http://docs.python.org/lib/module-httplib.html).

All HappyHTTP code is kept within the `happyhttp::` namespace

To issue and process a HTTP request, the basic steps are:

1.  Create a connection object
2.  Set up callbacks for handling responses
3.  Issue request(s)
4.  'pump' the connection at regular intervals. As responses are
    received, the callbacks will be invoked.

### Connection object methods

**`Connection( const char* host, int port )`**
 Constructor. Specifies host and port, but connection isn't made until
request is issued or connect() is called.

**`~Connection()`**
 Destructor. If connection is open, it'll be closed, and any outstanding
requests will be discarded.

**`void setcallbacks( ResponseBegin_CB begincb, ResponseData_CB datacb, ResponseComplete_CB completecb, void* userdata )`**
 Set up the response handling callbacks. These will be invoked during
calls to pump().
 `begincb` - called when the responses headers have been received\
 `datacb` - called repeatedly to handle body data
 `completecb` - response is completed
 `userdata` - passed as a param to all callbacks.

**`void connect()`**
 Don't need to call connect() explicitly as issuing a request will call
it automatically if needed. But it could block (for name lookup etc), so
you might prefer to call it in advance.

**`void close()`**
 // close connection, discarding any pending requests.

**`void pump()`**
 Update the connection (non-blocking) Just keep calling this regularly
to service outstanding requests. As responses are received, the
callbacks will be invoked.

**`bool outstanding() const`**
 Returns true if any requests are still outstanding.

**`void request( const char* method, const char* url, const char* headers[], const unsigned char* body, int bodysize )`**
 High-level request interface. Issues a request.
 `method` - "GET", "POST" etc...
 `url` - eg "/index.html"
 `headers` - array of name/value pairs, terminated by a null-ptr
 `body, bodysize` - specify body data of request (eg values for a form)

**`void putrequest( const char* method, const char* url )`** 
 (part of low-level request interface)
 Begin a request
 method is "GET", "POST" etc...
 url is only path part: eg "/index.html"

**`void putheader( const char* header, const char* value )`**
 **`void putheader( const char* header, int numericvalue )`**
 (part of low-level request interface)
 Add a header to the request (call after putrequest() )

**`void endheaders()`**
 (part of low-level request interface)
 Indicate that your are finished adding headers and the request can be
issued.

`void send( const unsigned char* buf, int numbytes )` 
 (part of low-level request interface)
 send body data if any. To be called after endheaders()

### Callback types

**`typedef void (*ResponseBegin_CB)( const Response* r, void* userdata )`**
 Invoked when all the headers for a response have been received.\
 The Response object can be queried to determine status and header
values.
 `userdata` is the same value that was passed in to
`Connection::setcallbacks()`.

**`typedef void (*ResponseData_CB)( const Response* r, void* userdata, const unsigned char* data, int numbytes )`**
 This callback is invoked to pass out data from the body of the
response. It may be called multiple times, or not at all (if there is no
body).

**`typedef void (*ResponseComplete_CB)( const Response* r, void* userdata )`**
 Once a response is completed, this callback is invoked. When the
callback returns, the respsonse object will be destroyed.

### Response object methods

When a callback is invoked, a response object is passed to it. The
following member functions can be used to query the response:

**`const char* getheader( const char* name ) const`**
 retrieve the value of a header (returns 0 if not present)

**`int getstatus() const`**
 Get the HTTP status code returned by the server

**`const char* getreason() const`**
 Get the HTTP response reason string returned by the server

### Error Handling

If an error occurs, a `Wobbly` is thrown. The `Wobbly::what()` method
returns a text description.

* * * * *

Example
-------

For more examples, see [test.cpp](test.cpp).


    static int count=0;

    // invoked when response headers have been received
    void OnBegin( const happyhttp::Response* r, void* userdata )
    {
        printf( "BEGIN (%d %s)\n", r-&gtgetstatus(), r-&gtgetreason() );
        count = 0;
    }

    // invoked to process response body data (may be called multiple times)
    void OnData( const happyhttp::Response* r, void* userdata, const unsigned char* data, int n )
    {
        fwrite( data,1,n, stdout );
        count += n;
    }

    // invoked when response is complete
    void OnComplete( const happyhttp::Response* r, void* userdata )
    {
        printf( "COMPLETE (%d bytes)\n", count );
    }


    void TestGET()
    {
        happyhttp::Connection conn( "www.scumways.com", 80 );
        conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );

        conn.request( "GET", "/happyhttp/test.php" );

        while( conn.outstanding() )
            conn.pump();
    }

* * * * *

TODO
----

-   Proxy support
-   Add helper functions for URL wrangling
-   Improve error text (and maybe some more exception types?)
-   HTTP 0.9 support
-   Improved documentation and examples

* * * * *

License
-------

HappyHTTP is licensed under the [zlib/libpng
license](http://www.opensource.org/licenses/zlib-license.php).

You are free to use this library however you wish, but if you
make changes, please send a patch!

If you use it, let us know.
