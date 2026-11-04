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

#include "widgets.hpp"

#include "theme.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

namespace layouts::ui {

/* ---------------------------------------------------------------- ScrubLabel */

ScrubLabel::ScrubLabel(const QString &text, QWidget *parent) : QLabel(text, parent)
{
	setCursor(Qt::SizeHorCursor);
	setAlignment(Qt::AlignCenter);
	setFixedWidth(tokens::kLabelWidth);
	QPalette p = palette();
	p.setColor(QPalette::WindowText, p.color(QPalette::Disabled, QPalette::WindowText));
	setPalette(p);
}

void ScrubLabel::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton)
		return QLabel::mousePressEvent(e);
	pressed_ = true;
	moved_ = false;
	lastX_ = e->globalPosition().x();
	accum_ = 0;
}

void ScrubLabel::mouseMoveEvent(QMouseEvent *e)
{
	if (!pressed_)
		return;
	const double x = e->globalPosition().x();
	double step = 1.0;
	if (e->modifiers() & Qt::ShiftModifier)
		step = 10.0;
	else if (e->modifiers() & Qt::AltModifier)
		step = 0.1;
	accum_ += (x - lastX_) * step;
	lastX_ = x;
	if (!moved_ && std::fabs(accum_) < 1e-9)
		return;
	moved_ = true;
	emit scrubbed(accum_, false);
}

void ScrubLabel::mouseReleaseEvent(QMouseEvent *e)
{
	if (!pressed_)
		return QLabel::mouseReleaseEvent(e);
	pressed_ = false;
	if (moved_)
		emit scrubbed(accum_, true);
}

/* -------------------------------------------------------------- NumericField */

NumericField::NumericField(const QString &label, const QString &tooltip, QWidget *parent) : QWidget(parent)
{
	auto *lay = new QHBoxLayout(this);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(4);
	label_ = new ScrubLabel(label, this);
	label_->setToolTip(tooltip);
	edit_ = new QLineEdit(this);
	edit_->setMinimumHeight(tokens::kFieldHeight);
	edit_->setMinimumWidth(tokens::kFieldMinWidth);
	edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	edit_->setToolTip(tooltip + "\n" + T("Field.Hint"));
	edit_->installEventFilter(this);
	lay->addWidget(label_);
	lay->addWidget(edit_, 1);

	connect(edit_, &QLineEdit::editingFinished, this, &NumericField::commit);
	connect(label_, &ScrubLabel::scrubbed, this, &NumericField::scrubbed);
}

void NumericField::setValue(double v)
{
	mixed_ = false;
	edit_->setPlaceholderText(QString());
	shown_ = QString::fromStdString(core::formatNumber(v, decimals_)) + suffix_;
	if (!edit_->hasFocus())
		edit_->setText(shown_);
}

void NumericField::setMixed()
{
	mixed_ = true;
	shown_.clear();
	edit_->setPlaceholderText(T("Field.Mixed"));
	if (!edit_->hasFocus())
		edit_->clear();
}

void NumericField::revert()
{
	edit_->setText(shown_);
	edit_->setProperty("invalid", false);
	edit_->setStyleSheet(QString());
}

void NumericField::showInvalid(const QString &why)
{
	edit_->setStyleSheet("QLineEdit { border: 1px solid #E5484D; }");
	edit_->setToolTip(T("Field.Invalid").arg(why));
	QTimer::singleShot(1500, this, [this] {
		if (!edit_->hasFocus())
			revert();
		else
			edit_->setStyleSheet(QString());
	});
}

void NumericField::commit()
{
	const QString text = edit_->text().trimmed();
	if (text.isEmpty() || text == shown_) {
		revert();
		return;
	}
	const core::Expression e = core::parseExpression(text.toStdString(), percentBase_);
	if (!e.ok) {
		showInvalid(QString::fromStdString(e.error));
		return;
	}
	edit_->setStyleSheet(QString());
	emit committed(e);
	/* The context refresh will push the real value back. */
	edit_->clearFocus();
}

bool NumericField::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == edit_ && event->type() == QEvent::KeyPress) {
		auto *ke = static_cast<QKeyEvent *>(event);
		if (ke->key() == Qt::Key_Escape) {
			revert();
			edit_->clearFocus();
			return true;
		}
		if (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down) {
			const double step = (ke->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
			const core::Expression e = core::parseExpression(
				(ke->key() == Qt::Key_Up ? "+" : "-") + core::formatNumber(step), 0);
			emit committed(e);
			return true;
		}
	}
	if (obj == edit_ && event->type() == QEvent::FocusIn) {
		QTimer::singleShot(0, edit_, &QLineEdit::selectAll);
	}
	return QWidget::eventFilter(obj, event);
}

/* ------------------------------------------------------------ RefPointPicker */

RefPointPicker::RefPointPicker(QWidget *parent) : QWidget(parent)
{
	setFixedSize(sizeHint());
	setFocusPolicy(Qt::StrongFocus);
	setCursor(Qt::PointingHandCursor);
	setToolTip(T("Ref.Tooltip"));
}

QSize RefPointPicker::sizeHint() const
{
	return {tokens::kRefGridSize, tokens::kRefGridSize};
}

QRectF RefPointPicker::cell(int i) const
{
	const qreal s = width() / 3.0;
	return QRectF((i % 3) * s, (i / 3) * s, s, s);
}

void RefPointPicker::setIndex(int i)
{
	i = std::clamp(i, 0, 8);
	if (i == index_)
		return;
	index_ = i;
	update();
}

void RefPointPicker::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QPalette pal = palette();
	QColor frame = pal.color(QPalette::Mid);
	p.setPen(QPen(frame, 1));
	p.setBrush(pal.color(QPalette::Base));
	p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

	/* Lines joining the dots, like the reference grid of design tools. */
	QColor line = pal.color(QPalette::Disabled, QPalette::WindowText);
	line.setAlphaF(0.35f);
	p.setPen(QPen(line, 1));
	for (int k = 0; k < 3; k++) {
		p.drawLine(cell(k * 3).center(), cell(k * 3 + 2).center());
		p.drawLine(cell(k).center(), cell(k + 6).center());
	}

	for (int i = 0; i < 9; i++) {
		const QPointF c = cell(i).center();
		if (i == index_) {
			p.setPen(Qt::NoPen);
			p.setBrush(pal.color(QPalette::Highlight));
			p.drawEllipse(c, 4.0, 4.0);
		} else {
			p.setPen(Qt::NoPen);
			p.setBrush(pal.color(QPalette::WindowText));
			p.drawEllipse(c, 1.8, 1.8);
		}
	}
	if (hasFocus()) {
		p.setPen(QPen(pal.color(QPalette::Highlight), 1));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4, 4);
	}
}

void RefPointPicker::mousePressEvent(QMouseEvent *e)
{
	for (int i = 0; i < 9; i++) {
		if (cell(i).contains(e->position())) {
			setIndex(i);
			emit changed(i);
			return;
		}
	}
}

void RefPointPicker::keyPressEvent(QKeyEvent *e)
{
	int i = index_;
	switch (e->key()) {
	case Qt::Key_Left:
		if (i % 3 > 0)
			i--;
		break;
	case Qt::Key_Right:
		if (i % 3 < 2)
			i++;
		break;
	case Qt::Key_Up:
		if (i >= 3)
			i -= 3;
		break;
	case Qt::Key_Down:
		if (i < 6)
			i += 3;
		break;
	default:
		return QWidget::keyPressEvent(e);
	}
	if (i != index_) {
		setIndex(i);
		emit changed(i);
	}
}

/* ------------------------------------------------------------------- helpers */

QToolButton *iconButton(const QString &iconName, const QString &tooltip, QWidget *parent, bool checkable)
{
	auto *b = new QToolButton(parent);
	b->setIcon(icon(iconName, parent));
	b->setIconSize(QSize(tokens::kIconSize, tokens::kIconSize));
	b->setToolTip(tooltip);
	b->setAccessibleName(tooltip);
	b->setCheckable(checkable);
	b->setAutoRaise(true);
	b->setFixedSize(tokens::kIconButton, tokens::kIconButton);
	b->setFocusPolicy(Qt::TabFocus);
	return b;
}

QWidget *sectionTitle(const QString &text, QWidget *parent)
{
	auto *l = new QLabel(text.toUpper(), parent);
	QFont f = l->font();
	f.setPointSizeF(f.pointSizeF() * tokens::kSectionTitlePt);
	f.setBold(true);
	f.setLetterSpacing(QFont::PercentageSpacing, 105);
	l->setFont(f);
	QPalette p = l->palette();
	p.setColor(QPalette::WindowText, p.color(QPalette::Disabled, QPalette::WindowText));
	l->setPalette(p);
	return l;
}

QButtonGroup *segmented(const QStringList &labels, QWidget *container)
{
	auto *lay = new QHBoxLayout(container);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(0);
	auto *group = new QButtonGroup(container);
	group->setExclusive(true);
	for (int i = 0; i < labels.size(); i++) {
		auto *b = new QToolButton(container);
		b->setText(labels[i]);
		b->setCheckable(true);
		b->setAutoRaise(false);
		b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
		b->setMinimumHeight(tokens::kFieldHeight);
		b->setMinimumWidth(40);
		b->setToolTip(labels[i]);
		group->addButton(b, i);
		lay->addWidget(b);
	}
	return group;
}

} // namespace layouts::ui
