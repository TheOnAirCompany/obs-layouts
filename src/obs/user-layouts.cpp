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

#include "user-layouts.hpp"

#include "edit-context.hpp"
#include "roles.hpp"

#include "core/user-layouts.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QFile>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <optional>

namespace layouts::userlayouts {

namespace {

std::string configPath()
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	char *p = obs_module_config_path("user-layouts.json");
	if (!p)
		return {};
	std::string out = p;
	bfree(p);
	return out;
}

bool writeFile(const QString &path, const std::string &text, QString *error)
{
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly) || f.write(text.data(), (qint64)text.size()) != (qint64)text.size() ||
	    !f.commit()) {
		if (error)
			*error = f.errorString();
		return false;
	}
	return true;
}

/* A browser source named "Chat..." is a chat; everything else follows its role. */
std::optional<core::Slot> slotOf(const ItemEntry &e)
{
	obs_source_t *src = obs_sceneitem_get_source(e.item);
	if (!src || e.isGroup)
		return std::nullopt;
	const core::Role role = roles::role(src);
	const char *id = obs_source_get_unversioned_id(src);
	const std::string name = core::normalizeName(obs_source_get_name(src) ? obs_source_get_name(src) : "");
	core::Slot s{core::SlotKind::Camera, {}};
	if (id && std::string(id) == "browser_source" && name.find("CHAT") != std::string::npos)
		s.kind = core::SlotKind::Chat;
	else if (core::isCameraRole(role))
		s.kind = core::SlotKind::Camera;
	else if (role == core::Role::Screen)
		s.kind = core::SlotKind::Screen;
	else
		return std::nullopt;
	s.host = role == core::Role::Host && s.kind == core::SlotKind::Camera;
	/* The visible frame in canvas space: crop and bounds are in the model, the quad is
	 * rotated; the slot keeps the unrotated rect around the same center. */
	const core::Quad &q = e.frame.quad;
	const double w = std::hypot(q.p[1].x - q.p[0].x, q.p[1].y - q.p[0].y);
	const double h = std::hypot(q.p[3].x - q.p[0].x, q.p[3].y - q.p[0].y);
	if (!(w > 0.5) || !(h > 0.5))
		return std::nullopt;
	const core::Vec2 c = q.center();
	s.cell = {c.x - w / 2, c.y - h / 2, w, h};
	s.rotation = std::remainder(e.frame.rotation, 360.0);
	if (std::fabs(s.rotation) < 1e-6)
		s.rotation = 0;
	s.ticker = s.kind == core::SlotKind::Chat && w > h * 4;
	return s;
}

std::vector<core::Slot> captureSlots()
{
	EditContext &ctx = EditContext::instance();
	std::vector<core::Slot> out;
	if (!ctx.sceneSource())
		return out;
	const std::vector<const ItemEntry *> sel = ctx.selection();
	if (!sel.empty()) {
		for (const ItemEntry *e : sel)
			if (auto s = slotOf(*e))
				out.push_back(*s);
		return out;
	}
	for (const ItemEntry &e : ctx.items())
		if (e.groupId == 0 && e.visible)
			if (auto s = slotOf(e))
				out.push_back(*s);
	return out;
}

} // namespace

void load()
{
	const std::string path = configPath();
	QFile f(QString::fromStdString(path));
	if (path.empty() || !f.open(QIODevice::ReadOnly))
		return;
	const QByteArray data = f.readAll();
	std::vector<core::UserLayout> list;
	std::string error;
	if (!core::userLayoutsFromJson(data.toStdString(), list, &error)) {
		blog(LOG_WARNING, "[layouts] user-layouts.json ignored: %s", error.c_str());
		return;
	}
	core::UserLayoutRegistry::instance().setAll({});
	core::UserLayoutRegistry::instance().merge(list);
}

bool save()
{
	const std::string path = configPath();
	if (path.empty())
		return false;
	return writeFile(QString::fromStdString(path),
			 core::userLayoutsToJson(core::UserLayoutRegistry::instance().all()), nullptr);
}

int capturableCount()
{
	return (int)captureSlots().size();
}

std::string saveFromScene(const QString &name)
{
	const core::Rect canvas = EditContext::instance().canvasRect();
	core::UserLayout l;
	l.tiles = core::normalizeSlots(captureSlots(), canvas);
	if (l.tiles.empty() || !(canvas.w > 0) || !(canvas.h > 0))
		return {};
	l.aspect = canvas.w / canvas.h;
	core::UserLayoutRegistry &reg = core::UserLayoutRegistry::instance();
	l.name = reg.uniqueName(name.trimmed().isEmpty() ? std::string("Layout") : name.trimmed().toStdString());
	const std::string key = reg.add(std::move(l));
	save();
	return key;
}

bool rename(const std::string &key, const QString &name)
{
	if (name.trimmed().isEmpty() || !core::UserLayoutRegistry::instance().rename(key, name.trimmed().toStdString()))
		return false;
	return save();
}

bool remove(const std::string &key)
{
	if (!core::UserLayoutRegistry::instance().remove(key))
		return false;
	return save();
}

bool exportTo(const QString &path, const std::vector<std::string> &keys, QString *error)
{
	std::vector<core::UserLayout> list;
	for (const core::UserLayout &l : core::UserLayoutRegistry::instance().all())
		if (keys.empty() || std::find(keys.begin(), keys.end(), l.key) != keys.end())
			list.push_back(l);
	return writeFile(path, core::userLayoutsToJson(list), error);
}

int importFrom(const QString &path, QString *error)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		if (error)
			*error = f.errorString();
		return -1;
	}
	std::vector<core::UserLayout> list;
	std::string why;
	if (!core::userLayoutsFromJson(f.readAll().toStdString(), list, &why)) {
		if (error)
			*error = QString::fromStdString(why);
		return -1;
	}
	const int added = core::UserLayoutRegistry::instance().merge(list);
	if (added > 0)
		save();
	return added;
}

} // namespace layouts::userlayouts
