# Kodi+

Kodi+ is a personal build of Kodi 22 "Piers" for an Nvidia Shield, used with an AV receiver and a
Control4 remote. It installs as `org.xbmc.kodi.plus`, separately from stock Kodi. It isn't an official
Kodi release, and it isn't published as a download.

## Where the code lives

| Code | Location | Visibility |
|---|---|---|
| Kodi+ C++ code | `fork/test-player` in `heit88/xbmc`, based on Kodi's `Piers` | Public |
| Fixes offered to Kodi | `fix/*` in `heit88/xbmc`, based on Kodi's `master` | Public |
| Skin and add-ons | `skin/` in `Shield_control`, branch `feature/trickplay-seek` | Private |

`master` and `Piers` in this repository follow Kodi's own branches. Don't add Kodi+ work to them.

## Features

"Needs" shows which parts a feature uses: **Fork** is C++ in this repository, and **Skin** is the
private skin or its add-ons.

### Audio

| Feature | What it does | Needs | Status |
|---|---|---|---|
| Passthrough after the receiver powers on | Rescans audio devices when the HDMI encodings change, so passthrough returns without a restart | Fork | [PR #30327](https://github.com/xbmc/xbmc/pull/30327) |
| Stable passthrough after a seek | Only syncs on the stream's own codec family, so TrueHD isn't mistaken for E-AC3 | Fork | [PR #30326](https://github.com/xbmc/xbmc/pull/30326) |
| No audio bounce after a rewind | Keeps passthrough muted while sync is measured | Fork | [PR #30332](https://github.com/xbmc/xbmc/pull/30332) |
| Audio device released in the background | Frees the HDMI audio device when Kodi+ is in the background and not playing | Fork | Kodi+ only |
| Theme music | Plays themes in the audio engine, with no player, fades and a volume setting | Fork and Skin | Kodi+ only |

### Video player

| Feature | What it does | Needs | Status |
|---|---|---|---|
| Seek thumbnails | Shows preview images from Jellyfin trickplay folders stored with the media | Fork and Skin | Kodi+ only; possible PR |
| Actual display mode | Adds `System.DisplayMode`, the mode the display is really in | Fork and Skin | Kodi+ only; possible PR |
| Scrub bar | Up shows it; it closes on Down, on Stop or after five seconds | Fork and Skin | Kodi+ only |
| Next-episode card | Up opens it near the end of an episode; it can also open on its own | Skin; Up needs Fork | Kodi+ only |
| Pause under menus | Video pauses while a menu is open and resumes when it closes; music keeps playing | Skin | Skin only |
| Back on Home | Stays on Home instead of reopening the player | Fork | Kodi+ only |

### Games

| Feature | What it does | Needs | Status |
|---|---|---|---|
| 60 Hz for games | Leaves a film rate while a game is on screen, then returns to it | Fork | [PR #30328](https://github.com/xbmc/xbmc/pull/30328) |
| Games tab | Box art, favourites, Continue Playing and rows by system | Skin | Skin only |
| RetroPlayer additions | Run-ahead, bezels and controller battery, from another contributor's PRs | Fork | [#30155](https://github.com/xbmc/xbmc/pull/30155), [#30156](https://github.com/xbmc/xbmc/pull/30156), [#30179](https://github.com/xbmc/xbmc/pull/30179) |

### Home automation and library

| Feature | What it does | Needs | Status |
|---|---|---|---|
| MQTT commands | Runs commands from the `kodi/command` topic, for remote buttons that can't reach Kodi | Skin | Skin only |
| Problem reports | Saves a report of what was playing, from the player or over MQTT | Skin | Skin only |
| Credits chapters | Adds a Credits chapter to MKV files, with backups for restoring them | `automation/` in `Shield_control` | Private script |

### Appearance

| Feature | What it does | Needs | Status |
|---|---|---|---|
| Cinema Lite | A light skin based on Estuary, with Now Playing and Settings tabs | Skin | Private |
| Branding | Kodi+ name, gold logo, banner and launcher icons | Fork | Kodi+ only |

## Building

Kodi+ follows Kodi's Android build, described in [README.Android.md](README.Android.md), with one extra
CMake option that sets the package name:

```sh
make -C tools/depends/target/cmakebuildsys BUILD_DIR=<build dir> \
  CMAKE_EXTRA_ARGUMENTS="-DAPP_PACKAGE=org.xbmc.kodi.plus"
make -C <build dir> -j4
make -C <build dir> apk
```

Debug and release builds need separate dependency folders. Use `--disable-debug` when configuring
`tools/depends` for a release build.

An update only installs over the existing app if it's signed with the same key. Changing the key means
uninstalling first, which removes the profile stored with the app.

## Adding to Kodi+

| Change | Where it goes |
|---|---|
| A bug in Kodi itself | A new `fix/*` branch from Kodi's `master`, plus a PR to `xbmc/xbmc` |
| A feature only Kodi+ needs | A commit on `fork/test-player`; prefix the subject with `[fork]` if it should never go upstream |
| Anything in the skin or its add-ons | `Shield_control` |

Fixes on `fork/test-player` can be dropped once the same change is in Kodi's `Piers` branch. Follow
[CODE_GUIDELINES.md](CODE_GUIDELINES.md) and [CONTRIBUTING.md](CONTRIBUTING.md) for anything going
upstream, and update the tables above when a feature or PR changes.

## Before publishing

Kodi+ is private for now. These need resolving before any public release:

- **Name and logo:** "Kodi" and its logo are trademarks of the XBMC Foundation, and a public build may need a new name and logo.
- **License:** Kodi is GPL, so a shared APK needs its matching source code available.
- **Skin repository:** the skin needs its own public repository, without the personal setup in `Shield_control`.
- **Signing:** a public APK needs its own signing key rather than the debug key.
