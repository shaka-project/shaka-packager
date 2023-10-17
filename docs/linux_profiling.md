# Linux Profiling

In theory we should be able to build packager using
[gperftools](https://github.com/gperftools/gperftools/tree/master) to
get back the profiling functionality described below. However actually
integrating this into the CMake build is not yet done. Pull requests
welcome. See https://github.com/shaka-project/shaka-packager/issues/1277

If packager was linked using `-ltcmalloc` then the following
instructions should work:

## CPU Profiling

In order to enable cpu profiling, run shaka-packager with the environment
variable `CPUPROFILE` set to a filename. For example:

    CPUPROFILE=/tmp/cpuprofile out/Release/packager

The cpu profile will be dumped periodically to the filename specified in the
CPUPROFILE environment variable. You can then analyze the dumps using the pprof
script (`packager/third_party/tcmalloc/chromium/src/pprof`). For example,

    pprof --gv out/Release/packager /tmp/cpuprofile

This will generate a visual representation of the cpu profile as a postscript
file and load it up using `gv`. For more powerful commands, please refer to the
pprof help output and the google-perftools documentation.

For further information, please refer to
http://google-perftools.googlecode.com/svn/trunk/doc/cpuprofile.html.

## Heap Profiling

To turn on the heap profiler on shaka-packager, use the `HEAPPROFILE`
environment variable to specify a filename for the heap profile. For example:

    HEAPPROFILE=/tmp/heapprofile out/Release/packager

The heap profile will be dumped periodically to the filename specified in the
`HEAPPROFILE` environment variable. The dumps can be analyzed using the same
command as cpu profiling above.

For further information, please refer to
http://google-perftools.googlecode.com/svn/trunk/doc/heapprofile.html.

Some tests fork short-living processes which have a small memory footprint. To
catch those, use the `HEAP_PROFILE_ALLOCATION_INTERVAL` environment variable.

#### Dumping a profile of a running process

To programmatically generate a heap profile before exit, use code like:

    #include <gperftools/heap-profiler.h>

    // "foobar" will be included in the message printed to the console
    HeapProfilerDump("foobar");

Or you can use gdb to attach at any point:

1.  Attach gdb to the process: `$ gdb -p 12345`
2.  Cause it to dump a profile: `(gdb) p HeapProfilerDump("foobar")`
3.  The filename will be printed on the console, e.g.
    "`Dumping heap profile to heap.0001.heap (foobar)`"


## Thread sanitizer (tsan)

To compile with the thread sanitizer library (tsan), you must set clang as your
compiler and set `-fsanitize=thread` in compiler flags.

NOTE: tsan and asan cannot be used at the same time.

## Adddress sanitizer (asan)

To compile with the address sanitizer library (asan), you must set clang as your
compiler and set `-fsanitize=address` in compiler and linker flags.

NOTE: tsan and asan cannot be used at the same time.

## Leak sanitizer (lsan)

To compile with the leak sanitizer library (lsan), you must set clang as your
compiler and use `-fsanitize=leak` in compiler and linker flags.
