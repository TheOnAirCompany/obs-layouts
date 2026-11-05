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

#include "builder-panel.hpp"

#include "role-ui.hpp"
#include "theme.hpp"
#include "widgets.hpp"

#include "obs/edit-context.hpp"
#include "obs/roles.hpp"
#include "obs/store.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

namespace layouts::ui {

namespace {

enum Column { kUse = 0, kName = 1, kScenes = 2, kType = 3 };
enum DataRole { kUuidRole = Qt::UserRole, kRoleRole };

const char *const kSignals[] = {"source_create", "source_remove", "source_rename", "source_destroy"};

} // namespace

/* ---- sources ------------------------------------------------------------------- */

BuilderSources::BuilderSources(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsBuilderSources");
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, 0, tokens::kPanelMargin);
	v->setSpacing(tokens::kRowSpacing);
	v->addWidget(sectionTitle(T("Builder.Sources"), this));
	filter_ = new RoleFilterBar(this);
	v->addWidget(filter_);

	tree_ = new QTreeWidget(this);
	tree_->setColumnCount(4);
	tree_->setHeaderHidden(true);
	tree_->setRootIsDecorated(false);
	tree_->setUniformRowHeights(true);
	tree_->setFrameShape(QFrame::NoFrame);
	tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	tree_->setIconSize(QSize(tokens::kIconSize, tokens::kIconSize));
	tree_->header()->setStretchLastSection(false);
	tree_->header()->setSectionResizeMode(kUse, QHeaderView::Fixed);
	tree_->header()->resizeSection(kUse, tokens::kIconButton);
	tree_->header()->setSectionResizeMode(kName, QHeaderView::Stretch);
	tree_->header()->setSectionResizeMode(kScenes, QHeaderView::ResizeToContents);
	tree_->header()->setSectionResizeMode(kType, QHeaderView::Fixed);
	tree_->header()->resizeSection(kType, rolePillSize().width() + 8);
	tree_->setItemDelegateForColumn(kType, new PillDelegate(tree_));
	tree_->setToolTip(T("Builder.SourcesTooltip"));
	v->addWidget(tree_, 1);

	empty_ = new QLabel(T("Builder.SourcesEmpty"), this);
	empty_->setWordWrap(true);
	v->addWidget(empty_);

	connect(filter_, &RoleFilterBar::changed, this, &BuilderSources::applyFilter);
	connect(tree_, &QTreeWidget::itemClicked, this, &BuilderSources::onClicked);
	EditContext &ctx = EditContext::instance();
	connect(&ctx, &EditContext::itemsChanged, this, &BuilderSources::refresh);
	connect(&ctx, &EditContext::sceneChanged, this, &BuilderSources::refresh);
	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *s : kSignals)
		signal_handler_connect(sh, s, onSourceSignal, this);
}

BuilderSources::~BuilderSources()
{
	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *s : kSignals)
		signal_handler_disconnect(sh, s, onSourceSignal, this);
}

void BuilderSources::onSourceSignal(void *data, calldata_t *)
{
	/* Any thread: refresh once, later, on the UI thread. */
	auto *self = static_cast<BuilderSources *>(data);
	QMetaObject::invokeMethod(
		self,
		[self] {
			if (self->queued_)
				return;
			self->queued_ = true;
			QTimer::singleShot(100, self, [self] {
				self->queued_ = false;
				self->refresh();
			});
		},
		Qt::QueuedConnection);
}

void BuilderSources::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	signature_.clear();
	refresh();
}

void BuilderSources::refresh()
{
	if (updating_ || !isVisible())
		return;
	QString sig;
	for (const OBSSource &s : roles::videoSources())
		sig += QString("|%1:%2:%3:%4")
			       .arg(obs_source_get_uuid(s))
			       .arg(obs_source_get_name(s))
			       .arg((int)roles::role(s))
			       .arg(roles::sceneCount(s));
	if (sig != signature_) {
		signature_ = sig;
		rebuild();
	}
}

void BuilderSources::rebuild()
{
	updating_ = true;
	std::set<std::string> selected;
	for (QTreeWidgetItem *row : tree_->selectedItems())
		selected.insert(row->data(kName, kUuidRole).toString().toStdString());
	tree_->clear();
	const QColor text = palette().color(QPalette::WindowText);
	QColor dim = text;
	dim.setAlphaF(0.5);
	QColor faint = text;
	faint.setAlphaF(0.2);
	std::map<core::Role, int> counts;
	const std::vector<OBSSource> sources = roles::videoSources();
	for (const OBSSource &s : sources) {
		const core::Role r = roles::role(s);
		const std::string uuid = obs_source_get_uuid(s);
		const int scenes = roles::sceneCount(s);
		counts[r]++;
		auto *row = new QTreeWidgetItem(tree_);
		row->setText(kName, QString::fromUtf8(obs_source_get_name(s)));
		row->setIcon(kName, icon(sourceIcon(s), text));
		row->setData(kName, kUuidRole, QString::fromStdString(uuid));
		row->setData(kName, kRoleRole, (int)r);
		row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		/* Taken in the show: a check mark; sources without a type can't be. */
		const bool used = r != core::Role::None && !unchecked_.count(uuid);
		row->setIcon(kUse,
			     icon(used ? "circle-check" : "circle",
				  used ? QColor::fromRgba(tokens::kSelection) : (r == core::Role::None ? faint : dim)));
		row->setToolTip(kUse, r == core::Role::None ? T("Builder.NoneTooltip")
							    : T(used ? "Builder.Included" : "Builder.Excluded"));
		row->setText(kScenes, scenes > 0 ? QString::number(scenes) : QString::fromUtf8("\xE2\x80\x93"));
		row->setForeground(kScenes, dim);
		row->setTextAlignment(kScenes, Qt::AlignRight | Qt::AlignVCenter);
		row->setToolTip(kScenes, T("Builder.SceneCount").arg(scenes));
		row->setIcon(kType, rolePill(r));
		row->setToolTip(kType, T("Layers.Type").arg(roleName(r)));
		row->setSelected(selected.count(uuid) > 0);
	}
	filter_->setCounts(counts, (int)sources.size());
	tree_->setVisible(!sources.empty());
	filter_->setVisible(!sources.empty());
	empty_->setVisible(sources.empty());
	updating_ = false;
	applyFilter();
	emit changed();
}

void BuilderSources::applyFilter()
{
	for (int i = 0; i < tree_->topLevelItemCount(); i++) {
		QTreeWidgetItem *row = tree_->topLevelItem(i);
		row->setHidden(!filter_->accepts((core::Role)row->data(kName, kRoleRole).toInt()));
	}
}

std::vector<OBSSource> BuilderSources::checkedSources() const
{
	std::vector<OBSSource> out;
	for (const OBSSource &s : roles::videoSources()) {
		if (roles::role(s) != core::Role::None && !unchecked_.count(obs_source_get_uuid(s)))
			out.push_back(s);
	}
	return out;
}

void BuilderSources::setIncluded(obs_source_t *source, bool on)
{
	if (on)
		unchecked_.erase(obs_source_get_uuid(source));
	else
		unchecked_.insert(obs_source_get_uuid(source));
	signature_.clear();
	refresh();
}

void BuilderSources::onClicked(QTreeWidgetItem *row, int column)
{
	if (column == kUse) {
		if ((core::Role)row->data(kName, kRoleRole).toInt() == core::Role::None)
			return;
		const std::string uuid = row->data(kName, kUuidRole).toString().toStdString();
		if (!unchecked_.erase(uuid))
			unchecked_.insert(uuid);
		signature_.clear();
		refresh();
		return;
	}
	if (column != kType)
		return;
	std::vector<obs_source_t *> sources;
	std::vector<OBSSourceAutoRelease> keep;
	const QList<QTreeWidgetItem *> rows = row->isSelected() ? tree_->selectedItems()
								: QList<QTreeWidgetItem *>{row};
	for (QTreeWidgetItem *r : rows) {
		OBSSourceAutoRelease s =
			obs_get_source_by_uuid(r->data(kName, kUuidRole).toString().toUtf8().constData());
		if (s) {
			sources.push_back(s);
			keep.push_back(std::move(s));
		}
	}
	const QRect cell = tree_->visualItemRect(row);
	const int x = tree_->header()->sectionPosition(kType);
	chooseRole(sources, tree_->viewport()->mapToGlobal(QPoint(x, cell.bottom())), this);
	signature_.clear();
	refresh();
}

/* ---- generation ---------------------------------------------------------------- */

BuilderPanel::BuilderPanel(BuilderSources *sources, QWidget *parent) : QWidget(parent), sources_(sources)
{
	setObjectName("LayoutsBuilderPanel");
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	outer->addWidget(scroll);
	auto *content = new QWidget(scroll);
	scroll->setWidget(content);
	auto *v = new QVBoxLayout(content);
	v->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin);
	v->setSpacing(tokens::kRowSpacing);

	v->addWidget(sectionTitle(T("Builder.Generate"), content));
	summary_ = new QLabel(content);
	summary_->setWordWrap(true);
	v->addWidget(summary_);

	auto *nums = new QWidget(content);
	auto *nl = new QHBoxLayout(nums);
	nl->setContentsMargins(0, 0, 0, 0);
	nl->setSpacing(tokens::kColumnSpacing);
	gap_ = new NumericField(QString::fromUtf8("\xE2\x86\x94"), T("Broadcast.Gap"), nums);
	margin_ = new NumericField("M", T("Broadcast.Margin"), nums);
	gap_->setDecimals(0);
	margin_->setDecimals(0);
	nl->addWidget(gap_);
	nl->addWidget(margin_);
	v->addWidget(nums);

	auto check = [&](const char *key, const char *tip) {
		auto *c = new QCheckBox(T(key), content);
		c->setToolTip(T(tip));
		v->addWidget(c);
		return c;
	};
	solo_ = check("Builder.Solo", "Builder.SoloTooltip");
	layouts_ = check("Builder.Layouts", "Builder.LayoutsTooltip");
	background_ = check("Builder.Background", "Builder.BackgroundTooltip");
	graphics_ = check("Builder.Graphics", "Builder.GraphicsTooltip");
	lock_ = check("Broadcast.Lock", "Broadcast.LockTooltip");

	scenes_ = new QLabel(content);
	scenes_->setWordWrap(true);
	scenes_->setTextFormat(Qt::PlainText);
	v->addWidget(scenes_);

	create_ = new QPushButton(content);
	v->addWidget(create_);
	v->addStretch(1);

	const ViewSettings &s = Store::instance().settings();
	updating_ = true;
	solo_->setChecked(s.builderSolo);
	layouts_->setChecked(s.builderLayouts);
	background_->setChecked(s.builderBackground);
	graphics_->setChecked(s.builderGraphics);
	lock_->setChecked(s.broadcastLock);
	gap_->setValue(s.broadcastGap);
	margin_->setValue(s.broadcastMargin);
	updating_ = false;

	auto saveChecks = [this] {
		if (updating_)
			return;
		ViewSettings &st = Store::instance().settings();
		st.builderSolo = solo_->isChecked();
		st.builderLayouts = layouts_->isChecked();
		st.builderBackground = background_->isChecked();
		st.builderGraphics = graphics_->isChecked();
		st.broadcastLock = lock_->isChecked();
		Store::instance().saveSettings();
		refresh();
	};
	for (QCheckBox *c : {solo_, layouts_, background_, graphics_, lock_})
		connect(c, &QCheckBox::toggled, this, saveChecks);
	auto number = [this](double ViewSettings::*field, NumericField *f) {
		return [this, field, f](const core::Expression &e) {
			ViewSettings &st = Store::instance().settings();
			double val = 0;
			if (e.apply(st.*field, val))
				st.*field = std::clamp(val, 0.0, 1000.0);
			Store::instance().saveSettings();
			f->setValue(st.*field);
			refresh();
		};
	};
	connect(gap_, &NumericField::committed, this, number(&ViewSettings::broadcastGap, gap_));
	connect(margin_, &NumericField::committed, this, number(&ViewSettings::broadcastMargin, margin_));
	connect(sources_, &BuilderSources::changed, this, &BuilderPanel::refresh);
	connect(create_, &QPushButton::clicked, this, [this] { create(); });
	refresh();
}

builder::Options BuilderPanel::options() const
{
	builder::Options o;
	const ViewSettings &s = Store::instance().settings();
	o.spec.gap = s.broadcastGap;
	o.spec.margin = s.broadcastMargin;
	o.solo = solo_->isChecked();
	o.layouts = layouts_->isChecked();
	o.background = background_->isChecked();
	o.graphics = graphics_->isChecked();
	o.lock = lock_->isChecked();
	return o;
}

builder::Plan BuilderPanel::plan() const
{
	const double aspect = canvasAspect();
	return builder::plan(
		sources_->checkedSources(), options(),
		[aspect](const std::string &key) { return broadcastLabel(key, aspect); },
		[](const QString &source) { return T("Broadcast.SoloScene").arg(source); });
}

void BuilderPanel::refresh()
{
	const builder::Plan p = plan();
	int hosts = 0;
	for (const OBSSource &c : p.cameras)
		hosts += roles::role(c) == core::Role::Host ? 1 : 0;
	QStringList parts;
	auto add = [&](int n, core::Role r) {
		if (n > 0)
			parts << QString("%1 %2").arg(n).arg(roleName(r));
	};
	add(hosts, core::Role::Host);
	add((int)p.cameras.size() - hosts, core::Role::Camera);
	add((int)p.screens.size(), core::Role::Screen);
	add(p.background ? 1 : 0, core::Role::Background);
	add((int)p.graphics.size(), core::Role::Graphics);
	summary_->setText(parts.isEmpty() ? T("Builder.NothingChecked") : parts.join(QString::fromUtf8(" \xC2\xB7 ")));
	background_->setEnabled(p.background);
	graphics_->setEnabled(!p.graphics.empty());

	QStringList names;
	for (const builder::SceneSpec &s : p.scenes)
		names << QString::fromUtf8("\xE2\x80\xA2 ") + s.name;
	scenes_->setText(names.isEmpty() ? QString()
					 : T("Builder.WillCreate").arg(names.size()) + "\n" + names.join("\n"));
	scenes_->setVisible(!names.isEmpty());
	create_->setText(T("Builder.Create").arg(names.size()));
	create_->setEnabled(!names.isEmpty());
}

std::vector<QString> BuilderPanel::create()
{
	const std::vector<QString> names = builder::create(plan(), options());
	if (!names.empty()) {
		/* Show the first new scene on the Canvas. */
		OBSSourceAutoRelease first = obs_get_source_by_name(names.front().toUtf8().constData());
		if (first)
			EditContext::instance().pin(first);
		create_->setText(T("Broadcast.ShowDone").arg(names.size()));
		create_->setEnabled(false);
		QTimer::singleShot(2500, this, [this] { refresh(); });
	}
	return names;
}

} // namespace layouts::ui
