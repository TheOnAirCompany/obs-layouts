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

#include "inspector-dock.hpp"

#include "theme.hpp"
#include "widgets.hpp"

#include "obs/constraints.hpp"
#include "obs/edit-context.hpp"
#include "obs/store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QSpinBox>
#include <QAbstractItemView>
#include <QEvent>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace layouts::ui {

namespace {

constexpr uint32_t kAlignValues[9] = {
	core::kAlignLeft | core::kAlignTop,
	core::kAlignTop,
	core::kAlignRight | core::kAlignTop,
	core::kAlignLeft,
	core::kAlignCenter,
	core::kAlignRight,
	core::kAlignLeft | core::kAlignBottom,
	core::kAlignBottom,
	core::kAlignRight | core::kAlignBottom,
};

const char *kAlignNames[9] = {"Pos.TopLeft", "Pos.Top",        "Pos.TopRight", "Pos.Left",       "Pos.Center",
			      "Pos.Right",   "Pos.BottomLeft", "Pos.Bottom",   "Pos.BottomRight"};

QHBoxLayout *row(QWidget *parent = nullptr)
{
	auto *l = new QHBoxLayout(parent);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(tokens::kColumnSpacing);
	return l;
}

bool allEqual(const std::vector<double> &v, double eps = 0.005)
{
	for (double x : v) {
		if (std::fabs(x - v.front()) > eps)
			return false;
	}
	return true;
}

} // namespace

InspectorDock::InspectorDock(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsInspector");
	build();

	EditContext &ctx = EditContext::instance();
	connect(&ctx, &EditContext::itemsChanged, this, &InspectorDock::refresh);
	connect(&ctx, &EditContext::sceneChanged, this, &InspectorDock::refresh);
	connect(&ctx, &EditContext::liveStateChanged, this, &InspectorDock::updateLive);
	connect(&ctx, &EditContext::editRefused, this, [this](const QString &why) {
		live_->setText(why);
		live_->setVisible(true);
	});
	connect(&Store::instance(), &Store::settingsChanged, this, &InspectorDock::refresh);
	refresh();
}

QWidget *InspectorDock::section(const QString &title, QWidget *body)
{
	auto *w = new QWidget(content_);
	auto *l = new QVBoxLayout(w);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(tokens::kRowSpacing);
	l->addWidget(sectionTitle(title, w));
	l->addWidget(body);
	return w;
}

void InspectorDock::bindField(NumericField *f, commands::Field field)
{
	fields_[field] = f;
	connect(f, &NumericField::committed, this,
		[field](const core::Expression &e) { commands::setField(field, e); });
	connect(f, &NumericField::scrubbed, this,
		[field](double delta, bool finished) { commands::scrubField(field, delta, finished); });
}

void InspectorDock::build()
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	outer->addWidget(scroll);

	content_ = new QWidget(scroll);
	scroll->setWidget(content_);
	auto *v = new QVBoxLayout(content_);
	v->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin);
	v->setSpacing(tokens::kSectionSpacing);

	/* ---- header ---- */
	{
		auto *h = new QWidget(content_);
		auto *hl = row(h);
		auto *names = new QVBoxLayout();
		names->setSpacing(0);
		title_ = new QLabel(h);
		QFont f = title_->font();
		f.setBold(true);
		title_->setFont(f);
		title_->setTextInteractionFlags(Qt::TextSelectableByMouse);
		title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		subtitle_ = new QLabel(h);
		QPalette p = subtitle_->palette();
		p.setColor(QPalette::WindowText, p.color(QPalette::Disabled, QPalette::WindowText));
		subtitle_->setPalette(p);
		subtitle_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		names->addWidget(title_);
		names->addWidget(subtitle_);
		hl->addLayout(names, 1);

		lock_ = iconButton("lock-open", T("Inspector.Lock"), h, true);
		connect(lock_, &QToolButton::clicked, this, [this](bool on) {
			commands::setLocked(on);
			lock_->setIcon(icon(on ? "lock" : "lock-open", this));
		});
		copy_ = iconButton("copy", T("Inspector.CopyProps"), h);
		connect(copy_, &QToolButton::clicked, this, [this] {
			commands::copyProperties();
			refresh();
		});
		paste_ = iconButton("clipboard-paste", T("Inspector.PasteProps"), h);
		paste_->setPopupMode(QToolButton::InstantPopup);
		auto *menu = new QMenu(paste_);
		const std::pair<const char *, unsigned> pastes[] = {
			{"Paste.All", commands::PasteAll},   {"Paste.Position", commands::PastePosition},
			{"Paste.Size", commands::PasteSize}, {"Paste.Rotation", commands::PasteRotation},
			{"Paste.Crop", commands::PasteCrop}, {"Paste.Bounds", commands::PasteBounds},
		};
		for (const auto &[key, mask] : pastes) {
			const unsigned m = mask;
			menu->addAction(T(key), this, [m] { commands::pasteProperties(m); });
			if (m == commands::PasteAll)
				menu->addSeparator();
		}
		paste_->setMenu(menu);
		hl->addWidget(lock_);
		hl->addWidget(copy_);
		hl->addWidget(paste_);
		v->addWidget(h);
	}

	live_ = new QLabel(content_);
	live_->setWordWrap(true);
	live_->setMargin(6);
	live_->setVisible(false);
	v->addWidget(live_);

	empty_ = new QLabel(content_);
	empty_->setWordWrap(true);
	empty_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	v->addWidget(empty_);

	/* ---- align ---- */
	{
		auto *body = new QWidget(content_);
		auto *bl = new QVBoxLayout(body);
		bl->setContentsMargins(0, 0, 0, 0);
		bl->setSpacing(tokens::kRowSpacing);

		alignTargetRow_ = new QWidget(body);
		auto *tl = new QVBoxLayout(alignTargetRow_);
		tl->setContentsMargins(0, 0, 0, 0);
		tl->setSpacing(tokens::kRowSpacing);
		auto *seg = new QWidget(alignTargetRow_);
		alignTarget_ = segmented({T("Align.ToSelection"), T("Align.ToCanvas"), T("Align.ToKey")}, seg);
		alignTarget_->button((int)commands::alignTarget())->setChecked(true);
		connect(alignTarget_, &QButtonGroup::idClicked, this, [this](int id) {
			commands::setAlignTarget((core::AlignTarget)id);
			refresh();
		});
		tl->addWidget(seg);
		keyObject_ = new QComboBox(alignTargetRow_);
		keyObject_->setToolTip(T("Align.KeyTooltip"));
		keyObject_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		keyObject_->setMinimumContentsLength(8);
		connect(keyObject_, &QComboBox::activated, this, [this](int i) {
			const auto sel = EditContext::instance().selection();
			if (i >= 0 && i < (int)sel.size())
				commands::setKeyItem(sel[(size_t)i]->groupId, sel[(size_t)i]->id);
			refresh();
		});
		tl->addWidget(keyObject_);
		bl->addWidget(alignTargetRow_);

		auto *buttons = new QWidget(body);
		auto *b = row(buttons);
		b->setSpacing(2);
		const std::tuple<const char *, const char *, core::AlignEdge> aligns[] = {
			{"align-start-vertical", "Align.Left", core::AlignEdge::Left},
			{"align-center-vertical", "Align.HCenter", core::AlignEdge::HCenter},
			{"align-end-vertical", "Align.Right", core::AlignEdge::Right},
			{"align-start-horizontal", "Align.TopEdge", core::AlignEdge::Top},
			{"align-center-horizontal", "Align.VCenter", core::AlignEdge::VCenter},
			{"align-end-horizontal", "Align.BottomEdge", core::AlignEdge::Bottom},
		};
		for (const auto &[ic, key, edge] : aligns) {
			auto *btn = iconButton(ic, T(key), buttons);
			const core::AlignEdge e = edge;
			connect(btn, &QToolButton::clicked, this, [e] { commands::align(e); });
			alignButtons_.push_back(btn);
			b->addWidget(btn);
			if (edge == core::AlignEdge::Right)
				b->addSpacing(6);
		}
		b->addStretch();
		bl->addWidget(buttons);
		v->addWidget(section(T("Section.Align"), body));
	}

	/* ---- position ---- */
	{
		auto *body = new QWidget(content_);
		auto *g = new QGridLayout(body);
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(tokens::kColumnSpacing);
		g->setVerticalSpacing(tokens::kRowSpacing);
		ref_ = new RefPointPicker(body);
		connect(ref_, &RefPointPicker::changed, this, [](int i) {
			Store::instance().settings().refPoint = i;
			Store::instance().notifySettingsChanged();
		});
		auto *x = new NumericField("X", T("Field.X"), body);
		auto *y = new NumericField("Y", T("Field.Y"), body);
		bindField(x, commands::Field::X);
		bindField(y, commands::Field::Y);
		g->addWidget(ref_, 0, 0, 2, 1, Qt::AlignTop);
		g->addWidget(x, 0, 1);
		g->addWidget(y, 0, 2);

		auto *rot = new NumericField(QString::fromUtf8("\xE2\x86\xBB"), T("Field.Rotation"), body);
		rot->setSuffix(QString::fromUtf8("\xC2\xB0"));
		bindField(rot, commands::Field::Rotation);
		auto *rotButtons = new QWidget(body);
		auto *rb = row(rotButtons);
		rb->setSpacing(2);
		auto *ccw = iconButton("rotate-ccw", T("Rotate.CCW"), rotButtons);
		auto *cw = iconButton("rotate-cw", T("Rotate.CW"), rotButtons);
		auto *fh = iconButton("flip-horizontal-2", T("Flip.Horizontal"), rotButtons);
		auto *fv = iconButton("flip-vertical-2", T("Flip.Vertical"), rotButtons);
		connect(ccw, &QToolButton::clicked, this, [] { commands::rotateBy(-90); });
		connect(cw, &QToolButton::clicked, this, [] { commands::rotateBy(90); });
		connect(fh, &QToolButton::clicked, this, [] { commands::flip(core::Axis::X); });
		connect(fv, &QToolButton::clicked, this, [] { commands::flip(core::Axis::Y); });
		for (auto *bt : {ccw, cw, fh, fv}) {
			rb->addWidget(bt);
			sizeButtons_.push_back(bt);
		}
		rb->addStretch();
		g->addWidget(rot, 1, 1);
		g->addWidget(rotButtons, 1, 2);
		g->setColumnStretch(1, 1);
		g->setColumnStretch(2, 1);
		v->addWidget(section(T("Section.Position"), body));
	}

	/* ---- size ---- */
	{
		auto *body = new QWidget(content_);
		auto *g = new QGridLayout(body);
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(tokens::kColumnSpacing);
		g->setVerticalSpacing(tokens::kRowSpacing);
		auto *w = new NumericField("W", T("Field.W"), body);
		auto *h = new NumericField("H", T("Field.H"), body);
		bindField(w, commands::Field::W);
		bindField(h, commands::Field::H);
		ratio_ = iconButton("link", T("Size.RatioLock"), body, true);
		connect(ratio_, &QToolButton::toggled, this, [this](bool on) {
			if (updating_)
				return;
			Store::instance().settings().ratioLock = on;
			Store::instance().notifySettingsChanged();
		});
		g->addWidget(w, 0, 0);
		g->addWidget(h, 0, 1);
		g->addWidget(ratio_, 0, 2);

		auto *fitRow = new QWidget(body);
		auto *fr = row(fitRow);
		fr->setSpacing(2);
		const std::tuple<const char *, const char *, int> fits[] = {
			{"shrink", "Size.Fit", 0},
			{"expand", "Size.Fill", 1},
			{"move-diagonal-2", "Size.Stretch", 2},
			{"scan", "Size.Original", 3},
			{"undo-2", "Size.Reset", 4},
		};
		for (const auto &[ic, key, id] : fits) {
			auto *btn = iconButton(ic, T(key), fitRow);
			const int which = id;
			connect(btn, &QToolButton::clicked, this, [which] {
				switch (which) {
				case 0:
					commands::fit(core::FitMode::Fit);
					break;
				case 1:
					commands::fit(core::FitMode::Fill);
					break;
				case 2:
					commands::fit(core::FitMode::Stretch);
					break;
				case 3:
					commands::originalSize();
					break;
				default:
					commands::resetTransform();
					break;
				}
			});
			fr->addWidget(btn);
			sizeButtons_.push_back(btn);
		}
		fr->addStretch();
		g->addWidget(fitRow, 1, 0, 1, 3);
		g->setColumnStretch(0, 1);
		g->setColumnStretch(1, 1);
		v->addWidget(section(T("Section.Size"), body));
	}

	/* ---- distribute & spacing (multi) ---- */
	{
		auto *body = new QWidget(content_);
		auto *bl = new QVBoxLayout(body);
		bl->setContentsMargins(0, 0, 0, 0);
		bl->setSpacing(tokens::kRowSpacing);

		auto *dist = new QWidget(body);
		auto *dl = row(dist);
		dl->setSpacing(2);
		const std::tuple<const char *, const char *, core::Axis, bool> dists[] = {
			{"align-horizontal-distribute-center", "Distribute.CentersH", core::Axis::X, false},
			{"align-horizontal-space-between", "Distribute.SpacingH", core::Axis::X, true},
			{"align-vertical-distribute-center", "Distribute.CentersV", core::Axis::Y, false},
			{"align-vertical-space-between", "Distribute.SpacingV", core::Axis::Y, true},
		};
		for (const auto &[ic, key, axis, spacing] : dists) {
			auto *btn = iconButton(ic, T(key), dist);
			const core::Axis a = axis;
			const bool s = spacing;
			connect(btn, &QToolButton::clicked, this, [a, s] { commands::distribute(a, s); });
			distributeButtons_.push_back(btn);
			dl->addWidget(btn);
		}
		dl->addStretch();
		bl->addWidget(dist);

		auto *gaps = new QWidget(body);
		auto *gl = row(gaps);
		gapX_ = new NumericField(QString::fromUtf8("\xE2\x86\x94"), T("Spacing.H"), gaps);
		gapY_ = new NumericField(QString::fromUtf8("\xE2\x86\x95"), T("Spacing.V"), gaps);
		gapX_->setPercentBase(EditContext::instance().canvasWidth());
		gapY_->setPercentBase(EditContext::instance().canvasHeight());
		auto gapCommit = [](core::Axis axis) {
			return [axis](const core::Expression &e) {
				std::vector<core::ArrangeItem> items;
				for (const ItemEntry *it : EditContext::instance().selection())
					items.push_back({it->frame.quad.bounds(), it->locked});
				const double current = core::uniformGap(items, axis).value_or(0.0);
				double v = 0;
				if (e.apply(current, v))
					commands::spaceExactly(axis, v);
			};
		};
		connect(gapX_, &NumericField::committed, this, gapCommit(core::Axis::X));
		connect(gapY_, &NumericField::committed, this, gapCommit(core::Axis::Y));
		gl->addWidget(gapX_);
		gl->addWidget(gapY_);
		bl->addWidget(gaps);

		auto *same = new QWidget(body);
		auto *sl = row(same);
		sl->setSpacing(4);
		const std::tuple<const char *, bool, bool> sames[] = {{"Same.Width", true, false},
								      {"Same.Height", false, true},
								      {"Same.Size", true, true}};
		for (const auto &[key, w, h] : sames) {
			auto *btn = new QToolButton(same);
			btn->setText(T(key));
			btn->setToolTip(T("Same.Tooltip"));
			btn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
			btn->setMinimumHeight(tokens::kFieldHeight);
			btn->setMinimumWidth(40);
			const bool sw = w, sh = h;
			connect(btn, &QToolButton::clicked, this, [sw, sh] { commands::sameSize(sw, sh); });
			sl->addWidget(btn);
			distributeButtons_.push_back(btn);
		}
		bl->addWidget(same);
		arrangeSection_ = section(T("Section.Distribute"), body);
		v->addWidget(arrangeSection_);
	}

	layoutSection_ = buildLayoutSection();
	v->addWidget(layoutSection_);
	presetSection_ = buildPresetSection();
	v->addWidget(presetSection_);
	constraintSection_ = buildConstraintSection();
	v->addWidget(constraintSection_);

	/* ---- crop ---- */
	{
		auto *body = new QWidget(content_);
		auto *g = new QGridLayout(body);
		g->setContentsMargins(0, 0, 0, 0);
		g->setHorizontalSpacing(tokens::kColumnSpacing);
		g->setVerticalSpacing(tokens::kRowSpacing);
		const std::tuple<const char *, const char *, commands::Field, int, int> crops[] = {
			{"L", "Crop.Left", commands::Field::CropLeft, 0, 0},
			{"R", "Crop.Right", commands::Field::CropRight, 0, 1},
			{"T", "Crop.Top", commands::Field::CropTop, 1, 0},
			{"B", "Crop.Bottom", commands::Field::CropBottom, 1, 1},
		};
		for (const auto &[label, key, field, r, c] : crops) {
			auto *f = new NumericField(label, T(key), body);
			f->setDecimals(0);
			bindField(f, field);
			g->addWidget(f, r, c);
		}
		cropSection_ = section(T("Section.Crop"), body);
		v->addWidget(cropSection_);
	}

	/* ---- bounds ---- */
	{
		auto *body = new QWidget(content_);
		auto *bl = new QVBoxLayout(body);
		bl->setContentsMargins(0, 0, 0, 0);
		bl->setSpacing(tokens::kRowSpacing);
		boundsType_ = new QComboBox(body);
		for (const char *key : {"Bounds.None", "Bounds.Stretch", "Bounds.Inner", "Bounds.Outer", "Bounds.Width",
					"Bounds.Height", "Bounds.MaxOnly"})
			boundsType_->addItem(T(key));
		boundsType_->setToolTip(T("Bounds.Tooltip"));
		boundsType_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		boundsType_->setMinimumContentsLength(10);
		connect(boundsType_, &QComboBox::activated, this,
			[](int i) { commands::setBoundsType((core::BoundsType)i); });
		boundsAlign_ = new QComboBox(body);
		for (const char *key : kAlignNames)
			boundsAlign_->addItem(T(key));
		boundsAlign_->setToolTip(T("Bounds.AlignTooltip"));
		boundsAlign_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		boundsAlign_->setMinimumContentsLength(6);
		connect(boundsAlign_, &QComboBox::activated, this,
			[](int i) { commands::setBoundsAlignment(kAlignValues[std::clamp(i, 0, 8)]); });
		cropToBounds_ = new QCheckBox(T("Bounds.Crop"), body);
		connect(cropToBounds_, &QCheckBox::clicked, this, [](bool on) { commands::setCropToBounds(on); });
		bl->addWidget(boundsType_);
		auto *r2 = new QWidget(body);
		auto *r2l = row(r2);
		r2l->addWidget(boundsAlign_, 1);
		bl->addWidget(r2);
		bl->addWidget(cropToBounds_);
		boundsSection_ = section(T("Section.Bounds"), body);
		v->addWidget(boundsSection_);
	}

	v->addStretch();
}

void InspectorDock::updateLive()
{
	EditContext &ctx = EditContext::instance();
	const LiveState s = ctx.liveState();
	if (s == LiveState::Off) {
		live_->setVisible(false);
		return;
	}
	const bool blocked = s == LiveState::OnAir && Store::instance().settings().blockOnAir;
	const QColor bg = QColor::fromRgba(s == LiveState::OnAir ? tokens::kLiveBanner : tokens::kProgramBanner);
	live_->setStyleSheet(QString("QLabel { background: %1; color: white; border-radius: 4px; font-weight: bold; }")
				     .arg(bg.name()));
	live_->setText(s == LiveState::OnAir ? (blocked ? T("Live.OnAirBlocked") : T("Live.OnAir"))
					     : T("Live.Program"));
	live_->setVisible(true);
}

void InspectorDock::refresh()
{
	updating_ = true;
	EditContext &ctx = EditContext::instance();
	const std::vector<const ItemEntry *> sel = ctx.selection();
	const size_t n = sel.size();
	const bool single = n == 1;
	const bool multi = n > 1;

	updateLive();
	ref_->setIndex(Store::instance().settings().refPoint);
	ratio_->setChecked(Store::instance().settings().ratioLock);
	ratio_->setIcon(icon(Store::instance().settings().ratioLock ? "link" : "unlink", this));

	/* Empty state */
	const bool hasScene = ctx.sceneSource() != nullptr;
	if (n == 0) {
		title_->setText(hasScene ? ctx.sceneName() : T("Inspector.NoScene"));
		subtitle_->setText(hasScene ? T("Inspector.Canvas").arg(ctx.canvasWidth()).arg(ctx.canvasHeight())
					    : QString());
		empty_->setText(T("Inspector.Empty"));
	} else if (single) {
		const ItemEntry *e = sel.front();
		title_->setText(e->name);
		QString where = ctx.sceneName();
		if (e->groupId) {
			if (const ItemEntry *g = ctx.find(e->groupId, 0))
				where += QString::fromUtf8(" \xE2\x80\xBA ") + g->name;
		}
		subtitle_->setText(e->typeName + QString::fromUtf8(" \xC2\xB7 ") + where);
	} else {
		int locked = 0;
		for (const ItemEntry *e : sel)
			locked += e->locked ? 1 : 0;
		title_->setText(T("Inspector.Selected").arg(n));
		subtitle_->setText(locked ? T("Inspector.LockedCount").arg(locked) : ctx.sceneName());
	}
	empty_->setVisible(n == 0);
	lock_->setVisible(n > 0);
	copy_->setVisible(n > 0);
	paste_->setVisible(n > 0);
	paste_->setEnabled(commands::canPaste());

	for (QWidget *w : content_->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		if (w != title_->parentWidget() && w != live_ && w != empty_)
			w->setVisible(n > 0);
	}
	arrangeSection_->setVisible(multi);
	layoutSection_->setVisible(multi);
	presetSection_->setVisible(n > 0);
	constraintSection_->setVisible(n > 0);
	alignTargetRow_->setVisible(multi);
	syncLayoutControls();
	cropSection_->setVisible(n > 0);
	boundsSection_->setVisible(n > 0);

	if (n == 0) {
		updating_ = false;
		clearPreviewIfIdle();
		return;
	}

	bool allLocked = true;
	int movable = 0;
	for (const ItemEntry *e : sel) {
		allLocked = allLocked && e->locked;
		movable += e->locked ? 0 : 1;
	}
	lock_->setChecked(allLocked);
	lock_->setIcon(icon(allLocked ? "lock" : "lock-open", this));

	for (auto *b : alignButtons_)
		b->setEnabled(movable >= 1);
	for (size_t i = 0; i < distributeButtons_.size(); i++) {
		/* 4 distribute buttons need 3 movable items; "same size" needs 2 items. */
		distributeButtons_[i]->setEnabled(i < 4 ? movable >= 3 : n >= 2);
	}
	gapX_->setEnabled(movable >= 2);
	gapY_->setEnabled(movable >= 2);

	/* Field values (mixed when they differ). */
	for (auto &[field, widget] : fields_) {
		std::vector<double> values;
		for (const ItemEntry *e : sel)
			values.push_back(commands::fieldValue(*e, field));
		widget->setPercentBase(commands::percentBase(field, sel.front()));
		if (allEqual(values))
			widget->setValue(values.front());
		else
			widget->setMixed();
	}

	if (multi) {
		std::vector<core::ArrangeItem> items;
		for (const ItemEntry *e : sel)
			items.push_back({e->frame.quad.bounds(), e->locked});
		auto gx = core::uniformGap(items, core::Axis::X);
		auto gy = core::uniformGap(items, core::Axis::Y);
		/* A negative gap means the elements overlap on that axis: not a spacing. */
		(gx && *gx >= -0.5) ? gapX_->setValue(*gx) : gapX_->setMixed();
		(gy && *gy >= -0.5) ? gapY_->setValue(*gy) : gapY_->setMixed();

		alignTarget_->button((int)commands::alignTarget())->setChecked(true);
		keyObject_->setVisible(commands::alignTarget() == core::AlignTarget::KeyObject);
		keyObject_->clear();
		for (const ItemEntry *e : sel)
			keyObject_->addItem(e->name);
		keyObject_->setCurrentIndex(commands::keyIndex());
	}

	/* Constraints */
	{
		std::vector<double> hs, vs;
		for (const ItemEntry *e : sel) {
			core::HConstraint h;
			core::VConstraint v;
			getConstraints(e->item, h, v);
			hs.push_back((double)h);
			vs.push_back((double)v);
		}
		constraintH_->setCurrentIndex(allEqual(hs) ? (int)hs.front() : -1);
		constraintV_->setCurrentIndex(allEqual(vs) ? (int)vs.front() : -1);
	}

	/* Bounds */
	std::vector<double> types, aligns, crops;
	for (const ItemEntry *e : sel) {
		types.push_back((double)e->model.t.boundsType);
		aligns.push_back((double)e->model.t.boundsAlign);
		crops.push_back(e->model.t.cropToBounds ? 1 : 0);
	}
	boundsType_->setCurrentIndex(allEqual(types) ? (int)types.front() : -1);
	int alignIndex = -1;
	if (allEqual(aligns)) {
		for (int i = 0; i < 9; i++) {
			if (kAlignValues[i] == (uint32_t)aligns.front())
				alignIndex = i;
		}
	}
	boundsAlign_->setCurrentIndex(alignIndex);
	const bool anyBounds = std::any_of(sel.begin(), sel.end(), [](const ItemEntry *e) {
		return e->model.t.boundsType != core::BoundsType::None;
	});
	boundsAlign_->setEnabled(anyBounds);
	cropToBounds_->setEnabled(anyBounds);
	cropToBounds_->setTristate(!allEqual(crops));
	if (allEqual(crops))
		cropToBounds_->setChecked(crops.front() > 0.5);
	else
		cropToBounds_->setCheckState(Qt::PartiallyChecked);

	clearPreviewIfIdle();
	updating_ = false;
}

/* ------------------------------------------------------------ constraints */

QWidget *InspectorDock::buildConstraintSection()
{
	auto *body = new QWidget(content_);
	auto *bl = new QVBoxLayout(body);
	bl->setContentsMargins(0, 0, 0, 0);
	bl->setSpacing(tokens::kRowSpacing);

	constraintH_ = new QComboBox(body);
	for (const char *k : {"Constraint.Default", "Constraint.Left", "Constraint.Right", "Constraint.Center",
			      "Constraint.LeftRight", "Constraint.Scale"})
		constraintH_->addItem(T(k));
	constraintV_ = new QComboBox(body);
	for (const char *k : {"Constraint.Default", "Constraint.Top", "Constraint.Bottom", "Constraint.Center",
			      "Constraint.TopBottom", "Constraint.Scale"})
		constraintV_->addItem(T(k));
	for (QComboBox *c : {constraintH_, constraintV_}) {
		c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		c->setMinimumContentsLength(6);
	}
	constraintH_->setToolTip(T("Constraint.HTooltip"));
	constraintV_->setToolTip(T("Constraint.VTooltip"));

	auto *w = new QWidget(body);
	auto *l = row(w);
	l->addWidget(constraintH_, 1);
	l->addWidget(constraintV_, 1);
	bl->addWidget(w);
	auto *hint = new QLabel(T("Constraint.Hint"), body);
	hint->setWordWrap(true);
	QPalette p = hint->palette();
	p.setColor(QPalette::WindowText, p.color(QPalette::Disabled, QPalette::WindowText));
	hint->setPalette(p);
	bl->addWidget(hint);

	auto apply = [this](bool horizontal, int index) {
		if (updating_ || index < 0)
			return;
		for (const ItemEntry *e : EditContext::instance().selection()) {
			core::HConstraint h;
			core::VConstraint v;
			getConstraints(e->item, h, v);
			if (horizontal)
				h = (core::HConstraint)index;
			else
				v = (core::VConstraint)index;
			setConstraints(e->item, h, v);
		}
		obs_frontend_save();
		refresh();
	};
	connect(constraintH_, &QComboBox::activated, this, [apply](int i) { apply(true, i); });
	connect(constraintV_, &QComboBox::activated, this, [apply](int i) { apply(false, i); });
	return section(T("Section.Constraints"), body);
}

/* ------------------------------------------------------------ auto layout */

QWidget *InspectorDock::buildLayoutSection()
{
	auto *body = new QWidget(content_);
	auto *bl = new QVBoxLayout(body);
	bl->setContentsMargins(0, 0, 0, 0);
	bl->setSpacing(tokens::kRowSpacing);

	auto *seg = new QWidget(body);
	flowDir_ = segmented({T("Flow.Horizontal"), T("Flow.Vertical"), T("Flow.Grid")}, seg);
	bl->addWidget(seg);

	auto combo = [body](std::initializer_list<const char *> keys, const char *tip) {
		auto *c = new QComboBox(body);
		for (const char *k : keys)
			c->addItem(T(k));
		c->setToolTip(T(tip));
		c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
		c->setMinimumContentsLength(6);
		return c;
	};
	flowContainer_ = combo({"Flow.InSelection", "Flow.InCanvas"}, "Flow.ContainerTooltip");
	flowSizing_ = combo({"Flow.Keep", "Flow.Fill"}, "Flow.SizingTooltip");
	flowMain_ = combo({"Flow.Start", "Flow.Center", "Flow.End"}, "Flow.MainTooltip");
	flowCross_ = combo({"Flow.Start", "Flow.Center", "Flow.End"}, "Flow.CrossTooltip");

	auto pair = [body](QWidget *a, QWidget *b) {
		auto *w = new QWidget(body);
		auto *l = row(w);
		l->addWidget(a, 1);
		l->addWidget(b, 1);
		return w;
	};
	bl->addWidget(pair(flowContainer_, flowSizing_));
	bl->addWidget(pair(flowMain_, flowCross_));

	flowGap_ = new NumericField(QString::fromUtf8("\xE2\x86\x94"), T("Flow.Gap"), body);
	flowPadding_ = new NumericField(QString::fromUtf8("\xE2\x96\xA2"), T("Flow.Padding"), body);
	bl->addWidget(pair(flowGap_, flowPadding_));

	columnsRow_ = new QWidget(body);
	auto *cr = row(columnsRow_);
	auto *cl = new QLabel(T("Flow.Columns"), columnsRow_);
	flowColumns_ = new QSpinBox(columnsRow_);
	flowColumns_->setRange(1, 50);
	cr->addWidget(cl);
	cr->addWidget(flowColumns_, 1);
	bl->addWidget(columnsRow_);

	auto *apply = new QToolButton(body);
	apply->setText(T("Flow.Apply"));
	apply->setToolTip(T("Flow.ApplyTooltip"));
	apply->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	apply->setMinimumHeight(tokens::kFieldHeight);
	bl->addWidget(apply);

	auto changed = [this] {
		if (updating_)
			return;
		ViewSettings &s = Store::instance().settings();
		s.flow.direction = (core::FlowDirection)std::max(0, flowDir_->checkedId());
		s.flowInCanvas = flowContainer_->currentIndex() == 1;
		s.flow.sizing = (core::FlowSizing)std::max(0, flowSizing_->currentIndex());
		s.flow.mainAlign = (core::FlowAlign)std::max(0, flowMain_->currentIndex());
		s.flow.crossAlign = (core::FlowAlign)std::max(0, flowCross_->currentIndex());
		s.flow.columns = flowColumns_->value();
		Store::instance().saveSettings();
		columnsRow_->setVisible(s.flow.direction == core::FlowDirection::Grid);
		previewLayout();
	};
	connect(flowDir_, &QButtonGroup::idClicked, this, changed);
	for (QComboBox *c : {flowContainer_, flowSizing_, flowMain_, flowCross_})
		connect(c, &QComboBox::activated, this, changed);
	connect(flowColumns_, &QSpinBox::valueChanged, this, changed);
	auto number = [this](double core::FlowSpec::*field, double lo) {
		return [this, field, lo](const core::Expression &e) {
			ViewSettings &s = Store::instance().settings();
			double v = 0;
			if (!e.apply(s.flow.*field, v))
				return;
			s.flow.*field = std::max(lo, v);
			Store::instance().saveSettings();
			syncLayoutControls();
			previewLayout();
		};
	};
	connect(flowGap_, &NumericField::committed, this, number(&core::FlowSpec::gap, -10000.0));
	connect(flowPadding_, &NumericField::committed, this, number(&core::FlowSpec::padding, 0.0));
	connect(apply, &QToolButton::clicked, this, [] {
		const ViewSettings &s = Store::instance().settings();
		commands::autoLayout(s.flow, s.flowInCanvas);
	});

	QWidget *sec = section(T("Section.Layout"), body);
	sec->setObjectName("LayoutsAutoLayoutSection");
	sec->installEventFilter(this);
	return sec;
}

QWidget *InspectorDock::buildPresetSection()
{
	auto *body = new QWidget(content_);
	auto *bl = new QVBoxLayout(body);
	bl->setContentsMargins(0, 0, 0, 0);
	bl->setSpacing(tokens::kRowSpacing);

	preset_ = new QComboBox(body);
	preset_->setToolTip(T("Preset.Tooltip"));
	preset_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	preset_->setMinimumContentsLength(10);
	bl->addWidget(preset_);

	auto *nums = new QWidget(body);
	auto *nl = row(nums);
	presetMargin_ = new NumericField("M", T("Preset.Margin"), nums);
	presetGap_ = new NumericField(QString::fromUtf8("\xE2\x86\x94"), T("Preset.Gap"), nums);
	presetCorner_ = new NumericField("%", T("Preset.Corner"), nums);
	nl->addWidget(presetMargin_);
	nl->addWidget(presetGap_);
	nl->addWidget(presetCorner_);
	bl->addWidget(nums);

	presetApply_ = new QToolButton(body);
	presetApply_->setText(T("Preset.Apply"));
	presetApply_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	presetApply_->setMinimumHeight(tokens::kFieldHeight);
	bl->addWidget(presetApply_);

	connect(preset_, &QComboBox::activated, this, [this] { previewPreset(true); });
	connect(preset_, &QComboBox::highlighted, this, [this](int i) { previewPreset(true, i); });
	auto number = [this](double core::PresetSpec::*field, double lo, double hi, double scale) {
		return [this, field, lo, hi, scale](const core::Expression &e) {
			ViewSettings &s = Store::instance().settings();
			double v = 0;
			if (!e.apply(s.preset.*field * scale, v))
				return;
			s.preset.*field = std::clamp(v / scale, lo, hi);
			Store::instance().saveSettings();
			syncLayoutControls();
			previewPreset();
		};
	};
	connect(presetMargin_, &NumericField::committed, this, number(&core::PresetSpec::margin, 0.0, 10000.0, 1.0));
	connect(presetGap_, &NumericField::committed, this, number(&core::PresetSpec::gap, 0.0, 10000.0, 1.0));
	connect(presetCorner_, &NumericField::committed, this,
		number(&core::PresetSpec::cornerScale, 0.05, 1.0, 100.0));
	connect(presetApply_, &QToolButton::clicked, this, [this] {
		const auto p = core::presetFromKey(preset_->currentData().toString().toStdString());
		if (p)
			commands::applyPreset(*p, Store::instance().settings().preset);
	});

	QWidget *sec = section(T("Section.Presets"), body);
	sec->setObjectName("LayoutsPresetSection");
	sec->installEventFilter(this);
	return sec;
}

void InspectorDock::syncLayoutControls()
{
	const bool was = updating_;
	updating_ = true;
	const ViewSettings &s = Store::instance().settings();
	if (QAbstractButton *b = flowDir_->button((int)s.flow.direction))
		b->setChecked(true);
	flowContainer_->setCurrentIndex(s.flowInCanvas ? 1 : 0);
	flowSizing_->setCurrentIndex((int)s.flow.sizing);
	flowMain_->setCurrentIndex((int)s.flow.mainAlign);
	flowCross_->setCurrentIndex((int)s.flow.crossAlign);
	flowColumns_->setValue(s.flow.columns);
	columnsRow_->setVisible(s.flow.direction == core::FlowDirection::Grid);
	flowGap_->setValue(s.flow.gap);
	flowPadding_->setValue(s.flow.padding);
	presetMargin_->setValue(s.preset.margin);
	presetGap_->setValue(s.preset.gap);
	presetCorner_->setValue(s.preset.cornerScale * 100.0);

	/* Presets available for the number of selected items. */
	const int n = (int)EditContext::instance().selection().size();
	QStringList keys;
	for (const core::PresetInfo &p : core::presets()) {
		if (n >= p.minItems && n <= p.maxItems)
			keys << QString::fromUtf8(p.key);
	}
	QStringList shown;
	for (int i = 0; i < preset_->count(); i++)
		shown << preset_->itemData(i).toString();
	if (keys != shown) {
		const QString current = preset_->currentData().toString();
		preset_->clear();
		for (const QString &k : keys)
			preset_->addItem(T(("Preset." + k.toStdString()).c_str()), k);
		const int idx = preset_->findData(current);
		preset_->setCurrentIndex(idx >= 0 ? idx : 0);
	}
	presetApply_->setEnabled(preset_->count() > 0);
	preset_->setEnabled(preset_->count() > 0);
	if (preset_->count() == 0)
		preset_->setPlaceholderText(T("Preset.None"));
	updating_ = was;
}

void InspectorDock::previewLayout(bool force)
{
	if (!force && !hoverLayout_)
		return;
	const ViewSettings &s = Store::instance().settings();
	commands::showPreview(commands::flowFrames(s.flow, s.flowInCanvas));
}

void InspectorDock::previewPreset(bool force, int index)
{
	if (!force && !hoverPreset_ && !preset_->view()->isVisible())
		return;
	const int i = index >= 0 ? index : preset_->currentIndex();
	const auto p = core::presetFromKey(preset_->itemData(i).toString().toStdString());
	if (!p) {
		commands::showPreview({});
		return;
	}
	std::vector<core::Frame> frames = commands::presetFrames(*p, Store::instance().settings().preset);
	commands::showPreview(frames);
}

bool InspectorDock::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::Enter) {
		if (obj == layoutSection_) {
			hoverLayout_ = true;
			previewLayout(true);
		} else if (obj == presetSection_) {
			hoverPreset_ = true;
			previewPreset(true);
		}
	} else if ((event->type() == QEvent::Leave || event->type() == QEvent::Hide) &&
		   (obj == layoutSection_ || obj == presetSection_)) {
		/* Hide too: a section hidden under the mouse never receives Leave. */
		(obj == layoutSection_ ? hoverLayout_ : hoverPreset_) = false;
		clearPreviewIfIdle();
	}
	return QWidget::eventFilter(obj, event);
}

void InspectorDock::clearPreviewIfIdle()
{
	const bool layoutActive = hoverLayout_ && layoutSection_->isVisible();
	const bool presetActive = (hoverPreset_ && presetSection_->isVisible()) || preset_->view()->isVisible();
	if (!layoutActive && !presetActive)
		commands::showPreview({});
}

void InspectorDock::hideEvent(QHideEvent *e)
{
	hoverLayout_ = hoverPreset_ = false;
	commands::showPreview({});
	QWidget::hideEvent(e);
}

} // namespace layouts::ui
