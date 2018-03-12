# Linux Profiling

Profiling code is enabled when the `use_allocator` variable in gyp is set to
`tcmalloc` (currently the default) and `profiling` variable in gyp is set to
`1`. That will build the tcmalloc library, including the cpu profiling and heap
profiling code into shaka-packager, e.g.

    GYP_DEFINES='profiling=1 use_allocator="tcmalloc"' gclient runhooks

If the stack traces in your profiles are incomplete, this may be due to missing
frame pointers in some of the libraries. A workaround is to use the
`linux_keep_shadow_stacks=1` gyp option. This will keep a shadow stack using the
`-finstrument-functions` option of gcc and consult the stack when unwinding.

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

    #include "packager/third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"

    // "foobar" will be included in the message printed to the console
    HeapProfilerDump("foobar");

Then add allocator.gyp dependency to the target with the above change:

    'conditions': [
      ['profiling==1', {
        'dependencies': [
          'base/allocator/allocator.gyp:allocator',
        ],
      }],
    ],

Or you can use gdb to attach at any point:

1.  Attach gdb to the process: `$ gdb -p 12345`
2.  Cause it to dump a profile: `(gdb) p HeapProfilerDump("foobar")`
3.  The filename will be printed on the console, e.g.
    "`Dumping heap profile to heap.0001.heap (foobar)`"

## Reference

[Linux Profiling in Chromium](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_profiling.md)
