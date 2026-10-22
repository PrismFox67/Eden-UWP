# Upstream candidates (Jadyx pull requests)

Improvements we want to offer back to [Jadyx](https://github.com/JulianDr14/jadyx) once they are proven.
Rules (user, 2026-10-22, swarm ticket #200):

1. One small, focused topic per branch `upstream/<topic>`, rebased on `jadyx/main`. Nothing Eden-UWP-specific
   in it (package identity, README, our CI, `boot.cfg` plumbing stay in separate commits on `main`).
2. Commit messages in English in Jadyx's style: `type(scope): what` (`perf(d3d12): ...`, `fix(jit): ...`,
   `docs(xbox): ...`, `test(d3d12): ...`), body says why.
3. Tested on the console before it is offered (Jadyx CONTRIBUTING: a PC run is not proof that it works on the
   Series). Record PC and console numbers below. No game screenshots in commits.
4. PRs go from a fork of Jadyx under PrismFox67 (Eden-UWP is not in its fork network). The interface creates the
   fork and opens each PR only after the user approves its text. PR text discloses AI assistance.

Eden itself (not Jadyx): `docs/policies/AI.md` requires disclosure and that commit messages and PR descriptions
are written by the human, so any Eden PR needs the user's own wording.

## Candidates

| Topic | Branch | Our commits | Evidence (PC / console) | Status |
|---|---|---|---|---|
| RWX JIT code memory with automatic W^X fallback, logged mode | `upstream/jit-rwx` (not made yet) | 99ccf4cf2c + build fix 0414022975 | PC: builds (VS 2026, 2026-10-22); console: pending QA A/B | waiting for console numbers |
| Readback buffers in a write-combined custom heap (Magnesis readback cost) | `upstream/readback-uncached` (not made yet) | eadd1e25b8 (D3D12 part only; the `boot.cfg` switch stays ours) | none yet | waiting for PC + console numbers |

Not candidates (Eden-UWP-specific): `LocalState\boot.cfg` override (0c125f29b2), package identity, ci-latest CI.

When a candidate is proven: cherry-pick its code onto `jadyx/main` as `upstream/<topic>`, squash our fix-ups,
rewrite the message in Jadyx style, put the setting behind Jadyx's own config (not our `boot.cfg`), build, then
file an `agentctl ask` with the draft PR text for the user.
