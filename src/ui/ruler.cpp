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

#include "ruler.hpp"

#include "canvas-view.hpp"
#include "theme.hpp"

#include "core/expression.hpp"
#include "core/grid.hpp"
#include "obs/edit-context.hpp"
#include "obs/store.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace layouts::ui {

RulerWidget::RulerWidget(core::Axis axis, CanvasView *view, QWidget *parent) : QWidget(parent), axis_(axis), view_(view)
{
	setCursor(axis == core::Axis::X ? Qt::SplitVCursor : Qt::SplitHCursor);
	setToolTip(T("Ruler.Tooltip"));
	if (axis == core::Axis::X)
		setFixedHeight(tokens::kRulerThickness);
	else
		setFixedWidth(tokens::kRulerThickness);
	connect(view, &CanvasView::viewChanged, this, qOverload<>(&QWidget::update));
	connect(view, &CanvasView::cursorChanged, this, qOverload<>(&QWidget::update));
	connect(&EditContext::instance(), &EditContext::itemsChanged, this, qOverload<>(&QWidget::update));
	connect(&Store::instance(), &Store::guidesChanged, this, qOverload<>(&QWidget::update));
}

QSize RulerWidget::sizeHint() const
{
	return axis_ == core::Axis::X ? QSize(200, tokens::kRulerThickness) : QSize(tokens::kRulerThickness, 200);
}

void RulerWidget::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	const QPalette pal = palette();
	const QColor bg = pal.color(QPalette::Window);
	QColor fg = pal.color(QPalette::WindowText);
	QColor minor = fg;
	minor.setAlphaF(0.35f);
	QColor major = fg;
	major.setAlphaF(0.6f);
	QColor text = fg;
	text.setAlphaF(0.75f);
	p.fillRect(rect(), bg);

	const bool horizontal = axis_ == core::Axis::X;
	const int len = horizontal ? width() : height();
	const int thick = horizontal ? height() : width();

	/* Selection range band. */
	if (const auto sel = view_->selectionBounds()) {
		const double a = horizontal ? view_->worldToView({sel->left(), 0}).x()
					    : view_->worldToView({0, sel->top()}).y();
		const double b = horizontal ? view_->worldToView({sel->right(), 0}).x()
					    : view_->worldToView({0, sel->bottom()}).y();
		QColor band = QColor::fromRgba(tokens::kSelection);
		band.setAlphaF(0.22f);
		if (horizontal)
			p.fillRect(QRectF(a, 0, b - a, thick), band);
		else
			p.fillRect(QRectF(0, a, thick, b - a), band);
	}

	const double zoom = view_->zoom();
	const core::RulerTicks ticks = core::chooseRulerTicks(zoom);
	const core::Vec2 w0 = view_->viewToWorld({0, 0});
	const core::Vec2 w1 = view_->viewToWorld({(double)width(), (double)height()});
	const double from = horizontal ? w0.x : w0.y;
	const double to = horizontal ? w1.x : w1.y;
	const double minorStep = ticks.major / ticks.subdivisions;

	QFont f = font();
	f.setPointSizeF(tokens::kRulerFontPt);
	p.setFont(f);

	const double first = std::floor(from / minorStep) * minorStep;
	for (double v = first; v <= to + minorStep; v += minorStep) {
		const double pos = horizontal ? view_->worldToView({v, 0}).x() : view_->worldToView({0, v}).y();
		const double k = v / ticks.major;
		const bool isMajor = std::fabs(k - std::round(k)) < 1e-6;
		const bool isHalf = !isMajor && ticks.subdivisions % 2 == 0 &&
				    std::fabs(v / (ticks.major / 2) - std::round(v / (ticks.major / 2))) < 1e-6;
		const double tickLen = isMajor ? thick : (isHalf ? thick * 0.45 : thick * 0.25);
		p.setPen(QPen(isMajor ? major : minor, 1));
		const double x = std::floor(pos) + 0.5;
		if (horizontal)
			p.drawLine(QPointF(x, thick - tickLen), QPointF(x, thick));
		else
			p.drawLine(QPointF(thick - tickLen, x), QPointF(thick, x));

		if (isMajor) {
			const QString label = QString::fromStdString(core::formatNumber(std::round(v), 0));
			p.setPen(text);
			if (horizontal) {
				p.drawText(QPointF(x + 3, f.pointSizeF() + 3), label);
			} else {
				p.save();
				p.translate(f.pointSizeF() + 3, x - 3);
				p.rotate(-90);
				p.drawText(QPointF(0, 0), label);
				p.restore();
			}
		}
	}

	/* Guides. */
	const ViewSettings &vs = Store::instance().settings();
	if (vs.guides) {
		const core::Axis guideAxis = horizontal ? core::Axis::X : core::Axis::Y;
		const std::string uuid = EditContext::instance().sceneUuid().toStdString();
		for (const auto &ref : Store::instance().guides().visibleGuides(uuid)) {
			if (ref.guide.axis != guideAxis)
				continue;
			const double pos = horizontal ? view_->worldToView({ref.guide.pos, 0}).x()
						      : view_->worldToView({0, ref.guide.pos}).y();
			QColor c = vs.guideColor;
			if (ref.global || ref.guide.locked)
				c.setAlphaF(0.6f);
			p.setPen(Qt::NoPen);
			p.setBrush(c);
			QPolygonF tri;
			if (horizontal)
				tri << QPointF(pos - 4, thick - 6) << QPointF(pos + 4, thick - 6)
				    << QPointF(pos, thick);
			else
				tri << QPointF(thick - 6, pos - 4) << QPointF(thick - 6, pos + 4)
				    << QPointF(thick, pos);
			p.drawPolygon(tri);
		}
	}

	/* Cursor position. */
	if (view_->cursorInside()) {
		const core::Vec2 c = view_->cursorWorld();
		const double pos = horizontal ? view_->worldToView(c).x() : view_->worldToView(c).y();
		QColor cc = QColor::fromRgba(tokens::kSelection);
		p.setPen(QPen(cc, 1));
		if (horizontal)
			p.drawLine(QPointF(std::floor(pos) + 0.5, 0), QPointF(std::floor(pos) + 0.5, thick));
		else
			p.drawLine(QPointF(0, std::floor(pos) + 0.5), QPointF(thick, std::floor(pos) + 0.5));
	}

	/* Separator towards the canvas. */
	p.setPen(QPen(pal.color(QPalette::Mid), 1));
	if (horizontal)
		p.drawLine(QPointF(0, thick - 0.5), QPointF(len, thick - 0.5));
	else
		p.drawLine(QPointF(thick - 0.5, 0), QPointF(thick - 0.5, len));
}

void RulerWidget::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton)
		return;
	dragging_ = true;
	/* The top ruler creates horizontal guides (constant Y), the left one vertical guides. */
	view_->beginGuideDrag(axis_ == core::Axis::X ? core::Axis::Y : core::Axis::X, e->globalPosition().toPoint());
}

void RulerWidget::mouseMoveEvent(QMouseEvent *e)
{
	if (dragging_)
		view_->guideDragMove(e->globalPosition().toPoint());
}

void RulerWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if (!dragging_)
		return;
	dragging_ = false;
	view_->guideDragEnd(e->globalPosition().toPoint());
}

RulerCorner::RulerCorner(QWidget *parent) : QWidget(parent)
{
	setFixedSize(tokens::kRulerThickness, tokens::kRulerThickness);
}

void RulerCorner::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), palette().color(QPalette::Window));
	p.setPen(QPen(palette().color(QPalette::Mid), 1));
	p.drawLine(QPointF(width() - 0.5, 0), QPointF(width() - 0.5, height()));
	p.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}

} // namespace layouts::ui
