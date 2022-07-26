# zlib fork

This uses a fork of zlib, which is at https://github.com/joeyparrish/zlib.

We forked from upstream https://github.com/madler/zlib and merged in the PR from
https://github.com/madler/zlib/pull/519, which contains code from
https://github.com/Togtja/zlib/tree/optional_zconf_rename.  This gives us an
option to disable zlib's default CMake behavior of modifying the working
directory during configuration.  This helps to keep the submodule clean.

When https://github.com/madler/zlib/pull/519 is merged, the fork can be deleted.
