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

#include "theme.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QWidget>

namespace layouts::ui {

QIcon icon(const QString &name, const QColor &color, int size)
{
	static QHash<QString, QIcon> cache;
	const QString key = name + '|' + color.name(QColor::HexArgb) + '|' + QString::number(size);
	auto it = cache.constFind(key);
	if (it != cache.constEnd())
		return it.value();

	QFile f(":/layouts/icons/" + name + ".svg");
	if (!f.open(QIODevice::ReadOnly))
		return QIcon();
	QByteArray svg = f.readAll();
	svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());
	QSvgRenderer renderer(svg);
	if (!renderer.isValid())
		return QIcon();

	QIcon ic;
	for (qreal dpr : {1.0, 2.0, 3.0}) {
		QPixmap pm(QSize(size, size) * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setOpacity(color.alphaF());
		renderer.render(&p, QRectF(0, 0, size, size));
		p.end();
		ic.addPixmap(pm);
	}
	cache.insert(key, ic);
	return ic;
}

QIcon icon(const QString &name, const QWidget *w)
{
	const QPalette pal = w ? w->palette() : QApplication::palette();
	QIcon base = icon(name, pal.color(QPalette::Active, QPalette::ButtonText), tokens::kIconSize);
	/* Disabled state: same glyph, dimmed. */
	QColor dim = pal.color(QPalette::Disabled, QPalette::ButtonText);
	if (dim == pal.color(QPalette::Active, QPalette::ButtonText))
		dim.setAlphaF(0.4f);
	const QIcon disabled = icon(name, dim, tokens::kIconSize);
	QIcon out;
	for (qreal dpr : {1.0, 2.0, 3.0}) {
		const QSize sz = QSize(tokens::kIconSize, tokens::kIconSize) * dpr;
		QPixmap a = base.pixmap(sz);
		a.setDevicePixelRatio(dpr);
		out.addPixmap(a, QIcon::Normal);
		QPixmap d = disabled.pixmap(sz);
		d.setDevicePixelRatio(dpr);
		out.addPixmap(d, QIcon::Disabled);
	}
	return out;
}

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace layouts::ui
