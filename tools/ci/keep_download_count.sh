#!/bin/bash
# SPDX-FileCopyrightText: Copyright 2026 Eden-UWP contributors
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Keeps a running download total for the ci-latest files (idea from Cemu-UWP's tools/ci). GitHub resets a file's
# download_count when it is replaced or deleted, so call this right before CI replaces the ci-latest assets: it adds
# the old asset's count to a total kept in the "download-stats" release as <key>.count.json =
# {"base": downloads of earlier builds, "counted_id": id of the asset already added, "counted": its count then}.
# The key drops the version, so EdenUWP_0.3.8.12_x64.appx and EdenUWP_0.3.8.13_x64.appx share EdenUWP_x64.appx.
# A rerun before the replacement happened (same asset id) re-counts instead of adding twice.
#   usage: tools/ci/keep_download_count.sh <asset name>   (needs GH_TOKEN with contents: write, GITHUB_REPOSITORY)
#          tools/ci/keep_download_count.sh --show         (prints the running totals)
#          tools/ci/keep_download_count.sh --json         (also writes totals.json and the "Downloads so far" line)
set -u
STATS=download-stats
REL=ci-latest
GITHUB_REPOSITORY=${GITHUB_REPOSITORY:-$(gh repo view --json nameWithOwner --jq .nameWithOwner)}

gh release view $STATS >/dev/null 2>&1 || gh release create $STATS --prerelease --title "Download stats" \
  --notes "Running download totals of the ci-latest builds (GitHub resets a file's count when CI replaces it). Not a build." >/dev/null 2>&1 || true

key() { sed -E 's/_[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+_/_/' <<<"$1"; }
field() { sed -n "s/.*\"$2\": *\([0-9]*\).*/\1/p" <<<"$1"; }

read_stats() {   # key -> json text (or defaults)
  local tmp; tmp=$(mktemp -d)
  if gh release download $STATS -p "$1.count.json" -D "$tmp" >/dev/null 2>&1; then cat "$tmp/$1.count.json"; else echo '{"base": 0, "counted_id": 0, "counted": 0}'; fi
  rm -rf "$tmp"
}

current() {      # asset -> "id count" of the asset now on ci-latest (empty if none)
  gh api "repos/$GITHUB_REPOSITORY/releases/tags/$REL" --jq ".assets[] | select(.name == \"$1\") | \"\(.id) \(.download_count)\"" 2>/dev/null
}

if [ "${1:-}" = "--show" ] || [ "${1:-}" = "--json" ]; then
  json="{"; notes=""
  for a in $(gh api "repos/$GITHUB_REPOSITORY/releases/tags/$REL" --jq '.assets[].name'); do
    k=$(key "$a")
    s=$(read_stats "$k"); base=$(field "$s" base); cid=$(field "$s" counted_id); cnt=$(field "$s" counted)
    read -r id n <<<"$(current "$a")"
    [ "${id:-0}" = "$cid" ] && n=$(( ${n:-0} - cnt ))
    total=$(( base + ${n:-0} ))
    echo "$k: $total total"
    json="$json\"$k\": $total, "
    notes="$notes$k: $total, "
  done
  if [ "$1" = "--json" ]; then
    xbox=$(sed -n 's/.*"EdenUWP_x64.appx": \([0-9]*\).*/\1/p' <<<"$json")
    tmp=$(mktemp -d)
    printf '%s"xbox": %s, "updated": "%s"}\n' "$json" "${xbox:-0}" "$(date -u +%Y-%m-%dT%H:%MZ)" > "$tmp/totals.json"
    gh release upload $STATS "$tmp/totals.json" --clobber >/dev/null 2>&1 || true
    rm -rf "$tmp"
    body=$(gh release view $REL --json body --jq .body | sed '/^Downloads so far/d')
    gh release edit $REL --notes "$(printf '%s\n\nDownloads so far (all CI builds): %s updated %s' "$body" "${notes%, }" "$(date -u +%Y-%m-%d\ %H:%M)")" >/dev/null 2>&1 || true
  fi
  exit 0
fi

A=$1
K=$(key "$A")
read -r id n <<<"$(current "$A")"
[ -z "${id:-}" ] && exit 0   # nothing to count yet
s=$(read_stats "$K"); base=$(field "$s" base); cid=$(field "$s" counted_id); cnt=$(field "$s" counted)
if [ "$id" = "$cid" ]; then base=$(( base - cnt + n )); else base=$(( base + n )); fi
tmp=$(mktemp -d)
printf '{"base": %d, "counted_id": %d, "counted": %d}\n' "$base" "$id" "$n" > "$tmp/$K.count.json"
for i in 1 2 3; do gh release upload $STATS "$tmp/$K.count.json" --clobber >/dev/null 2>&1 && break; sleep 10; done
rm -rf "$tmp"
echo "download total for $K: $base (before this build)"
