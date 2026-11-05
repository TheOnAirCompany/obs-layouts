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

/* Layers: the sources of the edited scene as a tree, like the layers panel of a design
 * tool. The top of the list is the front of the scene; groups unfold. Selection follows
 * the Canvas and OBS both ways. Each row toggles visibility and lock; double-click
 * renames the source; drag and drop reorders inside the same scene or group (one undo
 * entry). Each source shows its type (Host, Cam, Screen, BG, CG) as a pill that opens
 * the type menu, and the list can be filtered by type (0.4). */

#include <QWidget>

#include <cstdint>
#include <set>
#include <utility>

class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

namespace layouts::ui {

class LayerTree;
class RoleFilterBar;

class LayersPanel : public QWidget {
	Q_OBJECT

public:
	explicit LayersPanel(QWidget *parent = nullptr);

	/* Rebuilds the rows now (self-test). */
	void refresh();
	QTreeWidget *tree() const;
	RoleFilterBar *filter() const { return filter_; }
	/* Moves a row to `indexAfter` among its siblings (front = 0); drag and drop uses it. */
	void reorder(QTreeWidgetItem *moved, QTreeWidgetItem *parentAfter, int indexAfter);

private:
	void rebuild();
	void syncSelection();
	void onUserSelection();
	void onClicked(QTreeWidgetItem *row, int column);
	void onRenamed(QTreeWidgetItem *row, int column);
	void applyFilter();

	friend class LayerTree;
	LayerTree *tree_ = nullptr;
	RoleFilterBar *filter_ = nullptr;
	QLabel *empty_ = nullptr;
	QString signature_;
	std::set<int64_t> collapsed_; /* group ids folded by the user */
	bool updating_ = false;
};

} // namespace layouts::ui
