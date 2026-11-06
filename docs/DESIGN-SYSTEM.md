# Design system

Layouts follows the active OBS theme: every widget uses `QPalette` colors, so Yami, Grey, Rachni,
Light or custom themes apply. Visual values that are Layouts' own live in **one file**,
`src/ui/theme.hpp`. They were validated in a design review on 2026-11-04.

## Principles
Compact, precise, quiet. Dark mode first but theme-adaptive. Icons from
[Lucide](https://lucide.dev) (ISC), tinted with the theme text color. No decorative color: color
is used for state (selection, snapping, live).

## Tokens

### Inspector
| Token | Value | Use |
|---|---|---|
| `kPanelMargin` | 10 px | dock inner margin |
| `kSectionSpacing` | 10 px | between sections |
| `kRowSpacing` | 6 px | between rows |
| `kColumnSpacing` | 8 px | between fields |
| `kFieldHeight` | 24 px | inputs, segmented buttons |
| `kFieldMinWidth` | 44 px | numeric fields shrink to this |
| `kIconButton` | 26 px | square icon buttons |
| `kIconSize` | 16 px | icons |
| `kRefGridSize` | 46 px | reference point picker |
| `kSectionTitlePt` | 0.85 × | section titles, bold, uppercase |
| `kInspectorWidth` | 300 px | dock width on first launch |
| `kSwatchSize` (0.5) | 14 px | color swatch inside its button (border, shadow) |
| `kSwatchOutline` (0.5) | white 50 % | outline of a swatch |
| `kStyleFieldWidth` (0.5) | 72 px | px / width fields of the Style section |
| `kStyleRoleListWidth` (0.5) | 150 px | type list of the "Styles by type" dialog |
| `kOverrideDot` / `kOverrideDotColor` (0.5) | 6 px / `#18A0FB` | "modified" dot: the item's style differs from its type |

### Canvas
| Token | Value | Use |
|---|---|---|
| `kSelection` | `#18A0FB` | selection outline, handles border, size label, rubber band |
| `kHover` | `#18A0FB`, 2 px | hovered element |
| `kOutline` | white 25 % | outline of every element (option) |
| `kLocked` | `#8A8F98` | outline of a selected locked element |
| `kSmartGuide` | `#FF3D71` | snapping lines and gap labels |
| `kMeasure` | `#FF3D71` | Alt measurements |
| guide color (setting) | `#FF3D71` | guides (global guides at 60 %) |
| `kGrid` | white 14 % | square grid |
| `kTrackFill` | `#FF3D71` 10 % | column / row tracks |
| `kSafeArea` | `#FFD60A` 50 % | safe areas, custom margins |
| `kCenterLine` | `#FFD60A` 38 % | center and 4:3 lines |
| `kPreview` / `kPreviewFill` (0.2) | `#8B6CFF` / 19 % | ghost frames of presets and auto layout |
| `kCrop` (0.2) | `#FF9F0A` | crop mode frame and handles |
| `kThumbCamera` (0.2) | `#5FB4F5` | broadcast layouts gallery: camera tiles |
| `kThumbScreen` (0.2) | `#C9A24A` | broadcast layouts gallery: screen tiles |
| `kLayersWidth` (0.3) | 240 px | Layers panel width on first launch |
| `kRoleHost` (0.4) | `#7ED6C4` | type pill and filter dot: Host |
| `kRoleCamera` (0.4) | = `kThumbCamera` | type pill: Cam |
| `kRoleScreen` (0.4) | = `kThumbScreen` | type pill: Screen |
| `kRoleBackground` (0.4) | `#8E8AA3` | type pill: BG |
| `kRoleGraphics` (0.4) | `#E2708A` | type pill: CG |
| `kRoleChat` (0.5) | `#B5D96B` | type pill and filter dot: Chat |
| `kThumbChat` (0.5) | = `kRoleChat` | broadcast layouts gallery: chat tiles |
| `kThumbFrame` (0.5) | `#F4F1EA` | broadcast layouts gallery: Polaroid border of a framed slot |
| user layout marker (0.5) | `kSelection` dot, 9 % of the thumbnail height (min 4 px) | top-right corner of a layout saved by the user |
| `kRoleNone` (0.4) | `#7D828E` | dashed pill: type not set |
| `kRolePillText` (0.4) | `#15171C` | text on a filled pill |
| `kOutside` | `#1B1C1F` | around the canvas |
| `kCanvasBackdrop` | black | canvas behind the scene |
| `kLiveFrame` | `#E5484D`, 2 px | frame around the canvas when on air |
| `kSwap` / `kSwapOutline` (0.5) | `#18A0FB`, 2 px | outline of the slot a dragged item will swap with |
| `kSwapFill` (0.5) | `#18A0FB` 12 % | fill of that slot, with the ⇄ icon at its center |
| `kHandleSize` | 8 px | resize handles (white fill, selection border) |
| `kHandleHit` / `kRotateZone` / `kGuideHit` | 7 / 18 / 4 px | hit areas |
| `kRulerThickness` | 20 px | rulers |
| `kRulerFontPt` / `kLabelFontPt` | 8 / 9 pt | ruler numbers / canvas labels |
| `kDragThreshold` | 3 px | press → drag |

### Item style (0.5)
Defaults and presets of the Style section; the values are in canvas pixels and drawn by Layouts'
filters, so they are part of the video output.

| Value | Setting |
|---|---|
| Corners Auto | 4 % of the short side of the visible frame, between 6 and 48 px, never more than half the short side |
| Corners Round | half the short side (a circle or a pill) |
| Border, when turned on | 4 px, white, inside the frame, following the corners |
| Shadow Soft | blur 16, offset 0 / 6, opacity 25 %, black |
| Shadow Hard | blur 6, offset 0 / 6, opacity 60 %, black |
| Default style of a type | corners Auto, no border, no shadow |

### Switcher dock (0.5)
| Token | Value | Use |
|---|---|---|
| `kTallyProgram` | `#E5484D` | layout or source button on air (red) |
| `kTallyPreview` | `#30A46C` | layout or source button in preview (green); on air and in preview = red with a green underline |
| `kTallyText` | white | text on a lit button |
| `kRegieButton` | 28 px | height of a bus button (compact density) |
| `kRegieThumbW` / `kRegieThumbH` | 72 / 40 px | thumbnail of a layout button |
| `kRegieCutHeight` | 44 px | CUT and AUTO (AUTO greyed until 0.6) |
| lost source banner | `kTallyProgram` / `kTallyPreview`, white text | under the dock title: red while a source is lost, green for a few seconds once it is back |

### Banners
| Token | Value |
|---|---|
| `kLiveBanner` | `#E5484D` (on air) |
| `kProgramBanner` | `#8A5A00` (program) |

## Components
| Component | Implementation | States |
|---|---|---|
| Numeric field | `ui::NumericField` (scrub label + line edit) | default, hover, focus, disabled, mixed (placeholder), invalid (red border + tooltip) |
| Scrub label | `ui::ScrubLabel` | resize cursor, Shift ×10, Alt ×0.1 |
| Segmented control | `ui::segmented()` | checked, unchecked, focus |
| Icon button | `ui::iconButton()` | default, hover, pressed, checked, disabled (dimmed icon) |
| Reference point picker | `ui::RefPointPicker` | selected dot, focus ring, keyboard arrows |
| Section title | `ui::sectionTitle()` | — |
| Property rows | Qt layouts with the tokens above | — |

## Review
All values above were reviewed on screenshots of the Inspector and the Canvas in OBS 33.0.1 and
approved as is (2026-11-04), including the 0.2 tokens `kPreview`, `kPreviewFill` and `kCrop`, and the broadcast gallery colors `kThumbCamera` and `kThumbScreen`, and the 0.3 `kLayersWidth`. The 0.4 type colors (BG lavender, CG rose, pills filled) were chosen on the Builder review page (2026-11-05). The 0.5 values were approved on the 0.5 design review (2026-11-06): `kRoleChat`, `kRoleHost` (now final), the tally colors, the compact Switcher (28 px buttons, AUTO greyed), the lost source banner, the swap highlight (2 px accent outline, accent fill, ⇄ icon), the mirror toggles above the gallery, the user layout dot, the Style section as mocked, the Soft and Hard shadows and the default border. Changing a value means editing `src/ui/theme.hpp` only.
