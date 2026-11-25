# Stage Startpos

A [Geode](https://geode-sdk.org) mod for Geometry Dash 2.2081 (Windows) that shows the percent your current stage run has to reach next to the percentage, like `44.27%/74.27%`.

It works with Zhulis's stage practice mod, which Geode installs with it. See [about.md](about.md) for everything it does and [changelog.md](changelog.md) for what changed.

## Install

Download `dan2756.stage-startpos.geode` from the [latest release](https://github.com/dan2756/stage-startpos/releases/latest) and put it in Geometry Dash's `geode\mods` folder, then restart the game.

## Build

Every push is built by GitHub Actions with Geode's [build action](https://github.com/geode-sdk/build-geode-mod). Pushing a tag like `v1.1.1` also publishes a release with the `.geode` attached.

Locally, `build.ps1` builds with Visual Studio's CMake and Ninja and the Geode SDK in `%USERPROFILE%\Geode`.
