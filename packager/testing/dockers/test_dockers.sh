#!/bin/bash

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGER_DIR="$(dirname "$(dirname "$(dirname "$(dirname ${SCRIPT_DIR})")")")"

function docker_run() {
  docker run -v ${PACKAGER_DIR}:/shaka-packager -w /shaka-packager/src my_container "$@"
}

for docker_file in ${SCRIPT_DIR}/*_Dockerfile ; do
  docker build -t my_container -f ${docker_file} ${SCRIPT_DIR}
  docker_run gclient runhooks
  docker_run ninja -C out/Release
done
