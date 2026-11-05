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

#include "regie-dock.hpp"

#include "broadcast-gallery.hpp"
#include "theme.hpp"

#include "obs/regie.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace layouts::ui {

namespace {

QString loc(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QString layoutName(const std::string &key)
{
	return QString::fromUtf8(obs_module_text(("Broadcast." + key).c_str()));
}

QString tallyStyle(bool program, bool preview)
{
	if (!program && !preview)
		return QString();
	const QColor bg = QColor::fromRgba(program ? tokens::kTallyProgram : tokens::kTallyPreview);
	const QColor fg = QColor::fromRgba(tokens::kTallyText);
	/* On program and preview at once: red fill, green outline. */
	const QColor border = program && preview ? QColor::fromRgba(tokens::kTallyPreview) : bg;
	return QString("QAbstractButton { background: %1; color: %2; border: 2px solid %3; border-radius: 4px; }")
		.arg(bg.name(), fg.name(), border.name());
}

QLabel *rowTitle(const QString &text)
{
	auto *l = new QLabel(text);
	QFont f = l->font();
	f.setBold(true);
	f.setPointSizeF(f.pointSizeF() * tokens::kSectionTitlePt);
	l->setFont(f);
	return l;
}

} // namespace

RegieDock::RegieDock(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsRegieDock");
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin,
				 tokens::kPanelMargin);
	root->setSpacing(tokens::kSectionSpacing);

	auto *top = new QHBoxLayout;
	top->addWidget(new QLabel(loc("Regie.Plateau")));
	plateau_ = new QComboBox;
	plateau_->setToolTip(loc("Regie.PlateauTooltip"));
	top->addWidget(plateau_, 1);
	root->addLayout(top);

	studioBanner_ = new QWidget;
	auto *banner = new QHBoxLayout(studioBanner_);
	banner->setContentsMargins(0, 0, 0, 0);
	auto *bannerText = new QLabel(loc("Regie.StudioOff"));
	bannerText->setWordWrap(true);
	banner->addWidget(bannerText, 1);
	auto *enable = new QPushButton(loc("Regie.EnableStudio"));
	banner->addWidget(enable);
	root->addWidget(studioBanner_);
	connect(enable, &QPushButton::clicked, this, [] { obs_frontend_set_preview_program_mode(true); });

	auto *scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	rows_ = new QWidget;
	rowsLayout_ = new QVBoxLayout(rows_);
	rowsLayout_->setContentsMargins(0, 0, 0, 0);
	rowsLayout_->setSpacing(tokens::kRowSpacing);
	scroll->setWidget(rows_);
	root->addWidget(scroll, 1);

	summary_ = new QLabel;
	summary_->setWordWrap(true);
	root->addWidget(summary_);

	auto *buttons = new QHBoxLayout;
	cut_ = new QPushButton(loc("Regie.Cut"));
	cut_->setMinimumHeight(tokens::kRegieCutHeight);
	cut_->setToolTip(loc("Regie.CutTooltip"));
	auto_ = new QPushButton(loc("Regie.Auto"));
	auto_->setMinimumHeight(tokens::kRegieCutHeight);
	auto_->setEnabled(false);
	auto_->setToolTip("0.6");
	buttons->addWidget(cut_, 1);
	buttons->addWidget(auto_, 1);
	root->addLayout(buttons);
	connect(cut_, &QPushButton::clicked, this, [] { Regie::instance().take(); });

	connect(plateau_, &QComboBox::currentIndexChanged, this, [this](int i) {
		if (filling_)
			return;
		Regie::instance().setPlateau(i <= 0 ? QString() : plateau_->itemText(i));
	});
	connect(&Regie::instance(), &Regie::changed, this, &RegieDock::rebuild);
	rebuild();
}

void RegieDock::fillPlateaus()
{
	filling_ = true;
	plateau_->clear();
	plateau_->addItem(loc("Regie.NoPlateau"));
	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	const QString current = Regie::instance().plateau();
	const QString twin = current + " B";
	for (size_t i = 0; i < scenes.sources.num; i++) {
		const QString name = QString::fromUtf8(obs_source_get_name(scenes.sources.array[i]));
		if (!current.isEmpty() && name == twin)
			continue; /* the ping-pong twin is not a choice */
		plateau_->addItem(name);
	}
	obs_frontend_source_list_free(&scenes);
	const int idx = current.isEmpty() ? 0 : plateau_->findText(current);
	plateau_->setCurrentIndex(idx < 0 ? 0 : idx);
	filling_ = false;
}

void RegieDock::rebuild()
{
	fillPlateaus();
	Regie &r = Regie::instance();
	const bool studio = r.studioMode();
	studioBanner_->setVisible(!r.plateau().isEmpty() && !studio);
	cut_->setEnabled(studio && r.plateauSource());

	/* Rows are rebuilt on every change: they are few and the state is small. */
	while (QLayoutItem *it = rowsLayout_->takeAt(0)) {
		if (it->widget())
			it->widget()->deleteLater();
		delete it;
	}

	if (!r.plateauSource()) {
		auto *hint = new QLabel(loc("Regie.PickPlateau"));
		hint->setWordWrap(true);
		rowsLayout_->addWidget(hint);
		rowsLayout_->addStretch(1);
		summary_->clear();
		return;
	}
	if (r.layouts().empty()) {
		auto *hint = new QLabel(loc("Regie.NoSources"));
		hint->setWordWrap(true);
		rowsLayout_->addWidget(hint);
		rowsLayout_->addStretch(1);
		summary_->clear();
		return;
	}

	/* LAYOUTS */
	rowsLayout_->addWidget(rowTitle(loc("Regie.Layouts")));
	auto *grid = new QWidget;
	auto *gl = new QGridLayout(grid);
	gl->setContentsMargins(0, 0, 0, 0);
	gl->setSpacing(tokens::kRowSpacing);
	const QColor cam = QColor::fromRgba(tokens::kThumbCamera);
	const QColor scr = QColor::fromRgba(tokens::kThumbScreen);
	const QColor bg = palette().color(QPalette::Base);
	const double aspect = r.canvasAspect();
	constexpr int kColumns = 4;
	int n = 0;
	for (const core::BroadcastTemplate &t : r.layouts()) {
		auto *b = new QToolButton;
		b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		b->setIcon(broadcastThumb(t.key, cam, scr, bg, aspect));
		b->setIconSize(QSize(tokens::kRegieThumbW, tokens::kRegieThumbH));
		b->setText(n < 9 ? QString("%1 %2").arg(n + 1).arg(layoutName(t.key)) : layoutName(t.key));
		b->setToolTip(layoutName(t.key));
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		b->setStyleSheet(tallyStyle(t.key == r.program().key, studio && t.key == r.preview().key));
		const std::string key = t.key;
		connect(b, &QToolButton::clicked, this, [key] { Regie::instance().setLayout(key); });
		gl->addWidget(b, n / kColumns, n % kColumns);
		n++;
	}
	rowsLayout_->addWidget(grid);

	/* One BUS per slot of the preview layout */
	const std::vector<core::Slot> tiles = r.slotsOf(r.preview().key);
	for (size_t i = 0; i < tiles.size(); i++) {
		const bool screen = tiles[i].kind == core::SlotKind::Screen;
		rowsLayout_->addWidget(
			rowTitle(QString(loc(screen ? "Regie.BusScreen" : "Regie.BusCamera")).arg(i + 1)));
		auto *bus = new QWidget;
		auto *bl = new QGridLayout(bus);
		bl->setContentsMargins(0, 0, 0, 0);
		bl->setSpacing(tokens::kRowSpacing);
		int m = 0;
		for (const RegieSource &s : r.sources()) {
			if (s.kind != tiles[i].kind)
				continue;
			auto *b = new QPushButton(QString::fromStdString(s.name));
			b->setMinimumHeight(tokens::kRegieButton);
			b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			const bool inSlot = i < r.preview().tiles.size() && r.preview().tiles[i] == s.name;
			const bool inProgramSlot = i < r.program().tiles.size() && r.program().tiles[i] == s.name;
			/* The slot's buttons light like a mixer bus: this slot on program (red), this
			 * slot on preview (green). */
			b->setStyleSheet(tallyStyle(inProgramSlot && r.onProgram(s.name), studio && inSlot));
			b->setToolTip(QString(loc("Regie.SourceTooltip")).arg(QString::fromStdString(s.name)));
			const int slot = (int)i;
			const std::string name = s.name;
			connect(b, &QPushButton::clicked, this, [slot, name] { Regie::instance().assign(slot, name); });
			bl->addWidget(b, m / kColumns, m % kColumns);
			m++;
		}
		rowsLayout_->addWidget(bus);
	}
	rowsLayout_->addStretch(1);

	/* PREVIEW summary: "Duo — 1 Alice · 2 Bob" */
	QStringList parts;
	for (size_t i = 0; i < r.preview().tiles.size(); i++)
		parts << QString("%1 %2").arg(i + 1).arg(QString::fromStdString(r.preview().tiles[i]));
	summary_->setText(QString("<b>%1</b> %2 — %3")
				  .arg(loc(studio ? "Regie.Preview" : "Regie.Program"),
				       layoutName(r.preview().key).toHtmlEscaped(), parts.join(" · ").toHtmlEscaped()));
}

} // namespace layouts::ui
