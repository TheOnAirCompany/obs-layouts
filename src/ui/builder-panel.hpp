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

/* Scene Builder window (0.7; Layouts Builder in 0.4), three columns in work order.
 *
 * BuilderSources (1): every source of the collection once, whatever scene it is in,
 * with its type pill (click to change) and the number of scenes that use it. Search,
 * filter chips by type. Select the sources of the scene to make (Cmd / Shift).
 *
 * BuilderPanel: (2) the broadcast layouts gallery for the selection, with the target
 * canvas (16:9, 9:16, both) and the mirror toggles; (3) "Create": a large preview of
 * the chosen layout with the source names, the scene name, "Put in stack" (Stacks for
 * OBS), background and graphics, Create scene, Create show scenes and Open in Scene
 * Editor. 9:16 makes the scene in the canvas of Vertical for OBS; Both makes the 16:9
 * scene and its linked 9:16 twin. */

#include "obs/builder.hpp"

#include <obs.hpp>

#include <QWidget>

#include <set>
#include <string>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace layouts::ui {

class BroadcastGallery;
class LayoutPreview;
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
	QLineEdit *search() const { return search_; }

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
	QLineEdit *search_ = nullptr;
	QLabel *empty_ = nullptr;
	QString signature_;
	bool updating_ = false;
	bool queued_ = false;
};

class BuilderPanel : public QWidget {
	Q_OBJECT

public:
	enum class Target { Landscape = 0, Portrait = 1, Both = 2 };

	BuilderPanel(BuilderSources *sources, QWidget *parent = nullptr);

	/* Column 2 (the gallery); the window places it, this widget is column 3. */
	QWidget *galleryColumn() const { return galleryColumn_; }

	void refresh();
	BroadcastGallery *gallery() const { return gallery_; }
	QButtonGroup *targetGroup() const { return target_; }
	Target target() const;
	void setTarget(Target t);
	QLineEdit *nameField() const { return name_; }
	QComboBox *stackField() const { return stack_; }
	QWidget *stackRow() const { return stackRow_; }
	QPushButton *openButton() const { return open_; }
	QLabel *message() const { return message_; }
	/* The chosen layout as a new scene; returns its name (self-test). */
	QString createScene();
	/* A full-screen scene per source and one per layout that fits; returns the names. */
	std::vector<QString> createShow();
	/* Uuid of the last scene made ("Open in Scene Editor"). */
	QString lastCreated() const { return lastCreated_; }

signals:
	/* "Open in Scene Editor ↗": edit this scene there. */
	void openInEditor(const QString &sceneUuid);

protected:
	void showEvent(QShowEvent *e) override;

private:
	builder::Options options() const;
	void refreshTargets();
	void refreshStacks();
	void updatePreview();
	void showMessage(const QString &text);
	/* Canvas of the target (strong reference, release it) and its aspect. */
	obs_canvas_t *targetCanvas() const;
	double targetAspect() const;
	void fileIntoStack(const std::vector<std::string> &uuids);

	BuilderSources *sources_;
	QWidget *galleryColumn_ = nullptr;
	QLabel *summary_ = nullptr;
	QButtonGroup *target_ = nullptr;
	BroadcastGallery *gallery_ = nullptr;
	QLabel *empty_ = nullptr;
	NumericField *gap_ = nullptr;
	NumericField *margin_ = nullptr;
	LayoutPreview *preview_ = nullptr;
	QLineEdit *name_ = nullptr;
	QString autoName_;
	QWidget *stackRow_ = nullptr;
	QComboBox *stack_ = nullptr;
	QCheckBox *background_ = nullptr;
	QCheckBox *graphics_ = nullptr;
	QCheckBox *lock_ = nullptr;
	QPushButton *scene_ = nullptr;
	QPushButton *show_ = nullptr;
	QPushButton *open_ = nullptr;
	QLabel *message_ = nullptr;
	QString lastCreated_;
	bool updating_ = false;
};

} // namespace layouts::ui
