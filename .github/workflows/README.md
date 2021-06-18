# GitHub Actions CI

## Actions
 - `custom-actions/lint-packager`:
   Lints Shaka Packager.  You must pass `fetch-depth: 2` to `actions/checkout`
   in order to provide enough history for the linter to tell which files have
   changed.
 - `custom-actions/build-packager`:
   Builds Shaka Packager.  Leaves build artifacts in the "artifacts" folder.
   Requires OS-dependent and build-dependent inputs.
 - `custom-actions/test-packager`:
   Tests Shaka Packager.  Requires OS-dependent and build-dependent inputs.
 - `custom-actions/build-docs`:
   Builds Shaka Packager docs.

## Workflows
 - On PR:
   - `build_and_test.yaml`:
     Builds and tests all combinations of OS & build settings.  Also builds
     docs.
 - On release tag:
   - `github_release.yaml`:
     Creates a draft release on GitHub, builds and tests all combinations of OS
     & build settings, builds docs on all OSes, attaches static release binaries
     to the draft release, then fully publishes the release.
 - On release published:
   - `docker_hub_release.yaml`:
     Builds a Docker image to match the published GitHub release, then pushes it
     to Docker Hub.
   - `npm_release.yaml`:
     Builds an NPM package to match the published GitHub release, then pushes it
     to NPM.
   - `update_docs.yaml`:
     Builds updated docs and pushes them to the gh-pages branch.

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
 - `SHAKA_BOT_TOKEN`: A GitHub personal access token for the `shaka-bot`
   account, with `workflow` scope
   - To generate, visit https://github.com/settings/tokens/new and select the
     `workflow` scope
