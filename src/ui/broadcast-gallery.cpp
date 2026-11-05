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
#include "core/user-layouts.hpp"
#include "obs/store.hpp"
#include "obs/user-layouts.hpp"

#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

#include <algorithm>
#include <cmath>

namespace layouts::ui {

namespace {

int g_userGeneration = 0;

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
			p.save();
			if (s.rotation != 0) {
				const QPointF c(s.cell.x + s.cell.w / 2, s.cell.y + s.cell.h / 2);
				p.translate(c);
				p.rotate(s.rotation);
				p.translate(-c);
			}
			p.setBrush(s.kind == core::SlotKind::Camera ? camera : screen);
			if (s.kind == core::SlotKind::Chat)
				p.setOpacity(0.6);
			const QRectF cell(s.cell.x + 0.5, s.cell.y + 0.5, s.cell.w - 1, s.cell.h - 1);
			if (s.shape == core::SlotShape::Round) {
				const double r = std::min(cell.width(), cell.height()) / 2;
				p.drawRoundedRect(cell, r, r);
			} else if (s.shape == core::SlotShape::Polygon && s.polygon.size() >= 3) {
				QPolygonF poly;
				for (const core::Vec2 &v : s.polygon)
					poly << QPointF(cell.x() + v.x * cell.width(), cell.y() + v.y * cell.height());
				p.drawPolygon(poly);
			} else {
				p.drawRoundedRect(cell, 1.5, 1.5);
			}
			p.setOpacity(1);
			if (s.kind == core::SlotKind::Camera) {
				/* a head and shoulders */
				const QPointF c(s.cell.x + s.cell.w / 2, s.cell.y + s.cell.h * 0.42);
				const double r = std::min(s.cell.w, s.cell.h) * 0.16;
				p.setBrush(bg);
				p.drawEllipse(c, r, r);
				p.drawChord(QRectF(c.x() - r * 2, c.y() + r * 1.3, r * 4, r * 3.2), 0, 180 * 16);
			}
			p.restore();
		}
		if (core::isUserKey(key)) {
			/* Saved by the user: a small folded corner at the top right. */
			QPainterPath corner;
			corner.moveTo(area.right() - 11, area.top());
			corner.lineTo(area.right(), area.top());
			corner.lineTo(area.right(), area.top() + 11);
			corner.closeSubpath();
			p.setBrush(QColor::fromRgba(tokens::kSelection));
			p.drawPath(corner);
		}
		p.end();
		icon.addPixmap(pm);
		icon.addPixmap(pm, QIcon::Selected); /* no tint: the outline shows the selection */
	}
	return icon;
}

} // namespace

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
	if (keys == shown && std::fabs(property("aspect").toDouble() - aspect) <= 1e-3 &&
	    property("userGeneration").toInt() == g_userGeneration)
		return;
	setProperty("aspect", aspect);
	setProperty("userGeneration", g_userGeneration);
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
		it->setToolTip(core::isUserKey(k.toStdString()) ? T("UserLayout.Tooltip").arg(label) : label);
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

void BroadcastGallery::userLayoutsChanged()
{
	g_userGeneration++;
	Store::instance().notifySettingsChanged(false);
}

void BroadcastGallery::contextMenuEvent(QContextMenuEvent *e)
{
	QListWidgetItem *it = itemAt(e->pos());
	const std::string key = it ? it->data(Qt::UserRole).toString().toStdString() : std::string();
	QMenu menu(this);
	if (core::isUserKey(key)) {
		const QString name = it->text();
		menu.addAction(T("UserLayout.Rename"), this, [this, key, name] {
			bool ok = false;
			const QString text = QInputDialog::getText(this, T("UserLayout.Rename"), T("UserLayout.Name"),
								   QLineEdit::Normal, name, &ok);
			if (ok && userlayouts::rename(key, text))
				userLayoutsChanged();
		});
		menu.addAction(T("UserLayout.Export"), this, [this, key, name] {
			const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
			const QString path = QFileDialog::getSaveFileName(
				this, T("UserLayout.Export"), dir + "/" + name + ".json", T("UserLayout.FileFilter"));
			QString error;
			if (!path.isEmpty() && !userlayouts::exportTo(path, {key}, &error))
				QMessageBox::warning(this, T("UserLayout.Export"),
						     T("UserLayout.ExportFailed").arg(error));
		});
		menu.addAction(T("UserLayout.Delete"), this, [this, key, name] {
			if (QMessageBox::question(this, T("UserLayout.Delete"),
						  T("UserLayout.DeleteConfirm").arg(name)) != QMessageBox::Yes)
				return;
			if (userlayouts::remove(key))
				userLayoutsChanged();
		});
		menu.addSeparator();
	}
	menu.addAction(T("UserLayout.Save"), this, [this] { saveLayoutDialog(this); });
	menu.addAction(T("UserLayout.Import"), this, [this] { importLayoutsDialog(this); });
	if (!core::UserLayoutRegistry::instance().all().empty())
		menu.addAction(T("UserLayout.ExportAll"), this, [this] {
			const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
			const QString path = QFileDialog::getSaveFileName(
				this, T("UserLayout.ExportAll"), dir + "/layouts.json", T("UserLayout.FileFilter"));
			QString error;
			if (!path.isEmpty() && !userlayouts::exportTo(path, {}, &error))
				QMessageBox::warning(this, T("UserLayout.ExportAll"),
						     T("UserLayout.ExportFailed").arg(error));
		});
	menu.exec(e->globalPos());
}

bool saveLayoutDialog(QWidget *parent)
{
	const int n = userlayouts::capturableCount();
	if (n == 0) {
		QMessageBox::information(parent, T("UserLayout.Save"), T("UserLayout.Nothing"));
		return false;
	}
	const QString suggested = QString::fromStdString(
		core::UserLayoutRegistry::instance().uniqueName(T("UserLayout.DefaultName").arg(n).toStdString()));
	bool ok = false;
	const QString name = QInputDialog::getText(parent, T("UserLayout.Save"), T("UserLayout.SavePrompt").arg(n),
						   QLineEdit::Normal, suggested, &ok);
	if (!ok)
		return false;
	if (userlayouts::saveFromScene(name).empty())
		return false;
	BroadcastGallery::userLayoutsChanged();
	return true;
}

void importLayoutsDialog(QWidget *parent)
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QStringList files =
		QFileDialog::getOpenFileNames(parent, T("UserLayout.Import"), dir, T("UserLayout.FileFilter"));
	if (files.isEmpty())
		return;
	int added = 0;
	QStringList failed;
	for (const QString &f : files) {
		QString error;
		const int n = userlayouts::importFrom(f, &error);
		if (n < 0)
			failed << QString("%1 (%2)").arg(QFileInfo(f).fileName(), error);
		else
			added += n;
	}
	if (added > 0)
		BroadcastGallery::userLayoutsChanged();
	QString text = T("UserLayout.Imported").arg(added);
	if (!failed.isEmpty())
		text += "\n\n" + T("UserLayout.ImportFailed").arg(failed.join(", "));
	QMessageBox::information(parent, T("UserLayout.Import"), text);
}

void BroadcastGallery::resizeEvent(QResizeEvent *e)
{
	QListWidget::resizeEvent(e);
	fitRows();
}

} // namespace layouts::ui
