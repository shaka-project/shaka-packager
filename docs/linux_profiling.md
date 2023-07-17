# Linux Profiling

Profiling code is enabled by setting certain special compiler flags when you
invoke CMake.  For example:

```sh
cmake \
  -DCMAKE_CXX_FLAGS_DEBUG=-pg \
  -DCMAKE_EXE_LINKER_FLAGS_DEBUG=-pg \
  -DCMAKE_SHARED_LINKER_FLAGS_DEBUG=-pg \
  -DCMAKE_BUILD_TYPE=Debug
```

These flags work for both GCC and Clang.  For MSVC, the flags are:

```sh
cmake \
  -DCMAKE_EXE_LINKER_FLAGS_DEBUG=/PROFILE \
  -DCMAKE_BUILD_TYPE=Debug
```


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


## Thread sanitizer (tsan)

To compile with the thread sanitizer library (tsan), you must set clang as your
compiler and set the `-fsanitize=thread` compiler flag:

```sh
CC=clang CXX=clang++ \
cmake \
  -DCMAKE_CXX_FLAGS_DEBUG=-fsanitize=thread \
  -DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=thread \
  -DCMAKE_SHARED_LINKER_FLAGS_DEBUG=-fsanitize=thread \
  -DCMAKE_BUILD_TYPE=Debug
```

NOTE: tsan and asan cannot be used at the same time.

See also https://clang.llvm.org/docs/ThreadSanitizer.html


## Adddress sanitizer (asan)

To compile with the address sanitizer library (asan), you must set clang as your
compiler and set the `-fsanitize=address` compiler flag:

```sh
CC=clang CXX=clang++ \
cmake \
  -DCMAKE_CXX_FLAGS_DEBUG=-fsanitize=address \
  -DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address \
  -DCMAKE_SHARED_LINKER_FLAGS_DEBUG=-fsanitize=address \
  -DCMAKE_BUILD_TYPE=Debug
```

NOTE: tsan and asan cannot be used at the same time.

See also https://clang.llvm.org/docs/AddressSanitizer.html


## Leak sanitizer (lsan)

To compile with the leak sanitizer library (lsan), you must set clang as your
compiler and set the `-fsanitize=leak` compiler flag:

```sh
CC=clang CXX=clang++ \
cmake \
  -DCMAKE_CXX_FLAGS_DEBUG=-fsanitize=leak \
  -DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=leak \
  -DCMAKE_SHARED_LINKER_FLAGS_DEBUG=-fsanitize=leak \
  -DCMAKE_BUILD_TYPE=Debug
```

See also https://clang.llvm.org/docs/LeakSanitizer.html


## Other sanitizers in Clang

See also:
 - https://clang.llvm.org/docs/MemorySanitizer.html
 - https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
 - https://clang.llvm.org/docs/DataFlowSanitizer.html
