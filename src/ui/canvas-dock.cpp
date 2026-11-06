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

#include "canvas-dock.hpp"

#include "canvas-view.hpp"
#include "ruler.hpp"
#include "theme.hpp"
#include "widgets.hpp"

#include "core/expression.hpp"
#include "obs/edit-context.hpp"
#include "obs/scene-directory.hpp"
#include "obs/store.hpp"

#include <obs-frontend-api.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>
#include <tuple>

namespace layouts::ui {

namespace {

ViewSettings &vs()
{
	return Store::instance().settings();
}

QString fmt(double v, int d = 0)
{
	return QString::fromStdString(core::formatNumber(v, d));
}

} // namespace

CanvasDock::CanvasDock(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsCanvas");
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);

	/* ---- toolbar ---- */
	auto *bar = new QWidget(this);
	auto *b = new QHBoxLayout(bar);
	b->setContentsMargins(6, 4, 6, 4);
	b->setSpacing(2);

	/* Follow OBS (0.7): a pin, blue while on. */
	pin_ = iconButton("pin", T("Canvas.FollowTooltip"), bar, true);
	pin_->setObjectName("LayoutsFollowPin");
	pin_->setStyleSheet(QString("QToolButton:checked { background: %1; border-radius: 4px; }")
				    .arg(QColor::fromRgba(tokens::kSelection).name()));
	connect(pin_, &QToolButton::clicked, this, [](bool on) {
		EditContext &ctx = EditContext::instance();
		if (on)
			ctx.follow();
		else if (ctx.sceneSource())
			ctx.pin(ctx.sceneSource()); /* stay on this scene, stop following */
	});
	b->addWidget(pin_);
	b->addSpacing(4);

	/* Breadcrumb: 16:9 › stack › scene. */
	crumb_ = new QLabel(bar);
	crumb_->setObjectName("LayoutsBreadcrumb");
	crumb_->setTextFormat(Qt::RichText);
	crumb_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	crumb_->setMinimumWidth(60);
	b->addWidget(crumb_, 1);

	/* "● PROGRAM": only while the scene is really seen live (streamed or virtual camera). */
	chip_ = new QLabel(QString::fromUtf8("\xE2\x97\x8F ") + T("Live.Chip"), bar);
	chip_->setObjectName("LayoutsLiveChip");
	QFont cf = chip_->font();
	cf.setPointSizeF(cf.pointSizeF() * tokens::kSectionTitlePt);
	cf.setBold(true);
	cf.setLetterSpacing(QFont::PercentageSpacing, 105);
	chip_->setFont(cf);
	chip_->setStyleSheet(
		QString("QLabel { background: %1; color: white; border-radius: %2px; padding: %3px %4px; }")
			.arg(QColor::fromRgba(tokens::kChipLive).name())
			.arg(tokens::kChipRadius)
			.arg(tokens::kChipPadY)
			.arg(tokens::kChipPadX));
	chip_->setVisible(false);
	b->addWidget(chip_);
	b->addSpacing(6);

	auto *zoomOut = iconButton("zoom-out", T("Canvas.ZoomOut"), bar);
	zoomLabel_ = new QToolButton(bar);
	zoomLabel_->setAutoRaise(true);
	zoomLabel_->setPopupMode(QToolButton::InstantPopup);
	zoomLabel_->setToolTip(T("Canvas.ZoomMenu"));
	zoomLabel_->setMinimumWidth(68);
	zoomLabel_->setStyleSheet("QToolButton::menu-indicator { image: none; width: 0; }");
	auto *zoomIn = iconButton("zoom-in", T("Canvas.ZoomIn"), bar);
	b->addWidget(zoomOut);
	b->addWidget(zoomLabel_);
	b->addWidget(zoomIn);
	b->addSpacing(6);

	rulers_ = toggle("ruler", T("Canvas.Rulers"));
	guides_ = toggle("crosshair", T("Canvas.Guides"));
	grid_ = toggle("grid-3x3", T("Canvas.Grid"));
	snap_ = toggle("magnet", T("Canvas.Snapping"));
	safe_ = toggle("frame", T("Canvas.SafeAreas"));
	for (QToolButton *t : {rulers_, guides_, grid_, snap_, safe_})
		b->addWidget(t);
	auto *settings = iconButton("settings", T("Canvas.Settings"), bar);
	b->addWidget(settings);
	b->addSpacing(6);

	/* Gateway to the Scene Builder window. */
	auto *builder = new QToolButton(bar);
	builder->setObjectName("LayoutsOpenBuilder");
	builder->setText(T("Editor.OpenBuilder") + QString::fromUtf8(" \xE2\x86\x97"));
	builder->setToolTip(T("Editor.OpenBuilderTooltip"));
	builder->setAutoRaise(false);
	connect(builder, &QToolButton::clicked, this, &CanvasDock::openBuilderRequested);
	b->addWidget(builder);
	v->addWidget(bar);

	/* ---- rulers + canvas ---- */
	auto *area = new QWidget(this);
	auto *g = new QGridLayout(area);
	g->setContentsMargins(0, 0, 0, 0);
	g->setSpacing(0);
	view_ = new CanvasView(area);
	corner_ = new RulerCorner(area);
	hRuler_ = new RulerWidget(core::Axis::X, view_, area);
	vRuler_ = new RulerWidget(core::Axis::Y, view_, area);
	g->addWidget(corner_, 0, 0);
	g->addWidget(hRuler_, 0, 1);
	g->addWidget(vRuler_, 1, 0);
	g->addWidget(view_, 1, 1);
	g->setRowStretch(1, 1);
	g->setColumnStretch(1, 1);
	v->addWidget(area, 1);

	/* ---- status ---- */
	status_ = new QLabel(this);
	status_->setContentsMargins(8, 3, 8, 3);
	QPalette sp = status_->palette();
	sp.setColor(QPalette::WindowText, sp.color(QPalette::Disabled, QPalette::WindowText));
	status_->setPalette(sp);
	v->addWidget(status_);

	/* ---- wiring ---- */
	connect(zoomOut, &QToolButton::clicked, view_, &CanvasView::zoomOut);
	connect(zoomIn, &QToolButton::clicked, view_, &CanvasView::zoomIn);
	auto *zm = new QMenu(zoomLabel_);
	zm->addAction(T("Canvas.ZoomFit") + "\tShift+1", view_, &CanvasView::zoomFit);
	zm->addAction(T("Canvas.Zoom100") + "\tShift+0", view_, &CanvasView::zoom100);
	zm->addAction(T("Canvas.ZoomSelection") + "\tShift+2", view_, &CanvasView::zoomSelection);
	zoomLabel_->setMenu(zm);

	auto flip = [](bool ViewSettings::*field) {
		return [field](bool on) {
			Store::instance().settings().*field = on;
			Store::instance().notifySettingsChanged();
		};
	};
	connect(rulers_, &QToolButton::clicked, this, flip(&ViewSettings::rulers));
	connect(guides_, &QToolButton::clicked, this, flip(&ViewSettings::guides));
	connect(grid_, &QToolButton::clicked, this, flip(&ViewSettings::grid));
	connect(safe_, &QToolButton::clicked, this, flip(&ViewSettings::safeAreas));
	connect(snap_, &QToolButton::clicked, this, [](bool on) {
		vs().snap.enabled = on;
		Store::instance().notifySettingsChanged();
	});

	/* Snapping targets menu. */
	auto *snapMenu = new QMenu(snap_);
	const std::pair<const char *, bool core::SnapSettings::*> targets[] = {
		{"Snap.Guides", &core::SnapSettings::guides}, {"Snap.Grid", &core::SnapSettings::grid},
		{"Snap.Items", &core::SnapSettings::items},   {"Snap.Spacing", &core::SnapSettings::spacing},
		{"Snap.Canvas", &core::SnapSettings::canvas},
	};
	for (const auto &[key, field] : targets) {
		QAction *a = snapMenu->addAction(T(key));
		a->setCheckable(true);
		auto f = field;
		connect(snapMenu, &QMenu::aboutToShow, a, [a, f] { a->setChecked(vs().snap.*f); });
		connect(a, &QAction::toggled, this, [f](bool on) {
			if (vs().snap.*f == on)
				return;
			vs().snap.*f = on;
			Store::instance().notifySettingsChanged();
		});
	}
	snap_->setMenu(snapMenu);
	snap_->setPopupMode(QToolButton::MenuButtonPopup);

	/* Grid kinds menu. */
	auto *gridMenu = new QMenu(grid_);
	const std::pair<const char *, GridKind> kinds[] = {{"Grid.Square", GridKind::Square},
							   {"Grid.Columns", GridKind::Columns},
							   {"Grid.Rows", GridKind::Rows}};
	for (const auto &[key, kind] : kinds) {
		QAction *a = gridMenu->addAction(T(key));
		a->setCheckable(true);
		const GridKind k = kind;
		connect(gridMenu, &QMenu::aboutToShow, a, [a, k] { a->setChecked(vs().gridKind == k); });
		connect(a, &QAction::triggered, this, [k] {
			vs().gridKind = k;
			vs().grid = true;
			Store::instance().notifySettingsChanged();
		});
	}
	gridMenu->addSeparator();
	gridMenu->addAction(T("Canvas.Settings"), this, [this] { SettingsDialog(this).exec(); });
	grid_->setMenu(gridMenu);
	grid_->setPopupMode(QToolButton::MenuButtonPopup);

	connect(settings, &QToolButton::clicked, this, [this] { SettingsDialog(this).exec(); });

	connect(view_, &CanvasView::viewChanged, this, &CanvasDock::refreshStatus);
	connect(view_, &CanvasView::cursorChanged, this, &CanvasDock::refreshStatus);
	EditContext &ctx = EditContext::instance();
	connect(&ctx, &EditContext::sceneChanged, this, &CanvasDock::refreshScenes);
	connect(&ctx, &EditContext::itemsChanged, this, &CanvasDock::refreshStatus);
	connect(&ctx, &EditContext::liveStateChanged, this, &CanvasDock::refreshLive);
	connect(&SceneDirectory::instance(), &SceneDirectory::changed, this, &CanvasDock::refreshScenes);
	connect(&ctx, &EditContext::editRefused, this, [this](const QString &why) {
		/* Shown in the status line for a moment. */
		refused_ = why;
		refreshStatus();
		QTimer::singleShot(3000, this, [this] {
			refused_.clear();
			refreshStatus();
		});
	});
	connect(&Store::instance(), &Store::settingsChanged, this, &CanvasDock::refreshToggles);

	refreshScenes();
	refreshToggles();
	refreshStatus();
	refreshLive();
}

QToolButton *CanvasDock::toggle(const QString &iconName, const QString &tip)
{
	return iconButton(iconName, tip, this, true);
}

void CanvasDock::refreshScenes()
{
	EditContext &ctx = EditContext::instance();
	pin_->setChecked(ctx.following());
	pin_->setToolTip(ctx.following() ? T("Canvas.FollowOnTooltip") : T("Canvas.FollowTooltip"));

	/* 16:9 › stack › scene, the canvas and stacks dimmed. */
	QColor dim = palette().color(QPalette::Disabled, QPalette::WindowText);
	const QString sep = QString::fromUtf8(" \xE2\x80\xBA ");
	QStringList parts;
	QString full;
	if (ctx.sceneSource()) {
		const std::vector<const core::nav::Node *> path =
			SceneDirectory::instance().locate(ctx.sceneUuid().toStdString());
		QStringList plain;
		for (const core::nav::Node *n : path) {
			if (n->kind == core::nav::Node::Kind::Scene)
				break;
			const QString t = n->kind == core::nav::Node::Kind::Canvas ? QString::fromStdString(n->ratio)
										   : QString::fromStdString(n->name);
			if (t.isEmpty())
				continue;
			parts << QString("<span style=\"color:%1\">%2</span>").arg(dim.name(), t.toHtmlEscaped());
			plain << t;
		}
		parts << QString("<b>%1</b>").arg(ctx.sceneName().toHtmlEscaped());
		plain << ctx.sceneName();
		full = plain.join(sep);
	} else {
		parts << QString("<span style=\"color:%1\">%2</span>")
				 .arg(dim.name(), T("Inspector.NoScene").toHtmlEscaped());
	}
	crumb_->setText(parts.join(QString("<span style=\"color:%1\">%2</span>").arg(dim.name(), sep)));
	crumb_->setToolTip(full);
	refreshStatus();
	refreshLive();
}

void CanvasDock::refreshToggles()
{
	const ViewSettings &s = vs();
	rulers_->setChecked(s.rulers);
	guides_->setChecked(s.guides);
	grid_->setChecked(s.grid);
	snap_->setChecked(s.snap.enabled);
	safe_->setChecked(s.safeAreas);
	hRuler_->setVisible(s.rulers);
	vRuler_->setVisible(s.rulers);
	corner_->setVisible(s.rulers);
}

void CanvasDock::refreshStatus()
{
	EditContext &ctx = EditContext::instance();
	zoomLabel_->setText(fmt(view_->zoom() * 100.0) + "%");
	if (!refused_.isEmpty()) {
		status_->setText(refused_);
		return;
	}
	if (!ctx.sceneSource()) {
		status_->setText(T("Inspector.NoScene"));
		return;
	}
	QStringList parts;
	parts << QString("%1 \xC3\x97 %2").arg(ctx.canvasWidth()).arg(ctx.canvasHeight());
	if (view_->cursorInside()) {
		const core::Vec2 c = view_->cursorWorld();
		parts << QString("X %1  Y %2").arg(fmt(std::floor(c.x))).arg(fmt(std::floor(c.y)));
	}
	const size_t n = ctx.selection().size();
	if (n > 0)
		parts << T("Canvas.SelectedCount").arg(n);
	status_->setText(parts.join(QString::fromUtf8("   \xC2\xB7   ")));
}

void CanvasDock::refreshLive()
{
	const bool onAir = EditContext::instance().liveState() == LiveState::OnAir;
	chip_->setToolTip(vs().blockOnAir ? T("Live.OnAirBlocked") : T("Live.OnAir"));
	chip_->setVisible(onAir);
}

/* =========================================================== SettingsDialog */

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(T("Settings.Title"));
	ViewSettings s = vs();
	auto *v = new QVBoxLayout(this);

	auto spin = [this](double value, double lo, double hi, int decimals = 0) {
		auto *sb = new QDoubleSpinBox(this);
		sb->setRange(lo, hi);
		sb->setDecimals(decimals);
		sb->setValue(value);
		return sb;
	};

	/* Grid */
	auto *gridBox = new QGroupBox(T("Settings.Grid"), this);
	auto *gf = new QFormLayout(gridBox);
	auto *kind = new QComboBox(gridBox);
	kind->addItems({T("Grid.Square"), T("Grid.Columns"), T("Grid.Rows")});
	kind->setCurrentIndex((int)s.gridKind);
	auto *preset = new QComboBox(gridBox);
	preset->addItem(T("Settings.Custom"), 0);
	for (int p : {8, 10, 16, 20, 32, 64})
		preset->addItem(QString("%1 px").arg(p), p);
	auto *size = spin(s.square.size, 1, 1000, 1);
	auto *offX = spin(s.square.offsetX, -1000, 1000, 1);
	auto *offY = spin(s.square.offsetY, -1000, 1000, 1);
	const int pi = preset->findData((int)s.square.size);
	preset->setCurrentIndex(pi > 0 && s.square.size == (int)s.square.size ? pi : 0);
	connect(preset, &QComboBox::activated, this, [preset, size](int i) {
		if (preset->itemData(i).toInt() > 0)
			size->setValue(preset->itemData(i).toInt());
	});
	gf->addRow(T("Settings.GridKind"), kind);
	gf->addRow(T("Settings.Preset"), preset);
	gf->addRow(T("Settings.CellSize"), size);
	gf->addRow(T("Settings.OffsetX"), offX);
	gf->addRow(T("Settings.OffsetY"), offY);

	auto trackRows = [&](const core::TrackGrid &t, const QString &title) {
		auto *box = new QGroupBox(title, this);
		auto *f = new QFormLayout(box);
		auto *count = new QSpinBox(box);
		count->setRange(1, 100);
		count->setValue(t.count);
		auto *gutter = spin(t.gutter, 0, 2000, 1);
		auto *margin = spin(t.margin, 0, 2000, 1);
		auto *trackSize = spin(t.trackSize, 1, 4000, 1);
		auto *align = new QComboBox(box);
		align->addItems({T("Track.Stretch"), T("Track.Start"), T("Track.Center"), T("Track.End")});
		align->setCurrentIndex((int)t.align);
		f->addRow(T("Track.Count"), count);
		f->addRow(T("Track.Gutter"), gutter);
		f->addRow(T("Track.Margin"), margin);
		f->addRow(T("Track.Size"), trackSize);
		f->addRow(T("Track.Align"), align);
		return std::make_tuple(box, count, gutter, margin, trackSize, align);
	};
	QGroupBox *colBox, *rowBox;
	QSpinBox *colCount, *rowCount;
	QDoubleSpinBox *colGutter, *colMargin, *colSize, *rowGutter, *rowMargin, *rowSize;
	QComboBox *colAlign, *rowAlign;
	std::tie(colBox, colCount, colGutter, colMargin, colSize, colAlign) = trackRows(s.columns, T("Grid.Columns"));
	std::tie(rowBox, rowCount, rowGutter, rowMargin, rowSize, rowAlign) = trackRows(s.rows, T("Grid.Rows"));

	auto *grids = new QHBoxLayout();
	grids->addWidget(gridBox);
	grids->addWidget(colBox);
	grids->addWidget(rowBox);
	v->addLayout(grids);

	/* Snapping and view */
	auto *snapBox = new QGroupBox(T("Settings.Snapping"), this);
	auto *sf = new QFormLayout(snapBox);
	auto *threshold = spin(s.snap.thresholdPx, 1, 50);
	sf->addRow(T("Settings.Threshold"), threshold);
	auto *outlines = new QCheckBox(T("Settings.Outlines"), snapBox);
	outlines->setChecked(s.outlines);
	sf->addRow(outlines);
	auto *centers = new QCheckBox(T("Settings.CenterLines"), snapBox);
	centers->setChecked(s.centerLines);
	sf->addRow(centers);
	auto *marginX = spin(s.customMarginX, 0, 5000, 0);
	auto *marginY = spin(s.customMarginY, 0, 5000, 0);
	sf->addRow(T("Settings.MarginX"), marginX);
	sf->addRow(T("Settings.MarginY"), marginY);
	auto guideColor = std::make_shared<QColor>(s.guideColor);
	auto *colorBtn = new QPushButton(snapBox);
	auto paintColor = [colorBtn](const QColor &c) {
		colorBtn->setText(c.name());
		colorBtn->setStyleSheet(QString("QPushButton { background: %1; color: white; }").arg(c.name()));
	};
	paintColor(*guideColor);
	connect(colorBtn, &QPushButton::clicked, this, [this, guideColor, paintColor] {
		const QColor c = QColorDialog::getColor(*guideColor, this, T("Settings.GuideColor"));
		if (c.isValid()) {
			*guideColor = c;
			paintColor(c);
		}
	});
	sf->addRow(T("Settings.GuideColor"), colorBtn);

	auto *liveBox = new QGroupBox(T("Settings.Live"), this);
	auto *lf = new QVBoxLayout(liveBox);
	auto *block = new QCheckBox(T("Settings.BlockOnAir"), liveBox);
	block->setChecked(s.blockOnAir);
	auto *liveHelp = new QLabel(T("Settings.LiveHelp"), liveBox);
	liveHelp->setWordWrap(true);
	lf->addWidget(block);
	lf->addWidget(liveHelp);

	auto *bottom = new QHBoxLayout();
	bottom->addWidget(snapBox);
	bottom->addWidget(liveBox);
	v->addLayout(bottom);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(buttons, &QDialogButtonBox::accepted, this, [=] {
		ViewSettings &out = vs();
		out.gridKind = (GridKind)kind->currentIndex();
		out.square.size = size->value();
		out.square.offsetX = offX->value();
		out.square.offsetY = offY->value();
		auto readTrack = [](core::TrackGrid &t, QSpinBox *c, QDoubleSpinBox *g, QDoubleSpinBox *m,
				    QDoubleSpinBox *sz, QComboBox *a) {
			t.count = c->value();
			t.gutter = g->value();
			t.margin = m->value();
			t.trackSize = sz->value();
			t.align = (core::TrackAlign)a->currentIndex();
		};
		readTrack(out.columns, colCount, colGutter, colMargin, colSize, colAlign);
		readTrack(out.rows, rowCount, rowGutter, rowMargin, rowSize, rowAlign);
		out.snap.thresholdPx = threshold->value();
		out.outlines = outlines->isChecked();
		out.centerLines = centers->isChecked();
		out.customMarginX = marginX->value();
		out.customMarginY = marginY->value();
		out.guideColor = *guideColor;
		out.blockOnAir = block->isChecked();
		Store::instance().notifySettingsChanged();
		accept();
	});
	v->addWidget(buttons);
}

} // namespace layouts::ui
