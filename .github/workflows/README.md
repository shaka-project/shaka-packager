# GitHub Actions CI

## Reusable workflows
 - `build.yaml`:
   Build and test all combinations of OS & build settings.  Also builds docs on
   Linux.

 - `build-docs.yaml`:
   Build Packager docs.  Runs only on Linux.

 - `docker-image.yaml`:
   Build the official Docker image.

 - `lint.yaml`:
   Lint Shaka Packager.

 - `test-linux-distros.yaml`:
   Test the build on all Linux distros via docker.

## Composed workflows
 - On PR (`pr.yaml`), invoke:
   - `lint.yaml`
   - `build.yaml`
   - `build-docs.yaml`
   - `docker-image.yaml`
   - `test-linux-distros.yaml`

 - On release tag (`github-release.yaml`):
   - Create a draft release
   - Invoke:
     - `lint.yaml`
     - `build.yaml`
     - `test-linux-distros.yaml`
   - Publish the release with binaries from `build.yaml` attached

 - On release published:
   - `docker-hub-release.yaml`, publishes the official Docker image
   - `npm-release.yaml`, publishes the official NPM package
   - `update-docs.yaml`:
     - Invoke `build-docs.yaml`
     - Push the output to the `gh-pages` branch

## Common workflows from shaka-project
 - `sync-labels.yaml`
 - `update-issues.yaml`
 - `validate-pr-title.yaml`

## Required Repo Secrets
 - `DOCKERHUB_CI_USERNAME`: The username of the Docker Hub CI account
 - `DOCKERHUB_CI_TOKEN`: An access token for Docker Hub
   - To generate, visit https://hub.docker.com/settings/security
 - `DOCKERHUB_PACKAGE_NAME`: Not a true "secret", but stored here to avoid
   someone pushing bogus packages to Docker Hub during CI testing from a fork
   - In a fork, set to a private name which differs from the production one
 - `NPM_CI_TOKEN`: An "Automation"-type access token for NPM for the `shaka-bot`
   account
   - To generate, visit https://www.npmjs.com/settings/shaka-bot/tokens and
     select the "Automation" type
 - `NPM_PACKAGE_NAME`: Not a true "secret", but stored here to avoid someone
   pushing bogus packages to NPM during CI testing from a fork
   - In a fork, set to a private name which differs from the production one

## Optional Repo Secrets
 - `ENABLE_DEBUG`: Set to non-empty to enable debugging via SSH after a failure
 - `ENABLE_SELF_HOSTED`: Set to non-empty to enable self-hosted runners in the
   build matrix
