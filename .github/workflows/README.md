# GitHub Actions CI

## Reusable workflows
 - `build.yaml`:
   Build and test all combinations of OS & build settings.  Also builds docs on
   Linux.

 - `build-docs.yaml`:
   Build Packager docs.  Runs only on Linux.

 - `build-docker.yaml`:
   Build the official Docker image.

 - `lint.yaml`:
   Lint Shaka Packager.

 - `publish-docs.yaml`:
   Publish Packager docs.  Runs on the latest release.

 - `publish-docker.yaml`:
   Publish the official docker image.  Runs on all releases.

 - `publish-npm.yaml`:
   Publish binaries to NPM.  Runs on all releases.

 - `test-linux-distros.yaml`:
   Test the build on all Linux distros via docker.

## Composed workflows
 - On PR (`pr.yaml`), invoke:
   - `lint.yaml`
   - `build.yaml`
   - `build-docs.yaml`
   - `build-docker.yaml`
   - `test-linux-distros.yaml`

## Release workflow
 - `release-please.yaml`
   - Updates changelogs, version numbers based on conventional commits syntax
     and semantic versioning
     - https://conventionalcommits.org/
     - https://semver.org/
   - Generates/updates a PR on each push
   - When the PR is merged, runs additional steps:
     - Creates a GitHub release
     - Invokes `publish-docs.yaml` to publish the docs
     - Invokes `publish-docker.yaml` to publish the docker image
     - Invokes `build.yaml`
     - Attaches the binaries from `build.yaml` to the GitHub release
     - Invokes `publish-npm.yaml` to publish the binaries to NPM

## Common workflows from shaka-project
 - `sync-labels.yaml`
 - `update-issues.yaml`
 - `validate-pr-title.yaml`

## Required Repo Secrets
 - `RELEASE_PLEASE_TOKEN`: A PAT for `shaka-bot` to run the `release-please`
   action.  If missing, the release workflow will use the default
   `GITHUB_TOKEN`
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

## Repo Settings

Each of these workflow features can be enabled by creating a "GitHub
Environment" with the same name in your repo settings.  Forks will not have
these enabled by default.

 - `debug`: enable debugging via SSH after a failure
 - `self_hosted`: enable self-hosted runners in the build matrix
