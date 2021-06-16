# GitHub Actions CI

## Workflows
 - On PR:
   - `build_and_test.yaml`: builds and tests all combinations of OS & build
     settings
   - `update_docs.yaml`: builds updated docs
 - On release tag:
   - `draft_github_release.yaml`: creates a draft release on GitHub, triggers
     common `build_and_test` workflow
   - `build_and_test.yaml` builds and tests all combinations of OS & build
     settings, attaches official binaries to the GitHub draft release, triggers
     `publish_github_release` workflow
   - `publish_github_release.yaml`: finalizes the draft and published the GitHub
     release
   - `docker_hub_release.yaml`: builds a Docker image to match the final GitHub
     release and pushes it to Docker Hub
   - `npm_release.yaml`: builds an NPM package to match the final GitHub release
     and pushes it to NPM
   - `update_docs.yaml`: builds updated docs, pushes them to the gh-pages branch

## Required Repo Secrets
 - `DOCKERHUB_CI_USERNAME`: The username of the Docker Hub CI account
 - `DOCKERHUB_CI_TOKEN`: An access token for Docker Hub
   - To generate, visit https://hub.docker.com/settings/security
 - `NPM_CI_TOKEN`: An "Automation"-type access token for NPM for the `shaka-bot`
   account
   - To generate, visit https://www.npmjs.com/settings/shaka-bot/tokens and
     select the "Automation" type
 - `SHAKA_BOT_TOKEN`: A GitHub personal access token for the `shaka-bot`
   account, with `workflow` scope
   - To generate, visit https://github.com/settings/tokens/new and select the
     `workflow` scope
