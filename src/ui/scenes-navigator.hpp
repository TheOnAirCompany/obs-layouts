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

/* Scenes navigator of the Scene Editor (0.7), above Layers.
 *
 * A tree: one section per canvas ("Main" 16:9, "Vertical" 9:16...), the stacks of
 * Stacks for OBS inside when it is installed (color strip, scene count, collapsible),
 * else the scenes of the canvas. A search field (scene names; parents of what matches
 * stay) and a filter: All / 16:9 / 9:16 / favorites. Clicking a scene edits it without
 * changing what is on air and turns Follow OBS off. Red dot: on program; green dot: in
 * preview (Studio Mode); a collapsed stack shows the dot of a scene it holds. "+" makes
 * a new scene in the canvas (and stack) of the edited scene. */

#include <QWidget>

class QButtonGroup;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

#include <map>
#include <string>

namespace layouts::ui {

class ScenesNavigator : public QWidget {
	Q_OBJECT

public:
	explicit ScenesNavigator(QWidget *parent = nullptr);

	/* Rebuilds the rows now (self-test). */
	void refresh();
	QTreeWidget *tree() const { return tree_; }
	QLineEdit *search() const { return search_; }
	QButtonGroup *filter() const { return filter_; }
	QToolButton *addButton() const { return add_; }
	/* Row of a scene, by uuid (self-test). */
	QTreeWidgetItem *rowOf(const QString &uuid) const;
	/* Makes a new scene in the canvas of the edited scene; returns it (self-test). */
	QString newScene();

private:
	void rebuild();
	void updateTallies();
	void syncCurrent();
	void onClicked(QTreeWidgetItem *item, int column);

	QTreeWidget *tree_ = nullptr;
	QLineEdit *search_ = nullptr;
	QButtonGroup *filter_ = nullptr;
	QToolButton *add_ = nullptr;
	std::map<std::string, bool> expanded_; /* stack key -> expanded, set by the user */
	bool updating_ = false;
};

} // namespace layouts::ui
