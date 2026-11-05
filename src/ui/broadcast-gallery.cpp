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

#include "broadcast-gallery.hpp"

#include "role-ui.hpp"
#include "theme.hpp"

#include "core/broadcast.hpp"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace layouts::ui {

QIcon broadcastThumb(const std::string &key, const QColor &camera, const QColor &screen, const QColor &bg,
		     double aspect)
{
	const QSize size(112, 63);
	/* The canvas shape inside the thumbnail box (portrait canvases stay portrait). */
	const double bw = std::min<double>(size.width(), size.height() * aspect);
	const double bh = bw / aspect;
	const QRectF area((size.width() - bw) / 2, (size.height() - bh) / 2, bw, bh);
	QIcon icon;
	for (qreal dpr : {1.0, 2.0}) {
		QPixmap pm(size * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(bg);
		p.drawRoundedRect(area, 3, 3);
		core::BroadcastSpec spec;
		spec.gap = 2;
		for (const core::Slot &s :
		     core::buildBroadcast(key, {area.x(), area.y(), area.width(), area.height()}, spec)) {
			p.setBrush(s.kind == core::SlotKind::Camera ? camera : screen);
			p.drawRoundedRect(QRectF(s.cell.x + 0.5, s.cell.y + 0.5, s.cell.w - 1, s.cell.h - 1), 1.5, 1.5);
			if (s.kind == core::SlotKind::Camera) {
				/* a head and shoulders */
				const QPointF c(s.cell.x + s.cell.w / 2, s.cell.y + s.cell.h * 0.42);
				const double r = std::min(s.cell.w, s.cell.h) * 0.16;
				p.setBrush(bg);
				p.drawEllipse(c, r, r);
				p.drawChord(QRectF(c.x() - r * 2, c.y() + r * 1.3, r * 4, r * 3.2), 0, 180 * 16);
			}
		}
		p.end();
		icon.addPixmap(pm);
		icon.addPixmap(pm, QIcon::Selected); /* no tint: the outline shows the selection */
	}
	return icon;
}

BroadcastGallery::BroadcastGallery(QWidget *parent) : QListWidget(parent)
{
	setViewMode(QListView::IconMode);
	setIconSize(QSize(112, 63));
	setResizeMode(QListView::Adjust);
	setMovement(QListView::Static);
	setWrapping(true);
	setGridSize(QSize(124, 96));
	setUniformItemSizes(true);
	setWordWrap(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	/* Selection as an outline so the thumbnail keeps its colors. */
	setStyleSheet(QString("QListWidget { background: transparent; }"
			      "QListWidget::item { border: 2px solid transparent; border-radius: 4px; }"
			      "QListWidget::item:selected { background: transparent; border-color: %1; }"
			      "QListWidget::item:hover:!selected { border-color: palette(mid); }")
			      .arg(QColor::fromRgba(tokens::kSelection).name()));
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setFrameShape(QFrame::NoFrame);
	setSelectionMode(QAbstractItemView::SingleSelection);
}

void BroadcastGallery::setTemplates(const QStringList &keys, double aspect)
{
	QStringList shown;
	for (int i = 0; i < count(); i++)
		shown << item(i)->data(Qt::UserRole).toString();
	if (keys == shown && std::fabs(property("aspect").toDouble() - aspect) <= 1e-3)
		return;
	setProperty("aspect", aspect);
	const QString current = currentKey();
	const QSignalBlocker block(this);
	clear();
	const QColor cam = QColor::fromRgba(tokens::kThumbCamera);
	const QColor scr = QColor::fromRgba(tokens::kThumbScreen);
	const QColor bg = palette().color(QPalette::Base);
	for (const QString &k : keys) {
		const QString label = broadcastLabel(k.toStdString(), aspect);
		auto *it = new QListWidgetItem(broadcastThumb(k.toStdString(), cam, scr, bg, aspect), label, this);
		it->setData(Qt::UserRole, k);
		it->setToolTip(label);
		if (k == current)
			setCurrentItem(it);
	}
	if (!currentItem() && count() > 0)
		setCurrentRow(0);
	fitRows();
	emit currentItemChanged(currentItem(), nullptr);
}

QString BroadcastGallery::currentKey() const
{
	return currentItem() ? currentItem()->data(Qt::UserRole).toString() : QString();
}

QString BroadcastGallery::currentLabel() const
{
	return currentItem() ? currentItem()->text() : QString();
}

void BroadcastGallery::fitRows()
{
	const QSize cell = gridSize();
	const int cols = std::max(1, viewport()->width() / std::max(1, cell.width()));
	const int rows = std::max(1, (count() + cols - 1) / cols);
	setFixedHeight(rows * cell.height() + 2 * frameWidth() + 2 * spacing());
}

void BroadcastGallery::resizeEvent(QResizeEvent *e)
{
	QListWidget::resizeEvent(e);
	fitRows();
}

} // namespace layouts::ui
