# Release plan

## Versioning
Semantic versioning. `buildspec.json` holds the version; a tag `x.y.z` (or `x.y.z-beta1`) on `main`
triggers the release workflow of the OBS plugin template, which builds, packages and creates a
**draft** GitHub release. A maintainer reviews and publishes it.

## Compatibility
| | Status |
|---|---|
| OBS Studio | 33.0.1 and later (uses the OBS 33 canvas API); older versions are not supported |
| macOS 13+ (Apple Silicon, Intel, universal binary) | built and tested (Apple Silicon) |
| Windows 10/11 x64 | built by CI, not tested at runtime |
| Ubuntu 26.04 x86_64 (needs the obs-studio package 33+) | built by CI, not tested at runtime |

## Packages
| Platform | Artifact | Install |
|---|---|---|
| macOS | `.pkg` (installs to `~/Library/Application Support/obs-studio/plugins`) | signed (Developer ID) and notarized |
| Windows | `.zip` | extract into `C:\ProgramData\obs-studio\plugins` |
| Linux | `.deb` (depends on `obs-studio >= 33`) | `sudo apt install ./obs-layouts-*.deb` |

The macOS package is signed with a Developer ID certificate and notarized on the maintainer's Mac,
then replaces the unsigned package of the CI draft (checksums updated). The CI is ready for
signing secrets (template variables) if signing moves there. Windows and Linux packages are not signed.

## Checklist
1. Unit tests, in-OBS self-test and the regression checklist of [TEST-PLAN.md](TEST-PLAN.md) green.
2. `buildspec.json` and `manifest.json` versions updated, `CHANGELOG.md` entry written.
3. Release build configured **without** `LAYOUTS_SELFTEST`.
4. Tag, wait for CI, check the three artifacts download and install.
5. Edit the draft release notes, then publish.

## Distribution later
OBS forum resource page, then the OBS 32+ plugin manager metadata (`manifest.json`) once stable.
