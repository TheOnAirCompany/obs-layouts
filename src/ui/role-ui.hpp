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

/* Source types in the interface: colored pills, the filter row ("All, Host, Cam...") and
 * the menu that sets the type of one or several sources. Shared by Layers and the
 * Builder. */

#include "core/roles.hpp"

#include <obs.hpp>

#include <QIcon>
#include <QStyledItemDelegate>
#include <QWidget>

#include <map>
#include <string>
#include <vector>

class QToolButton;

namespace layouts::ui {

/* Display order: Host, Camera, Screen, Background, Graphics, None. */
const std::vector<core::Role> &roleOrder();
QColor roleColor(core::Role r);
QString roleName(core::Role r);  /* "Host", "Cam", "Screen"... */
QString roleShort(core::Role r); /* pill text: "HOST", "CAM"... */
/* Pill icon (filled, or dashed outline for None). */
QIcon rolePill(core::Role r);
QSize rolePillSize();

/* Paints a column's icon (a pill) at its own size, centered, instead of the view's
 * square icon size. */
class PillDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

/* Lucide icon name for the kind of source (camera, screen, image, text...). */
QString sourceIcon(obs_source_t *src);

/* Broadcast template name; some change on a portrait canvas (columns become rows). */
QString broadcastLabel(const std::string &key, double aspect);
/* Width / height of the edited canvas (16:9 when unknown). */
double canvasAspect();

/* Menu to set the type of these sources (one undo step). */
void chooseRole(const std::vector<obs_source_t *> &sources, const QPoint &globalPos, QWidget *parent);

/* Filter chips: "All" then one chip per type present, with counts. */
class RoleFilterBar : public QWidget {
	Q_OBJECT

public:
	explicit RoleFilterBar(QWidget *parent = nullptr);

	void setCounts(const std::map<core::Role, int> &counts, int total);
	bool accepts(core::Role r) const { return all_ || r == current_; }
	bool filtering() const { return !all_; }
	/* Self-test and keyboard: select a filter (all = true shows everything). */
	void select(bool all, core::Role r = core::Role::None);

signals:
	void changed();

private:
	void sync();

	std::vector<QToolButton *> buttons_; /* All, then roleOrder() */
	bool all_ = true;
	core::Role current_ = core::Role::None;
	std::map<core::Role, int> counts_;
};

} // namespace layouts::ui
