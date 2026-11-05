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

#include "broadcast-gallery.hpp"
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

enum Column { kName = 0, kScenes = 1, kType = 2 };
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
	tree_->setColumnCount(3);
	tree_->setHeaderHidden(true);
	tree_->setRootIsDecorated(false);
	tree_->setUniformRowHeights(true);
	tree_->setFrameShape(QFrame::NoFrame);
	tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	tree_->setIconSize(QSize(tokens::kIconSize, tokens::kIconSize));
	tree_->header()->setStretchLastSection(false);
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
	connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this] {
		if (!updating_)
			emit changed();
	});
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
	dim.setAlphaF(0.5f);
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

std::vector<OBSSource> BuilderSources::selectedSources() const
{
	std::vector<OBSSource> out;
	for (int i = 0; i < tree_->topLevelItemCount(); i++) {
		QTreeWidgetItem *row = tree_->topLevelItem(i);
		if (!row->isSelected())
			continue;
		OBSSourceAutoRelease s =
			obs_get_source_by_uuid(row->data(kName, kUuidRole).toString().toUtf8().constData());
		if (s)
			out.push_back(OBSSource(s.Get()));
	}
	return out;
}

void BuilderSources::selectNames(const QStringList &names)
{
	updating_ = true;
	for (int i = 0; i < tree_->topLevelItemCount(); i++)
		tree_->topLevelItem(i)->setSelected(names.contains(tree_->topLevelItem(i)->text(kName)));
	updating_ = false;
	emit changed();
}

void BuilderSources::onClicked(QTreeWidgetItem *row, int column)
{
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

/* ---- layouts for the selection --------------------------------------------------- */

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

	v->addWidget(sectionTitle(T("Section.Broadcast"), content));
	summary_ = new QLabel(content);
	summary_->setWordWrap(true);
	v->addWidget(summary_);

	gallery_ = new BroadcastGallery(content);
	v->addWidget(gallery_);
	empty_ = new QLabel(content);
	empty_->setWordWrap(true);
	v->addWidget(empty_);

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
	background_ = check("Builder.Background", "Builder.BackgroundTooltip");
	graphics_ = check("Builder.Graphics", "Builder.GraphicsTooltip");
	lock_ = check("Broadcast.Lock", "Broadcast.LockTooltip");

	scene_ = new QPushButton(T("Broadcast.NewScene"), content);
	scene_->setToolTip(T("Builder.NewSceneTooltip"));
	scene_->setDefault(true);
	v->addWidget(scene_);
	show_ = new QPushButton(T("Broadcast.Show"), content);
	show_->setToolTip(T("Builder.ShowTooltip"));
	v->addWidget(show_);
	v->addStretch(1);

	const ViewSettings &s = Store::instance().settings();
	updating_ = true;
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
		st.builderBackground = background_->isChecked();
		st.builderGraphics = graphics_->isChecked();
		st.broadcastLock = lock_->isChecked();
		Store::instance().saveSettings();
	};
	for (QCheckBox *c : {background_, graphics_, lock_})
		connect(c, &QCheckBox::toggled, this, saveChecks);
	auto number = [this](double ViewSettings::*field, NumericField *f) {
		return [this, field, f](const core::Expression &e) {
			ViewSettings &st = Store::instance().settings();
			double val = 0;
			if (e.apply(st.*field, val))
				st.*field = std::clamp(val, 0.0, 1000.0);
			Store::instance().saveSettings();
			f->setValue(st.*field);
		};
	};
	connect(gap_, &NumericField::committed, this, number(&ViewSettings::broadcastGap, gap_));
	connect(margin_, &NumericField::committed, this, number(&ViewSettings::broadcastMargin, margin_));
	connect(sources_, &BuilderSources::changed, this, &BuilderPanel::refresh);
	connect(scene_, &QPushButton::clicked, this, [this] { createScene(); });
	connect(gallery_, &QListWidget::itemDoubleClicked, this, [this] { createScene(); });
	connect(show_, &QPushButton::clicked, this, [this] { createShow(); });
	refresh();
}

builder::Options BuilderPanel::options() const
{
	builder::Options o;
	const ViewSettings &s = Store::instance().settings();
	o.spec.gap = s.broadcastGap;
	o.spec.margin = s.broadcastMargin;
	o.background = background_->isChecked();
	o.graphics = graphics_->isChecked();
	o.lock = lock_->isChecked();
	return o;
}

void BuilderPanel::refresh()
{
	const std::vector<OBSSource> picked = sources_->selectedSources();
	int hosts = 0, cams = 0, screens = 0, chats = 0, bgs = 0, cgs = 0, unset = 0;
	for (const OBSSource &s : picked) {
		switch (roles::role(s)) {
		case core::Role::Host:
			hosts++;
			break;
		case core::Role::Camera:
			cams++;
			break;
		case core::Role::Screen:
			screens++;
			break;
		case core::Role::Chat:
			chats++;
			break;
		case core::Role::Background:
			bgs++;
			break;
		case core::Role::Graphics:
			cgs++;
			break;
		case core::Role::None:
			unset++;
			break;
		}
	}
	QStringList parts;
	auto add = [&](int n, core::Role r) {
		if (n > 0)
			parts << QString("%1 %2").arg(n).arg(roleName(r));
	};
	add(hosts, core::Role::Host);
	add(cams, core::Role::Camera);
	add(screens, core::Role::Screen);
	add(chats, core::Role::Chat);
	add(bgs, core::Role::Background);
	add(cgs, core::Role::Graphics);
	QString text = parts.isEmpty() ? T("Builder.SelectHint")
				       : T("Builder.Selection").arg(parts.join(QString::fromUtf8(" \xC2\xB7 ")));
	if (unset > 0)
		text += "\n" + T("Builder.UnsetIgnored").arg(unset);
	summary_->setText(text);

	QStringList keys;
	const double aspect = canvasAspect();
	for (const core::BroadcastTemplate &t : core::templatesFor(hosts + cams, screens, chats, aspect < 1))
		keys << QString::fromStdString(t.key);
	gallery_->setTemplates(keys, aspect);
	const bool any = !keys.isEmpty();
	gallery_->setVisible(any);
	empty_->setVisible(!any && !parts.isEmpty());
	empty_->setText(T("Broadcast.None"));
	background_->setEnabled(bgs > 0);
	graphics_->setEnabled(cgs > 0);
	scene_->setEnabled(any);
	show_->setEnabled(hosts + cams + screens + chats > 0);
}

void BuilderPanel::flash(QPushButton *button, const QString &text)
{
	const QString normal = button == scene_ ? T("Broadcast.NewScene") : T("Broadcast.Show");
	button->setText(text);
	QTimer::singleShot(2500, button, [button, normal] { button->setText(normal); });
}

QString BuilderPanel::createScene()
{
	const QString key = gallery_->currentKey();
	if (key.isEmpty())
		return {};
	const std::vector<QString> names = builder::create(
		builder::single(sources_->selectedSources(), key.toStdString(), gallery_->currentLabel()), options());
	if (names.empty())
		return {};
	OBSSourceAutoRelease created = obs_get_source_by_name(names.front().toUtf8().constData());
	if (created)
		EditContext::instance().pin(created); /* the new scene on the Canvas */
	flash(scene_, T("Builder.Created").arg(names.front()));
	return names.front();
}

std::vector<QString> BuilderPanel::createShow()
{
	const double aspect = canvasAspect();
	const builder::Plan plan = builder::plan(
		sources_->selectedSources(), options(),
		[aspect](const std::string &key) { return broadcastLabel(key, aspect); },
		[](const QString &source) { return T("Broadcast.SoloScene").arg(source); });
	const std::vector<QString> names = builder::create(plan, options());
	if (!names.empty()) {
		OBSSourceAutoRelease first = obs_get_source_by_name(names.front().toUtf8().constData());
		if (first)
			EditContext::instance().pin(first);
	}
	flash(show_, T("Broadcast.ShowDone").arg(names.size()));
	return names;
}

} // namespace layouts::ui
