#!/bin/bash
# SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Writes the ci-latest release notes (idea from Cemu-UWP's tools/ci): the newest sections of CHANGELOG.md and every
# commit since the previous build (kept in the notes as an HTML comment). Run by CI after the uploads; needs GH_TOKEN
# (contents: write), GITHUB_REPOSITORY, GITHUB_SHA and the package version as $1. The "Downloads so far" line that
# keep_download_count.sh --json maintains is kept.
set -u
REL=ci-latest
R=${GITHUB_REPOSITORY:-$(gh repo view --json nameWithOwner --jq .nameWithOwner)}
SHA=${GITHUB_SHA:-$(git rev-parse HEAD)}
SHORT=${SHA::7}
PKG=${1:-?}
BLOB="https://github.com/$R/blob/main"

body=$(gh release view $REL --json body --jq .body 2>/dev/null || true)
prev=$(sed -n 's/.*<!-- build-sha: \([0-9a-f]*\) -->.*/\1/p' <<<"$body" | head -n 1)
downloads=$(grep '^Downloads so far' <<<"$body" | tail -n 1)

# the newest two sections of CHANGELOG.md, headings one level down so they nest under "What's new"
whatsnew=$(awk '/^## /{n++} n>=1 && n<=2' CHANGELOG.md | sed 's/^###/####/; s/^## /### /')

commits=""
if [ -n "$prev" ] && [ "$prev" != "$SHA" ]; then
  commits=$(gh api "repos/$R/compare/$prev...$SHA" --paginate \
    --jq '.commits[] | "- " + (.commit.message | split("\n")[0]) + " (" + .sha[0:7] + ")"' 2>/dev/null || true)
fi
if [ -n "$commits" ]; then
  since="## Commits since the previous build (${prev::7} → $SHORT)
$commits"
elif [ -n "$prev" ] && [ "$prev" = "$SHA" ]; then
  since="## Commits since the previous build
This build is a rerun of the same commit."
else
  since="## Commits since the previous build
See the [commit history](https://github.com/$R/commits/main)."
fi

notes="Automatic Xbox (UWP) build of Eden-UWP, refreshed on every push to \`main\`. Untested between releases - for testers. Not an official Eden or Jadyx release. No keys, firmware or games are included.

**Build:** \`$SHORT\` (package $PKG) · [Full changelog]($BLOB/CHANGELOG.md)

| File | What it is |
|---|---|
| \`EdenUWP_${PKG}_x64.appx\` | **The Xbox app.** Install it through the Xbox Device Portal (\`https://<xbox-ip>:11443\`), as an update over the previous build. |
| \`Microsoft.VCLibs.x64.14.00.appx\` | Dependency: add it in the same install the first time. |
| \`EdenUWP.cer\` | The signing certificate, only needed to install on a Windows PC. |
| \`EdenUWP_${PKG}_symbols.zip\` | Debug symbols, for crash reports. |
| \`SHA256SUMS.txt\`, \`build-info.json\` | Checksums and the commit/run this build came from. |

## What's new
$whatsnew

$since

<!-- build-sha: $SHA -->
${downloads:+
$downloads}"

gh release edit $REL --notes "$notes" && echo "release notes updated for $SHORT (previous build ${prev:-none})"
