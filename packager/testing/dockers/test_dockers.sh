#!/bin/bash

# Exit on first error.
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGER_DIR="$(dirname "$(dirname "$(dirname "$(dirname ${SCRIPT_DIR})")")")"

function docker_run() {
  docker run \
    -v ${PACKAGER_DIR}:/shaka-packager \
    -w /shaka-packager/src \
    --user $(id -u):$(id -g) \
    ${CONTAINER} "$@"
}

# Command line arguments will be taken as an allowlist of OSes to run.
# By default, a regex that matches everything.
FILTER=".*"
if [[ $# != 0 ]]; then
  # Join arguments with a pipe, to make a regex alternation to match any of
  # them.  The syntax is a mess, but that's bash.  Set IFS (the separator
  # variable) in a subshell and print the array.  This has the effect of joining
  # them by the character in IFS. Then add parentheses to make a complete regex
  # to match all the arguments.
  FILTER=$(IFS="|"; echo "$*")
  FILTER="($FILTER)"
fi

# On exit, print the name of the OS we were on.  This helps identify what to
# debug when the start of a test run scrolls off-screen.
trap 'echo "Failed on $OS_NAME!"' exit

echo "Using OS filter: $FILTER"
RAN_SOMETHING=0
for DOCKER_FILE in ${SCRIPT_DIR}/*_Dockerfile ; do
  # Take the basename of the dockerfile path, then remove the trailing
  # "_Dockerfile" from the file name.  This is the OS name.
  OS_NAME="$( basename "$DOCKER_FILE" | sed -e 's/_Dockerfile//' )"

  if echo "$OS_NAME" | grep -Eqi "$FILTER"; then
     echo "Testing $OS_NAME."
     # Fall through.
  else
     echo "Skipping $OS_NAME."
     continue
  fi

  # Build a unique container name per OS for debugging purposes and to improve
  # caching.  Containers names must be in lowercase.
  # To debug a failure in Alpine, for example, use:
  #   docker run -it -v /path/to/packager:/shaka-packager \
  #       packager_test_alpine:latest /bin/bash
  CONTAINER="$( echo "packager_test_${OS_NAME}" | tr A-Z a-z )"

  RAN_SOMETHING=1
  docker build -t ${CONTAINER} -f ${DOCKER_FILE} ${SCRIPT_DIR}
  docker_run rm -rf out/Release
  docker_run gclient runhooks
  docker_run ninja -C out/Release
  docker_run out/Release/packager_test.py -v
done

# Clear the exit trap from above.
trap - exit

if [[ "$RAN_SOMETHING" == "0" ]]; then
  echo "No tests were run!  The filter $FILTER did not match any OSes." 1>&2
  exit 1
fi
