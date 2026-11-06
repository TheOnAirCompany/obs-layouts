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

#include "layouts-window.hpp"

#include "builder-panel.hpp"
#include "canvas-dock.hpp"
#include "canvas-view.hpp"
#include "inspector-dock.hpp"
#include "layers-panel.hpp"
#include "theme.hpp"

#include "obs/store.hpp"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QScreen>
#include <QSplitter>

#include <algorithm>

namespace layouts::ui {

/* ---- ToolWindow ---- */

ToolWindow::ToolWindow(QWidget *obsMainWindow, QByteArray *geometry, QByteArray *splitter)
	: QWidget(obsMainWindow, Qt::Window),
	  obsMain_(obsMainWindow),
	  geometry_(geometry),
	  splitterState_(splitter)
{
	setMinimumSize(640, 400);
	auto *lay = new QHBoxLayout(this);
	lay->setContentsMargins(0, 0, 0, 0);
	split_ = new QSplitter(Qt::Horizontal, this);
	split_->setChildrenCollapsible(false);
	lay->addWidget(split_);
}

void ToolWindow::restoreSplitter(const QList<int> &defaults)
{
	split_->setStretchFactor(0, 0);
	split_->setStretchFactor(1, 1);
	split_->setStretchFactor(2, 0);
	if (splitterState_->isEmpty() || !split_->restoreState(*splitterState_))
		split_->setSizes(defaults);
}

void ToolWindow::open()
{
	if (!placed_) {
		placed_ = true;
		if (geometry_->isEmpty() || !restoreGeometry(*geometry_)) {
			/* A comfortable size, centered on OBS, never larger than its screen. */
			QScreen *screen = obsMain_ ? obsMain_->screen() : this->screen();
			const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1440, 900);
			const QSize size(std::min(1280, avail.width() * 9 / 10),
					 std::min(800, avail.height() * 9 / 10));
			QRect r(QPoint(0, 0), size);
			r.moveCenter(obsMain_ ? obsMain_->geometry().center() : avail.center());
			r.moveLeft(std::clamp(r.left(), avail.left(), avail.right() - size.width()));
			r.moveTop(std::clamp(r.top(), avail.top(), avail.bottom() - size.height()));
			setGeometry(r);
		}
	}
	if (isMinimized())
		showNormal();
	else
		show();
	raise();
	activateWindow();
}

void ToolWindow::saveState()
{
	*geometry_ = saveGeometry();
	*splitterState_ = split_->saveState();
	Store::instance().saveSettings();
}

void ToolWindow::closeEvent(QCloseEvent *e)
{
	saveState();
	e->accept(); /* hides the window; it is never deleted while OBS runs */
}

void ToolWindow::hideEvent(QHideEvent *e)
{
	if (placed_)
		saveState();
	QWidget::hideEvent(e);
}

/* ---- Scene Editor ---- */

EditorWindow::EditorWindow(QWidget *obsMainWindow)
	: ToolWindow(obsMainWindow, &Store::instance().settings().windowGeometry,
		     &Store::instance().settings().splitterState)
{
	setObjectName("LayoutsWindow");
	setWindowTitle(T("Window.Title"));

	/* Left: Scenes navigator above Layers. */
	layers_ = new LayersPanel(split_);
	layers_->setMinimumWidth(160);
	canvas_ = new CanvasDock(split_);
	inspector_ = new InspectorDock(split_);
	inspector_->setMinimumWidth(260);
	split_->addWidget(layers_);
	split_->addWidget(canvas_);
	split_->addWidget(inspector_);
	restoreSplitter({tokens::kLayersWidth, 900, tokens::kInspectorWidth});
	connect(canvas_, &CanvasDock::openBuilderRequested, this, &EditorWindow::openBuilderRequested);
}

void EditorWindow::open()
{
	ToolWindow::open();
	canvas_->view()->setFocus();
}

/* ---- Scene Builder ---- */

BuilderWindow::BuilderWindow(QWidget *obsMainWindow)
	: ToolWindow(obsMainWindow, &Store::instance().settings().builderGeometry,
		     &Store::instance().settings().builderSplitterState)
{
	setObjectName("LayoutsBuilderWindow");
	setWindowTitle(T("Window.BuilderTitle"));

	sources_ = new BuilderSources(split_);
	sources_->setMinimumWidth(180);
	panel_ = new BuilderPanel(sources_, split_);
	panel_->setMinimumWidth(240);
	split_->addWidget(sources_);
	split_->addWidget(panel_->galleryColumn());
	split_->addWidget(panel_);
	restoreSplitter({tokens::kBuilderSourcesWidth, 700, tokens::kBuilderCreateWidth});
	connect(panel_, &BuilderPanel::openInEditor, this, &BuilderWindow::openInEditor);
}

} // namespace layouts::ui
