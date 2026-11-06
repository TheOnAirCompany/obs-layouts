/*
Layouts for OBS
Copyright (C) 2026 The OnAir Company

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

/* Design tokens. Every visual value of Layouts lives here (reviewed and approved,
 * see docs/DESIGN-SYSTEM.md). Widgets otherwise inherit the
 * active OBS theme through QPalette. */

#include <QColor>
#include <QIcon>
#include <QString>

class QWidget;

namespace layouts::tokens {

/* ---- Inspector ---- */
constexpr int kSectionSpacing = 10; /* vertical space between sections */
constexpr int kRowSpacing = 6;      /* between rows inside a section */
constexpr int kColumnSpacing = 8;   /* between fields on a row */
constexpr int kPanelMargin = 10;    /* inner margin of the dock */
constexpr int kFieldHeight = 24;
constexpr int kFieldMinWidth = 44;   /* numeric fields shrink down to this */
constexpr int kInspectorWidth = 300; /* default dock width on first launch */
constexpr int kLayersWidth = 250;    /* default Scenes + Layers column width on first launch (0.7) */
constexpr int kIconButton = 26;      /* square icon button */
constexpr int kIconSize = 16;
constexpr int kLabelWidth = 16;         /* scrub label "X", "W"... */
constexpr int kRefGridSize = 46;        /* 3x3 reference point picker */
constexpr qreal kSectionTitlePt = 0.85; /* section title font, relative */
/* Style section (0.5) */
constexpr int kSwatchSize = 14;                /* provisional: color swatch inside its button */
constexpr QRgb kSwatchOutline = 0x80FFFFFF;    /* provisional */
constexpr int kStyleFieldWidth = 72;           /* provisional: px / width fields next to a row */
constexpr int kStyleRoleListWidth = 150;       /* provisional: "Styles by type" role list */
constexpr int kOverrideDot = 6;                /* provisional: "differs from its type" dot */
constexpr QRgb kOverrideDotColor = 0xFF18A0FB; /* provisional */

/* ---- Canvas overlays (canvas colors are ARGB) ---- */
constexpr QRgb kSelection = 0xFF18A0FB;  /* selection outline and handles */
constexpr QRgb kHover = 0xFF18A0FB;      /* hovered item outline */
constexpr QRgb kOutline = 0x40FFFFFF;    /* "show outlines" for every item */
constexpr QRgb kLocked = 0xFF8A8F98;     /* outline of a locked selected item */
constexpr QRgb kSmartGuide = 0xFFFF3D71; /* snapping feedback */
constexpr QRgb kMeasure = 0xFFFF3D71;    /* measurements (Alt) */
constexpr QRgb kGrid = 0x24FFFFFF;       /* square grid lines */
constexpr QRgb kTrackFill = 0x1AFF3D71;  /* column / row tracks */
constexpr QRgb kSafeArea = 0x80FFD60A;   /* action / title safe */
constexpr QRgb kCenterLine = 0x60FFD60A;
constexpr QRgb kRubberFill = 0x2018A0FB;
constexpr QRgb kPreview = 0xFF8B6CFF; /* ghost frames before a preset / layout is applied */
constexpr QRgb kPreviewFill = 0x308B6CFF;
constexpr QRgb kThumbCamera = 0xFF5FB4F5; /* broadcast gallery: camera tiles */
constexpr QRgb kThumbScreen = 0xFFC9A24A; /* broadcast gallery: screen tiles */
constexpr QRgb kThumbFrame = 0xFFF4F1EA;  /* broadcast gallery: Polaroid border, provisional */
/* Source types (0.4): pills in Layers and the Builder, filter dots. */
constexpr QRgb kRoleHost = 0xFF7ED6C4;
constexpr QRgb kRoleCamera = kThumbCamera;
constexpr QRgb kRoleScreen = kThumbScreen;
constexpr QRgb kRoleBackground = 0xFF8E8AA3;
constexpr QRgb kRoleGraphics = 0xFFE2708A;
constexpr QRgb kRoleChat = 0xFFB5D96B;
constexpr QRgb kThumbChat = kRoleChat;       /* broadcast gallery: chat tiles */
constexpr QRgb kRoleNone = 0xFF7D828E;       /* dashed outline, "not set" */
constexpr QRgb kRolePillText = 0xFF15171C;   /* text on a filled pill */
constexpr QRgb kCrop = 0xFFFF9F0A;           /* crop mode handles and frame */
constexpr QRgb kCanvasBackdrop = 0xFF000000; /* behind the scene, inside the canvas */
constexpr QRgb kOutside = 0xFF1B1C1F;        /* around the canvas (dark themes) */
constexpr QRgb kLiveFrame = 0xFFE5484D;      /* frame around the canvas when on air */
constexpr QRgb kSwap = 0xFF18A0FB;           /* outline of the slot a dragged item swaps with */
constexpr QRgb kSwapFill = 0x1F18A0FB;       /* 12 % fill of that slot */
constexpr int kSwapOutline = 2;              /* outline width, device px at 1x */
/* Switcher dock (0.5): tally of the source and layout buttons. */
constexpr QRgb kTallyProgram = 0xFFE5484D; /* on program (red) */
constexpr QRgb kTallyPreview = 0xFF30A46C; /* on preview (green) */
constexpr QRgb kTallyText = 0xFFFFFFFF;    /* text on a lit button */
constexpr int kRegieButton = 28;           /* height of a bus button (compact) */
constexpr int kRegieThumbW = 72;           /* provisional: layout button thumbnail */
constexpr int kRegieThumbH = 40;           /* provisional */
constexpr int kRegieCutHeight = 44;        /* provisional: CUT / AUTO buttons */

constexpr int kHandleSize = 8;      /* logical px */
constexpr int kHandleHit = 7;       /* hit radius around a handle */
constexpr int kRotateZone = 18;     /* rotate when this far outside a corner */
constexpr int kGuideHit = 4;        /* hit distance of a guide */
constexpr int kRulerThickness = 20; /* logical px */
constexpr qreal kRulerFontPt = 8.0;
constexpr qreal kLabelFontPt = 9.0; /* canvas labels (measures, sizes) */
constexpr int kLabelPadX = 4;
constexpr int kLabelPadY = 2;
constexpr int kDragThreshold = 3; /* px before a press becomes a drag */

/* ---- Scene Editor and Scene Builder windows (0.7, from the validated mocks) ---- */
constexpr int kBuilderSourcesWidth = 270;     /* Builder column 1, first launch */
constexpr int kBuilderCreateWidth = 290;      /* Builder column 3, first launch */
constexpr int kNavRowHeight = 26;             /* Scenes navigator row */
constexpr int kNavRowPadX = 6;                /* inner left / right padding of a row */
constexpr int kNavRowRadius = 5;              /* edited scene highlight */
constexpr QRgb kNavSelectedFill = 0x2E18A0FB; /* 18 % of kSelection, with a kSelection outline */
constexpr int kNavStripWidth = 3;             /* stack color strip */
constexpr int kNavStripHeight = 14;
constexpr int kNavStripRadius = 2;
constexpr int kTallyDot = 7;              /* program (kTallyProgram) / preview (kTallyPreview) dot */
constexpr QRgb kNavFavorite = 0xFFE6C547; /* favorite star */
constexpr int kRatioTagRadius = 4;        /* "16:9" tag of a canvas section */
constexpr qreal kRatioTagPt = 0.8;        /* its font, relative */
constexpr QRgb kChipLive = kLiveFrame;    /* "● PROGRAM" chip: shown only when the scene is seen live */
constexpr int kChipRadius = 4;
constexpr int kChipPadX = 7;
constexpr int kChipPadY = 2;
constexpr int kGalleryCardMin = 150; /* Builder gallery: minimum card width */
constexpr int kGalleryGap = 12;      /* Builder gallery: space between cards */
constexpr int kButtonRadius = 6;     /* Builder: primary "Create scene" button */
constexpr int kButtonPadX = 10;
constexpr int kButtonPadY = 7;
constexpr int kPreviewRadius = 6; /* Builder: large preview of the chosen layout */
constexpr int kPreviewTileRadius = 3;
constexpr QRgb kPreviewBackdrop = 0xFF0E0F12; /* behind the tiles of the previews */
constexpr int kCreatedMessageMs = 4000;       /* provisional: how long "Created: <name>" stays */

} // namespace layouts::tokens

namespace layouts::ui {

/* Lucide icon tinted for the widget palette (or an explicit color). */
QIcon icon(const QString &name, const QWidget *w = nullptr);
QIcon icon(const QString &name, const QColor &color, int size = 16);

/* Localized text of the module. */
QString T(const char *key);

} // namespace layouts::ui
