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
constexpr int kLayersWidth = 240;    /* default Layers panel width on first launch */
constexpr int kIconButton = 26;      /* square icon button */
constexpr int kIconSize = 16;
constexpr int kLabelWidth = 16;         /* scrub label "X", "W"... */
constexpr int kRefGridSize = 46;        /* 3x3 reference point picker */
constexpr qreal kSectionTitlePt = 0.85; /* section title font, relative */

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
/* Source types (0.4): pills in Layers and the Builder, filter dots. */
constexpr QRgb kRoleHost = 0xFF7ED6C4; /* provisional, see the Builder review page */
constexpr QRgb kRoleCamera = kThumbCamera;
constexpr QRgb kRoleScreen = kThumbScreen;
constexpr QRgb kRoleBackground = 0xFF8E8AA3;
constexpr QRgb kRoleGraphics = 0xFFE2708A;
constexpr QRgb kRoleChat = 0xFFB5D96B;       /* provisional */
constexpr QRgb kThumbChat = kRoleChat;       /* provisional: broadcast gallery, chat tiles */
constexpr QRgb kRoleNone = 0xFF7D828E;       /* dashed outline, "not set" */
constexpr QRgb kRolePillText = 0xFF15171C;   /* text on a filled pill */
constexpr QRgb kCrop = 0xFFFF9F0A;           /* crop mode handles and frame */
constexpr QRgb kCanvasBackdrop = 0xFF000000; /* behind the scene, inside the canvas */
constexpr QRgb kOutside = 0xFF1B1C1F;        /* around the canvas (dark themes) */
constexpr QRgb kLiveFrame = 0xFFE5484D;      /* frame around the canvas when on air */

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

/* ---- Live banner ---- */
constexpr QRgb kLiveBanner = 0xFFE5484D;
constexpr QRgb kProgramBanner = 0xFF8A5A00;

} // namespace layouts::tokens

namespace layouts::ui {

/* Lucide icon tinted for the widget palette (or an explicit color). */
QIcon icon(const QString &name, const QWidget *w = nullptr);
QIcon icon(const QString &name, const QColor &color, int size = 16);

/* Localized text of the module. */
QString T(const char *key);

} // namespace layouts::ui
