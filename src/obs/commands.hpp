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

/* Editing commands shared by the Inspector, the Canvas and the context menus.
 * Each command reads the current selection, computes new transforms with the core
 * engines and applies them as one transaction (one undo entry). Locked items are
 * never moved. */

#include "core/arrange.hpp"
#include "core/expression.hpp"
#include "core/item-model.hpp"
#include "core/broadcast.hpp"
#include "core/layout.hpp"

#include <obs.h>

#include <functional>
#include <optional>
#include <string>

#include <QString>

namespace layouts {

struct ItemEntry;
using Axis = core::Axis;

namespace commands {

enum class Field { X, Y, W, H, Rotation, CropLeft, CropTop, CropRight, CropBottom };

/* Value shown in a field for an item (world units, reference point applied). */
double fieldValue(const ItemEntry &e, Field f);
/* Percent base of a field for "n%" expressions. */
double percentBase(Field f, const ItemEntry *e);

/* Applies an expression to a field of every selected item. Relative expressions
 * apply to each item's own value. */
bool setField(Field f, const core::Expression &expr);

/* Live variant for scrubbing: delta added to the value captured at scrub start.
 * Call with finished = true once to commit the undo entry. */
void scrubField(Field f, double delta, bool finished);

bool align(core::AlignEdge edge);
bool distribute(Axis axis, bool spacing);
bool spaceExactly(Axis axis, double gap);
bool sameSize(bool width, bool height);

bool rotateBy(double deg);
bool flip(Axis axis);
bool fit(core::FitMode mode);
bool originalSize();
bool resetTransform();

/* How each item fills its frame when W and H change its shape (saved on the scene item,
 * Fill by default). Setting it refills the current frame of the selection. */
core::FrameMode frameMode(obs_sceneitem_t *item);
bool setFrameMode(core::FrameMode mode);
/* The largest frame with this width / height ratio inside each selected item's frame,
 * anchored on the reference point. */
bool setRatio(double ratio);
bool setBoundsType(core::BoundsType type);
bool setBoundsAlignment(uint32_t align);
bool setCropToBounds(bool crop);
bool setLocked(bool locked);
bool nudge(double dx, double dy);

/* ---- layouts on several items (selection order) ---- */
/* Target frames for the current selection, without applying them (previews). */
std::vector<core::Frame> flowFrames(const core::FlowSpec &spec, bool inCanvas);
std::vector<core::Frame> presetFrames(core::Preset preset, const core::PresetSpec &spec);
bool autoLayout(const core::FlowSpec &spec, bool inCanvas);
bool applyPreset(core::Preset preset, const core::PresetSpec &spec);
/* ---- broadcast layouts (cameras + screens, TV style) ---- */
enum class KindMode { Auto, AllCameras, AllScreens };
/* Kind of each selected top-level item, in selection order. */
std::vector<core::SlotKind> selectionKinds(KindMode mode);

/* Transform that places a source in a broadcast tile: cameras fill it (cropped on the
 * face), screens fit; a flipped source stays flipped. */
core::ItemTransform slotTransform(const core::ItemModel &m, core::SlotKind kind, const core::Rect &cell);
/* Locks an item and makes it scale with the canvas. */
void lockItem(obs_sceneitem_t *item);
/* Target frames (one per selected item that gets a slot) for a template. */
std::vector<core::Frame> broadcastFrames(const std::string &key, const core::BroadcastSpec &spec, KindMode mode);
/* Applies a template to the selection (one undo entry). lock: lock the items and
 * pin them with "scale" constraints so the layout never moves. */
bool applyBroadcast(const std::string &key, const core::BroadcastSpec &spec, KindMode mode, bool lock);
/* Creates a new scene with the selected sources arranged by the template (the
 * sources are shared, never duplicated). Returns the new scene name, empty on failure. */
QString createBroadcastScene(const std::string &key, const QString &sceneName, const core::BroadcastSpec &spec,
			     KindMode mode, bool lock);
/* One-click show: a full-screen scene for each selected source, then one scene per
 * layout that fits the selection. Names come from the callbacks. Returns the number of
 * scenes created; the edited scene does not change. */
int createShowScenes(const core::BroadcastSpec &spec, KindMode mode, bool lock,
		     const std::function<QString(const std::string &key)> &templateName,
		     const std::function<QString(const QString &sourceName)> &soloName);

/* ---- swap (0.5) ---- */
/* True when both items can exchange their slots: different items of the same scene
 * with compatible roles (camera/host with camera/host, screen with screen). */
bool canSwapItems(obs_sceneitem_t *a, obs_sceneitem_t *b);
/* The two items exchange their slots (one undo entry): visible cell, rotation, z-order,
 * style, lock and constraints stay with the slot; each item keeps its flips and frame
 * mode, its crop is recomputed for the new cell. Works on locked items (a locked
 * broadcast layout can still be recast) and on any scene, not only the edited one. */
bool swapItems(obs_sceneitem_t *a, obs_sceneitem_t *b);

/* Shows the frames as a ghost preview on the Canvas (empty clears it). */
void showPreview(const std::vector<core::Frame> &frames);

/* ---- properties clipboard ---- */
enum PasteMask : unsigned {
	PastePosition = 1u << 0,
	PasteSize = 1u << 1,
	PasteRotation = 1u << 2,
	PasteCrop = 1u << 3,
	PasteBounds = 1u << 4,
	PasteAll = 0x1Fu,
};
bool copyProperties();
bool canPaste();
bool pasteProperties(unsigned mask);

/* Align / key object state shared by the Inspector and the Canvas. */
core::AlignTarget alignTarget();
void setAlignTarget(core::AlignTarget t);
int keyIndex(); /* index in the current selection, -1 if none */
void setKeyItem(int64_t groupId, int64_t id);

} // namespace commands

} // namespace layouts
