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

#include "role-ui.hpp"

#include "theme.hpp"

#include "obs/edit-context.hpp"
#include "obs/roles.hpp"

#include "core/user-layouts.hpp"

#include <obs-module.h>

#include <QApplication>
#include <QLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QToolButton>

#include <algorithm>
#include <cstring>

namespace layouts::ui {

namespace {

const char *roleKeySuffix(core::Role r)
{
	switch (r) {
	case core::Role::Host:
		return "Host";
	case core::Role::Camera:
		return "Camera";
	case core::Role::Screen:
		return "Screen";
	case core::Role::Background:
		return "Background";
	case core::Role::Graphics:
		return "Graphics";
	case core::Role::Chat:
		return "Chat";
	case core::Role::None:
		break;
	}
	return "None";
}

/* Wrapping row of widgets (the chips must fit a narrow panel). */
class FlowLayout : public QLayout {
public:
	explicit FlowLayout(QWidget *parent, int spacing) : QLayout(parent), spacing_(spacing)
	{
		setContentsMargins(0, 0, 0, 0);
	}
	~FlowLayout() override
	{
		while (QLayoutItem *item = takeAt(0))
			delete item;
	}
	void addItem(QLayoutItem *item) override { items_.push_back(item); }
	int count() const override { return (int)items_.size(); }
	QLayoutItem *itemAt(int i) const override { return i >= 0 && i < count() ? items_[i] : nullptr; }
	QLayoutItem *takeAt(int i) override
	{
		if (i < 0 || i >= count())
			return nullptr;
		QLayoutItem *item = items_[i];
		items_.erase(items_.begin() + i);
		return item;
	}
	Qt::Orientations expandingDirections() const override { return {}; }
	bool hasHeightForWidth() const override { return true; }
	int heightForWidth(int w) const override { return place(QRect(0, 0, w, 0), false); }
	void setGeometry(const QRect &r) override
	{
		QLayout::setGeometry(r);
		place(r, true);
	}
	QSize sizeHint() const override { return minimumSize(); }
	QSize minimumSize() const override
	{
		QSize s;
		for (QLayoutItem *item : items_)
			s = s.expandedTo(item->minimumSize());
		return s;
	}

private:
	int place(const QRect &r, bool apply) const
	{
		int x = r.x(), y = r.y(), lineHeight = 0;
		for (QLayoutItem *item : items_) {
			if (item->widget() && item->widget()->isHidden())
				continue;
			const QSize hint = item->sizeHint();
			if (x > r.x() && x + hint.width() > r.right() + 1) {
				x = r.x();
				y += lineHeight + spacing_;
				lineHeight = 0;
			}
			if (apply)
				item->setGeometry(QRect(QPoint(x, y), hint));
			x += hint.width() + spacing_;
			lineHeight = std::max(lineHeight, hint.height());
		}
		return y + lineHeight - r.y();
	}

	std::vector<QLayoutItem *> items_;
	int spacing_;
};

QIcon dotIcon(const QColor &c)
{
	QIcon icon;
	for (qreal dpr : {1.0, 2.0}) {
		QPixmap pm(QSize(8, 8) * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		p.drawEllipse(QRectF(0.5, 0.5, 7, 7));
		p.end();
		icon.addPixmap(pm);
		icon.addPixmap(pm, QIcon::Selected);
	}
	return icon;
}

} // namespace

const std::vector<core::Role> &roleOrder()
{
	static const std::vector<core::Role> order{core::Role::Host, core::Role::Camera,     core::Role::Screen,
						   core::Role::Chat, core::Role::Background, core::Role::Graphics,
						   core::Role::None};
	return order;
}

QColor roleColor(core::Role r)
{
	switch (r) {
	case core::Role::Host:
		return QColor::fromRgba(tokens::kRoleHost);
	case core::Role::Camera:
		return QColor::fromRgba(tokens::kRoleCamera);
	case core::Role::Screen:
		return QColor::fromRgba(tokens::kRoleScreen);
	case core::Role::Background:
		return QColor::fromRgba(tokens::kRoleBackground);
	case core::Role::Graphics:
		return QColor::fromRgba(tokens::kRoleGraphics);
	case core::Role::Chat:
		return QColor::fromRgba(tokens::kRoleChat);
	case core::Role::None:
		break;
	}
	return QColor::fromRgba(tokens::kRoleNone);
}

QString roleName(core::Role r)
{
	return T((std::string("Role.") + roleKeySuffix(r)).c_str());
}

QString roleShort(core::Role r)
{
	return T((std::string("Role.Short.") + roleKeySuffix(r)).c_str());
}

QString sourceIcon(obs_source_t *src)
{
	if (!src)
		return "square-dashed";
	if (obs_source_is_group(src))
		return "folder";
	if (obs_source_is_scene(src))
		return "clapperboard";
	const char *id = obs_source_get_unversioned_id(src);
	const uint32_t flags = obs_source_get_output_flags(src);
	if (!(flags & OBS_SOURCE_VIDEO))
		return "audio-lines";
	if (!id)
		return "square-dashed";
	auto has = [id](const char *part) {
		return strstr(id, part) != nullptr;
	};
	if (has("av_capture") || has("avcapture") || has("dshow") || has("v4l2") || has("decklink") ||
	    has("ndi_source") || has("aja"))
		return "video";
	if (has("display_capture") || has("monitor_capture") || has("screen_capture") || has("xshm") ||
	    has("desktop-capture"))
		return "monitor";
	if (has("window_capture") || has("game_capture") || has("window-capture"))
		return "app-window";
	if (has("image_source") || has("slideshow"))
		return "image";
	if (has("text_"))
		return "type";
	if (has("browser"))
		return "globe";
	if (has("ffmpeg_source") || has("vlc_source") || has("media"))
		return "film";
	if (has("color_source"))
		return "square";
	return "square-dashed";
}

QString broadcastLabel(const std::string &key, double aspect)
{
	if (core::isUserKey(key)) {
		const core::UserLayout *l = core::UserLayoutRegistry::instance().find(key);
		return l ? QString::fromStdString(l->name) : QString();
	}
	const std::string base = "Broadcast." + key;
	const char *portrait = nullptr;
	if (aspect < 1 && obs_module_get_string((base + ".Portrait").c_str(), &portrait))
		return QString::fromUtf8(portrait);
	return QString::fromUtf8(obs_module_text(base.c_str()));
}

double canvasAspect()
{
	const core::Rect canvas = EditContext::instance().canvasRect();
	return canvas.w > 0 && canvas.h > 0 ? canvas.w / canvas.h : 16.0 / 9.0;
}

QSize rolePillSize()
{
	return {44, 16};
}

QIcon rolePill(core::Role r)
{
	static std::map<core::Role, QIcon> cache;
	if (auto it = cache.find(r); it != cache.end())
		return it->second;
	const QSize size = rolePillSize();
	const QColor color = roleColor(r);
	QIcon icon;
	for (qreal dpr : {1.0, 2.0}) {
		QPixmap pm(size * dpr);
		pm.setDevicePixelRatio(dpr);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		const QRectF box(0.5, 0.5, size.width() - 1, size.height() - 1);
		if (r == core::Role::None) {
			QPen pen(color, 1, Qt::DashLine);
			p.setPen(pen);
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(box, 4, 4);
			p.setPen(color);
		} else {
			p.setPen(Qt::NoPen);
			p.setBrush(color);
			p.drawRoundedRect(box, 4, 4);
			p.setPen(QColor::fromRgba(tokens::kRolePillText));
		}
		QFont f = p.font();
		f.setPixelSize(9);
		f.setBold(true);
		f.setLetterSpacing(QFont::PercentageSpacing, 104);
		p.setFont(f);
		p.drawText(box, Qt::AlignCenter, roleShort(r));
		p.end();
		icon.addPixmap(pm);
		icon.addPixmap(pm, QIcon::Selected); /* keep the type color on selected rows */
	}
	cache[r] = icon;
	return icon;
}

void PillDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt(option);
	initStyleOption(&opt, index);
	const QIcon pill = opt.icon;
	opt.icon = QIcon();
	opt.text.clear();
	/* Selection background and focus as any cell, then the pill on top. */
	const QWidget *w = option.widget;
	(w ? w->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &opt, p, w);
	if (pill.isNull())
		return;
	QRect r(QPoint(0, 0), rolePillSize());
	r.moveCenter(option.rect.center());
	pill.paint(p, r);
}

QSize PillDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	return QStyledItemDelegate::sizeHint(option, index).expandedTo(rolePillSize() + QSize(8, 4));
}

void chooseRole(const std::vector<obs_source_t *> &sources, const QPoint &globalPos, QWidget *parent)
{
	if (sources.empty())
		return;
	const core::Role current = sources.size() == 1 ? roles::explicitRole(sources.front()) : core::Role::None;
	QMenu menu(parent);
	menu.setToolTipsVisible(true);
	for (core::Role r : roleOrder()) {
		QAction *a = menu.addAction(rolePill(r), roleName(r));
		a->setCheckable(true);
		a->setChecked(sources.size() == 1 && r == current && current != core::Role::None);
		a->setData((int)r);
		a->setToolTip(T((std::string("Role.Tooltip.") + roleKeySuffix(r)).c_str()));
	}
	if (QAction *chosen = menu.exec(globalPos))
		roles::setRole(sources, (core::Role)chosen->data().toInt());
}

RoleFilterBar::RoleFilterBar(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsRoleFilter");
	auto *flow = new FlowLayout(this, 4);
	auto make = [&](const QString &text, const QIcon &icon) {
		auto *b = new QToolButton(this);
		b->setCheckable(true);
		b->setAutoRaise(true);
		b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		b->setText(text);
		if (!icon.isNull()) {
			b->setIcon(icon);
			b->setIconSize(QSize(8, 8));
		}
		QFont f = b->font();
		f.setPointSizeF(f.pointSizeF() * 0.9);
		b->setFont(f);
		flow->addWidget(b);
		buttons_.push_back(b);
		return b;
	};
	make(T("Role.All"), QIcon());
	for (core::Role r : roleOrder())
		make(roleName(r), dotIcon(roleColor(r)));
	for (size_t i = 0; i < buttons_.size(); i++) {
		connect(buttons_[i], &QToolButton::clicked, this, [this, i] {
			if (i == 0)
				select(true);
			else
				select(false, roleOrder()[i - 1]);
		});
	}
	setStyleSheet(QString("#LayoutsRoleFilter QToolButton { border: 1px solid palette(mid); border-radius: 9px;"
			      " padding: 1px 7px; }"
			      "#LayoutsRoleFilter QToolButton:checked { border-color: %1; }")
			      .arg(QColor::fromRgba(tokens::kSelection).name()));
	sync();
}

void RoleFilterBar::setCounts(const std::map<core::Role, int> &counts, int total)
{
	counts_ = counts;
	buttons_[0]->setText(T("Role.All") + QString(" %1").arg(total));
	sync();
}

void RoleFilterBar::select(bool all, core::Role r)
{
	all_ = all;
	current_ = r;
	sync();
	emit changed();
}

void RoleFilterBar::sync()
{
	buttons_[0]->setChecked(all_);
	const std::vector<core::Role> &order = roleOrder();
	for (size_t i = 0; i < order.size(); i++) {
		QToolButton *b = buttons_[i + 1];
		const auto it = counts_.find(order[i]);
		const int n = it == counts_.end() ? 0 : it->second;
		const bool current = !all_ && current_ == order[i];
		b->setText(roleName(order[i]) + QString(" %1").arg(n));
		b->setChecked(current);
		/* Only the types present (and the active filter) take room. */
		b->setVisible(n > 0 || current);
	}
	if (layout())
		layout()->invalidate();
	updateGeometry();
}

} // namespace layouts::ui
