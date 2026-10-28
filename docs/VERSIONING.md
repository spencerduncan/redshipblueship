## About Versions

The Ship of Harkinian's versioning system is a unique system, comprising of a three-numbered version (`x.y.z`) and a version codename.  The main version number `x` signifies major feature updates that require a new `oot.otr` file.  The version number `y` signifies major feature updates and bugfixes that do not require a new `oot.otr` file.  The version number `z` signifies minor updates such as a collection of bugfixes.

The version codename consists of a sci-fi film character followed by a phonetic alphabet code word. The film character represents a major release version which increments with the major `x` or `y` version bumps. The code word represents a minor release version which increments with the minor `z` version bump.

## Cutting a release

A release is cut by pushing a `v*` tag (`generate-builds` builds on `v*` tag pushes and its `create-release` job publishes the assets), or without a tag push through the other two triggers: a `release/vX.Y.Z` branch, or a `workflow_dispatch` of `generate-builds` with `release_tag` set (the run creates the tag at the dispatched commit). The tag must equal `RSBS_VERSION_STRING`.

1. Bump the version on `main` and wait for the batch gate to merge it.
2. Tag the merged commit and push the tag: `git tag vX.Y.Z-<stage> <sha>` then `git push origin vX.Y.Z-<stage>`. The stage word is `RSBS_VERSION_STAGE` in `src/common/rsbs_version.h` (`prealpha` up to `v0.3.1-prealpha`, `alpha` from `v0.4.0-alpha`), and the GitHub Release title names it.
3. Confirm the `generate-builds` run on the tag got past `Git Checkout` and that `create-release` ran.

**Tag type.** Lightweight and annotated (`git tag -a`) tags both work. Annotated tags used to fail every checkout step before any build ran, because `actions/checkout` fetched `<sha>:refs/tags/<tag>` and `fetch-tags: true` fetched the same ref again, and git refuses to fetch a tag object and its commit to one ref (`Cannot fetch both <sha> and refs/tags/<tag>`, #1059, seen on the v0.2.0-prealpha tag push). The workflow now passes `ref: ${{ github.sha }}` to those checkouts on tag pushes, which avoids the duplicate fetch. `fetch-tags` stays on because `CMakeLists.txt` bakes `git describe --tags --exact-match HEAD` into the build (`CMAKE_PROJECT_GIT_COMMIT_TAG`), so the build jobs need the tag present.

If a tag push fails before building for any reason, `workflow_dispatch` of `generate-builds` with `release_tag` set re-runs the whole path from `main` and uploads the assets to the existing release (this is how v0.2.0-prealpha got its assets).
