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

/* Layouts Builder (0.4).
 *
 * BuilderSources (left): every source of the collection once, whatever scene it is in,
 * with its type pill (click to change), the scenes that use it and a check box to take
 * it in the show. Filter row by type, as in Layers.
 *
 * BuilderPanel (right): what will be generated (a full-screen scene per host, camera
 * and screen, every broadcast layout that fits, background and graphics in each scene,
 * lock), the list of scene names, and the button that creates them. */

#include "obs/builder.hpp"

#include <obs.hpp>

#include <QWidget>

#include <set>
#include <string>
#include <vector>

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace layouts::ui {

class NumericField;
class RoleFilterBar;

class BuilderSources : public QWidget {
	Q_OBJECT

public:
	explicit BuilderSources(QWidget *parent = nullptr);
	~BuilderSources() override;

	void refresh();
	std::vector<OBSSource> checkedSources() const;
	/* Takes a source in or out of the show (self-test). */
	void setIncluded(obs_source_t *source, bool on);
	QTreeWidget *tree() const { return tree_; }
	RoleFilterBar *filter() const { return filter_; }

signals:
	void changed();

protected:
	void showEvent(QShowEvent *e) override;

private:
	void rebuild();
	void applyFilter();
	void onClicked(QTreeWidgetItem *row, int column);
	static void onSourceSignal(void *data, calldata_t *cd);

	QTreeWidget *tree_ = nullptr;
	RoleFilterBar *filter_ = nullptr;
	QLabel *empty_ = nullptr;
	std::set<std::string> unchecked_; /* source uuids taken out of the show */
	QString signature_;
	bool updating_ = false;
	bool queued_ = false;
};

class BuilderPanel : public QWidget {
	Q_OBJECT

public:
	BuilderPanel(BuilderSources *sources, QWidget *parent = nullptr);

	void refresh();
	builder::Plan plan() const;
	/* Creates the scenes; returns their names (self-test). */
	std::vector<QString> create();

private:
	builder::Options options() const;

	BuilderSources *sources_;
	QLabel *summary_ = nullptr;
	QLabel *scenes_ = nullptr;
	NumericField *gap_ = nullptr;
	NumericField *margin_ = nullptr;
	QCheckBox *solo_ = nullptr;
	QCheckBox *layouts_ = nullptr;
	QCheckBox *background_ = nullptr;
	QCheckBox *graphics_ = nullptr;
	QCheckBox *lock_ = nullptr;
	QPushButton *create_ = nullptr;
	bool updating_ = false;
};

} // namespace layouts::ui
