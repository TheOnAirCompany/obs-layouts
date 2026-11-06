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
#include "widgets.hpp"

#include "core/broadcast.hpp"
#include "core/mirror.hpp"
#include "obs/store.hpp"

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include "core/user-layouts.hpp"
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

struct ThumbColors {
	QColor camera, screen, chat, bg;
};

/* Outline of a slot in its own (unrotated) cell. */
QPainterPath slotPath(const core::Slot &s, const QRectF &r)
{
	QPainterPath path;
	if (s.shape == core::SlotShape::Polygon && s.polygon.size() >= 3) {
		QPolygonF poly;
		for (const core::Vec2 &p : s.polygon)
			poly << QPointF(r.x() + p.x * r.width(), r.y() + p.y * r.height());
		poly << poly.first();
		path.addPolygon(poly);
	} else if (s.shape == core::SlotShape::Round) {
		const double rad = std::min(r.width(), r.height()) / 2;
		path.addRoundedRect(r, rad, rad);
	} else {
		const double rad = s.rotation != 0 ? 2.5 : 1.5;
		path.addRoundedRect(r, rad, rad);
	}
	return path;
}

void drawSlot(QPainter &p, const core::Slot &s, const ThumbColors &c, double k)
{
	const QRectF cell(s.cell.x, s.cell.y, s.cell.w, s.cell.h);
	const QRectF r = cell.adjusted(0.5, 0.5, -0.5, -0.5);
	p.save();
	if (s.rotation != 0) {
		const QPointF center = cell.center();
		p.translate(center);
		p.rotate(s.rotation); /* degrees, clockwise on screen */
		p.translate(-center);
	}
	/* Polaroid frame: a white border, deeper at the bottom. */
	if (s.frame) {
		const double b = std::max(1.0, 22 * k);
		p.setBrush(QColor::fromRgba(tokens::kThumbFrame));
		p.drawRoundedRect(r.adjusted(-b, -b, b, std::max(2.5, 88 * k)), 0.8, 0.8);
	}
	const QPainterPath path = slotPath(s, r);
	const QColor fill = s.kind == core::SlotKind::Camera ? c.camera
			    : s.kind == core::SlotKind::Chat ? c.chat
							     : c.screen;
	p.setBrush(fill);
	p.drawPath(path);
	p.setClipPath(path, Qt::IntersectClip);

	QColor ink = c.bg;
	if (s.kind == core::SlotKind::Camera) {
		/* a head and shoulders */
		const QPointF h(r.x() + r.width() / 2, r.y() + r.height() * 0.42);
		const double rr = std::min(r.width(), r.height()) * 0.16;
		p.setBrush(ink);
		p.drawEllipse(h, rr, rr);
		p.drawChord(QRectF(h.x() - rr * 2, h.y() + rr * 1.3, rr * 4, rr * 3.2), 0, 180 * 16);
	} else if (s.kind == core::SlotKind::Chat) {
		if (s.ticker) {
			/* a band of messages: a row of dots */
			const double d = std::clamp(r.height() * 0.32, 0.8, 2.0);
			ink.setAlphaF(0.6f);
			p.setBrush(ink);
			const double step = d * 3.2;
			for (double x = r.x() + step / 2; x < r.right() - d; x += step)
				p.drawEllipse(QPointF(x, r.center().y()), d / 2, d / 2);
		} else {
			/* three message bubbles: an avatar dot and a line */
			const double lh = std::clamp(r.height() / 7, 1.0, 3.0);
			const double pad = std::max(1.5, std::min(r.width(), r.height()) * 0.12);
			for (int i = 0; i < 3; i++) {
				const double y = r.y() + pad + i * lh * 2;
				if (y + lh > r.bottom() - 0.5)
					break;
				ink.setAlphaF(0.65f);
				p.setBrush(ink);
				p.drawEllipse(QRectF(r.x() + pad, y, lh, lh));
				ink.setAlphaF(0.4f);
				p.setBrush(ink);
				const double len = (r.width() - pad * 2 - lh * 1.6) * (0.55 + ((i * 37) % 40) / 100.0);
				p.drawRoundedRect(QRectF(r.x() + pad + lh * 1.6, y, std::max(1.0, len), lh), lh / 2,
						  lh / 2);
			}
		}
	}
	p.restore();
}

QIcon broadcastThumb(const std::string &key, const ThumbColors &c, double aspect, bool mirrorH, bool mirrorV)
{
	const QSize size(112, 63);
	/* The canvas shape inside the thumbnail box (portrait canvases stay portrait). */
	const double bw = std::min<double>(size.width(), size.height() * aspect);
	const double bh = bw / aspect;
	const QRectF area((size.width() - bw) / 2, (size.height() - bh) / 2, bw, bh);
	const core::Rect canvas{area.x(), area.y(), area.width(), area.height()};
	core::BroadcastSpec spec;
	spec.gap = 2;
	const std::vector<core::Slot> tiles =
		core::mirrorSlots(core::buildBroadcast(key, canvas, spec), canvas, mirrorH, mirrorV);
	/* Template values are in 1920-wide canvas pixels (frame borders). */
	const double k = area.width() / 1920.0;
	QIcon icon;
	for (qreal dpr : {1.0, 2.0}) {
		QPixmap pm(size * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(c.bg);
		p.drawRoundedRect(area, 3, 3);
		p.setClipRect(area);
		for (const core::Slot &s : tiles)
			drawSlot(p, s, c, k);
		if (core::isUserKey(key)) {
			/* Saved by the user: a small accent dot at the top right. */
			const double d = std::max(4.0, area.height() * 0.09);
			p.setBrush(QColor::fromRgba(tokens::kSelection));
			p.drawEllipse(QRectF(area.right() - d * 1.6, area.top() + d * 0.6, d, d));
		}
		p.end();
		icon.addPixmap(pm);
		icon.addPixmap(pm, QIcon::Selected); /* no tint: the outline shows the selection */
	}
	return icon;
}

int mirrorBits()
{
	const ViewSettings &s = Store::instance().settings();
	return (s.broadcastMirrorH ? 1 : 0) | (s.broadcastMirrorV ? 2 : 0);
}

} // namespace

/* Exported for the Switcher dock: the gallery colors, the stored mirror. */
QIcon broadcastThumb(const std::string &key, const QColor &camera, const QColor &screen, const QColor &bg,
		     double aspect)
{
	const ViewSettings &s = Store::instance().settings();
	return broadcastThumb(key, ThumbColors{camera, screen, QColor::fromRgba(tokens::kThumbChat), bg}, aspect,
			      s.broadcastMirrorH, s.broadcastMirrorV);
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

	/* Mirror set from the other gallery (Inspector / Builder): same thumbnails here. */
	connect(&Store::instance(), &Store::settingsChanged, this, [this] {
		syncMirrorButtons();
		if (mirrorBits() != renderedMirror_)
			renderThumbs();
	});
}

bool BroadcastGallery::mirrorH() const
{
	return Store::instance().settings().broadcastMirrorH;
}

bool BroadcastGallery::mirrorV() const
{
	return Store::instance().settings().broadcastMirrorV;
}

QWidget *BroadcastGallery::mirrorBar()
{
	if (mirrorBar_)
		return mirrorBar_;
	mirrorBar_ = new QWidget(parentWidget());
	auto *l = new QHBoxLayout(mirrorBar_);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(2);
	mirrorH_ = iconButton("flip-horizontal-2", T("Broadcast.MirrorH"), mirrorBar_, true);
	mirrorV_ = iconButton("flip-vertical-2", T("Broadcast.MirrorV"), mirrorBar_, true);
	l->addWidget(mirrorH_);
	l->addWidget(mirrorV_);
	auto toggle = [this](bool ViewSettings::*field) {
		return [this, field](bool on) {
			Store::instance().settings().*field = on;
			Store::instance().notifySettingsChanged(); /* saves, and the other gallery follows */
			emit mirrorChanged();
		};
	};
	connect(mirrorH_, &QToolButton::toggled, this, toggle(&ViewSettings::broadcastMirrorH));
	connect(mirrorV_, &QToolButton::toggled, this, toggle(&ViewSettings::broadcastMirrorV));
	syncMirrorButtons();
	mirrorBar_->setVisible(isVisibleTo(parentWidget()));
	return mirrorBar_;
}

void BroadcastGallery::syncMirrorButtons()
{
	if (!mirrorBar_)
		return;
	const QSignalBlocker bh(mirrorH_), bv(mirrorV_);
	mirrorH_->setChecked(mirrorH());
	mirrorV_->setChecked(mirrorV());
}

void BroadcastGallery::setVisible(bool visible)
{
	QListWidget::setVisible(visible);
	if (mirrorBar_)
		mirrorBar_->setVisible(visible);
}

void BroadcastGallery::renderThumbs()
{
	const double aspect = property("aspect").toDouble();
	if (aspect <= 0)
		return;
	const ThumbColors colors{QColor::fromRgba(tokens::kThumbCamera), QColor::fromRgba(tokens::kThumbScreen),
				 QColor::fromRgba(tokens::kRoleChat), palette().color(QPalette::Base)};
	renderedMirror_ = mirrorBits();
	for (int i = 0; i < count(); i++)
		item(i)->setIcon(broadcastThumb(item(i)->data(Qt::UserRole).toString().toStdString(), colors, aspect,
						mirrorH(), mirrorV()));
}

void BroadcastGallery::setTemplates(const QStringList &keys, double aspect)
{
	QStringList shown;
	for (int i = 0; i < count(); i++)
		shown << item(i)->data(Qt::UserRole).toString();
	if (keys == shown && std::fabs(property("aspect").toDouble() - aspect) <= 1e-3 &&
	    property("userGeneration").toInt() == g_userGeneration) {
		if (mirrorBits() != renderedMirror_)
			renderThumbs();
		return;
	}
	setProperty("aspect", aspect);
	setProperty("userGeneration", g_userGeneration);
	const QString current = currentKey();
	const QSignalBlocker block(this);
	clear();
	for (const QString &k : keys) {
		const QString label = broadcastLabel(k.toStdString(), aspect);
		auto *it = new QListWidgetItem(label, this);
		it->setData(Qt::UserRole, k);
		it->setToolTip(core::isUserKey(k.toStdString()) ? T("UserLayout.Tooltip").arg(label) : label);
		if (k == current)
			setCurrentItem(it);
	}
	renderThumbs();
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
