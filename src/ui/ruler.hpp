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

#include "core/geometry.hpp"

#include <QWidget>

namespace layouts::ui {

class CanvasView;

/* Horizontal (axis X, above the canvas) or vertical (axis Y, left of the canvas)
 * ruler in canvas coordinates. Dragging from a ruler creates a guide. */
class RulerWidget : public QWidget {
	Q_OBJECT

public:
	RulerWidget(core::Axis axis, CanvasView *view, QWidget *parent = nullptr);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	core::Axis axis_;
	CanvasView *view_;
	bool dragging_ = false;
};

/* Square in the corner between the two rulers. */
class RulerCorner : public QWidget {
	Q_OBJECT

public:
	explicit RulerCorner(QWidget *parent = nullptr);

protected:
	void paintEvent(QPaintEvent *) override;
};

} // namespace layouts::ui
