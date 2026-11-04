# Design system

Layouts follows the active OBS theme: every widget uses `QPalette` colors, so Yami, Grey, Rachni,
Light or custom themes apply. Visual values that are Layouts' own live in **one file**,
`src/ui/theme.hpp`, and are **provisional** until reviewed.

## Principles
Compact, precise, quiet. Dark mode first but theme-adaptive. Icons from
[Lucide](https://lucide.dev) (ISC), tinted with the theme text color. No decorative color: color
is used for state (selection, snapping, live).

## Tokens (provisional)

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
| `kOutside` | `#1B1C1F` | around the canvas |
| `kCanvasBackdrop` | black | canvas behind the scene |
| `kLiveFrame` | `#E5484D`, 2 px | frame around the canvas when on air |
| `kHandleSize` | 8 px | resize handles (white fill, selection border) |
| `kHandleHit` / `kRotateZone` / `kGuideHit` | 7 / 18 / 4 px | hit areas |
| `kRulerThickness` | 20 px | rulers |
| `kRulerFontPt` / `kLabelFontPt` | 8 / 9 pt | ruler numbers / canvas labels |
| `kDragThreshold` | 3 px | press → drag |

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
The values above were chosen without a design review and are submitted for validation (design
review page with screenshots of the Inspector and the Canvas). Changing a value means editing
`src/ui/theme.hpp` only.
