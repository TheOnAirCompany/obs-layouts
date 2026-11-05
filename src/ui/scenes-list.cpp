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

#include "scenes-list.hpp"

#include "theme.hpp"

#include "obs/edit-context.hpp"

#include <obs.hpp>

#include <QPainter>
#include <QTimer>

#include <cstring>

namespace layouts::ui {

namespace {

QIcon dot(const QColor &c)
{
	QIcon icon;
	for (qreal dpr : {1.0, 2.0}) {
		QPixmap pm(QSize(10, 10) * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		p.drawEllipse(QRectF(1.5, 1.5, 7, 7));
		p.end();
		icon.addPixmap(pm);
		icon.addPixmap(pm, QIcon::Selected);
	}
	return icon;
}

} // namespace

ScenesList::ScenesList(QWidget *parent) : QListWidget(parent)
{
	setObjectName("LayoutsScenesList");
	setFrameShape(QFrame::NoFrame);
	setUniformItemSizes(true);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setIconSize(QSize(10, 10));
	setToolTip(T("Scenes.Tooltip"));
	connect(this, &QListWidget::itemClicked, this, &ScenesList::onClicked);
	connect(&EditContext::instance(), &EditContext::sceneChanged, this, &ScenesList::refresh);
	obs_frontend_add_event_callback(onFrontendEvent, this);
	refresh();
}

ScenesList::~ScenesList()
{
	obs_frontend_remove_event_callback(onFrontendEvent, this);
}

void ScenesList::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	/* The program or the list changed: update the red dot and the names. */
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED)
		static_cast<ScenesList *>(data)->refresh();
}

void ScenesList::refresh()
{
	updating_ = true;
	EditContext &ctx = EditContext::instance();
	clear();
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	const QIcon live = dot(QColor::fromRgba(tokens::kLiveFrame));
	const QIcon none = dot(Qt::transparent);

	auto *follow = new QListWidgetItem(
		none, ctx.following() ? T("Canvas.FollowScene").arg(ctx.sceneName()) : T("Canvas.Follow"), this);
	follow->setData(Qt::UserRole, QString());
	QFont italic = follow->font();
	italic.setItalic(true);
	follow->setFont(italic);
	if (ctx.following())
		setCurrentItem(follow);

	/* In Studio Mode the program may be a private copy of the scene: same name. */
	auto onProgram = [&](obs_source_t *s) {
		return program &&
		       (s == program.Get() || (obs_obj_is_private(program.Get()) &&
					       strcmp(obs_source_get_name(s), obs_source_get_name(program)) == 0));
	};
	auto add = [&](obs_source_t *s, const QString &prefix) {
		const QString uuid = QString::fromUtf8(obs_source_get_uuid(s));
		const bool live_ = onProgram(s);
		auto *it = new QListWidgetItem(live_ ? live : none, prefix + QString::fromUtf8(obs_source_get_name(s)),
					       this);
		it->setData(Qt::UserRole, uuid);
		if (live_)
			it->setToolTip(T("Scenes.OnProgram"));
		if (!ctx.following() && uuid == ctx.sceneUuid())
			setCurrentItem(it);
	};
	obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++)
		add(list.sources.array[i], QString());
	obs_frontend_source_list_free(&list);

	obs_frontend_canvas_list canvases = {};
	obs_frontend_get_canvases(&canvases);
	for (size_t i = 0; i < canvases.canvases.num; i++) {
		obs_canvas_t *c = canvases.canvases.array[i];
		struct Ctx {
			decltype(add) *add;
			QString prefix;
		} p{&add, QString::fromUtf8(obs_canvas_get_name(c)) + QString::fromUtf8(" \xE2\x80\xBA ")};
		obs_canvas_enum_scenes(
			c,
			[](void *param, obs_source_t *src) {
				auto *pc = static_cast<Ctx *>(param);
				(*pc->add)(src, pc->prefix);
				return true;
			},
			&p);
	}
	obs_frontend_canvas_list_free(&canvases);
	updating_ = false;
}

void ScenesList::onClicked(QListWidgetItem *item)
{
	if (updating_ || !item)
		return;
	const QString uuid = item->data(Qt::UserRole).toString();
	/* After the click: switching scenes rebuilds this list. */
	QTimer::singleShot(0, this, [uuid] {
		EditContext &ctx = EditContext::instance();
		if (uuid.isEmpty()) {
			ctx.follow();
			return;
		}
		OBSSourceAutoRelease src = obs_get_source_by_uuid(uuid.toUtf8().constData());
		if (src)
			ctx.pin(src);
	});
}

} // namespace layouts::ui
