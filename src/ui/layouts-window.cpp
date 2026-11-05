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
#include "widgets.hpp"

#include "obs/store.hpp"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QScreen>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

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
	/* Left: the mode switch above Layers (Scene) or the collection's sources (Builder). */
	auto *left = new QWidget(split_);
	left->setMinimumWidth(160);
	auto *lv = new QVBoxLayout(left);
	lv->setContentsMargins(0, 0, 0, 0);
	lv->setSpacing(0);
	auto *switchRow = new QWidget(left);
	switchRow->setObjectName("LayoutsModeSwitch");
	modeSwitch_ = segmented({T("Mode.Scene"), T("Mode.Builder")}, switchRow);
	switchRow->layout()->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, 0, 0);
	modeSwitch_->button(0)->setToolTip(T("Mode.SceneTooltip"));
	modeSwitch_->button(1)->setToolTip(T("Mode.BuilderTooltip"));
	lv->addWidget(switchRow);
	leftStack_ = new QStackedWidget(left);
	layers_ = new LayersPanel(leftStack_);
	builderSources_ = new BuilderSources(leftStack_);
	leftStack_->addWidget(layers_);
	leftStack_->addWidget(builderSources_);
	lv->addWidget(leftStack_, 1);

	canvas_ = new CanvasDock(split_);

	rightStack_ = new QStackedWidget(split_);
	rightStack_->setMinimumWidth(260);
	inspector_ = new InspectorDock(rightStack_);
	builderPanel_ = new BuilderPanel(builderSources_, rightStack_);
	rightStack_->addWidget(inspector_);
	rightStack_->addWidget(builderPanel_);

	split_->addWidget(left);
	split_->addWidget(canvas_);
	split_->addWidget(rightStack_);
	connect(modeSwitch_, &QButtonGroup::idClicked, this,
		[this](int id) { setMode(id ? Mode::Builder : Mode::Scene); });
	setMode(Mode::Scene);
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

void LayoutsWindow::setMode(Mode m)
{
	mode_ = m;
	const int i = m == Mode::Builder ? 1 : 0;
	modeSwitch_->button(i)->setChecked(true);
	leftStack_->setCurrentIndex(i);
	rightStack_->setCurrentIndex(i);
	setWindowTitle(m == Mode::Builder ? T("Window.BuilderTitle") : T("Window.Title"));
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
