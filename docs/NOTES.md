# Notes: Eden on Xbox (UWP)

Decisions, open questions, and what has been verified on hardware (with build and date). Newest first.

## Decisions
- 2026-10-22 (owner): build on Jadyx. Branch `main` starts at jadyx/main (a46b3e75b) with our docs on top; the
  first docs commits were made on `xbox` (clean Eden master) and cherry-picked. Steps revised in PLAN.md.
- 2026-10-22 (owner): published as the public repo PrismFox67/Eden-UWP; the owner pushes. App name
  "Eden UWP (unofficial)", package identity distinct from Jadyx's so both install side by side.
- 2026-10-22 (owner): the owner dumps their own keys, firmware and games from their Switch; we never download or
  reference sources for them.
- 2026-10-22: Repo set up locally from git.eden-emu.dev/eden-emu/eden (master at
  989768aa4). Fetch-only reference remotes: `jadyx` (JulianDr14/jadyx), `nxbox` (NspxMiguel/NXbox), `juan`
  (juanresendiz813/eden-xbox), `mirror` (eden-emulator/mirror).

## Open questions (for the owner)
- (none open)

## Verified on hardware
- (nothing yet for this project; Cemu facts are in CEMU_LESSONS.md)
