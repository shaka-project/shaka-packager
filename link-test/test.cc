// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// This is a simple app to test linking against a shared libpackager on all
// platforms.  It's not meant to do anything useful at all.

#include <cstdio>
#include <vector>

#include <packager/packager.h>

int main(int argc, char** argv) {
  // Unused.  Silence warnings.
  (void)argc;
  (void)argv;

  // Print the packager version.
  std::cout << "Packager v" + shaka::Packager::GetLibraryVersion() + "\n";

  // Don't bother filling these out.  Just make sure it links.
  shaka::PackagingParams packaging_params;
  std::vector<shaka::StreamDescriptor> stream_descriptors;

  // This will fail.
  shaka::Packager packager;
  shaka::Status status =
      packager.Initialize(packaging_params, stream_descriptors);

  // Just print the status to make sure we can do that in a custom app.
  std::cout << status.ToString() + "\n";
  return 0;
}
