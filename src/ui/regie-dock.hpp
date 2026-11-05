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

/* The Switcher dock (OBS main window, Docks menu): a vision-mixer panel for the
 * Plateau scene. Rows: LAYOUTS (thumbnails), one BUS per slot (source buttons), the
 * PREVIEW summary, CUT and AUTO. Red = on program, green = on preview. */

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace layouts::ui {

class RegieDock : public QWidget {
	Q_OBJECT

public:
	explicit RegieDock(QWidget *parent = nullptr);

private:
	void rebuild();
	void fillPlateaus();

	QComboBox *plateau_ = nullptr;
	QWidget *studioBanner_ = nullptr;
	QWidget *rows_ = nullptr;
	QVBoxLayout *rowsLayout_ = nullptr;
	QLabel *summary_ = nullptr;
	QPushButton *cut_ = nullptr;
	QPushButton *auto_ = nullptr;
	bool filling_ = false;
};

} // namespace layouts::ui
