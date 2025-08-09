#!/bin/bash

# Exit on first error.
set -e

# To debug a failure, run with the variable DEBUG=1.  For example:
#   DEBUG=1 ./packager/testing/test_dockers.sh

SCRIPT_DIR="$(dirname "$0")"
PACKAGER_DIR="$(realpath "$SCRIPT_DIR/../..")"
TEMP_BUILD_DIR="$(mktemp -d)"

function docker_run_internal() {
  if [[ "$DEBUG" == "1" ]]; then
    # For debugging, allocate an interactive terminal in docker.
    INTERACTIVE_ARG="-it"
  else
    INTERACTIVE_ARG=""
  fi

  docker run \
      ${INTERACTIVE_ARG} \
      -v ${PACKAGER_DIR}:/shaka-packager \
      -v ${TEMP_BUILD_DIR}:/shaka-packager/build \
      -w /shaka-packager \
      -e HOME=/tmp \
      --user $(id -u):$(id -g) \
      ${CONTAINER} "$@"
}

function docker_run() {
  if ! docker_run_internal "$@"; then
    echo "Command failed in ${CONTAINER}: $@"
    if [[ "$DEBUG" == "1" ]]; then
      echo "Launching interactive shell to debug."
      docker_run_internal /bin/bash
      exit 1
    else
      echo "Run with DEBUG=1 to debug!"
      exit 1
    fi
  fi
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

function on_exit() {
  # On exit, print the name of the OS we were on.  This helps identify what to
  # debug when the start of a test run scrolls off-screen.
  echo "Failed on $OS_NAME!"
  rm -rf "${TEMP_BUILD_DIR}"
}
trap 'on_exit' exit

echo "Using OS filter: $FILTER"
RAN_SOMETHING=0
for DOCKER_FILE in ${SCRIPT_DIR}/dockers/*; do
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
  CONTAINER="$( echo "packager_test_${OS_NAME}" | tr A-Z a-z )"

  RAN_SOMETHING=1
  docker buildx build --pull -t ${CONTAINER} -f ${DOCKER_FILE} ${SCRIPT_DIR}/dockers/
  mkdir -p "${TEMP_BUILD_DIR}"
  docker_run cmake -S . -B build/ -DCMAKE_BUILD_TYPE=Debug -G Ninja \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5
  docker_run cmake --build build/ --config Debug --parallel
  docker_run bash -c "cd build && ctest -C Debug -V"
  rm -rf "${TEMP_BUILD_DIR}"
done

# Clear the exit trap from above.
trap - exit

if [[ "$RAN_SOMETHING" == "0" ]]; then
  echo "No tests were run!  The filter $FILTER did not match any OSes." 1>&2
  exit 1
fi
