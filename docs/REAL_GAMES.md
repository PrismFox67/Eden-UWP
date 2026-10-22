# Real-game profiling on the Xbox (procedure)

For the evening the owner has dumped their own keys, firmware and games from their own Switch
(owner decision 2026-10-22). This page only covers where those files go and what to measure; it
never says where to get them. Nothing from this page goes into git (keys, firmware, games, logs
with title data stay on the console and in the local `Logs/` folder).

## 1. Getting the files onto the console

Install the current `ci-latest` package (appx + VCLibs + cer, Device Portal -> Add, Game mode).
Then, with the app open on the library screen, either:

- **Jadyx Portal** (Settings > Network > portal; open the shown address from a PC or phone on the
  same network): upload keys / firmware ZIP in the Keys and Firmware pages, games in Library.
  See docs/xbox/xbox_network_portal.md.
- **USB drive**: games in any folder on an exFAT/NTFS drive, added from the library's import tile
  (USB direct mode keeps them on the drive; see docs/xbox/xbox_content.md).

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
