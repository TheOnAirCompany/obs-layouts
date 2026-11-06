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

/* The two windows of Layouts (0.7), regular top-level windows (resizable, maximizable,
 * native full screen), never docks. Both can be open at once; closing one only hides
 * it. Their geometry is remembered.
 *
 * Scene Editor (Tools > Scene Editor): Scenes navigator above Layers on the left, the
 * Canvas in the center, the Inspector on the right, in resizable panels.
 * Scene Builder (Tools > Scene Builder): sources of the collection, layouts for the
 * selection, and Create. Each has a button that opens the other. */

#include <QWidget>

class QSplitter;

namespace layouts::ui {

class BuilderPanel;
class BuilderSources;
class CanvasDock;
class InspectorDock;
class LayersPanel;

/* Shared by both windows: placement on first open, geometry and splitter saved. */
class ToolWindow : public QWidget {
	Q_OBJECT

public:
	ToolWindow(QWidget *obsMainWindow, QByteArray *geometry, QByteArray *splitter);

public slots:
	/* Shows the window (first time: a comfortable size centered on OBS), brings it to
	 * the front. */
	virtual void open();

protected:
	void closeEvent(QCloseEvent *e) override;
	void hideEvent(QHideEvent *e) override;
	void restoreSplitter(const QList<int> &defaults);
	void saveState();

	QWidget *obsMain_;
	QSplitter *split_ = nullptr;
	QByteArray *geometry_;
	QByteArray *splitterState_;
	bool placed_ = false;
};

class EditorWindow : public ToolWindow {
	Q_OBJECT

public:
	explicit EditorWindow(QWidget *obsMainWindow);

	CanvasDock *canvas() const { return canvas_; }
	InspectorDock *inspector() const { return inspector_; }
	LayersPanel *layers() const { return layers_; }

	void open() override; /* also gives the Canvas the keyboard focus */

signals:
	void openBuilderRequested();

private:
	CanvasDock *canvas_ = nullptr;
	InspectorDock *inspector_ = nullptr;
	LayersPanel *layers_ = nullptr;
};

class BuilderWindow : public ToolWindow {
	Q_OBJECT

public:
	explicit BuilderWindow(QWidget *obsMainWindow);

	BuilderSources *builderSources() const { return sources_; }
	BuilderPanel *builderPanel() const { return panel_; }

signals:
	/* "Open in Scene Editor ↗" with the scene just made. */
	void openInEditor(const QString &sceneUuid);

private:
	BuilderSources *sources_ = nullptr;
	BuilderPanel *panel_ = nullptr;
};

} // namespace layouts::ui
