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
 * with its type pill (click to change) and the scenes that use it. Select the sources
 * of the scene to make (Cmd / Shift for several). Filter row by type, as in Layers.
 *
 * BuilderPanel (right): the broadcast layouts gallery of Scene mode for the selected
 * sources (types decide cameras and screens), gap, margin, background and graphics in the
 * scene, lock; "New scene" makes the chosen layout, "Create show scenes" makes a
 * full-screen scene per source and one per layout that fits. */

#include "obs/builder.hpp"

#include <obs.hpp>

#include <QWidget>

#include <set>
#include <string>
#include <vector>

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace layouts::ui {

class BroadcastGallery;
class NumericField;
class RoleFilterBar;

class BuilderSources : public QWidget {
	Q_OBJECT

public:
	explicit BuilderSources(QWidget *parent = nullptr);
	~BuilderSources() override;

	void refresh();
	/* Selected sources, in list order. */
	std::vector<OBSSource> selectedSources() const;
	/* Selects these sources by name (self-test). */
	void selectNames(const QStringList &names);
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
	QString signature_;
	bool updating_ = false;
	bool queued_ = false;
};

class BuilderPanel : public QWidget {
	Q_OBJECT

public:
	BuilderPanel(BuilderSources *sources, QWidget *parent = nullptr);

	void refresh();
	BroadcastGallery *gallery() const { return gallery_; }
	/* The chosen layout as a new scene; returns its name (self-test). */
	QString createScene();
	/* A full-screen scene per source and one per layout that fits; returns the names. */
	std::vector<QString> createShow();

private:
	builder::Options options() const;
	void flash(QPushButton *button, const QString &text);

	BuilderSources *sources_;
	QLabel *summary_ = nullptr;
	BroadcastGallery *gallery_ = nullptr;
	QLabel *empty_ = nullptr;
	NumericField *gap_ = nullptr;
	NumericField *margin_ = nullptr;
	QCheckBox *background_ = nullptr;
	QCheckBox *graphics_ = nullptr;
	QCheckBox *lock_ = nullptr;
	QPushButton *scene_ = nullptr;
	QPushButton *show_ = nullptr;
	bool updating_ = false;
};

} // namespace layouts::ui
