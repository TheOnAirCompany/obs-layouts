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

/* The Layouts window: a regular top-level window (resizable, maximizable, native
 * full screen), never a dock. Layers on the left, the Canvas in the center, the
 * Inspector on the right, in resizable panels. Opened from Tools > Layouts or a hotkey; closing it
 * only hides it. Its geometry is remembered.
 *
 * Two modes (0.4), switched at the top left: Scene (Layers + Inspector, editing the scene)
 * and Builder (every source of the collection + scene generation). Tools > Layouts
 * Builder opens the window in Builder mode. The Canvas is shared. */

#include <QWidget>

class QButtonGroup;
class QSplitter;
class QStackedWidget;

namespace layouts::ui {

class BuilderPanel;
class BuilderSources;
class CanvasDock;
class InspectorDock;
class LayersPanel;

class LayoutsWindow : public QWidget {
	Q_OBJECT

public:
	enum class Mode { Scene, Builder };

	explicit LayoutsWindow(QWidget *obsMainWindow);

	CanvasDock *canvas() const { return canvas_; }
	InspectorDock *inspector() const { return inspector_; }
	LayersPanel *layers() const { return layers_; }
	BuilderSources *builderSources() const { return builderSources_; }
	BuilderPanel *builderPanel() const { return builderPanel_; }
	Mode mode() const { return mode_; }
	void setMode(Mode m);

public slots:
	/* Shows the window (first time: a comfortable size centered on OBS), brings it
	 * to the front and gives the Canvas the keyboard focus. */
	void open();

protected:
	void closeEvent(QCloseEvent *e) override;
	void hideEvent(QHideEvent *e) override;

private:
	void saveState();

	QWidget *obsMain_;
	QSplitter *split_ = nullptr;
	CanvasDock *canvas_ = nullptr;
	InspectorDock *inspector_ = nullptr;
	LayersPanel *layers_ = nullptr;
	BuilderSources *builderSources_ = nullptr;
	BuilderPanel *builderPanel_ = nullptr;
	QStackedWidget *leftStack_ = nullptr;
	QStackedWidget *rightStack_ = nullptr;
	QButtonGroup *modeSwitch_ = nullptr;
	Mode mode_ = Mode::Scene;
	bool placed_ = false;
};

} // namespace layouts::ui
