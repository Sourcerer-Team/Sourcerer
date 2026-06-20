# Changelog

## [1.0.1] - 2026-06-11

### Bug Fixes

- **Fixed incorrect API endpoints in the release build** — The production build was pointing to wrong API endpoints. `ConfigManager` now correctly resolves API URLs in release mode. ([commit](https://github.com/Sourcerer-Team/Sourcerer/commit/1798d06df435788f8fd73a2e3e67beaaccef7e43))
- **Fixed missing TLS Qt plugin and JWT key on Linux** — The Linux AppImage was missing the Qt TLS plugin required for HTTPS connections, and the JWT public key could not be loaded at runtime. The `build-appimage.sh` script and `Dockerfile.linux` have been updated to bundle these files correctly. ([commit](https://github.com/Sourcerer-Team/Sourcerer/commit/56ddace96c7644af3d3bae92cbca6b0b655d5ada))
- **Fixed missing Ninja in Windows Docker build** — The Windows Docker container was missing the Ninja build tool, causing CMake builds to fail. `Dockerfile.windows` and `build-windows-docker.ps1` have been updated to install Ninja. ([commit](https://github.com/Sourcerer-Team/Sourcerer/commit/5453f42697ddfc6296137fe38ad0d11911f86c62))

### Changes

- **Updated license to a proprietary license** — The software license has changed from the MIT License to the Sourcerer Proprietary Software License Agreement. Binary distribution is permitted for personal or internal business use; reverse engineering, redistribution, and creation of derivative works are prohibited. The updated license is included in both `LICENSE.txt` and `LICENSE.rtf` installer files. ([commit](https://github.com/Sourcerer-Team/Sourcerer/commit/f5c1aa4679247e8ceae9485646c06daf4d01c2d4))
- **Updated installer version to 1.0.1** — The Windows installer (`Sourcerer.wxs`) version has been bumped from `0.2.1` to `1.0.1`. License files are now also bundled in the Linux AppImage. ([commit](https://github.com/Sourcerer-Team/Sourcerer/commit/6df33be0ec39758f8d5a383765486e2560ceb83f))

### Pull Requests

| PR | Title | Author |
|----|-------|--------|
| [#103](https://github.com/Sourcerer-Team/Sourcerer/pull/103) | Updates build version to 1.0.1 | @dezelin |
| [#104](https://github.com/Sourcerer-Team/Sourcerer/pull/104) | Releases version 1.0.1 | @dezelin |

**Full diff**: https://github.com/Sourcerer-Team/Sourcerer/compare/v1.0...v1.0.1

---

## [1.0] - 2026-06-09

Initial release.
