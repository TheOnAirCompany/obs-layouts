# Test plan

## 1. Unit tests (core, no OBS)

```sh
cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests
```

69 cases (0.2 branch), run on every push to `main` and every pull request by CI on Linux, macOS and Windows:

| Area | Covered |
|---|---|
| Geometry | affine composition and inversion, clockwise rotation, angle normalization, rects, rotated quad hit testing, reference points |
| Item model | box for alignments, rotation, crop, bounds, flips; move to a reference point; resize keeping the anchor and flips; rotated resize; bounds resize; rotation and flip keep the center; crop keeps the content; crop never removes the source; fit / fill / stretch; original size; bounds switching both ways; group parents; zero-sized sources |
| Arrange | align to selection, canvas, key object; locked items; distribute centers; distribute spacing; minimum counts; exact spacing in spatial order; uniform gap detection |
| Snapping | canvas edges and center; threshold; tier priority (guide > grid > item > canvas); disabled; centered between neighbours; same gap as neighbours; single value snapping; measurements (gap and containment) |
| Grid & rulers | 1-2-5 ticks per zoom, subdivisions, square lines and nearest line, column tracks stretch / fixed / invalid |
| Expressions | arithmetic, precedence, parentheses, comma decimals, relative operators, `=` absolute, percentages, units, every error path (empty, letters, division by zero, unbalanced, overflow, depth, length) |
| Layout (0.2) | selection scaling, fit / fill / stretch in a cell, horizontal / vertical / grid flow (order, gap, padding, alignment, fill sizing), presets (corner, split screen, picture in picture, 2 × 2), constraints (pins, stretch, center, scale, none) and OBS's relative mapping round trip |
| Guides | add / move / lock / remove, invalid positions, scene ↔ global, records round trip, v0 migration, rejected entries, future versions, adopt by name, orphan pruning |

## 2. In-OBS self-test

Build with `-DLAYOUTS_SELFTEST=ON`, start OBS with `LAYOUTS_SELFTEST=1` on a **disposable** OBS
configuration (never a production one). It creates its own scene collections, logs
`[selftest] PASS|FAIL name detail` and, with `LAYOUTS_SELFTEST_QUIT=1`, quits OBS at the end.
`LAYOUTS_SELFTEST_ONLY=core,window,split,collections,output,regie,corners,live,canvas,stacks,perf`
selects groups.

Isolated instance on macOS (the user's own OBS keeps running):

```sh
CFFIXED_USER_HOME=/tmp/obs-test-home OBS_PLUGINS_PATH=/tmp/obs-test-plugins \
  "/Applications/OBS.app/Contents/MacOS/OBS" --multi --disable-updater --disable-missing-files-check
```

`OBS_PLUGINS_PATH` loads Layouts from a scratch folder, so the real plugins folder is untouched.

| Group | Checks |
|---|---|
| core | geometry fidelity vs libobs on 10 transforms; align, distribute, exact spacing, same size with key object; X with math, relative X, X at the bottom-right reference point, W with ratio lock; rotate keeps center; crop keeps content; fit; paste position + size; OBS *Undo* / *Redo*; a mouse drag in the Canvas = one undo entry; drag snaps to a guide; ruler drag creates a guide; group child positioned in world coordinates |
| collections | guides saved in the collection, empty in a new collection, restored when switching back (scene and global) |
| output | grid, guides, safe areas and center lines visible; OBS output screenshot has 0 differing pixels |
| live (0.7) | Studio Mode duplicate is *Off*; program without output: no chip; program while recording: still not on air, edits allowed with blocking on; streaming to a local RTMP listener (ffmpeg `-listen 1` on 127.0.0.1:19350): chip shown, blocking refuses an edit, allowing applies it, another scene is not on air, chip gone after the stream; vertical canvas: its program scene shows no chip until an `rtmp_output` on that canvas video (127.0.0.1:19351) runs, then the chip shows while the main program is not on air. Skipped (logged) without the listeners. The virtual camera paths are covered by the unit tests only |
| split (0.7) | Scene Editor and Scene Builder opened from their Tools actions, both visible at once, titles; no mode switch, no scene menu; navigator above Layers; Scene Builder ↗ gateway; both hotkeys; navigator lists the main and the vertical canvas (Vertical for OBS loaded, else a test canvas), search and 16:9 / 9:16 filters, a click edits a vertical scene without touching the program, breadcrumb `9:16 › …`, "+" makes a scene in that canvas, pin on and off; Builder creates in 16:9, 9:16 (canvas of Vertical for OBS) and Both (twin in the vertical canvas), Open in Scene Editor |
| stacks (0.7) | with Stacks for OBS loaded and a prepared collection (stack "Plateau" holding the favorite "Duo"): the navigator shows the stack, the ★ filter, "Put in stack" lists the stacks and files a new scene into "Plateau" |
| canvas | OBS 33 additional canvas 1080×1920: canvas size, align right/bottom in that canvas |
| dock | the Canvas keeps rendering after being docked and floated again |
| core (0.2) | auto layout horizontal and grid, preview shown and cleared, hovering the presets section previews, corner and split-screen presets, mouse-driven multi-selection resize keeps the ratio, crop mode drag crops 50 source px with the image in place, hotkeys (toggle grid, align left) |
| canvas (0.2) | constraints on an extra canvas reset from 1920×1080 to 1280×720: right/bottom keeps its margins, left-and-right/top stretches, unconstrained follows OBS's own mapping |
| perf | 10 / 50 / 100 / 250 items: Canvas draw time, OBS frame time, lagged frames, no rendering when hidden |

Result on the 0.2 branch, complete suite: **77 checks, 0 failures, no crash**.

Result for 0.1.0 on macOS 27 / Apple Silicon / OBS 33.0.1, complete suite on the final code:
**60 checks, 0 failures, no crash** (the Studio Mode duplicate check runs only when Studio Mode is
on at the start; it passed in the earlier runs).

## 3. Manual tests with real input

Driven with real mouse and keyboard events (CGEvent) in the isolated instance:

| Scenario | Result |
|---|---|
| Click an element in the Canvas: selected in Canvas, Sources dock, native preview and Inspector | pass |
| Drag an element 14 px towards a guide 16 px away: lands exactly on the guide | pass |
| Type `+100` in X with two elements selected: both move by 100 | pass |
| Drag from the top ruler: horizontal guide created, snapped to the canvas center, saved in the collection file | pass |
| Toggle the column grid from the toolbar | pass |

## 4. Regression checklist before a release

- Unit tests and self-test green.
- Open OBS with an existing collection: no change to the collection until the user edits.
- Disable the plugin: every scene and source unchanged (Layouts only adds the `layouts` module data).
- Studio Mode on/off, scene switch, collection switch, profile switch.
- Undo / redo after each operation type.
- HiDPI and non-HiDPI screens; light and dark OBS themes.

## 5. Known limitations found by testing
- **Termination by the system (SIGTERM, logout) while OBS is open** crashed at exit in 1 of 3 runs
  with the Layouts docks shown, in `libobs-opengl` (`gl_update` deferred block running after its
  view was destroyed). Quitting OBS normally never crashed. Layouts now destroys its display as
  soon as the Canvas is hidden, which reduced the rate (3 of 3 before) but the race is in libobs.
- On such a system termination, OBS shows its "You just closed a dockable window" message for a
  floating Canvas, as for any floating dock.

## 6. Not tested yet
- Windows and Linux at runtime (CI builds only).
- Wayland surfaces for the Canvas.
- Browser and capture sources in the performance probe (color sources only).
- Very large canvases (8K) and extreme zoom on all GPU backends.
