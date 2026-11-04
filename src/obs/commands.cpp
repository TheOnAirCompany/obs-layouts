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

#include "commands.hpp"

#include "edit-context.hpp"
#include "edit-session.hpp"
#include "store.hpp"

#include <obs-module.h>

#include <cmath>
#include <functional>
#include <memory>

namespace layouts::commands {

namespace {

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

std::vector<ItemEntry> selectionCopy()
{
	std::vector<ItemEntry> out;
	for (const ItemEntry *e : EditContext::instance().selection())
		out.push_back(*e);
	return out;
}

core::RefPoint ref()
{
	return core::RefPoint::fromIndex(Store::instance().settings().refPoint);
}

core::ItemModel modelWith(const ItemEntry &e, const core::ItemTransform &t)
{
	core::ItemModel m = e.model;
	m.t = t;
	return m;
}

core::AlignTarget g_target = core::AlignTarget::Selection;
std::pair<int64_t, int64_t> g_key{-1, -1};

/* Returns the transform after setting one field of an item to v. */
core::ItemTransform withField(const ItemEntry &e, Field f, double v)
{
	const core::ItemModel &m = e.model;
	const core::RefPoint r = ref();
	switch (f) {
	case Field::X: {
		core::Vec2 p = e.frame.point(r);
		p.x = v;
		return core::moveRefTo(m, e.parent, r, p);
	}
	case Field::Y: {
		core::Vec2 p = e.frame.point(r);
		p.y = v;
		return core::moveRefTo(m, e.parent, r, p);
	}
	case Field::W:
	case Field::H: {
		const bool lock = Store::instance().settings().ratioLock;
		double w = e.frame.width, h = e.frame.height;
		if (f == Field::W) {
			if (lock && w > 0)
				h = h * v / w;
			w = v;
		} else {
			if (lock && h > 0)
				w = w * v / h;
			h = v;
		}
		return core::setWorldSize(m, e.parent, w, h, r);
	}
	case Field::Rotation:
		return core::setWorldRotation(m, e.parent, v);
	case Field::CropLeft:
	case Field::CropTop:
	case Field::CropRight:
	case Field::CropBottom: {
		core::Crop c = m.t.crop;
		const int iv = (int)std::lround(std::max(0.0, v));
		if (f == Field::CropLeft)
			c.left = iv;
		else if (f == Field::CropTop)
			c.top = iv;
		else if (f == Field::CropRight)
			c.right = iv;
		else
			c.bottom = iv;
		return core::setCrop(m, e.parent, c, r);
	}
	}
	return m.t;
}

const char *fieldUndoKey(Field f)
{
	switch (f) {
	case Field::X:
	case Field::Y:
		return "Undo.Position";
	case Field::W:
	case Field::H:
		return "Undo.Size";
	case Field::Rotation:
		return "Undo.Rotation";
	default:
		return "Undo.Crop";
	}
}

bool applyPerItem(const char *undoKey, const std::function<core::ItemTransform(const ItemEntry &)> &fn, bool skipLocked,
		  bool repeatable = false)
{
	std::vector<Change> changes;
	for (const ItemEntry &e : selectionCopy()) {
		if (skipLocked && e.locked)
			continue;
		changes.push_back({e.item, fn(e)});
	}
	return EditSession::run(tr(undoKey), changes, repeatable);
}

std::vector<core::ArrangeItem> arrangeItems(const std::vector<ItemEntry> &sel)
{
	std::vector<core::ArrangeItem> items;
	for (const ItemEntry &e : sel)
		items.push_back({e.frame.quad.bounds(), e.locked});
	return items;
}

bool applyDeltas(const char *undoKey, const std::vector<ItemEntry> &sel, const std::vector<core::Vec2> &deltas)
{
	std::vector<Change> changes;
	for (size_t i = 0; i < sel.size() && i < deltas.size(); i++) {
		if (sel[i].locked || (deltas[i].x == 0.0 && deltas[i].y == 0.0))
			continue;
		changes.push_back({sel[i].item, core::moveBy(sel[i].model, sel[i].parent, deltas[i])});
	}
	return EditSession::run(tr(undoKey), changes);
}

struct Clipboard {
	bool valid = false;
	core::Vec2 position; /* world point at the reference point used when copying */
	core::RefPoint ref;
	double width = 0, height = 0, rotation = 0;
	core::Crop crop;
	core::BoundsType boundsType = core::BoundsType::None;
	uint32_t boundsAlign = 0;
	bool cropToBounds = false;
};
Clipboard g_clip;

struct Scrub {
	std::unique_ptr<EditSession> session;
	std::vector<ItemEntry> items;
	std::vector<double> base;
	Field field;
};
Scrub g_scrub;

} // namespace

double fieldValue(const ItemEntry &e, Field f)
{
	switch (f) {
	case Field::X:
		return e.frame.point(ref()).x;
	case Field::Y:
		return e.frame.point(ref()).y;
	case Field::W:
		return e.frame.width;
	case Field::H:
		return e.frame.height;
	case Field::Rotation:
		return core::worldRotation(e.model, e.parent);
	case Field::CropLeft:
		return e.model.t.crop.left;
	case Field::CropTop:
		return e.model.t.crop.top;
	case Field::CropRight:
		return e.model.t.crop.right;
	case Field::CropBottom:
		return e.model.t.crop.bottom;
	}
	return 0.0;
}

double percentBase(Field f, const ItemEntry *e)
{
	EditContext &ctx = EditContext::instance();
	switch (f) {
	case Field::X:
	case Field::W:
		return ctx.canvasWidth();
	case Field::Y:
	case Field::H:
		return ctx.canvasHeight();
	case Field::Rotation:
		return 360.0;
	case Field::CropLeft:
	case Field::CropRight:
		return e ? e->model.sourceWidth : 0.0;
	case Field::CropTop:
	case Field::CropBottom:
		return e ? e->model.sourceHeight : 0.0;
	}
	return 0.0;
}

bool setField(Field f, const core::Expression &expr)
{
	if (!expr.ok)
		return false;
	return applyPerItem(
		fieldUndoKey(f),
		[&](const ItemEntry &e) {
			double v = 0.0;
			if (!expr.apply(fieldValue(e, f), v))
				return e.model.t;
			return withField(e, f, v);
		},
		false);
}

void scrubField(Field f, double delta, bool finished)
{
	if (!g_scrub.session || g_scrub.field != f) {
		if (g_scrub.session)
			g_scrub.session->commit();
		g_scrub = Scrub();
		g_scrub.session = std::make_unique<EditSession>();
		if (!g_scrub.session->begin(tr(fieldUndoKey(f)))) {
			g_scrub = Scrub();
			return;
		}
		g_scrub.field = f;
		g_scrub.items = selectionCopy();
		for (const ItemEntry &e : g_scrub.items)
			g_scrub.base.push_back(fieldValue(e, f));
	}

	std::vector<Change> changes;
	for (size_t i = 0; i < g_scrub.items.size(); i++) {
		double v = g_scrub.base[i] + delta;
		if (f == Field::W || f == Field::H)
			v = std::max(1.0, v);
		changes.push_back({g_scrub.items[i].item, withField(g_scrub.items[i], f, v)});
	}
	g_scrub.session->apply(changes);

	if (finished) {
		g_scrub.session->commit();
		g_scrub = Scrub();
	}
}

core::AlignTarget alignTarget()
{
	return g_target;
}

void setAlignTarget(core::AlignTarget t)
{
	g_target = t;
}

void setKeyItem(int64_t groupId, int64_t id)
{
	g_key = {groupId, id};
}

int keyIndex()
{
	const auto sel = EditContext::instance().selection();
	for (size_t i = 0; i < sel.size(); i++) {
		if (sel[i]->groupId == g_key.first && sel[i]->id == g_key.second)
			return (int)i;
	}
	return sel.empty() ? -1 : 0; /* default key object: first selected item */
}

bool align(core::AlignEdge edge)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	if (sel.empty())
		return false;
	const auto items = arrangeItems(sel);
	core::AlignTarget target = g_target;
	if (sel.size() == 1 && target != core::AlignTarget::Canvas)
		target = core::AlignTarget::Canvas; /* a single item aligns to the canvas, like Figma */
	const int key = target == core::AlignTarget::KeyObject ? keyIndex() : -1;
	const auto refRect = core::alignReference(items, target, EditContext::instance().canvasRect(), key);
	if (!refRect)
		return false;
	return applyDeltas("Undo.Align", sel, core::align(items, edge, *refRect, key));
}

bool distribute(Axis axis, bool spacing)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	const auto items = arrangeItems(sel);
	if (core::movableCount(items) < 3)
		return false;
	const auto deltas = spacing ? core::distributeSpacing(items, axis) : core::distributeCenters(items, axis);
	return applyDeltas("Undo.Distribute", sel, deltas);
}

bool spaceExactly(Axis axis, double gap)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	const auto items = arrangeItems(sel);
	if (core::movableCount(items) < 2)
		return false;
	return applyDeltas("Undo.Spacing", sel, core::spaceExactly(items, axis, gap));
}

bool sameSize(bool width, bool height)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	if (sel.size() < 2)
		return false;
	const int k = (g_target == core::AlignTarget::KeyObject) ? keyIndex() : 0;
	const ItemEntry &refItem = sel[(size_t)std::max(0, k)];
	std::vector<Change> changes;
	for (size_t i = 0; i < sel.size(); i++) {
		const ItemEntry &e = sel[i];
		if ((int)i == k || e.locked)
			continue;
		const double w = width ? refItem.frame.width : e.frame.width;
		const double h = height ? refItem.frame.height : e.frame.height;
		changes.push_back({e.item, core::setWorldSize(e.model, e.parent, w, h, ref())});
	}
	return EditSession::run(tr("Undo.Size"), changes);
}

bool rotateBy(double deg)
{
	return applyPerItem(
		"Undo.Rotation", [&](const ItemEntry &e) { return core::rotateBy(e.model, e.parent, deg); }, false);
}

bool flip(Axis axis)
{
	return applyPerItem(
		"Undo.Flip", [&](const ItemEntry &e) { return core::flip(e.model, e.parent, axis); }, false);
}

bool fit(core::FitMode mode)
{
	const core::Rect canvas = EditContext::instance().canvasRect();
	return applyPerItem(
		"Undo.Fit",
		[&](const ItemEntry &e) {
			/* Items inside a group fit the canvas too, in world space. */
			return core::fitInto(e.model, e.parent, canvas, mode);
		},
		false);
}

bool originalSize()
{
	return applyPerItem(
		"Undo.Size", [&](const ItemEntry &e) { return core::originalSize(e.model, e.parent, ref()); }, false);
}

bool resetTransform()
{
	return applyPerItem(
		"Undo.Reset",
		[&](const ItemEntry &) {
			core::ItemTransform t; /* OBS defaults: top-left, scale 1, no rotation, crop or bounds */
			return t;
		},
		false);
}

bool setBoundsType(core::BoundsType type)
{
	return applyPerItem(
		"Undo.Bounds", [&](const ItemEntry &e) { return core::setBoundsType(e.model, e.parent, type, ref()); },
		false);
}

bool setBoundsAlignment(uint32_t a)
{
	return applyPerItem(
		"Undo.Bounds",
		[&](const ItemEntry &e) {
			core::ItemTransform t = e.model.t;
			t.boundsAlign = a;
			return t;
		},
		false);
}

bool setCropToBounds(bool crop)
{
	return applyPerItem(
		"Undo.Bounds",
		[&](const ItemEntry &e) {
			core::ItemTransform t = e.model.t;
			t.cropToBounds = crop;
			return t;
		},
		false);
}

bool setLocked(bool locked)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	for (const ItemEntry &e : sel)
		obs_sceneitem_set_locked(e.item, locked);
	EditContext::instance().markDirty();
	return !sel.empty();
}

bool nudge(double dx, double dy)
{
	return applyPerItem(
		"Undo.Nudge", [&](const ItemEntry &e) { return core::moveBy(e.model, e.parent, {dx, dy}); }, true,
		true);
}

namespace {

std::vector<core::Frame> framesOf(const std::vector<ItemEntry> &sel)
{
	std::vector<core::Frame> frames;
	for (const ItemEntry &e : sel)
		frames.push_back({e.frame.center(), e.frame.width, e.frame.height});
	return frames;
}

/* Moves and resizes each item to its target frame (center and visual size);
 * rotation is kept. Locked items are skipped. */
bool applyFrames(const char *undoKey, const std::vector<ItemEntry> &sel, const std::vector<core::Frame> &frames)
{
	std::vector<Change> changes;
	const core::RefPoint center{0.5, 0.5};
	for (size_t i = 0; i < sel.size() && i < frames.size(); i++) {
		const ItemEntry &e = sel[i];
		if (e.locked)
			continue;
		core::ItemModel m = e.model;
		if (frames[i].width > 0 && frames[i].height > 0)
			m.t = core::setWorldSize(m, e.parent, frames[i].width, frames[i].height, center);
		m.t = core::moveRefTo(m, e.parent, center, frames[i].center);
		changes.push_back({e.item, m.t});
	}
	return EditSession::run(tr(undoKey), changes);
}

} // namespace

std::vector<core::Frame> flowFrames(const core::FlowSpec &spec, bool inCanvas)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	const std::vector<core::Frame> frames = framesOf(sel);
	if (frames.empty())
		return frames;
	core::Rect container = EditContext::instance().canvasRect();
	if (!inCanvas) {
		std::vector<core::ArrangeItem> items;
		for (const ItemEntry &e : sel)
			items.push_back({e.frame.quad.bounds(), false});
		container = core::selectionBounds(items);
	}
	return core::flow(frames, container, spec);
}

std::vector<core::Frame> presetFrames(core::Preset preset, const core::PresetSpec &spec)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	return core::applyPreset(preset, framesOf(sel), EditContext::instance().canvasRect(), spec);
}

bool autoLayout(const core::FlowSpec &spec, bool inCanvas)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	if (sel.size() < 2)
		return false;
	const std::vector<core::Frame> frames = flowFrames(spec, inCanvas);
	showPreview({});
	return applyFrames("Undo.Layout", sel, frames);
}

bool applyPreset(core::Preset preset, const core::PresetSpec &spec)
{
	const std::vector<ItemEntry> sel = selectionCopy();
	if (sel.empty())
		return false;
	const std::vector<core::Frame> frames = presetFrames(preset, spec);
	showPreview({});
	return applyFrames("Undo.Preset", sel, frames);
}

void showPreview(const std::vector<core::Frame> &frames)
{
	std::vector<core::Quad> quads;
	for (const core::Frame &f : frames) {
		const core::Rect r = f.rect();
		quads.push_back(core::Quad{{core::Vec2{r.left(), r.top()}, core::Vec2{r.right(), r.top()},
					    core::Vec2{r.right(), r.bottom()}, core::Vec2{r.left(), r.bottom()}}});
	}
	EditContext::instance().setPreview(quads);
}

bool copyProperties()
{
	const std::vector<ItemEntry> sel = selectionCopy();
	if (sel.empty())
		return false;
	const ItemEntry &e = sel.front();
	g_clip.valid = true;
	g_clip.ref = ref();
	g_clip.position = e.frame.point(g_clip.ref);
	g_clip.width = e.frame.width;
	g_clip.height = e.frame.height;
	g_clip.rotation = core::worldRotation(e.model, e.parent);
	g_clip.crop = e.model.t.crop;
	g_clip.boundsType = e.model.t.boundsType;
	g_clip.boundsAlign = e.model.t.boundsAlign;
	g_clip.cropToBounds = e.model.t.cropToBounds;
	return true;
}

bool canPaste()
{
	return g_clip.valid;
}

bool pasteProperties(unsigned mask)
{
	if (!g_clip.valid)
		return false;
	return applyPerItem(
		"Undo.Paste",
		[&](const ItemEntry &e) {
			core::ItemModel m = e.model;
			if (mask & PasteBounds) {
				m.t = core::setBoundsType(m, e.parent, g_clip.boundsType, g_clip.ref);
				m.t.boundsAlign = g_clip.boundsAlign;
				m.t.cropToBounds = g_clip.cropToBounds;
			}
			if (mask & PasteCrop)
				m.t = core::setCrop(m, e.parent, g_clip.crop, g_clip.ref);
			if (mask & PasteRotation)
				m.t = core::setWorldRotation(m, e.parent, g_clip.rotation);
			if (mask & PasteSize)
				m.t = core::setWorldSize(m, e.parent, g_clip.width, g_clip.height, g_clip.ref);
			if (mask & PastePosition)
				m.t = core::moveRefTo(m, e.parent, g_clip.ref, g_clip.position);
			return m.t;
		},
		false);
}

} // namespace layouts::commands
