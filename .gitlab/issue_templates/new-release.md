<!--
This template is for tracking a release of VTK. Please replace the
following strings with the associated values:

  - `@VERSION@` - replace with base version, e.g., 9.1.0
  - `@RC@` - for release candidates, replace with ".rc?". For final, replace with "".
  - `@MAJOR@` - replace with major version number
  - `@MINOR@` - replace with minor version number
  - `@PATCH@` - replace with patch version number
  - `@BASEBRANCH@`: The branch to create the release on (for `x.y.0.rc1`,
    `master`, otherwise `release`)
  - `@BRANCHPOINT@`: The commit where the release should be started

Please remove this comment.
-->

# Update VTK

  - Update the local copy of `@BASEBRANCH@`.
    - If `@PATCH@@RC@` is `0.rc1`, update `master`
    - Otherwise, update `release`
```
git fetch origin
git checkout @BASEBRANCH@
git merge --ff-only origin/@BASEBRANCH@ # if this fails, there are local commits that need to be removed
git submodule update --recursive --init
```
  - If `@BASEBRANCH@` is not `master`, ensure merge requests which should be
    in the release have been merged. The [`backport-mrs.py`][backport-mrs]
    script can be used to find and ensure that merge requests assigned to the
    associated milestone are available on the `release` branch.

  - Make a commit for each of these changes on a single topic (suggested branch
    name: `update-to-v@VERSION@`):
    - [ ] Move release note files to `Documentation/release/@MAJOR@.@MINOR@` folder.
    - [ ] If `@BASEBRANCH@` is `master`, update the non-patch version in a
          separate commit (so that `master` gets it as well).
    - [ ] If `@BASEBRANCH@` is `master`, Update `.gitlab/ci/cdash-groups.json` to track the `release` CDash
          groups
    - [ ] Update `CMake/vtkVersion.cmake` and tag the commit (tag this commit below)
```
$EDITOR CMake/vtkVersion.cmake
git commit -m 'Update version number to @VERSION@@RC@' CMake/vtkVersion.cmake
```
  - Create a merge request targeting `release`
    - [ ] Obtain a GitLab API token for the `kwrobot.release.vtk` user (ask
          @utils/maintainers/release if you do not have one)
    - [ ] Add the `kwrobot.release.vtk` user to your fork with at least
          `Developer` privileges (so it can open MRs)
    - [ ] Use [the `release-mr`][release-mr] script to open the create the
          Merge Request (see script for usage)
      - Pull the script for each release; it may have been updated since it
        was last used
      - The script outputs the information it will be using to create the
        merge request. Please verify that it is all correct before creating
        the merge request. See usage at the top of the script to provide
        information that is either missing or incorrect (e.g., if its data
        extraction heuristics fail).
    - [ ] Get positive review
    - [ ] `Do: merge`
    - [ ] If `@BASEBRANCH@` is `master` and `@PATCH@ == 0`, note the date this merge occurs, this is the SPLIT_DATE.
      - It can be recovered easily if needed by running `git log --first-parent --reverse origin/master '^origin/release'`
    - [ ] Push the tag to the main repository
      - [ ] `git tag -a -m 'VTK @VERSION@@RC@' v@VERSION@@RC@ commit-that-updated-vtkVersion.cmake`
      - [ ] `git push origin v@VERSION@@RC@`
  - [ ] If `@RC@` is `""`, Update `vtk.org/download` with the new release
      - Hashes can be found in the output of the `release-artifacts:upload` job
      - [ ] email `marketing@kitware.com` with filenames and hashes
  - If `@PATCH@ == 0` Software process updates **these can all be done independently**
    - [ ] Update kwrobot with the new `release` branch rules (@utils/maintainers/ghostflow)
    - [ ] Run [this script][cdash-update-groups] to update the CDash groups
      - This must be done after a nightly run to ensure all builds are in the `release` group
      - See the script itself for usage documentation
    - Deprecation updates (if `@BASEBRANCH@` is `master`)
      - This should be done as soon as possible after merging to not block development
      - [ ] Update deprecation macros for the next release, use `VTK_VERSION_CHECK(@MAJOR@, @MINOR@, SPLIT_DATE)`
      - [ ] Update `VTK_EPOCH_VERSION` to the day *after* the SPLIT_DATE
      - [ ] Remove deprecated symbols from before the *prior* release
      - [ ] Update `VTK_MINIMUM_DEPRECATION_LEVEL` to be that of the *prior* release
    - Assemble release notes from `Documentation/release/@MAJOR@.@MINOR@` into `Documentation/release/@MAJOR@.@MINOR@.md`.
      - This can be done during the release cycles but must be done before the full release
      - [ ] Recover new .md files as they are being added during release cycles and incorporate them into a coherent release note.
      - [ ] If `PATCH` is greater than 0, add items to the end of `@MAJOR@.@MINOR@.md` file.

[backport-mrs]: https://gitlab.kitware.com/utils/release-utils/-/blob/master/backport-mrs.py
[release-mr]: https://gitlab.kitware.com/utils/release-utils/-/blob/master/release-mr.py
[cdash-update-groups]: https://gitlab.kitware.com/utils/cdash-utils/-/blob/master/cdash-update-groups.py

# Post-release

  - [ ] Post an announcement in the Announcements category on
        [discourse.vtk.org](https://discourse.vtk.org/).

/cc @ben.boeckel
/cc @berkgeveci
/cc @vbolea
/cc @sankhesh
/cc @mwestphal
/milestone %"@VERSION@@RC@"
/label ~"priority:required"
