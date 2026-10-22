<!--
SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Changelog

What changed for players and testers, newest first. Builds of `main` appear as the
[ci-latest](https://github.com/PrismFox67/Eden-UWP/releases/tag/ci-latest) pre-release; the newest two
sections below go into its release notes. Eden-UWP is built on [Jadyx](https://github.com/JulianDr14/jadyx)
and [Eden](https://git.eden-emu.dev/eden-emu/eden); their own changes are in their histories.

## Unreleased

### For testers
- Automatic test builds: every change to `main` is published as the `ci-latest` pre-release, with the
  appx, the VCLibs dependency, the certificate and debug symbols. Each build installs as an update over
  the previous one.
- Experimental switches in `LocalState\boot.cfg` for A/B tests on the console: `jit_rwx=1` (faster CPU
  code memory, falls back automatically if the console refuses it) and `readback_uncached`. The log says
  which mode is active. Leave them off unless a test asks for them.
- A tiny open-source test program (`boot.nro`) is built by CI for the first-boot check on the console.

### Changes
- Own package identity (`EdenUWP.Unofficial`), so it installs next to Jadyx instead of replacing it.
  The built-in updater is off for now.

## 2026-10-22 - start of the fork

- Eden-UWP forked from Jadyx (Eden on Xbox with a Direct3D 12 renderer).
