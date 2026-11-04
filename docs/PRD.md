# Product requirements — Layouts for OBS

**Tagline:** Design your scenes. Pixel perfectly.

## Vision
Give OBS users the composition tools of a design application — a permanent properties panel,
rulers, guides, grids, smart snapping, alignment and measurements — natively inside OBS Studio,
without touching the video output and without replacing OBS.

## Problem
OBS can move and transform sources, but precise layout work is slow: positions are approximate,
the transform window is modal, aligning or distributing several sources is manual, there are no
rulers or guides, snapping is not configurable enough and spacing cannot be measured.

## Personas
- **Streamer / creator** building overlays and scene layouts once, then tweaking them.
- **Broadcast operator** preparing shows with precise safe areas and consistent positions.
- **Motion / graphic designer** used to Figma who sets up OBS scenes for a team.

## Goals (0.1.0)
1. Read and edit any scene item numerically, with math, in a permanent panel.
2. Align, distribute and space several items in one click, with one undo entry.
3. Compose visually with rulers, guides, layout grids, smart snapping and measurements.
4. Be safe during a live show.
5. Public OBS APIs only, OBS 33+, macOS, Windows and Linux builds.

## Non-goals
Replacing the OBS renderer, preview, Sources dock or transitions; vector drawing; components;
auto layout, constraints and presets (planned for 0.2); per-item opacity; editing collection files.

## User stories and priorities

| # | Story | Priority | Status 0.1.0 |
|---|---|---|---|
| 1 | Select a source and read its position, size, rotation, crop | P0 | done |
| 2 | Type X/Y/W/H/rotation/crop, with math and relative values | P0 | done |
| 3 | Lock the ratio when resizing | P0 | done |
| 4 | Choose the reference point of X/Y | P0 | done |
| 5 | Fit, fill, stretch, original size, reset | P0 | done |
| 6 | Align several sources to the selection, the canvas or a key object | P0 | done |
| 7 | Distribute centers or spacing, set an exact gap, make sizes equal | P0 | done |
| 8 | See mixed values on a multi-selection | P0 | done |
| 9 | Undo any Layouts change with OBS's Edit > Undo | P0 | done |
| 10 | Rulers in canvas pixels, adaptive to the zoom | P0 | done |
| 11 | Create, move, lock, delete guides; per scene or global; saved | P0 | done |
| 12 | Snap to guides, grid, elements, equal spacing, canvas | P0 | done |
| 13 | Square, column and row grids | P1 | done |
| 14 | Measure distances (Alt) | P1 | done |
| 15 | Smart guides while moving | P1 | done |
| 16 | Scrub values by dragging labels | P1 | done |
| 17 | Copy / paste selected properties | P1 | done |
| 18 | Safe areas and custom margins | P1 | done |
| 19 | Zoom (fit, 100 %, selection), pan | P1 | done |
| 20 | Edit scenes of any OBS 33 canvas (vertical...) | P1 | done |
| 21 | Banner when editing a scene on air, optional blocking | P0 | done |
| 22 | Auto layout, constraints, presets | P2 | 0.2 |

## Acceptance criteria
See [TEST-PLAN.md](TEST-PLAN.md): every criterion of the original brief is mapped to an automated
or manual test, with the 0.1.0 result.

## Decisions
1. Hybrid architecture, public APIs only. 2. OBS 33+. 3. Live: banner by default, blocking as an
option. 4. W/H = visible size (Figma). 5. Guides per scene + global. 6. No opacity. 7. Shortcuts
follow OBS where they overlap.
