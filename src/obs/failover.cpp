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

#include "obs/failover.hpp"

#include "core/broadcast.hpp"
#include "obs/commands.hpp"
#include "obs/obs-convert.hpp"
#include "obs/source-health.hpp"
#include "obs/store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <algorithm>

namespace layouts {

namespace {

QString tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

/* Shows or hides an item at once: its show / hide transition is set aside for the call. */
void setVisibleDirect(obs_sceneitem_t *item, bool visible)
{
	if (obs_sceneitem_visible(item) == visible)
		return;
	obs_source_t *transition = obs_source_get_ref(obs_sceneitem_get_transition(item, visible));
	if (transition)
		obs_sceneitem_set_transition(item, visible, nullptr);
	obs_sceneitem_set_visible(item, visible);
	if (transition) {
		obs_sceneitem_set_transition(item, visible, transition);
		obs_source_release(transition);
	}
}

struct ItemOp {
	obs_sceneitem_t *item;
	obs_transform_info info;
	obs_sceneitem_crop crop;
	bool visible;
};

/* Every change in one scene update: the program never shows a half-moved layout. */
void applyOps(obs_scene_t *scene, std::vector<ItemOp> &ops)
{
	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *) {
			for (ItemOp &op : *static_cast<std::vector<ItemOp> *>(data)) {
				obs_sceneitem_defer_update_begin(op.item);
				obs_sceneitem_set_info2(op.item, &op.info);
				obs_sceneitem_set_crop(op.item, &op.crop);
				obs_sceneitem_defer_update_end(op.item);
				setVisibleDirect(op.item, op.visible);
			}
		},
		&ops);
}

QString joinNames(const std::set<std::string> &names)
{
	QStringList l;
	for (const std::string &n : names)
		l << QString::fromStdString(n);
	return l.join(QStringLiteral(", "));
}

} // namespace

Failover &Failover::instance()
{
	static Failover *failover = new Failover; /* lives until the process ends */
	return *failover;
}

Failover::Failover()
{
	connect(&SourceHealth::instance(), &SourceHealth::sourceLost, this, &Failover::onLost);
	connect(&SourceHealth::instance(), &SourceHealth::sourceBack, this, &Failover::onBack);
}

void Failover::arm(const QString &scene, const std::string &key, const core::SlotAssignment &assignment)
{
	if (applied_)
		restore();
	scene_ = scene;
	key_ = key;
	assignment_ = assignment;
	SourceHealth::instance().watch(assignment);
	lost_.clear();
	for (const std::string &n : assignment) {
		if (!n.empty() && SourceHealth::instance().isLost(n))
			lost_.insert(n);
	}
	update();
}

void Failover::disarm()
{
	if (applied_)
		restore();
	scene_.clear();
	key_.clear();
	assignment_.clear();
	lost_.clear();
	SourceHealth::instance().clear();
}

void Failover::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		/* A drop seen while the layout was off program applies once it goes live. */
		update();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
	case OBS_FRONTEND_EVENT_EXIT:
		disarm();
		break;
	default:
		break;
	}
}

void Failover::onLost(const QString &name)
{
	const std::string n = name.toStdString();
	if (std::find(assignment_.begin(), assignment_.end(), n) == assignment_.end())
		return;
	blog(LOG_WARNING, "[Layouts] failover: source '%s' lost on '%s'", n.c_str(), scene_.toUtf8().constData());
	lost_.insert(n);
	update();
}

void Failover::onBack(const QString &name)
{
	const std::string n = name.toStdString();
	if (!lost_.erase(n))
		return;
	blog(LOG_INFO, "[Layouts] failover: source '%s' back on '%s'", n.c_str(), scene_.toUtf8().constData());
	update();
}

bool Failover::sceneOnProgram() const
{
	obs_source_t *program = obs_frontend_get_current_scene();
	const bool on = program && scene_ == QString::fromUtf8(obs_source_get_name(program));
	obs_source_release(program);
	return on;
}

void Failover::update()
{
	if (key_.empty())
		return;
	if (lost_.empty() || !Store::instance().settings().failoverEnabled) {
		if (applied_)
			restore();
		return;
	}
	/* Only the layout on program reorganizes; one already in failover follows the drops. */
	if (!applied_ && !sceneOnProgram())
		return;

	obs_source_t *sceneSource = obs_get_source_by_name(scene_.toUtf8().constData());
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (!scene) {
		obs_source_release(sceneSource);
		return;
	}

	if (!applied_) {
		snapshot_.clear();
		for (const std::string &n : assignment_) {
			obs_sceneitem_t *item = n.empty() ? nullptr : obs_scene_find_source(scene, n.c_str());
			if (!item)
				continue;
			Snapshot s;
			s.name = n;
			obs_sceneitem_get_info2(item, &s.info);
			obs_sceneitem_get_crop(item, &s.crop);
			s.visible = obs_sceneitem_visible(item);
			snapshot_.push_back(s);
		}
	}

	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const core::Rect canvas{0, 0, (double)ovi.base_width, (double)ovi.base_height};
	const ViewSettings &vs = Store::instance().settings();
	core::BroadcastSpec spec;
	spec.gap = vs.broadcastGap;
	spec.margin = vs.broadcastMargin;

	const core::FailoverChoice choice =
		core::chooseFallback(key_, assignment_, lost_, ovi.base_height > ovi.base_width);
	const std::vector<core::Slot> slots = choice.ok ? core::buildBroadcast(choice.key, canvas, spec)
							: std::vector<core::Slot>();

	std::vector<ItemOp> ops;
	for (Snapshot &s : snapshot_) {
		obs_sceneitem_t *item = obs_scene_find_source(scene, s.name.c_str());
		if (!item)
			continue;
		obs_source_t *source = obs_sceneitem_get_source(item);
		ItemOp op{item, s.info, s.crop, s.visible};
		if (lost_.count(s.name)) {
			op.visible = false;
			/* Keep the source running while hidden: a capture set to stop when not
			 * shown would otherwise never come back. */
			if (!s.held) {
				obs_source_inc_showing(source);
				s.held = true;
			}
		} else {
			if (s.held) {
				obs_source_dec_showing(source);
				s.held = false;
			}
			const auto it = std::find(choice.assignment.begin(), choice.assignment.end(), s.name);
			const size_t i = (size_t)(it - choice.assignment.begin());
			if (it != choice.assignment.end() && i < slots.size()) {
				const core::ItemModel m{fromObs(s.info, s.crop), obs_source_get_width(source),
							obs_source_get_height(source)};
				toObs(commands::slotTransform(m, slots[i].kind, slots[i].cell), op.info, op.crop);
			}
		}
		ops.push_back(op);
	}
	applyOps(scene, ops);
	obs_source_release(sceneSource);

	applied_ = true;
	fallbackKey_ = choice.ok ? choice.key : key_;
	const QString names = joinNames(lost_);
	blog(LOG_WARNING, "[Layouts] failover on '%s': %s -> %s (lost: %s)", scene_.toUtf8().constData(), key_.c_str(),
	     fallbackKey_.c_str(), names.toUtf8().constData());
	emit alert((choice.ok ? tr("Failover.Lost") : tr("Failover.LostNoLayout")).arg(names), true);
}

void Failover::restore()
{
	obs_source_t *sceneSource = obs_get_source_by_name(scene_.toUtf8().constData());
	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	std::vector<ItemOp> ops;
	for (Snapshot &s : snapshot_) {
		obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, s.name.c_str()) : nullptr;
		if (s.held) {
			obs_source_t *source = obs_get_source_by_name(s.name.c_str());
			if (source) {
				obs_source_dec_showing(source);
				obs_source_release(source);
			}
			s.held = false;
		}
		if (item)
			ops.push_back({item, s.info, s.crop, s.visible});
	}
	if (scene)
		applyOps(scene, ops);
	obs_source_release(sceneSource);
	snapshot_.clear();
	applied_ = false;
	fallbackKey_.clear();
	blog(LOG_INFO, "[Layouts] failover on '%s': layout %s restored", scene_.toUtf8().constData(), key_.c_str());
	emit alert(tr("Failover.Restored"), false);
}

} // namespace layouts
