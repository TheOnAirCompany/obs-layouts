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

namespace layouts::ui {

LayoutsWindow::LayoutsWindow(QWidget *obsMainWindow) : QWidget(obsMainWindow, Qt::Window), obsMain_(obsMainWindow)
{
	setObjectName("LayoutsWindow");
	setWindowTitle(T("Window.Title"));
	setMinimumSize(640, 400);

	auto *lay = new QHBoxLayout(this);
	lay->setContentsMargins(0, 0, 0, 0);
	split_ = new QSplitter(Qt::Horizontal, this);
	split_->setChildrenCollapsible(false);
	layers_ = new LayersPanel(split_);
	layers_->setMinimumWidth(160);
	canvas_ = new CanvasDock(split_);
	inspector_ = new InspectorDock(split_);
	inspector_->setMinimumWidth(260);
	split_->addWidget(layers_);
	split_->addWidget(canvas_);
	split_->addWidget(inspector_);
	split_->setStretchFactor(0, 0);
	split_->setStretchFactor(1, 1);
	split_->setStretchFactor(2, 0);
	lay->addWidget(split_);

	const ViewSettings &s = Store::instance().settings();
	if (!s.splitterState.isEmpty())
		split_->restoreState(s.splitterState);
	else
		split_->setSizes({tokens::kLayersWidth, 900, tokens::kInspectorWidth});
}

void LayoutsWindow::open()
{
	if (!placed_) {
		placed_ = true;
		const ViewSettings &s = Store::instance().settings();
		if (s.windowGeometry.isEmpty() || !restoreGeometry(s.windowGeometry)) {
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
	canvas_->view()->setFocus();
}

void LayoutsWindow::saveState()
{
	ViewSettings &s = Store::instance().settings();
	s.windowGeometry = saveGeometry();
	s.splitterState = split_->saveState();
	Store::instance().saveSettings();
}

void LayoutsWindow::closeEvent(QCloseEvent *e)
{
	saveState();
	e->accept(); /* hides the window; it is never deleted while OBS runs */
}

void LayoutsWindow::hideEvent(QHideEvent *e)
{
	if (placed_)
		saveState();
	QWidget::hideEvent(e);
}

} // namespace layouts::ui
