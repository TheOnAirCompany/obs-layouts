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

#include "edit-context.hpp"

#include "obs-convert.hpp"
#include "store.hpp"

#include <obs-module.h>

#include <QMetaObject>

#include <algorithm>

namespace layouts {

namespace {

const char *kSceneSignals[] = {"item_add",    "item_remove",   "reorder",        "refresh",    "item_visible",
			       "item_select", "item_deselect", "item_transform", "item_locked"};

void onSceneSignal(void *param, calldata_t *)
{
	static_cast<EditContext *>(param)->markDirty();
}

void onSceneRemoved(void *param, calldata_t *)
{
	auto *ctx = static_cast<EditContext *>(param);
	QMetaObject::invokeMethod(ctx, [ctx] { ctx->follow(); }, Qt::QueuedConnection);
}

ItemEntry makeEntry(obs_sceneitem_t *item, obs_sceneitem_t *group, const core::Affine &parent)
{
	ItemEntry e;
	e.item = item;
	e.group = group;
	e.id = obs_sceneitem_get_id(item);
	e.groupId = group ? obs_sceneitem_get_id(group) : 0;

	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_info2(item, &info);
	obs_sceneitem_get_crop(item, &crop);
	e.model.t = fromObs(info, crop);

	obs_source_t *src = obs_sceneitem_get_source(item);
	e.model.sourceWidth = obs_source_get_width(src);
	e.model.sourceHeight = obs_source_get_height(src);
	e.parent = parent;
	e.frame = core::visualFrame(e.model, e.parent);
	e.locked = obs_sceneitem_locked(item);
	e.visible = obs_sceneitem_visible(item);
	e.selected = obs_sceneitem_selected(item);
	e.isGroup = obs_sceneitem_is_group(item);
	e.name = QString::fromUtf8(obs_source_get_name(src));
	const char *display = obs_source_get_display_name(obs_source_get_id(src));
	e.typeName = QString::fromUtf8(display ? display : obs_source_get_id(src));
	return e;
}

struct EnumState {
	std::vector<ItemEntry> *out;
	std::vector<OBSSource> *groups;
};

bool enumChild(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *p = static_cast<std::pair<std::vector<ItemEntry> *, obs_sceneitem_t *> *>(param);
	matrix4 m;
	obs_sceneitem_get_draw_transform(p->second, &m);
	p->first->push_back(makeEntry(item, p->second, fromMatrix(m)));
	return true;
}

bool enumTop(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *st = static_cast<EnumState *>(param);
	st->out->push_back(makeEntry(item, nullptr, core::Affine::identity()));
	if (obs_sceneitem_is_group(item)) {
		st->groups->emplace_back(obs_sceneitem_get_source(item));
		std::pair<std::vector<ItemEntry> *, obs_sceneitem_t *> p{st->out, item};
		obs_sceneitem_group_enum_items(item, enumChild, &p);
	}
	return true;
}

bool anyOutputActive()
{
	return obs_frontend_streaming_active() || obs_frontend_recording_active() ||
	       obs_frontend_replay_buffer_active() || obs_frontend_virtualcam_active();
}

} // namespace

EditContext &EditContext::instance()
{
	static EditContext *ctx = new EditContext();
	return *ctx;
}

EditContext::EditContext()
{
	liveTimer_.setInterval(500);
	connect(&liveTimer_, &QTimer::timeout, this, [this] {
		const LiveState s = liveState();
		if (s != lastLive_) {
			lastLive_ = s;
			emit liveStateChanged();
		}
	});
	liveTimer_.start();
}

obs_scene_t *EditContext::scene() const
{
	return scene_ ? obs_scene_from_source(scene_) : nullptr;
}

QString EditContext::sceneName() const
{
	return scene_ ? QString::fromUtf8(obs_source_get_name(scene_)) : QString();
}

QString EditContext::sceneUuid() const
{
	return scene_ ? QString::fromUtf8(obs_source_get_uuid(scene_)) : QString();
}

obs_source_t *EditContext::followedScene() const
{
	/* Returns a new reference. */
	if (obs_frontend_preview_program_mode_active())
		return obs_frontend_get_current_preview_scene();
	return obs_frontend_get_current_scene();
}

void EditContext::follow()
{
	following_ = true;
	OBSSourceAutoRelease s = followedScene();
	setScene(s);
}

void EditContext::pin(obs_source_t *sceneSource)
{
	if (!sceneSource || !obs_source_is_scene(sceneSource)) {
		follow();
		return;
	}
	following_ = false;
	setScene(sceneSource);
}

void EditContext::setScene(obs_source_t *source)
{
	if (source == scene_.Get()) {
		emit sceneChanged(); /* following state may have changed */
		return;
	}
	disconnectSignals();
	scene_ = source;
	selectionOrder_.clear();
	groupScenes_.clear();
	dirty_ = true;
	if (scene_) {
		rebuild();
		connectSignals();
	} else {
		retire();
	}
	lastLive_ = liveState();
	emit sceneChanged();
	emit itemsChanged();
	emit liveStateChanged();
}

void EditContext::connectSignals()
{
	if (!scene_)
		return;
	auto connectTo = [this](obs_source_t *src) {
		signal_handler_t *sh = obs_source_get_signal_handler(src);
		for (const char *name : kSceneSignals)
			signals_.push_back(std::make_unique<OBSSignal>(sh, name, onSceneSignal, this));
	};
	connectTo(scene_);
	signals_.push_back(
		std::make_unique<OBSSignal>(obs_source_get_signal_handler(scene_), "remove", onSceneRemoved, this));
	for (const OBSSource &g : groupScenes_)
		connectTo(g);
}

void EditContext::disconnectSignals()
{
	signals_.clear(); /* OBSSignal disconnects in its destructor */
}

void EditContext::shutdown()
{
	disconnectSignals();
	liveTimer_.stop();
	scene_ = nullptr;
	items_ = std::make_shared<std::vector<ItemEntry>>();
	retired_.clear();
	groupScenes_.clear();
	emit sceneChanged(); /* views release what they hold on the scene */
}

void EditContext::retire()
{
	retired_.push_back(items_);
	items_ = std::make_shared<std::vector<ItemEntry>>();
	if (!releasePosted_) {
		releasePosted_ = true;
		QTimer::singleShot(0, this, [this] {
			retired_.clear();
			releasePosted_ = false;
		});
	}
}

void EditContext::markDirty()
{
	dirty_ = true;
	if (!posted_.exchange(true))
		QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
}

void EditContext::setPreview(const std::vector<core::Quad> &quads)
{
	if (quads.empty() && preview_.empty())
		return;
	preview_ = quads;
	emit previewChanged(); /* not itemsChanged: the Inspector must not refresh for a preview */
}

void EditContext::refresh()
{
	posted_ = false;
	emit itemsChanged();
}

void EditContext::rebuild()
{
	dirty_ = false;
	retire();
	obs_scene_t *s = scene();
	if (!s)
		return;

	std::vector<OBSSource> groups;
	EnumState st{items_.get(), &groups};
	obs_scene_enum_items(s, enumTop, &st);

	/* Follow groups that appeared or disappeared: their items signal on their own scene. */
	bool sameGroups = groups.size() == groupScenes_.size();
	for (size_t i = 0; sameGroups && i < groups.size(); i++)
		sameGroups = groups[i].Get() == groupScenes_[i].Get();
	if (!sameGroups) {
		disconnectSignals();
		groupScenes_ = std::move(groups);
		connectSignals();
	}

	/* Reconcile the selection order with the real selection flags. */
	std::vector<std::pair<int64_t, int64_t>> order;
	for (const auto &key : selectionOrder_) {
		const bool stillSelected = std::any_of(items_->begin(), items_->end(), [&](const ItemEntry &e) {
			return e.selected && e.groupId == key.first && e.id == key.second;
		});
		if (stillSelected)
			order.push_back(key);
	}
	for (const ItemEntry &e : *items_) {
		const std::pair<int64_t, int64_t> key{e.groupId, e.id};
		if (e.selected && std::find(order.begin(), order.end(), key) == order.end())
			order.push_back(key);
	}
	selectionOrder_ = std::move(order);
}

const std::vector<ItemEntry> &EditContext::items()
{
	if (dirty_)
		rebuild();
	return *items_;
}

const ItemEntry *EditContext::find(int64_t id, int64_t groupId)
{
	for (const ItemEntry &e : items()) {
		if (e.id == id && e.groupId == groupId)
			return &e;
	}
	return nullptr;
}

std::vector<const ItemEntry *> EditContext::selection()
{
	items();
	std::vector<const ItemEntry *> out;
	for (const auto &key : selectionOrder_) {
		if (const ItemEntry *e = find(key.second, key.first))
			out.push_back(e);
	}
	return out;
}

const ItemEntry *EditContext::topmostAt(const core::Vec2 &world, bool deep)
{
	const std::vector<ItemEntry> &list = items();
	for (auto it = list.rbegin(); it != list.rend(); ++it) {
		const ItemEntry &e = *it;
		if (!e.visible)
			continue;
		if (e.groupId != 0) {
			if (!deep)
				continue;
			const ItemEntry *g = find(e.groupId, 0);
			if (g && !g->visible)
				continue;
		} else if (deep && e.isGroup) {
			continue; /* look inside groups */
		}
		if (e.frame.quad.contains(world))
			return &e;
	}
	if (deep)
		return topmostAt(world, false);
	return nullptr;
}

void EditContext::select(const std::vector<const ItemEntry *> &list, bool additive)
{
	/* Copy the refs first: selecting fires signals that may rebuild the snapshot. */
	std::vector<OBSSceneItem> want;
	std::vector<std::pair<int64_t, int64_t>> keys;
	for (const ItemEntry *e : list) {
		want.push_back(e->item);
		keys.emplace_back(e->groupId, e->id);
	}
	std::vector<OBSSceneItem> deselect;
	if (!additive) {
		for (const ItemEntry &e : items()) {
			if (e.selected &&
			    std::find(keys.begin(), keys.end(), std::make_pair(e.groupId, e.id)) == keys.end())
				deselect.push_back(e.item);
		}
		selectionOrder_.clear();
	}
	for (const OBSSceneItem &i : deselect)
		obs_sceneitem_select(i, false);
	for (size_t i = 0; i < want.size(); i++) {
		obs_sceneitem_select(want[i], true);
		selectionOrder_.erase(std::remove(selectionOrder_.begin(), selectionOrder_.end(), keys[i]),
				      selectionOrder_.end());
		selectionOrder_.push_back(keys[i]);
	}
	markDirty();
}

void EditContext::toggle(const ItemEntry *e)
{
	if (!e)
		return;
	OBSSceneItem item = e->item;
	const bool sel = !e->selected;
	const std::pair<int64_t, int64_t> key{e->groupId, e->id};
	obs_sceneitem_select(item, sel);
	selectionOrder_.erase(std::remove(selectionOrder_.begin(), selectionOrder_.end(), key), selectionOrder_.end());
	if (sel)
		selectionOrder_.push_back(key);
	markDirty();
}

void EditContext::clearSelection()
{
	std::vector<OBSSceneItem> deselect;
	for (const ItemEntry &e : items()) {
		if (e.selected)
			deselect.push_back(e.item);
	}
	for (const OBSSceneItem &i : deselect)
		obs_sceneitem_select(i, false);
	selectionOrder_.clear();
	markDirty();
}

uint32_t EditContext::canvasWidth() const
{
	if (scene_)
		return obs_source_get_width(scene_);
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_width : 1920;
}

uint32_t EditContext::canvasHeight() const
{
	if (scene_)
		return obs_source_get_height(scene_);
	obs_video_info ovi = {};
	return obs_get_video_info(&ovi) ? ovi.base_height : 1080;
}

bool EditContext::isProgramOfMainCanvas() const
{
	if (!scene_ || obs_frontend_preview_program_mode_active())
		return false;
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	if (program.Get() != scene_.Get())
		return false;
	obs_canvas_t *canvas = obs_source_get_canvas(scene_);
	obs_canvas_t *main = obs_get_main_canvas();
	const bool same = canvas == main;
	obs_canvas_release(canvas);
	obs_canvas_release(main);
	return same;
}

LiveState EditContext::liveState() const
{
	if (!scene_ || !obs_source_active(scene_))
		return LiveState::Off;
	return anyOutputActive() ? LiveState::OnAir : LiveState::Program;
}

bool EditContext::canEdit()
{
	if (!scene_)
		return false;
	if (Store::instance().settings().blockOnAir && liveState() == LiveState::OnAir) {
		emit editRefused(QString::fromUtf8(obs_module_text("Live.Blocked")));
		return false;
	}
	return true;
}

void EditContext::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		if (following_)
			follow();
		else
			emit liveStateChanged();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		following_ = true;
		setScene(nullptr);
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		if (following_ || !scene_)
			follow();
		else
			emit sceneChanged(); /* refresh scene pickers */
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		lastLive_ = liveState();
		emit liveStateChanged();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		shutdown();
		break;
	default:
		break;
	}
}

} // namespace layouts
