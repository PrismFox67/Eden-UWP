# Real-game profiling on the Xbox (procedure)

For the evening the owner has dumped their own keys, firmware and games from their own Switch
(owner decision 2026-10-22). This page only covers where those files go and what to measure; it
never says where to get them. Nothing from this page goes into git (keys, firmware, games, logs
with title data stay on the console and in the local `Logs/` folder).

## 1. Getting the files onto the console

Install the current `ci-latest` package (appx + VCLibs + cer, Device Portal -> Add, Game mode).
Then:

- **Keys and firmware**: from a USB drive, with the app's own importer (the library's system
  import, `jadyx/content` + `system_import`). The portal's web upload for keys/firmware was only a
  candidate branch on 2026-10-19 (docs/xbox/xbox_network_portal.md); check the build before
  relying on it.
- **Games**: the Jadyx Portal (game upload over the LAN after pairing, docs/xbox/xbox_network_portal.md),
  or a USB drive through the library's import tile (USB direct mode keeps them on the drive,
  docs/xbox/xbox_content.md).

The exact menu names are in the app; this page was written without a console session, so QA notes
the real path on the ticket the first time.

Check: the game shows in the library with its cover and name; the log says nothing about missing
keys (`jadyx_log.txt`, grep `key`).

## 2. Which games first

1. **Mario Kart 8 Deluxe** - README reports 60 FPS stable on Series X: the known-good baseline.
   Profile a full race.
2. **Zelda: Breath of the Wild** - the heavy one (PORTING.md ranks per-draw CPU as its limit):
   profile the Great Plateau outdoors, 2 minutes of walking.
3. Optional: **Super Mario Odyssey** (README: 45-50 FPS in open areas) - Cascade Kingdom.

## 3. What to run and collect (per game)

`LocalState\boot.cfg` (Device Portal file explorer), one session per line set:

```
profile=1
profile_window=20
```

Play the scene for ~2 minutes, close the game from the in-game menu (back to the library), then
pull from `LocalState\jadyx\log`: `jadyx_profile.txt`, `jadyx_log.txt`, `jadyx_diag.txt`.
On the PC: `python tools/xbox/profile-report.py jadyx_profile.txt > <game>_report.md`.

Post on the QA ticket: FPS range seen, the report's "CPU time per thread" table and the GPU
thread's phase table, and any `CRASH` / last-session notice. Then repeat once with
`timer_wait=coarse` added (one switch at a time) for the A/B.

Compare against docs/PERF_DRAW.md (PC, draw_stress): GPU-thread us/draw = GPU ms/s / draws/s.
