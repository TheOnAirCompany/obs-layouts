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

#include "style-editor.hpp"

#include "role-ui.hpp"
#include "theme.hpp"
#include "widgets.hpp"

#include "obs/item-style.hpp"
#include "obs/store.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace layouts::ui {

namespace {

QHBoxLayout *row(QWidget *parent)
{
	auto *l = new QHBoxLayout(parent);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(tokens::kColumnSpacing);
	return l;
}

QIcon swatchIcon(const QColor &c, bool mixed)
{
	const int s = tokens::kSwatchSize;
	QPixmap pm(s, s);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r(0.5, 0.5, s - 1, s - 1);
	if (mixed) {
		p.setPen(QPen(QColor::fromRgba(tokens::kSwatchOutline), 1, Qt::DashLine));
		p.drawRoundedRect(r, 3, 3);
	} else {
		p.setPen(QPen(QColor::fromRgba(tokens::kSwatchOutline), 1));
		p.setBrush(c);
		p.drawRoundedRect(r, 3, 3);
	}
	return QIcon(pm);
}

QToolButton *swatch(const QString &tooltip, QWidget *parent)
{
	auto *b = new QToolButton(parent);
	b->setToolTip(tooltip);
	b->setIconSize(QSize(tokens::kSwatchSize, tokens::kSwatchSize));
	b->setFixedSize(tokens::kIconButton, tokens::kFieldHeight);
	return b;
}

/* Checks the button of `id` in an exclusive group, or none (mixed) when id < 0. */
void checkSegment(QButtonGroup *g, int id)
{
	g->setExclusive(false);
	for (QAbstractButton *b : g->buttons())
		b->setChecked(g->id(b) == id);
	g->setExclusive(true);
}

void setCheck(QCheckBox *box, bool mixed, bool on)
{
	box->setTristate(mixed);
	box->setCheckState(mixed ? Qt::PartiallyChecked : on ? Qt::Checked : Qt::Unchecked);
}

/* Value shown by a field: the common one, or "Mixed". */
template<typename Get> void showValue(NumericField *f, const std::vector<core::Style> &styles, Get get)
{
	const double v = get(styles.front());
	const bool same = std::all_of(styles.begin(), styles.end(),
				      [&](const core::Style &s) { return std::abs(get(s) - v) < 1e-6; });
	if (same)
		f->setValue(v);
	else
		f->setMixed();
}

template<typename Get> int commonId(const std::vector<core::Style> &styles, Get get)
{
	const int v = get(styles.front());
	for (const core::Style &s : styles)
		if (get(s) != v)
			return -1;
	return v;
}

} // namespace

StyleEditor::StyleEditor(QWidget *parent) : QWidget(parent)
{
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(tokens::kRowSpacing);

	/* ---- corners ---- */
	{
		auto *w = new QWidget(this);
		auto *l = row(w);
		auto *seg = new QWidget(w);
		corners_ = segmented({T("Style.Corners.Auto"), T("Style.Corners.Px"), T("Style.Corners.Round"),
				      T("Style.Corners.None")},
				     seg);
		corners_->button(0)->setToolTip(T("Style.Corners.AutoTooltip"));
		corners_->button(1)->setToolTip(T("Style.Corners.PxTooltip"));
		corners_->button(2)->setToolTip(T("Style.Corners.RoundTooltip"));
		corners_->button(3)->setToolTip(T("Style.Corners.NoneTooltip"));
		connect(corners_, &QButtonGroup::idClicked, this, [this](int id) {
			const auto mode = (core::RadiusMode)id;
			emitChange([mode](core::Style &s) { s.radiusMode = mode; });
		});
		radiusPx_ = new NumericField(QString::fromUtf8("\xE2\x95\xAD"), T("Field.Radius"), w);
		radiusPx_->setDecimals(0);
		radiusPx_->setMaximumWidth(tokens::kStyleFieldWidth);
		connect(radiusPx_, &NumericField::committed, this, [this](const core::Expression &e) {
			emitChange([e](core::Style &s) {
				double out = 0;
				if (e.apply(s.radiusPx, out)) {
					s.radiusPx = std::max(0.0, out);
					s.radiusMode = core::RadiusMode::Pixels;
				}
			});
		});
		l->addWidget(seg, 1);
		l->addWidget(radiusPx_);
		v->addWidget(w);
	}

	/* ---- border ---- */
	{
		auto *w = new QWidget(this);
		auto *l = row(w);
		border_ = new QCheckBox(T("Style.Border"), w);
		border_->setToolTip(T("Style.BorderTooltip"));
		connect(border_, &QCheckBox::clicked, this, [this] {
			const bool on = border_->checkState() != Qt::Unchecked;
			emitChange([on](core::Style &s) { s.border = on; });
		});
		borderWidth_ = new NumericField(QString::fromUtf8("\xE2\x96\xAD"), T("Style.BorderWidth"), w);
		borderWidth_->setDecimals(1);
		borderWidth_->setMaximumWidth(tokens::kStyleFieldWidth);
		connect(borderWidth_, &NumericField::committed, this, [this](const core::Expression &e) {
			emitChange([e](core::Style &s) {
				double out = 0;
				if (e.apply(s.borderWidth, out)) {
					s.borderWidth = std::max(0.0, out);
					s.border = true;
				}
			});
		});
		borderColor_ = swatch(T("Style.BorderColor"), w);
		connect(borderColor_, &QToolButton::clicked, this, [this] { pickColor(true); });
		l->addWidget(border_, 1);
		l->addWidget(borderWidth_);
		l->addWidget(borderColor_);
		v->addWidget(w);
	}

	/* ---- shadow ---- */
	{
		auto *seg = new QWidget(this);
		shadow_ = segmented({T("Style.Shadow.None"), T("Style.Shadow.Soft"), T("Style.Shadow.Hard"),
				     T("Style.Shadow.Custom")},
				    seg);
		for (QAbstractButton *b : shadow_->buttons())
			b->setToolTip(T("Style.ShadowTooltip"));
		connect(shadow_, &QButtonGroup::idClicked, this, [this](int id) {
			const auto preset = (core::ShadowPreset)id;
			emitChange([preset](core::Style &s) { s.shadow = preset; });
		});
		auto *titled = new QWidget(this);
		auto *tl = row(titled);
		auto *label = new QLabel(T("Style.Shadow"), titled);
		tl->addWidget(label);
		tl->addWidget(seg, 1);
		v->addWidget(titled);

		custom_ = new QWidget(this);
		auto *g = new QGridLayout(custom_);
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(tokens::kColumnSpacing);
		g->setVerticalSpacing(tokens::kRowSpacing);
		using Member = double core::Style::*;
		auto bindNumber = [this](NumericField *f, Member m, double scale, double lo, double hi) {
			connect(f, &NumericField::committed, this, [this, m, scale, lo, hi](const core::Expression &e) {
				emitChange([e, m, scale, lo, hi](core::Style &s) {
					double out = 0;
					if (e.apply(s.*m * scale, out))
						s.*m = std::clamp(out, lo, hi) / scale;
				});
			});
		};
		blur_ = new NumericField(QString::fromUtf8("\xE2\x97\x8C"), T("Style.ShadowBlur"), custom_);
		blur_->setDecimals(0);
		bindNumber(blur_, &core::Style::shadowBlur, 1.0, 0.0, 500.0);
		offsetX_ = new NumericField("X", T("Style.ShadowX"), custom_);
		offsetX_->setDecimals(0);
		bindNumber(offsetX_, &core::Style::shadowOffsetX, 1.0, -1000.0, 1000.0);
		offsetY_ = new NumericField("Y", T("Style.ShadowY"), custom_);
		offsetY_->setDecimals(0);
		bindNumber(offsetY_, &core::Style::shadowOffsetY, 1.0, -1000.0, 1000.0);
		opacity_ = new NumericField(QString::fromUtf8("\xE2\x97\x90"), T("Style.ShadowOpacity"), custom_);
		opacity_->setDecimals(0);
		opacity_->setSuffix("%");
		bindNumber(opacity_, &core::Style::shadowOpacity, 100.0, 0.0, 100.0);
		shadowColor_ = swatch(T("Style.ShadowColor"), custom_);
		connect(shadowColor_, &QToolButton::clicked, this, [this] { pickColor(false); });
		g->addWidget(blur_, 0, 0);
		g->addWidget(opacity_, 0, 1);
		g->addWidget(shadowColor_, 0, 2);
		g->addWidget(offsetX_, 1, 0);
		g->addWidget(offsetY_, 1, 1);
		g->setColumnStretch(0, 1);
		g->setColumnStretch(1, 1);
		v->addWidget(custom_);
	}

	fade_ = new QCheckBox(T("Style.Fade"), this);
	fade_->setToolTip(T("Style.FadeTooltip"));
	connect(fade_, &QCheckBox::clicked, this, [this] {
		const bool on = fade_->checkState() != Qt::Unchecked;
		emitChange([on](core::Style &s) { s.fadeShadowInTransition = on; });
	});
	v->addWidget(fade_);

	setStyles({core::Style{}});
}

void StyleEditor::emitChange(const StyleChange &change)
{
	if (updating_)
		return;
	/* Show the result right away; the owner refreshes with the real values after. */
	for (core::Style &s : styles_)
		change(s);
	setStyles(styles_);
	emit changed(change);
}

void StyleEditor::pickColor(bool border)
{
	if (styles_.empty())
		return;
	const uint32_t current = border ? styles_.front().borderColor : styles_.front().shadowColor;
	const QColor c = QColorDialog::getColor(QColor::fromRgba(current), this,
						T(border ? "Style.BorderColor" : "Style.ShadowColor"),
						QColorDialog::ShowAlphaChannel);
	if (!c.isValid())
		return;
	const uint32_t argb = c.rgba();
	if (border)
		emitChange([argb](core::Style &s) {
			s.borderColor = argb;
			s.border = true;
		});
	else
		emitChange([argb](core::Style &s) { s.shadowColor = argb; });
}

void StyleEditor::setStyles(const std::vector<core::Style> &styles)
{
	styles_ = styles.empty() ? std::vector<core::Style>{core::Style{}} : styles;
	updating_ = true;
	const auto &all = styles_;

	const int corners = commonId(all, [](const core::Style &s) { return (int)s.radiusMode; });
	checkSegment(corners_, corners);
	showValue(radiusPx_, all, [](const core::Style &s) { return s.radiusPx; });
	radiusPx_->setEnabled(corners == (int)core::RadiusMode::Pixels || corners < 0);

	const int border = commonId(all, [](const core::Style &s) { return (int)s.border; });
	setCheck(border_, border < 0, border == 1);
	showValue(borderWidth_, all, [](const core::Style &s) { return s.borderWidth; });
	borderWidth_->setEnabled(border != 0);
	const int bColor = commonId(all, [](const core::Style &s) { return (int)s.borderColor; });
	borderColor_->setIcon(swatchIcon(QColor::fromRgba(all.front().borderColor), bColor == -1 && all.size() > 1));
	borderColor_->setEnabled(border != 0);

	const int shadow = commonId(all, [](const core::Style &s) { return (int)s.shadow; });
	checkSegment(shadow_, shadow);
	custom_->setVisible(shadow == (int)core::ShadowPreset::Custom);
	showValue(blur_, all, [](const core::Style &s) { return s.shadowBlur; });
	showValue(offsetX_, all, [](const core::Style &s) { return s.shadowOffsetX; });
	showValue(offsetY_, all, [](const core::Style &s) { return s.shadowOffsetY; });
	showValue(opacity_, all, [](const core::Style &s) { return s.shadowOpacity * 100.0; });
	const int sColor = commonId(all, [](const core::Style &s) { return (int)s.shadowColor; });
	shadowColor_->setIcon(swatchIcon(QColor::fromRgba(all.front().shadowColor), sColor == -1 && all.size() > 1));

	const int fade = commonId(all, [](const core::Style &s) { return (int)s.fadeShadowInTransition; });
	setCheck(fade_, fade < 0, fade == 1);
	fade_->setEnabled(shadow != (int)core::ShadowPreset::None);

	updating_ = false;
}

/* ---- Styles by type ---- */

StylesByTypeDialog::StylesByTypeDialog(core::Role initial, QWidget *parent) : QDialog(parent)
{
	setWindowTitle(T("Style.ByType.Title"));
	setAttribute(Qt::WA_DeleteOnClose);

	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin);
	v->setSpacing(tokens::kSectionSpacing);

	auto *hint = new QLabel(T("Style.ByType.Hint"), this);
	hint->setWordWrap(true);
	v->addWidget(hint);

	auto *body = new QWidget(this);
	auto *h = new QHBoxLayout(body);
	h->setContentsMargins(0, 0, 0, 0);
	h->setSpacing(tokens::kSectionSpacing);

	list_ = new QListWidget(body);
	list_->setIconSize(rolePillSize());
	list_->setItemDelegate(new PillDelegate(list_));
	list_->setFixedWidth(tokens::kStyleRoleListWidth);
	for (core::Role r : roleOrder()) {
		auto *item = new QListWidgetItem(rolePill(r), roleName(r), list_);
		item->setData(Qt::UserRole, (int)r);
		if (r == initial)
			list_->setCurrentItem(item);
	}
	if (!list_->currentItem())
		list_->setCurrentRow(0);
	h->addWidget(list_);

	editor_ = new StyleEditor(body);
	editor_->setMinimumWidth(tokens::kInspectorWidth - 2 * tokens::kPanelMargin);
	auto *right = new QVBoxLayout();
	right->addWidget(editor_);
	right->addStretch();
	h->addLayout(right, 1);
	v->addWidget(body, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	v->addWidget(buttons);

	connect(list_, &QListWidget::currentRowChanged, this, [this] { sync(); });
	connect(editor_, &StyleEditor::changed, this, [this](const StyleChange &change) {
		const core::Role role = current();
		core::Style s = Store::instance().typeStyle(role);
		change(s);
		Store::instance().setTypeStyle(role, s);
		itemstyle::reapplyType(role);
	});
	sync();
}

core::Role StylesByTypeDialog::current() const
{
	const QListWidgetItem *item = list_->currentItem();
	return item ? (core::Role)item->data(Qt::UserRole).toInt() : core::Role::None;
}

void StylesByTypeDialog::sync()
{
	editor_->setStyles({Store::instance().typeStyle(current())});
}

} // namespace layouts::ui
