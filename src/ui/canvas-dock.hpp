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

#include <QDialog>
#include <QWidget>

class QComboBox;
class QLabel;
class QToolButton;

namespace layouts::ui {

class CanvasView;
class RulerWidget;
class RulerCorner;

class CanvasDock : public QWidget {
	Q_OBJECT

public:
	explicit CanvasDock(QWidget *parent = nullptr);
	CanvasView *view() const { return view_; }
	QToolButton *followPin() const { return pin_; }
	QLabel *liveChip() const { return chip_; }
	QLabel *breadcrumb() const { return crumb_; }

signals:
	/* "Scene Builder ↗" in the toolbar. */
	void openBuilderRequested();

private slots:
	void refreshScenes();
	void refreshStatus();
	void refreshToggles();
	void refreshLive();

private:
	QToolButton *toggle(const QString &icon, const QString &tip);

	CanvasView *view_ = nullptr;
	RulerWidget *hRuler_ = nullptr;
	RulerWidget *vRuler_ = nullptr;
	RulerCorner *corner_ = nullptr;
	QToolButton *pin_ = nullptr;
	QLabel *crumb_ = nullptr;
	QLabel *chip_ = nullptr;
	QToolButton *zoomLabel_ = nullptr;
	QToolButton *rulers_ = nullptr;
	QToolButton *guides_ = nullptr;
	QToolButton *grid_ = nullptr;
	QToolButton *snap_ = nullptr;
	QToolButton *safe_ = nullptr;
	QLabel *status_ = nullptr;
	QString refused_;
};

/* Grid, snapping, safe areas, guide color and live policy. */
class SettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit SettingsDialog(QWidget *parent = nullptr);
};

} // namespace layouts::ui
