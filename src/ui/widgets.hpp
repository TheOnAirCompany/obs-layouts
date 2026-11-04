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

/* Small Figma-like building blocks for the docks. */

#include "core/expression.hpp"

#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QWidget>

class QButtonGroup;

namespace layouts::ui {

/* Label that changes a value when dragged horizontally.
 * Shift = x10, Alt = x0.1. */
class ScrubLabel : public QLabel {
	Q_OBJECT

public:
	explicit ScrubLabel(const QString &text, QWidget *parent = nullptr);

signals:
	void scrubbed(double delta, bool finished);

protected:
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;

private:
	bool pressed_ = false;
	bool moved_ = false;
	double lastX_ = 0;
	double accum_ = 0;
};

/* Numeric input with math expressions, a mixed state and a scrub label. */
class NumericField : public QWidget {
	Q_OBJECT

public:
	NumericField(const QString &label, const QString &tooltip, QWidget *parent = nullptr);

	void setValue(double v);
	void setMixed();
	void setDecimals(int d) { decimals_ = d; }
	void setPercentBase(double b) { percentBase_ = b; }
	void setSuffix(const QString &s) { suffix_ = s; }
	QLineEdit *edit() const { return edit_; }

signals:
	void committed(const layouts::core::Expression &e);
	void scrubbed(double delta, bool finished);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	void commit();
	void showInvalid(const QString &why);
	void revert();

	ScrubLabel *label_;
	QLineEdit *edit_;
	QString shown_;
	QString suffix_;
	int decimals_ = 2;
	double percentBase_ = 0;
	bool mixed_ = false;
};

/* 3x3 reference point picker. */
class RefPointPicker : public QWidget {
	Q_OBJECT

public:
	explicit RefPointPicker(QWidget *parent = nullptr);
	void setIndex(int i);
	int index() const { return index_; }
	QSize sizeHint() const override;

signals:
	void changed(int index);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;

private:
	QRectF cell(int i) const;
	int index_ = 0;
};

/* Square tool button with a Lucide icon. */
QToolButton *iconButton(const QString &iconName, const QString &tooltip, QWidget *parent, bool checkable = false);

/* Section title row (uppercase, small). */
QWidget *sectionTitle(const QString &text, QWidget *parent);

/* A row of exclusive checkable text buttons. Returns the group (ids = indexes). */
QButtonGroup *segmented(const QStringList &labels, QWidget *container);

} // namespace layouts::ui
