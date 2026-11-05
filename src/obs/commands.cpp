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
#include "constraints.hpp"
#include "roles.hpp"
#include "edit-session.hpp"
#include "obs-convert.hpp"
#include "store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <cmath>
#include <algorithm>
#include <cstring>
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

constexpr const char *kFrameKey = "layouts_frame";

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
		return core::setFrame(m, e.parent, w, h, frameMode(e.item), r);
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

/* ------------------------------------------------------------ broadcast */

namespace {

struct Placed {
	ItemEntry entry;
	core::SlotKind kind;
	core::Rect cell;
};

/* Selected top-level sources that go in tiles: backgrounds and graphics never do. Hosts
 * come first so that they get the large tile of host + guests layouts. */
std::vector<ItemEntry> topLevelSelection()
{
	std::vector<ItemEntry> out;
	for (const ItemEntry &e : selectionCopy()) {
		const core::Role r = roles::role(obs_sceneitem_get_source(e.item));
		if (e.groupId == 0 && r != core::Role::Background && r != core::Role::Graphics)
			out.push_back(e);
	}
	std::stable_partition(out.begin(), out.end(), [](const ItemEntry &e) {
		return roles::role(obs_sceneitem_get_source(e.item)) == core::Role::Host;
	});
	return out;
}

std::vector<Placed> assignSlotsFor(const std::string &key, const core::BroadcastSpec &spec,
				   const std::vector<ItemEntry> &items, const std::vector<core::SlotKind> &kinds)
{
	const std::vector<core::Slot> targets = core::buildBroadcast(key, EditContext::instance().canvasRect(), spec);
	std::vector<Placed> out;
	std::vector<bool> used(items.size(), false);
	for (const core::Slot &slot : targets) {
		for (size_t i = 0; i < items.size(); i++) {
			if (!used[i] && kinds[i] == slot.kind) {
				used[i] = true;
				out.push_back({items[i], slot.kind, slot.cell});
				break;
			}
		}
	}
	return out;
}

std::vector<Placed> assignSlots(const std::string &key, const core::BroadcastSpec &spec, KindMode mode);

} // namespace

/* Transform that places a source in a slot: cameras fill it (cropped), screens fit. */
core::ItemTransform slotTransform(const core::ItemModel &m, core::SlotKind kind, const core::Rect &cell)
{
	core::ItemTransform t;
	t.align = core::kAlignLeft | core::kAlignTop;
	t.rot = 0;
	t.boundsType = core::BoundsType::None;
	if (m.sourceWidth == 0 || m.sourceHeight == 0)
		return m.t;
	if (kind == core::SlotKind::Camera) {
		const core::CoverResult c = core::coverCell(m.sourceWidth, m.sourceHeight, cell.w, cell.h);
		t.crop = c.crop;
		t.scale = c.scale;
		t.pos = {cell.x, cell.y};
	} else {
		const core::Frame f = core::fitInCell((double)m.sourceWidth / m.sourceHeight, cell, core::FitMode::Fit);
		t.scale = {f.width / m.sourceWidth, f.height / m.sourceHeight};
		const core::Rect r = f.rect();
		t.pos = {r.x, r.y};
	}
	/* A source flipped by the user (a mirrored webcam, a camera mounted upside down) stays
	 * flipped in its slot: same place, same crop of the content, only the mirror kept. */
	const core::ItemModel placed{t, m.sourceWidth, m.sourceHeight};
	if (m.t.scale.x < 0)
		t = core::flip(placed, core::Affine::identity(), core::Axis::X);
	if (m.t.scale.y < 0)
		t = core::flip({t, m.sourceWidth, m.sourceHeight}, core::Affine::identity(), core::Axis::Y);
	return t;
}

void lockItem(obs_sceneitem_t *item)
{
	obs_sceneitem_set_locked(item, true);
	/* Keep the same proportions if the canvas resolution ever changes. */
	setConstraints(item, core::HConstraint::Scale, core::VConstraint::Scale);
}

std::vector<core::SlotKind> selectionKinds(KindMode mode)
{
	std::vector<core::SlotKind> kinds;
	for (const ItemEntry &e : topLevelSelection()) {
		if (mode == KindMode::AllCameras)
			kinds.push_back(core::SlotKind::Camera);
		else if (mode == KindMode::AllScreens)
			kinds.push_back(core::SlotKind::Screen);
		else
			kinds.push_back(core::isCameraRole(roles::role(obs_sceneitem_get_source(e.item)))
						? core::SlotKind::Camera
						: core::SlotKind::Screen);
	}
	return kinds;
}

std::vector<core::Frame> broadcastFrames(const std::string &key, const core::BroadcastSpec &spec, KindMode mode)
{
	std::vector<core::Frame> frames;
	for (const Placed &p : assignSlots(key, spec, mode)) {
		const core::ItemModel m{slotTransform(p.entry.model, p.kind, p.cell), p.entry.model.sourceWidth,
					p.entry.model.sourceHeight};
		core::ItemModel full = p.entry.model;
		full.t = m.t;
		const core::VisualFrame f = core::visualFrame(full);
		frames.push_back({f.center(), f.width, f.height});
	}
	return frames;
}

bool applyBroadcast(const std::string &key, const core::BroadcastSpec &spec, KindMode mode, bool lock)
{
	const std::vector<Placed> placed = assignSlots(key, spec, mode);
	if (placed.empty())
		return false;
	std::vector<Change> changes;
	for (const Placed &p : placed)
		changes.push_back({p.entry.item, slotTransform(p.entry.model, p.kind, p.cell)});
	showPreview({});
	if (!EditSession::run(tr("Undo.Broadcast"), changes))
		return false;
	if (key.find("Pip") != std::string::npos) {
		/* The inset camera overlaps the screen: bring it to the front. */
		for (const Placed &p : placed) {
			if (p.kind == core::SlotKind::Camera)
				obs_sceneitem_set_order(p.entry.item, OBS_ORDER_MOVE_TOP);
		}
	}
	/* The checkbox decides the final state: a locked layout re-applied without the lock is
	 * freed again (constraints set by the lock go back to the default too). */
	for (const Placed &p : placed) {
		if (lock) {
			lockItem(p.entry.item);
		} else if (p.entry.locked) {
			obs_sceneitem_set_locked(p.entry.item, false);
			core::HConstraint h;
			core::VConstraint v;
			getConstraints(p.entry.item, h, v);
			if (h == core::HConstraint::Scale && v == core::VConstraint::Scale)
				setConstraints(p.entry.item, core::HConstraint::Left, core::VConstraint::Top);
		}
	}
	obs_frontend_save();
	EditContext::instance().markDirty();
	return true;
}

namespace {

std::vector<Placed> assignSlots(const std::string &key, const core::BroadcastSpec &spec, KindMode mode)
{
	return assignSlotsFor(key, spec, topLevelSelection(), selectionKinds(mode));
}

/* A new scene (in the edited scene's canvas) holding these sources in their slots; the
 * sources are shared, never duplicated. The name is made unique ("Duo", "Duo 2"...). */
obs_source_t *makeScene(const std::vector<Placed> &all, const QString &sceneName, bool lock, QString &name)
{
	/* A group belongs to its scene: OBS never shares one between scenes. */
	std::vector<Placed> placed;
	for (const Placed &p : all) {
		if (!obs_source_is_group(obs_sceneitem_get_source(p.entry.item)))
			placed.push_back(p);
	}
	if (placed.empty())
		return nullptr;
	name = sceneName;
	for (int i = 2;; i++) {
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
		if (!existing)
			break;
		name = sceneName + " " + QString::number(i);
	}

	obs_canvas_t *canvas = EditContext::instance().sceneSource()
				       ? obs_source_get_canvas(EditContext::instance().sceneSource())
				       : obs_get_main_canvas();
	obs_scene_t *scene = canvas ? obs_canvas_scene_create(canvas, name.toUtf8().constData())
				    : obs_scene_create(name.toUtf8().constData());
	if (canvas)
		obs_canvas_release(canvas);
	if (!scene)
		return nullptr;

	/* Screens first, cameras on top (a picture-in-picture camera must stay visible). */
	std::vector<Placed> stacked = placed;
	std::stable_partition(stacked.begin(), stacked.end(),
			      [](const Placed &p) { return p.kind == core::SlotKind::Screen; });
	for (const Placed &p : stacked) {
		obs_sceneitem_t *item = obs_scene_add(scene, obs_sceneitem_get_source(p.entry.item));
		if (!item)
			continue;
		obs_transform_info info = {};
		obs_sceneitem_crop crop = {};
		obs_sceneitem_get_info2(item, &info);
		toObs(slotTransform(p.entry.model, p.kind, p.cell), info, crop);
		obs_sceneitem_set_info2(item, &info);
		obs_sceneitem_set_crop(item, &crop);
		if (lock)
			lockItem(item);
	}
	/* OBS keeps the scene in its list; the caller gets a weak use of the source. */
	obs_source_t *source = obs_scene_get_source(scene);
	obs_scene_release(scene);
	return source;
}

} // namespace

QString createBroadcastScene(const std::string &key, const QString &sceneName, const core::BroadcastSpec &spec,
			     KindMode mode, bool lock)
{
	const std::vector<Placed> placed = assignSlots(key, spec, mode);
	if (placed.empty())
		return QString();
	QString name;
	obs_source_t *source = makeScene(placed, sceneName, lock, name);
	if (!source)
		return QString();
	if (obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(source);
	EditContext::instance().pin(source);
	obs_frontend_save();
	return name;
}

int createShowScenes(const core::BroadcastSpec &spec, KindMode mode, bool lock,
		     const std::function<QString(const std::string &key)> &templateName,
		     const std::function<QString(const QString &sourceName)> &soloName)
{
	const std::vector<ItemEntry> items = topLevelSelection();
	const std::vector<core::SlotKind> kinds = selectionKinds(mode);
	int created = 0, cams = 0, screens = 0;
	QString name;
	/* One full-screen scene per source: the close-ups of the show. */
	for (size_t i = 0; i < items.size(); i++) {
		const bool camera = kinds[i] == core::SlotKind::Camera;
		(camera ? cams : screens)++;
		const std::vector<Placed> placed =
			assignSlotsFor(camera ? "Cam1Full" : "Screen1Full", spec, {items[i]}, {kinds[i]});
		const QString source = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(items[i].item)));
		if (!placed.empty() && makeScene(placed, soloName(source), lock, name))
			created++;
	}
	/* Then every layout made for the whole selection. */
	for (const core::BroadcastTemplate &t : core::templatesFor(cams, screens)) {
		if (t.cameras + t.screens < 2)
			continue; /* the same as the close-up above */
		const std::vector<Placed> placed = assignSlotsFor(t.key, spec, items, kinds);
		if (!placed.empty() && makeScene(placed, templateName(t.key), lock, name))
			created++;
	}
	if (created > 0)
		obs_frontend_save();
	return created;
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

core::FrameMode frameMode(obs_sceneitem_t *item)
{
	if (!item)
		return core::FrameMode::Fill;
	OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(item);
	const char *v = obs_data_get_string(priv, kFrameKey);
	if (strcmp(v, "fit") == 0)
		return core::FrameMode::Fit;
	if (strcmp(v, "stretch") == 0)
		return core::FrameMode::Stretch;
	return core::FrameMode::Fill;
}

bool setFrameMode(core::FrameMode mode)
{
	const char *key = mode == core::FrameMode::Fit ? "fit" : mode == core::FrameMode::Stretch ? "stretch" : "fill";
	const std::vector<ItemEntry> sel = selectionCopy();
	if (sel.empty() || !EditContext::instance().canEdit())
		return false;
	for (const ItemEntry &e : sel) {
		OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(e.item);
		obs_data_set_string(priv, kFrameKey, key);
	}
	/* The same frame, filled the new way. */
	std::vector<Change> changes;
	for (const ItemEntry &e : sel) {
		if (!e.locked && !e.isGroup)
			changes.push_back({e.item, core::setFrame(e.model, e.parent, e.frame.width, e.frame.height,
								  mode, ref())});
	}
	const bool ok = changes.empty() || EditSession::run(tr("Undo.Frame"), changes);
	EditContext::instance().markDirty();
	return ok;
}

bool setRatio(double ratio)
{
	return applyPerItem(
		"Undo.Frame",
		[&](const ItemEntry &e) {
			if (e.isGroup)
				return e.model.t;
			return core::setFrameRatio(e.model, e.parent, ratio, frameMode(e.item), ref());
		},
		true);
}

} // namespace layouts::commands
