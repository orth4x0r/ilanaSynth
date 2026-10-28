# Vendored copy

Impeccable 4.8.0 by Paul Bakaus (Apache 2.0, see LICENSE and NOTICE.md), copied unchanged from
https://github.com/pbakaus/impeccable/tree/5adaee704634e9031ec8943721c4ffa1a3c22221/.claude/skills/impeccable
on 2026-10-28, so every Claude session in this repo can run `/impeccable`.

For ilanaSynth, PRODUCT.md and DESIGN.md at the repo root give it the product and the design system.
ilanaSynth is a JUCE plugin, not a web page: skip `impeccable detect`, `live` and the web-only checks,
and judge the UI from `ilanaSnapshot` PNGs at `ILANA_SNAPSHOT_SCALE=1.75`. ilana's rules in HANDOFF.md
and the project memory win over the skill (one-line scope ok before any UI change, no subagents, her
typed "go" for every merge).

To update: copy the folder again from upstream and keep this file, LICENSE and NOTICE.md.
