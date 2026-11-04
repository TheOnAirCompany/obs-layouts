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

#include "obs/commands.hpp"

#include <QWidget>

#include <map>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QToolButton;
class QVBoxLayout;

namespace layouts::ui {

class NumericField;
class RefPointPicker;

class InspectorDock : public QWidget {
	Q_OBJECT

public:
	explicit InspectorDock(QWidget *parent = nullptr);

public slots:
	void refresh();

private:
	void build();
	void updateLive();
	void bindField(NumericField *f, commands::Field field);
	QWidget *section(const QString &title, QWidget *body);
	QWidget *buildLayoutSection();
	QWidget *buildPresetSection();
	void syncLayoutControls();
	void previewLayout(bool force = false);
	void previewPreset(bool force = false, int index = -1);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	QLabel *title_ = nullptr;
	QLabel *subtitle_ = nullptr;
	QLabel *live_ = nullptr;
	QLabel *empty_ = nullptr;
	QToolButton *lock_ = nullptr;
	QToolButton *copy_ = nullptr;
	QToolButton *paste_ = nullptr;
	QWidget *content_ = nullptr;

	/* sections that depend on the mode */
	QWidget *alignTargetRow_ = nullptr;
	QButtonGroup *alignTarget_ = nullptr;
	QComboBox *keyObject_ = nullptr;
	QWidget *arrangeSection_ = nullptr;
	QWidget *cropSection_ = nullptr;
	QWidget *boundsSection_ = nullptr;

	std::map<commands::Field, NumericField *> fields_;
	RefPointPicker *ref_ = nullptr;
	QToolButton *ratio_ = nullptr;
	std::vector<QToolButton *> alignButtons_;
	std::vector<QToolButton *> distributeButtons_;
	std::vector<QToolButton *> sizeButtons_;
	NumericField *gapX_ = nullptr;
	NumericField *gapY_ = nullptr;
	QComboBox *boundsType_ = nullptr;
	QComboBox *boundsAlign_ = nullptr;
	QCheckBox *cropToBounds_ = nullptr;
	bool updating_ = false;

	/* auto layout (multi) */
	QWidget *layoutSection_ = nullptr;
	QButtonGroup *flowDir_ = nullptr;
	QComboBox *flowContainer_ = nullptr;
	QComboBox *flowMain_ = nullptr;
	QComboBox *flowCross_ = nullptr;
	QComboBox *flowSizing_ = nullptr;
	QSpinBox *flowColumns_ = nullptr;
	QWidget *columnsRow_ = nullptr;
	NumericField *flowGap_ = nullptr;
	NumericField *flowPadding_ = nullptr;

	/* presets */
	QWidget *presetSection_ = nullptr;
	QComboBox *preset_ = nullptr;
	NumericField *presetMargin_ = nullptr;
	NumericField *presetGap_ = nullptr;
	NumericField *presetCorner_ = nullptr;
	QToolButton *presetApply_ = nullptr;
};

} // namespace layouts::ui
