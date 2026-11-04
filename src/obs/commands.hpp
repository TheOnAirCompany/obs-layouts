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

#include <optional>

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
bool setBoundsType(core::BoundsType type);
bool setBoundsAlignment(uint32_t align);
bool setCropToBounds(bool crop);
bool setLocked(bool locked);
bool nudge(double dx, double dy);

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
