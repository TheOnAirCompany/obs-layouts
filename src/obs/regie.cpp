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

#include "regie.hpp"
#include "failover.hpp"
#include "morph.hpp"

#include "commands.hpp"
#include "obs-convert.hpp"
#include "roles.hpp"
#include "store.hpp"

#include "third-party/obs-websocket-api.h"

#include <obs-module.h>
#include <plugin-support.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <functional>
#include <memory>

namespace layouts {

namespace {

constexpr const char *kSaveKey = "layouts-regie";

struct ItemRef {
	obs_sceneitem_t *item;
	std::string name;
};

/* Top-level items, from the top of the OBS list (reading order of the Sources dock). */
std::vector<ItemRef> topItems(obs_scene_t *scene)
{
	std::vector<ItemRef> out;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			const char *n = obs_source_get_name(obs_sceneitem_get_source(item));
			static_cast<std::vector<ItemRef> *>(p)->push_back({item, n ? n : ""});
			return true;
		},
		&out);
	std::reverse(out.begin(), out.end());
	return out;
}

bool visibleIn(obs_source_t *sceneSource, const std::string &name)
{
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene || name.empty())
		return false;
	obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str());
	return item && obs_sceneitem_visible(item);
}

core::Rect canvasRect(obs_source_t *scene)
{
	obs_video_info ovi = {};
	bool ok = false;
	if (scene) {
		obs_canvas_t *canvas = obs_source_get_canvas(scene);
		if (canvas) {
			ok = obs_canvas_get_video_info(canvas, &ovi);
			obs_canvas_release(canvas);
		}
	}
	if (!ok)
		ok = obs_get_video_info(&ovi);
	if (!ok || !ovi.base_width || !ovi.base_height)
		return {0, 0, 1920, 1080};
	return {0, 0, (double)ovi.base_width, (double)ovi.base_height};
}

core::BroadcastSpec spec()
{
	core::BroadcastSpec s;
	s.gap = Store::instance().settings().broadcastGap;
	s.margin = Store::instance().settings().broadcastMargin;
	return s;
}

bool duplicationOn()
{
	config_t *cfg = obs_frontend_get_user_config();
	return !cfg || config_get_bool(cfg, "BasicWindow", "SceneDuplicationMode");
}

/* Runs on the UI thread and waits (hotkeys and websocket requests come from other
 * threads). */
void onUiThread(const std::function<void()> &fn)
{
	QCoreApplication *app = QCoreApplication::instance();
	if (!app || QThread::currentThread() == app->thread())
		fn();
	else
		QMetaObject::invokeMethod(app, fn, Qt::BlockingQueuedConnection);
}

/* ---- hotkeys ---- */
struct Action {
	std::string name;
	std::string description;
	std::function<void()> run;
	obs_hotkey_id id = OBS_INVALID_HOTKEY_ID;
};
std::vector<std::unique_ptr<Action>> g_actions;
constexpr int kHotkeySlots = 4;
constexpr int kHotkeySources = 8;

void onHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *a = static_cast<Action *>(data);
	QMetaObject::invokeMethod(QCoreApplication::instance(), [a] { a->run(); }, Qt::QueuedConnection);
}

std::string hotkeysPath()
{
	char *p = obs_module_config_path("regie-hotkeys.json");
	std::string s = p ? p : "";
	bfree(p);
	return s;
}

/* ---- obs-websocket ---- */
void reply(obs_data_t *res, bool ok, const char *error = nullptr)
{
	obs_data_set_bool(res, "ok", ok);
	if (error)
		obs_data_set_string(res, "error", error);
}

void wsGetState(obs_data_t *, obs_data_t *res, void *)
{
	onUiThread([res] {
		Regie::instance().stateTo(res);
		reply(res, true);
	});
}

void wsListLayouts(obs_data_t *, obs_data_t *res, void *)
{
	onUiThread([res] {
		OBSDataArrayAutoRelease arr = obs_data_array_create();
		for (const core::BroadcastTemplate &t : Regie::instance().layouts()) {
			OBSDataAutoRelease d = obs_data_create();
			obs_data_set_string(d, "key", t.key.c_str());
			obs_data_set_string(d, "name", obs_module_text(("Broadcast." + t.key).c_str()));
			obs_data_set_int(d, "cameras", t.cameras);
			obs_data_set_int(d, "screens", t.screens);
			obs_data_set_int(d, "chats", t.chats);
			obs_data_array_push_back(arr, d);
		}
		obs_data_set_array(res, "layouts", arr);
		reply(res, true);
	});
}

void wsSetLayout(obs_data_t *req, obs_data_t *res, void *)
{
	const std::string key = obs_data_get_string(req, "key");
	onUiThread([res, key] { reply(res, Regie::instance().setLayout(key), "unknown or unfit layout"); });
}

void wsAssignSlot(obs_data_t *req, obs_data_t *res, void *)
{
	/* slot is 1-based, like the hotkeys and the dock */
	const int slot = (int)obs_data_get_int(req, "slot") - 1;
	const std::string source = obs_data_get_string(req, "source");
	onUiThread(
		[res, slot, source] { reply(res, Regie::instance().assign(slot, source), "unknown slot or source"); });
}

void wsSwap(obs_data_t *req, obs_data_t *res, void *)
{
	const int a = (int)obs_data_get_int(req, "a") - 1;
	const int b = (int)obs_data_get_int(req, "b") - 1;
	onUiThread([res, a, b] { reply(res, Regie::instance().swap(a, b), "unknown slot"); });
}

void wsTake(obs_data_t *, obs_data_t *res, void *)
{
	onUiThread([res] { reply(res, Regie::instance().take(), "Studio Mode is off"); });
}

void wsAuto(obs_data_t *, obs_data_t *res, void *)
{
	onUiThread([res] { reply(res, Regie::instance().autoTake(), "Nothing to animate"); });
}

} // namespace

Regie &Regie::instance()
{
	static Regie r;
	return r;
}

OBSSource Regie::plateauSource() const
{
	if (!plateauUuid_.empty()) {
		OBSSourceAutoRelease byId = obs_get_source_by_uuid(plateauUuid_.c_str());
		if (byId && obs_scene_from_source(byId)) {
			/* Renamed since: follow the scene. */
			const QString now = QString::fromUtf8(obs_source_get_name(byId));
			if (now != plateau_)
				plateau_ = now;
			return OBSSource(byId.Get());
		}
	}
	if (plateau_.isEmpty())
		return nullptr;
	OBSSourceAutoRelease s = obs_get_source_by_name(plateau_.toUtf8().constData());
	if (!s || !obs_scene_from_source(s))
		return nullptr;
	return OBSSource(s.Get());
}

void Regie::setPlateau(const QString &sceneName)
{
	if (sceneName == plateau_)
		return;
	plateau_ = sceneName;
	{
		OBSSourceAutoRelease src = obs_get_source_by_name(sceneName.toUtf8().constData());
		plateauUuid_ = src ? obs_source_get_uuid(src) : "";
	}
	preview_ = {};
	program_ = {};
	preparedOn_ = nullptr;
	Failover::instance().disarm();
	refresh();
	obs_frontend_save();
}

double Regie::canvasAspect() const
{
	const core::Rect r = canvasRect(plateauSource());
	return r.w / r.h;
}

std::vector<core::Slot> Regie::slotsOf(const std::string &key) const
{
	return core::buildBroadcast(key, canvasRect(plateauSource()), spec());
}

bool Regie::studioMode() const
{
	return obs_frontend_preview_program_mode_active();
}

bool Regie::onProgram(const std::string &source) const
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	return visibleIn(scene, source);
}

bool Regie::onPreview(const std::string &source) const
{
	if (!studioMode())
		return false;
	OBSSourceAutoRelease scene = obs_frontend_get_current_preview_scene();
	return visibleIn(scene, source);
}

void Regie::scan()
{
	sources_.clear();
	layouts_.clear();
	OBSSource plateau = plateauSource();
	obs_scene_t *scene = obs_scene_from_source(plateau);
	if (!scene)
		return;
	int cams = 0, screens = 0, chats = 0;
	for (const ItemRef &r : topItems(scene)) {
		obs_source_t *src = obs_sceneitem_get_source(r.item);
		if (obs_source_is_group(src) || obs_source_is_scene(src))
			continue;
		const core::Role role = roles::role(src);
		if (core::isCameraRole(role)) {
			sources_.push_back({r.name, core::SlotKind::Camera});
			cams++;
		} else if (role == core::Role::Screen) {
			sources_.push_back({r.name, core::SlotKind::Screen});
			screens++;
		} else if (role == core::Role::Chat) {
			sources_.push_back({r.name, core::SlotKind::Chat});
			chats++;
		}
	}
	/* Every layout the Plateau can fill (a mixer also wants the shots with fewer
	 * guests than the set holds). */
	const core::Rect c = canvasRect(plateau);
	const bool portrait = c.h > c.w;
	for (const core::BroadcastTemplate &t : core::broadcastTemplates()) {
		if (portrait && t.landscapeOnly)
			continue;
		if (t.cameras + t.screens > 0 && t.cameras <= cams && t.screens <= screens && t.chats <= chats)
			layouts_.push_back(t);
	}
}

void Regie::fill(RegieComposition &c) const
{
	const std::vector<core::Slot> tiles = slotsOf(c.key);
	std::vector<std::string> kept(tiles.size());
	std::vector<std::string> used;
	auto kindOf = [this](const std::string &name, core::SlotKind &kind) {
		for (const RegieSource &s : sources_) {
			if (s.name == name) {
				kind = s.kind;
				return true;
			}
		}
		return false;
	};
	/* Keep what the operator chose when it still fits the slot... */
	for (size_t i = 0; i < tiles.size() && i < c.tiles.size(); i++) {
		core::SlotKind kind;
		if (kindOf(c.tiles[i], kind) && kind == tiles[i].kind &&
		    std::find(used.begin(), used.end(), c.tiles[i]) == used.end()) {
			kept[i] = c.tiles[i];
			used.push_back(c.tiles[i]);
		}
	}
	/* ...then fill the empty tiles with the next free sources of the right kind, the
	 * sources of the previous composition first (a layout change keeps the guests). */
	std::vector<std::string> order = c.tiles;
	for (const RegieSource &s : sources_)
		order.push_back(s.name);
	for (size_t i = 0; i < tiles.size(); i++) {
		if (!kept[i].empty())
			continue;
		for (const std::string &name : order) {
			core::SlotKind kind;
			if (kindOf(name, kind) && kind == tiles[i].kind &&
			    std::find(used.begin(), used.end(), name) == used.end()) {
				kept[i] = name;
				used.push_back(name);
				break;
			}
		}
	}
	c.tiles = kept;
}

void Regie::refresh()
{
	scan();
	const bool fits = std::any_of(layouts_.begin(), layouts_.end(),
				      [this](const core::BroadcastTemplate &t) { return t.key == preview_.key; });
	if (!fits)
		preview_.key = layouts_.empty() ? std::string() : layouts_.front().key;
	fill(preview_);
	emitChanged();
}

OBSSource Regie::twin(bool create)
{
	OBSSource plateau = plateauSource();
	if (!plateau)
		return nullptr;
	const QString name = plateau_ + " B";
	OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
	if (existing && obs_scene_from_source(existing)) {
		/* Sources added to the Plateau since: add them to the twin too. */
		obs_scene_t *b = obs_scene_from_source(existing);
		for (const ItemRef &r : topItems(obs_scene_from_source(plateau))) {
			if (obs_scene_find_source(b, r.name.c_str()))
				continue;
			obs_source_t *src = obs_sceneitem_get_source(r.item);
			if (obs_source_is_group(src))
				continue;
			obs_sceneitem_t *added = obs_scene_add(b, src);
			if (added) {
				obs_transform_info info = {};
				obs_sceneitem_crop crop = {};
				obs_sceneitem_get_info2(r.item, &info);
				obs_sceneitem_get_crop(r.item, &crop);
				obs_sceneitem_set_info2(added, &info);
				obs_sceneitem_set_crop(added, &crop);
			}
		}
		return OBSSource(existing.Get());
	}
	if (!create)
		return nullptr;
	/* Same sources, shared (references), groups copied by OBS. */
	obs_scene_t *dup =
		obs_scene_duplicate(obs_scene_from_source(plateau), name.toUtf8().constData(), OBS_SCENE_DUP_REFS);
	if (!dup)
		return nullptr;
	OBSSource out(obs_scene_get_source(dup));
	obs_scene_release(dup);
	return out;
}

obs_source_t *Regie::targetScene()
{
	OBSSource plateau = plateauSource();
	if (!plateau)
		return nullptr;
	if (!studioMode() || duplicationOn())
		return plateau;
	/* No duplication: never touch the scene on program, use the other one. */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	if (program.Get() != plateau.Get())
		return plateau;
	OBSSource b = twin(true);
	return b ? b.Get() : nullptr;
}

/* Where every managed item of a scene goes for a composition: slot transform (mirror,
 * rotation, flips kept) or hidden. Items in slot order first, back to front. */
std::vector<morph::Target> Regie::targetsFor(obs_source_t *sceneSource, const RegieComposition &c,
					     std::vector<std::pair<obs_sceneitem_t *, core::Slot>> &placed) const
{
	std::vector<morph::Target> out;
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene || c.key.empty())
		return out;
	const std::vector<core::Slot> tiles = commands::broadcastSlots(c.key, canvasRect(sceneSource), spec());
	if (tiles.empty())
		return out;
	auto managed = [this](const std::string &name) {
		return std::any_of(sources_.begin(), sources_.end(),
				   [&](const RegieSource &s) { return s.name == name; });
	};
	for (const ItemRef &r : topItems(scene)) {
		if (!managed(r.name))
			continue;
		const auto it = std::find(c.tiles.begin(), c.tiles.end(), r.name);
		const size_t i = (size_t)(it - c.tiles.begin());
		obs_source_t *src = obs_sceneitem_get_source(r.item);
		obs_transform_info info = {};
		obs_sceneitem_crop crop = {};
		obs_sceneitem_get_info2(r.item, &info);
		obs_sceneitem_get_crop(r.item, &crop);
		const core::ItemModel m{fromObs(info, crop), obs_source_get_width(src), obs_source_get_height(src)};
		if (it == c.tiles.end() || i >= tiles.size()) {
			out.push_back({OBSSceneItem(r.item), m.t, false});
			continue;
		}
		out.push_back({OBSSceneItem(r.item), commands::slotTransform(m, tiles[i]), true});
		placed.push_back({r.item, tiles[i]});
	}
	return out;
}

/* After the items are in place: slot styles (round, diagonal, frame) and stacking. */
static void finishPlacement(std::vector<std::pair<obs_sceneitem_t *, core::Slot>> &placed,
			    const std::vector<core::Slot> &order)
{
	std::vector<obs_sceneitem_t *> stack;
	for (const core::Slot &slot : order) {
		for (const auto &[item, sl] : placed) {
			if (sl.cell == slot.cell && sl.kind == slot.kind)
				stack.push_back(item);
		}
	}
	for (const auto &[item, sl] : placed)
		commands::applySlotStyle(item, sl);
	commands::stackInSlotOrder(stack);
}

bool Regie::apply(obs_source_t *sceneSource, const RegieComposition &c)
{
	std::vector<std::pair<obs_sceneitem_t *, core::Slot>> placed;
	const std::vector<morph::Target> targets = targetsFor(sceneSource, c, placed);
	if (targets.empty())
		return false;
	for (const morph::Target &t : targets) {
		if (t.visible) {
			obs_transform_info info = {};
			obs_sceneitem_crop crop = {};
			toObs(t.t, info, crop);
			obs_sceneitem_defer_update_begin(t.item);
			obs_sceneitem_set_info2(t.item, &info);
			obs_sceneitem_set_crop(t.item, &crop);
			obs_sceneitem_defer_update_end(t.item);
		}
		obs_sceneitem_set_visible(t.item, t.visible);
	}
	finishPlacement(placed, commands::broadcastSlots(c.key, canvasRect(sceneSource), spec()));
	return true;
}

/* AUTO (0.6): the program scene glides to the prepared composition, no transition. */
bool Regie::autoTake()
{
	if (!plateauSource() || preview_.key.empty())
		return false;
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	obs_scene_t *scene = program ? obs_scene_from_source(program) : nullptr;
	if (!scene)
		return false;
	std::vector<std::pair<obs_sceneitem_t *, core::Slot>> placed;
	const std::vector<morph::Target> targets = targetsFor(program, preview_, placed);
	if (targets.empty())
		return false;
	morph::applyAnimated(scene, targets);
	finishPlacement(placed, commands::broadcastSlots(preview_.key, canvasRect(program), spec()));
	program_ = preview_;
	armFailover();
	emitChanged();
	return true;
}

bool Regie::prepare()
{
	fill(preview_);
	obs_source_t *target = targetScene();
	if (!target || !apply(target, preview_)) {
		emitChanged();
		return false;
	}
	preparedOn_ = OBSGetWeakRef(target);
	if (studioMode()) {
		OBSSourceAutoRelease shown = obs_frontend_get_current_preview_scene();
		if (shown.Get() != target)
			obs_frontend_set_current_preview_scene(target);
	} else {
		program_ = preview_; /* no preview: on air at once */
		armFailover();
	}
	emitChanged();
	return true;
}

bool Regie::setLayout(const std::string &key)
{
	const bool fits = std::any_of(layouts_.begin(), layouts_.end(),
				      [&](const core::BroadcastTemplate &t) { return t.key == key; });
	if (!fits)
		return false;
	preview_.key = key;
	return prepare();
}

bool Regie::nextLayout(int step)
{
	if (layouts_.empty())
		return false;
	int i = 0;
	for (size_t k = 0; k < layouts_.size(); k++) {
		if (layouts_[k].key == preview_.key)
			i = (int)k;
	}
	const int n = (int)layouts_.size();
	i = ((i + step) % n + n) % n;
	return setLayout(layouts_[(size_t)i].key);
}

bool Regie::favorite(int index)
{
	if (index < 0 || index >= (int)layouts_.size())
		return false;
	return setLayout(layouts_[(size_t)index].key);
}

bool Regie::assign(int slot, const std::string &source)
{
	const std::vector<core::Slot> tiles = slotsOf(preview_.key);
	if (slot < 0 || slot >= (int)tiles.size())
		return false;
	const auto src =
		std::find_if(sources_.begin(), sources_.end(), [&](const RegieSource &s) { return s.name == source; });
	if (src == sources_.end())
		return false;
	preview_.tiles.resize(tiles.size());
	const auto other = std::find(preview_.tiles.begin(), preview_.tiles.end(), source);
	if (other != preview_.tiles.end()) {
		const int o = (int)(other - preview_.tiles.begin());
		if (o == slot)
			return true;
		/* Already in another slot: the two swap (when the kinds allow it). */
		if (tiles[(size_t)o].kind != tiles[(size_t)slot].kind && !preview_.tiles[(size_t)slot].empty())
			return false;
		return swap(o, slot);
	}
	if (src->kind != tiles[(size_t)slot].kind)
		return false;
	preview_.tiles[(size_t)slot] = source;
	return prepare();
}

bool Regie::swap(int a, int b)
{
	const std::vector<core::Slot> tiles = slotsOf(preview_.key);
	if (a < 0 || b < 0 || a >= (int)tiles.size() || b >= (int)tiles.size())
		return false;
	preview_.tiles.resize(tiles.size());
	std::swap(preview_.tiles[(size_t)a], preview_.tiles[(size_t)b]);
	return prepare();
}

bool Regie::take()
{
	if (!studioMode() || !plateauSource())
		return false;
	OBSSource prepared = OBSGetStrongRef(preparedOn_);
	if (!prepared) {
		if (!prepare())
			return false;
		prepared = OBSGetStrongRef(preparedOn_);
	}
	OBSSourceAutoRelease shown = obs_frontend_get_current_preview_scene();
	if (prepared && shown.Get() != prepared.Get())
		obs_frontend_set_current_preview_scene(prepared);
	obs_frontend_preview_program_trigger_transition();
	program_ = preview_;
	armFailover();
	emitChanged();
	return true;
}

/* The lost-camera failover watches what is on air: the scene that just went to program
 * (the Plateau or its twin) with its layout and who sits in which slot. */
void Regie::armFailover()
{
	if (program_.key.empty()) {
		Failover::instance().disarm();
		return;
	}
	OBSSource scene = OBSGetStrongRef(preparedOn_);
	if (!scene)
		scene = plateauSource();
	if (!scene)
		return;
	Failover::instance().arm(QString::fromUtf8(obs_source_get_name(scene)), program_.key, program_.tiles);
}

void Regie::stateTo(obs_data_t *out) const
{
	obs_data_set_string(out, "plateau", plateau_.toUtf8().constData());
	obs_data_set_bool(out, "studioMode", studioMode());
	auto comp = [this](const RegieComposition &c) {
		OBSDataAutoRelease d = obs_data_create();
		obs_data_set_string(d, "layout", c.key.c_str());
		OBSDataArrayAutoRelease arr = obs_data_array_create();
		const std::vector<core::Slot> tiles = slotsOf(c.key);
		for (size_t i = 0; i < c.tiles.size(); i++) {
			OBSDataAutoRelease s = obs_data_create();
			obs_data_set_int(s, "slot", (long long)i + 1);
			obs_data_set_string(s, "kind",
					    i >= tiles.size()                         ? "camera"
					    : tiles[i].kind == core::SlotKind::Screen ? "screen"
					    : tiles[i].kind == core::SlotKind::Chat   ? "chat"
										      : "camera");
			obs_data_set_string(s, "source", c.tiles[i].c_str());
			obs_data_array_push_back(arr, s);
		}
		obs_data_set_array(d, "slots", arr);
		return d;
	};
	OBSDataAutoRelease pv = comp(preview_);
	OBSDataAutoRelease pg = comp(program_);
	obs_data_set_obj(out, "preview", pv);
	obs_data_set_obj(out, "program", pg);
	OBSDataArrayAutoRelease srcs = obs_data_array_create();
	for (const RegieSource &s : sources_) {
		OBSDataAutoRelease d = obs_data_create();
		obs_data_set_string(d, "name", s.name.c_str());
		obs_data_set_string(d, "kind", s.kind == core::SlotKind::Screen ? "screen" : "camera");
		obs_data_set_bool(d, "program", onProgram(s.name));
		obs_data_set_bool(d, "preview", onPreview(s.name));
		obs_data_array_push_back(srcs, d);
	}
	obs_data_set_array(out, "sources", srcs);
}

void Regie::emitChanged()
{
	emit changed();
	if (vendor_) {
		OBSDataAutoRelease d = obs_data_create();
		stateTo(d);
		obs_websocket_vendor_emit_event(vendor_, "StateChanged", d);
	}
}

void Regie::save(obs_data_t *data)
{
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "plateau", plateau_.toUtf8().constData());
	obs_data_set_string(d, "plateau_uuid", plateauUuid_.c_str());
	obs_data_set_string(d, "layout", preview_.key.c_str());
	OBSDataArrayAutoRelease arr = obs_data_array_create();
	for (const std::string &s : preview_.tiles) {
		OBSDataAutoRelease e = obs_data_create();
		obs_data_set_string(e, "source", s.c_str());
		obs_data_array_push_back(arr, e);
	}
	obs_data_set_array(d, "slots", arr);
	obs_data_set_obj(data, kSaveKey, d);
}

void Regie::load(obs_data_t *data)
{
	OBSDataAutoRelease d = obs_data_get_obj(data, kSaveKey);
	plateau_ = d ? QString::fromUtf8(obs_data_get_string(d, "plateau")) : QString();
	plateauUuid_ = d ? obs_data_get_string(d, "plateau_uuid") : "";
	preview_ = {};
	program_ = {};
	preparedOn_ = nullptr;
	if (d) {
		preview_.key = obs_data_get_string(d, "layout");
		OBSDataArrayAutoRelease arr = obs_data_get_array(d, "slots");
		for (size_t i = 0; arr && i < obs_data_array_count(arr); i++) {
			OBSDataAutoRelease e = obs_data_array_item(arr, i);
			preview_.tiles.push_back(obs_data_get_string(e, "source"));
		}
		program_ = preview_;
	}
	refresh();
}

void Regie::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		plateau_.clear();
		preview_ = {};
		program_ = {};
		preparedOn_ = nullptr;
		sources_.clear();
		layouts_.clear();
		emitChanged();
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		refresh();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		emitChanged(); /* tally */
		break;
	default:
		break;
	}
}

void Regie::registerHotkeys()
{
	if (!g_actions.empty())
		return;
	auto add = [](std::string name, std::string desc, std::function<void()> fn) {
		auto a = std::make_unique<Action>();
		a->name = std::move(name);
		a->description = std::string("Layouts: ") + desc;
		a->run = std::move(fn);
		g_actions.push_back(std::move(a));
	};
	add("layouts.regie.take", obs_module_text("Regie.Hotkey.Take"), [] { Regie::instance().take(); });
	add("layouts.regie.next", obs_module_text("Regie.Hotkey.Next"), [] { Regie::instance().nextLayout(1); });
	add("layouts.regie.previous", obs_module_text("Regie.Hotkey.Previous"),
	    [] { Regie::instance().nextLayout(-1); });
	for (int i = 0; i < 9; i++) {
		add("layouts.regie.layout_" + std::to_string(i + 1),
		    QString::fromUtf8(obs_module_text("Regie.Hotkey.Layout")).arg(i + 1).toStdString(),
		    [i] { Regie::instance().favorite(i); });
	}
	for (int s = 0; s < kHotkeySlots; s++) {
		for (int m = 0; m < kHotkeySources; m++) {
			add("layouts.regie.slot_" + std::to_string(s + 1) + "_source_" + std::to_string(m + 1),
			    QString::fromUtf8(obs_module_text("Regie.Hotkey.SlotSource"))
				    .arg(s + 1)
				    .arg(m + 1)
				    .toStdString(),
			    [s, m] {
				    const auto &src = Regie::instance().sources();
				    if (m < (int)src.size())
					    Regie::instance().assign(s, src[(size_t)m].name);
			    });
		}
	}
	OBSDataAutoRelease saved = obs_data_create_from_json_file_safe(hotkeysPath().c_str(), "bak");
	for (auto &a : g_actions) {
		a->id = obs_hotkey_register_frontend(a->name.c_str(), a->description.c_str(), onHotkey, a.get());
		if (saved) {
			OBSDataArrayAutoRelease arr = obs_data_get_array(saved, a->name.c_str());
			if (arr)
				obs_hotkey_load(a->id, arr);
		}
	}
}

void Regie::saveHotkeys()
{
	if (g_actions.empty())
		return;
	OBSDataAutoRelease d = obs_data_create();
	for (auto &a : g_actions) {
		OBSDataArrayAutoRelease arr = obs_hotkey_save(a->id);
		obs_data_set_array(d, a->name.c_str(), arr);
	}
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	if (!obs_data_save_json_safe(d, hotkeysPath().c_str(), "tmp", "bak"))
		obs_log(LOG_WARNING, "could not save Switcher hotkeys");
}

void Regie::unregisterHotkeys()
{
	for (auto &a : g_actions)
		obs_hotkey_unregister(a->id);
	g_actions.clear();
}

void Regie::registerVendor()
{
	if (vendor_)
		return;
	vendor_ = obs_websocket_register_vendor("layouts");
	if (!vendor_) {
		obs_log(LOG_INFO, "obs-websocket not found: Switcher requests unavailable");
		return;
	}
	obs_websocket_vendor_register_request(vendor_, "GetState", wsGetState, nullptr);
	obs_websocket_vendor_register_request(vendor_, "ListLayouts", wsListLayouts, nullptr);
	obs_websocket_vendor_register_request(vendor_, "SetLayout", wsSetLayout, nullptr);
	obs_websocket_vendor_register_request(vendor_, "AssignSlot", wsAssignSlot, nullptr);
	obs_websocket_vendor_register_request(vendor_, "Swap", wsSwap, nullptr);
	obs_websocket_vendor_register_request(vendor_, "Take", wsTake, nullptr);
	obs_websocket_vendor_register_request(vendor_, "Auto", wsAuto, nullptr);
}

void Regie::unregisterVendor()
{
	if (!vendor_)
		return;
	for (const char *r : {"GetState", "ListLayouts", "SetLayout", "AssignSlot", "Swap", "Take", "Auto"})
		obs_websocket_vendor_unregister_request(vendor_, r);
	vendor_ = nullptr;
}

} // namespace layouts
