# zlib fork

This uses a fork of zlib, which is at
https://github.com/joeyparrish/zlib/tree/preview

We forked from upstream https://github.com/madler/zlib and merged the following
PRs:

 - https://github.com/madler/zlib/pull/519, which contains code from
   https://github.com/Togtja/zlib/tree/optional_zconf_rename.  This gives us an
   option to disable zlib's default CMake behavior of modifying the working
   directory during configuration.  This helps to keep the submodule clean.

 - https://github.com/madler/zlib/pull/691, which contains code from
   https://github.com/joeyparrish/zlib/tree/optional_tests.  This gives us a
   way to disable zlib's test targets.

When these PRs are merged, the fork can be deleted.
