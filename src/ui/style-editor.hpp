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

/* Style editor (0.5): corners, border, shadow and the transition fade. Shared by the
 * Inspector (selected items) and the "Styles by type" dialog (type defaults). It never
 * writes anything itself: each user change is emitted as a function applied to every
 * edited style, so a multi-selection keeps its other differences. */

#include "core/roles.hpp"
#include "core/style.hpp"

#include <QDialog>
#include <QWidget>

#include <functional>
#include <vector>

class QButtonGroup;
class QCheckBox;
class QListWidget;
class QToolButton;

namespace layouts::ui {

class NumericField;

using StyleChange = std::function<void(core::Style &)>;

class StyleEditor : public QWidget {
	Q_OBJECT

public:
	explicit StyleEditor(QWidget *parent = nullptr);

	/* Shows these styles; a control is mixed (nothing checked, "Mixed") where they differ. */
	void setStyles(const std::vector<core::Style> &styles);

signals:
	void changed(const layouts::ui::StyleChange &change);

private:
	void emitChange(const StyleChange &change);
	void pickColor(bool border);

	std::vector<core::Style> styles_;
	bool updating_ = false;

	QButtonGroup *corners_ = nullptr;
	NumericField *radiusPx_ = nullptr;
	QCheckBox *border_ = nullptr;
	NumericField *borderWidth_ = nullptr;
	QToolButton *borderColor_ = nullptr;
	QButtonGroup *shadow_ = nullptr;
	QWidget *custom_ = nullptr;
	NumericField *blur_ = nullptr;
	NumericField *offsetX_ = nullptr;
	NumericField *offsetY_ = nullptr;
	NumericField *opacity_ = nullptr;
	QToolButton *shadowColor_ = nullptr;
	QCheckBox *fade_ = nullptr;
};

/* "Styles by type": the default style of each source type. A change re-applies to
 * every item of that type without its own style, in the current collection. */
class StylesByTypeDialog : public QDialog {
	Q_OBJECT

public:
	explicit StylesByTypeDialog(core::Role initial, QWidget *parent = nullptr);

private:
	core::Role current() const;
	void sync();

	QListWidget *list_ = nullptr;
	StyleEditor *editor_ = nullptr;
};

} // namespace layouts::ui
