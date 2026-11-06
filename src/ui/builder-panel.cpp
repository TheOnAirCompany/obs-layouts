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

#include "core/broadcast.hpp"
#include "core/mirror.hpp"
#include "core/scene-nav.hpp"
#include "core/slot-apply.hpp"
#include "obs/dual.hpp"
#include "obs/edit-context.hpp"
#include "obs/roles.hpp"
#include "obs/scene-directory.hpp"
#include "obs/store.hpp"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
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
	v->addWidget(sectionTitle(T("Builder.SourcesStep"), this));
	search_ = new QLineEdit(this);
	search_->setObjectName("LayoutsBuilderSearch");
	search_->setPlaceholderText(T("Builder.Search"));
	search_->setClearButtonEnabled(true);
	search_->addAction(icon("search", this), QLineEdit::LeadingPosition);
	v->addWidget(search_);
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
	auto *hint = new QLabel(T("Builder.SourcesHint"), this);
	hint->setWordWrap(true);
	QPalette hp = hint->palette();
	hp.setColor(QPalette::WindowText, hp.color(QPalette::Disabled, QPalette::WindowText));
	hint->setPalette(hp);
	v->addWidget(hint);

	connect(filter_, &RoleFilterBar::changed, this, &BuilderSources::applyFilter);
	connect(search_, &QLineEdit::textChanged, this, &BuilderSources::applyFilter);
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
	const std::string query = search_->text().trimmed().toStdString();
	for (int i = 0; i < tree_->topLevelItemCount(); i++) {
		QTreeWidgetItem *row = tree_->topLevelItem(i);
		const bool match = query.empty() || core::nav::containsFolded(row->text(kName).toStdString(), query);
		row->setHidden(!match || !filter_->accepts((core::Role)row->data(kName, kRoleRole).toInt()));
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

/* ---- large preview of the chosen layout --------------------------------------------- */

class LayoutPreview : public QWidget {
public:
	explicit LayoutPreview(QWidget *parent) : QWidget(parent)
	{
		setObjectName("LayoutsBuilderPreview");
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	}

	void setContent(const QString &key, double aspect, const builder::Plan &plan, bool background)
	{
		key_ = key.toStdString();
		aspect_ = aspect > 0 ? aspect : 16.0 / 9.0;
		/* Names and types only: holding the sources would keep them alive when the
		 * collection changes. */
		tiles_.clear();
		auto add = [this](const std::vector<OBSSource> &list, core::SlotKind kind) {
			for (const OBSSource &s : list)
				tiles_.push_back({QString::fromUtf8(obs_source_get_name(s)), roles::role(s), kind});
		};
		add(plan.cameras, core::SlotKind::Camera);
		add(plan.screens, core::SlotKind::Screen);
		add(plan.chats, core::SlotKind::Chat);
		background_ = background;
		updateGeometry();
		setFixedHeight(heightForWidth(width()));
		update();
	}

	bool hasHeightForWidth() const override { return true; }
	/* The canvas shape, never taller than wide (a 9:16 canvas fits in a square). */
	int heightForWidth(int w) const override { return std::max(40, (int)std::min<double>(w / aspect_, w)); }
	QSize sizeHint() const override
	{
		return {tokens::kBuilderCreateWidth, heightForWidth(tokens::kBuilderCreateWidth)};
	}

	/* Names drawn in the tiles (self-test). */
	std::vector<QString> names;

protected:
	void resizeEvent(QResizeEvent *e) override
	{
		QWidget::resizeEvent(e);
		setFixedHeight(heightForWidth(width()));
	}

	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		const QRectF box = rect();
		const double bw = std::min(box.width(), box.height() * aspect_);
		const double bh = bw / aspect_;
		const QRectF area(box.x() + (box.width() - bw) / 2, box.y() + (box.height() - bh) / 2, bw, bh);
		p.setBrush(QColor::fromRgba(background_ ? tokens::kRoleBackground : tokens::kPreviewBackdrop));
		p.drawRoundedRect(area, tokens::kPreviewRadius, tokens::kPreviewRadius);
		names.clear();
		if (key_.empty())
			return;

		/* Same layout, gap and margin as the scene will get, at this scale. */
		const ViewSettings &st = Store::instance().settings();
		const double k = area.width() / (aspect_ >= 1 ? 1920.0 : 1080.0);
		core::BroadcastSpec spec;
		spec.gap = st.broadcastGap * k;
		spec.margin = st.broadcastMargin * k;
		const core::Rect canvas{area.x(), area.y(), area.width(), area.height()};
		const std::vector<core::Slot> tiles = core::mirrorSlots(
			core::buildBroadcast(key_, canvas, spec), canvas, st.broadcastMirrorH, st.broadcastMirrorV);
		/* Which source goes in which tile: as builder::create does. */
		std::vector<core::SlotKind> kinds;
		std::vector<bool> hosts;
		for (const Source &src : tiles_) {
			kinds.push_back(src.kind);
			hosts.push_back(src.role == core::Role::Host);
		}
		const std::vector<int> assigned = core::assignItems(tiles, kinds, hosts);
		std::vector<QColor> fills;
		for (size_t i = 0; i < tiles.size(); i++) {
			const int a = i < assigned.size() ? assigned[i] : -1;
			if (a >= 0) {
				fills.push_back(roleColor(tiles_[(size_t)a].role));
				names.push_back(tiles_[(size_t)a].name);
			} else {
				const core::SlotKind kind = tiles[i].kind;
				fills.push_back(QColor::fromRgba(kind == core::SlotKind::Camera ? tokens::kThumbCamera
								 : kind == core::SlotKind::Chat
									 ? tokens::kThumbChat
									 : tokens::kThumbScreen));
				names.push_back(QString());
			}
		}
		QFont f = font();
		f.setPointSizeF(f.pointSizeF() * tokens::kSectionTitlePt);
		f.setBold(true);
		p.setClipRect(area);
		paintLayoutPreview(p, tiles, fills, names, f);
	}

private:
	struct Source {
		QString name;
		core::Role role;
		core::SlotKind kind;
	};
	std::string key_;
	double aspect_ = 16.0 / 9.0;
	std::vector<Source> tiles_;
	bool background_ = false;
};

/* ---- layouts for the selection, and Create --------------------------------------------- */

BuilderPanel::BuilderPanel(BuilderSources *sources, QWidget *parent) : QWidget(parent), sources_(sources)
{
	setObjectName("LayoutsBuilderPanel");

	/* ---- column 2: gallery ---- */
	galleryColumn_ = new QWidget();
	galleryColumn_->setObjectName("LayoutsBuilderGallery");
	auto *gv = new QVBoxLayout(galleryColumn_);
	gv->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin, tokens::kPanelMargin);
	gv->setSpacing(tokens::kRowSpacing);
	auto *head = new QWidget(galleryColumn_);
	auto *hl = new QHBoxLayout(head);
	hl->setContentsMargins(0, 0, 0, 0);
	hl->setSpacing(tokens::kColumnSpacing);
	summary_ = new QLabel(head);
	summary_->setTextFormat(Qt::RichText);
	summary_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	hl->addWidget(summary_, 1);
	auto *targets = new QWidget(head);
	targets->setObjectName("LayoutsBuilderTarget");
	target_ = segmented({"16:9", "9:16", T("Builder.Both")}, targets);
	target_->button(0)->setChecked(true);
	target_->button(0)->setToolTip(T("Builder.Target169Tooltip"));
	for (QAbstractButton *b : target_->buttons())
		b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	hl->addWidget(targets);
	gallery_ = new BroadcastGallery(galleryColumn_);
	gallery_->setLarge(QSize(tokens::kGalleryCardMin, tokens::kGalleryCardMin * 9 / 16), tokens::kGalleryGap);
	hl->addWidget(gallery_->mirrorBar());
	gv->addWidget(head);
	gv->addWidget(gallery_, 1);
	empty_ = new QLabel(galleryColumn_);
	empty_->setWordWrap(true);
	empty_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	gv->addWidget(empty_, 1);
	auto *nums = new QWidget(galleryColumn_);
	auto *nl = new QHBoxLayout(nums);
	nl->setContentsMargins(0, 0, 0, 0);
	nl->setSpacing(tokens::kColumnSpacing);
	gap_ = new NumericField(QString::fromUtf8("\xE2\x86\x94"), T("Broadcast.Gap"), nums);
	margin_ = new NumericField("M", T("Broadcast.Margin"), nums);
	gap_->setDecimals(0);
	margin_->setDecimals(0);
	nl->addWidget(gap_);
	nl->addWidget(margin_);
	nl->addStretch(1);
	gv->addWidget(nums);

	/* ---- column 3: create ---- */
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
	v->addWidget(sectionTitle(T("Builder.CreateStep"), content));
	preview_ = new LayoutPreview(content);
	v->addWidget(preview_);
	v->addSpacing(tokens::kRowSpacing);

	auto label = [&](const char *key) {
		auto *l = new QLabel(T(key), content);
		QPalette lp = l->palette();
		lp.setColor(QPalette::WindowText, lp.color(QPalette::Disabled, QPalette::WindowText));
		l->setPalette(lp);
		return l;
	};
	v->addWidget(label("Builder.SceneName"));
	name_ = new QLineEdit(content);
	name_->setObjectName("LayoutsBuilderName");
	v->addWidget(name_);

	stackRow_ = new QWidget(content);
	auto *sl = new QVBoxLayout(stackRow_);
	sl->setContentsMargins(0, 0, 0, 0);
	sl->setSpacing(tokens::kRowSpacing);
	sl->addWidget(label("Builder.PutInStack"));
	stack_ = new QComboBox(stackRow_);
	stack_->setObjectName("LayoutsBuilderStack");
	stack_->setToolTip(T("Builder.PutInStackTooltip"));
	sl->addWidget(stack_);
	v->addWidget(stackRow_);

	auto check = [&](const char *key, const char *tip) {
		auto *c = new QCheckBox(T(key), content);
		c->setToolTip(T(tip));
		v->addWidget(c);
		return c;
	};
	background_ = check("Builder.Background", "Builder.BackgroundTooltip");
	graphics_ = check("Builder.Graphics", "Builder.GraphicsTooltip");
	lock_ = check("Broadcast.Lock", "Broadcast.LockTooltip");
	v->addSpacing(tokens::kRowSpacing);

	scene_ = new QPushButton(T("Builder.CreateScene"), content);
	scene_->setObjectName("LayoutsBuilderCreate");
	scene_->setToolTip(T("Builder.NewSceneTooltip"));
	scene_->setDefault(true);
	scene_->setStyleSheet(QString("QPushButton { background: %1; color: white; font-weight: bold; border: none;"
				      " border-radius: %2px; padding: %3px %4px; }"
				      "QPushButton:disabled { background: palette(mid); color: palette(window); }")
				      .arg(QColor::fromRgba(tokens::kSelection).name())
				      .arg(tokens::kButtonRadius)
				      .arg(tokens::kButtonPadY)
				      .arg(tokens::kButtonPadX));
	v->addWidget(scene_);
	show_ = new QPushButton(T("Builder.CreateShow"), content);
	show_->setToolTip(T("Builder.ShowTooltip"));
	v->addWidget(show_);
	open_ = new QPushButton(T("Builder.OpenInEditor") + QString::fromUtf8(" \xE2\x86\x97"), content);
	open_->setObjectName("LayoutsBuilderOpenEditor");
	open_->setToolTip(T("Builder.OpenInEditorTooltip"));
	open_->setEnabled(false);
	v->addWidget(open_);
	message_ = new QLabel(content);
	message_->setObjectName("LayoutsBuilderMessage");
	message_->setWordWrap(true);
	message_->setVisible(false);
	v->addWidget(message_);
	auto *note = label("Builder.DoubleClickHint");
	note->setWordWrap(true);
	v->addWidget(note);
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
		updatePreview();
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
			updatePreview();
		};
	};
	connect(gap_, &NumericField::committed, this, number(&ViewSettings::broadcastGap, gap_));
	connect(margin_, &NumericField::committed, this, number(&ViewSettings::broadcastMargin, margin_));
	connect(sources_, &BuilderSources::changed, this, &BuilderPanel::refresh);
	connect(target_, &QButtonGroup::idClicked, this, [this] { refresh(); });
	connect(gallery_, &QListWidget::currentItemChanged, this, [this] {
		/* The name follows the chosen layout until the user types one. */
		const QString label = gallery_->currentLabel();
		if (name_->text().isEmpty() || name_->text() == autoName_)
			name_->setText(label);
		autoName_ = label;
		updatePreview();
	});
	connect(gallery_, &BroadcastGallery::mirrorChanged, this, &BuilderPanel::updatePreview);
	connect(scene_, &QPushButton::clicked, this, [this] { createScene(); });
	connect(gallery_, &QListWidget::itemDoubleClicked, this, [this] { createScene(); });
	connect(show_, &QPushButton::clicked, this, [this] {
		const double aspect = targetAspect();
		builder::Options o = options();
		const size_t n = builder::plan(
					 sources_->selectedSources(), o,
					 [aspect](const std::string &key) { return broadcastLabel(key, aspect); },
					 [](const QString &src) { return src; })
					 .scenes.size();
		if (n == 0)
			return;
		if (QMessageBox::question(this, T("Builder.CreateShowTitle"), T("Builder.ShowConfirm").arg(n)) ==
		    QMessageBox::Yes)
			createShow();
	});
	connect(open_, &QPushButton::clicked, this, [this] {
		if (!lastCreated_.isEmpty())
			emit openInEditor(lastCreated_);
	});
	connect(&SceneDirectory::instance(), &SceneDirectory::changed, this, [this] {
		refreshStacks();
		refreshTargets();
	});
	refreshStacks();
	refresh();
}

void BuilderPanel::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	refreshTargets();
	refreshStacks();
	refresh();
}

BuilderPanel::Target BuilderPanel::target() const
{
	return (Target)std::max(0, target_->checkedId());
}

void BuilderPanel::setTarget(Target t)
{
	if (QAbstractButton *b = target_->button((int)t); b && b->isEnabled())
		b->setChecked(true);
	refresh();
}

void BuilderPanel::refreshTargets()
{
	const bool vertical = SceneDirectory::verticalReady();
	for (int i : {1, 2}) {
		target_->button(i)->setEnabled(vertical);
		target_->button(i)->setToolTip(!vertical ? T("Builder.NeedsVertical")
					       : i == 1  ? T("Builder.Target916Tooltip")
							 : T("Builder.TargetBothTooltip"));
	}
	if (!vertical && target() != Target::Landscape) {
		target_->button(0)->setChecked(true);
		refresh();
	}
}

void BuilderPanel::refreshStacks()
{
	SceneDirectory &dir = SceneDirectory::instance();
	const bool stacks = dir.stacksPresent();
	stackRow_->setVisible(stacks);
	if (!stacks)
		return;
	const QString keep = stack_->currentData().toString();
	const QSignalBlocker block(stack_);
	stack_->clear();
	stack_->addItem(T("Builder.NoStack"), QString());
	for (const std::string &path : dir.stackPaths())
		stack_->addItem(QString::fromStdString(path), QString::fromStdString(path));
	const int i = stack_->findData(keep);
	stack_->setCurrentIndex(i >= 0 ? i : 0);
	/* Stacks organizes the main canvas: nothing to file for a 9:16-only scene. */
	stack_->setEnabled(target() != Target::Portrait);
}

obs_canvas_t *BuilderPanel::targetCanvas() const
{
	if (target() == Target::Portrait)
		return dual::verticalCanvas(false);
	return obs_get_main_canvas();
}

double BuilderPanel::targetAspect() const
{
	obs_canvas_t *c = targetCanvas();
	const core::Rect r = c ? builder::canvasRect(c) : core::Rect{0, 0, 1920, 1080};
	obs_canvas_release(c);
	return r.w > 0 && r.h > 0 ? r.w / r.h : 16.0 / 9.0;
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
	o.twin = target() == Target::Both;
	return o;
}

void BuilderPanel::refresh()
{
	const std::vector<OBSSource> picked = sources_->selectedSources();
	std::map<core::Role, int> n;
	for (const OBSSource &s : picked)
		n[roles::role(s)]++;
	const int cameras = n[core::Role::Host] + n[core::Role::Camera];
	QStringList parts;
	for (core::Role r : {core::Role::Host, core::Role::Camera, core::Role::Screen, core::Role::Chat,
			     core::Role::Background, core::Role::Graphics}) {
		if (n[r] > 0)
			parts << (n[r] > 1 ? QString("%1 %2").arg(n[r]).arg(roleName(r)) : roleName(r));
	}
	const QString sep = QString::fromUtf8(" \xC2\xB7 ");
	summary_->setText(
		parts.isEmpty()
			? T("Builder.LayoutsStep")
			: T("Builder.LayoutsFor").arg(QString("<b>%1</b>").arg(parts.join(sep).toHtmlEscaped())));

	QStringList keys;
	const double aspect = targetAspect();
	for (const core::BroadcastTemplate &t :
	     core::templatesFor(cameras, n[core::Role::Screen], n[core::Role::Chat], aspect < 1))
		keys << QString::fromStdString(t.key);
	gallery_->setTemplates(keys, aspect);
	const bool any = !keys.isEmpty();
	gallery_->setVisible(any);
	QString hint = parts.isEmpty() ? T("Builder.SelectHint") : any ? QString() : T("Broadcast.None");
	if (n[core::Role::None] > 0)
		hint += (hint.isEmpty() ? "" : "\n") + T("Builder.UnsetIgnored").arg(n[core::Role::None]);
	empty_->setText(hint);
	empty_->setVisible(!any);
	if (any && !hint.isEmpty()) {
		empty_->setVisible(true);
		empty_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	} else {
		empty_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	}
	background_->setEnabled(n[core::Role::Background] > 0);
	graphics_->setEnabled(n[core::Role::Graphics] > 0);
	scene_->setEnabled(any);
	show_->setEnabled(cameras + n[core::Role::Screen] + n[core::Role::Chat] > 0);
	stack_->setEnabled(target() != Target::Portrait);
	updatePreview();
}

void BuilderPanel::updatePreview()
{
	const QString key = scene_->isEnabled() ? gallery_->currentKey() : QString();
	const builder::Plan plan = builder::single(sources_->selectedSources(), key.toStdString(), QString());
	preview_->setContent(key, targetAspect(), plan, background_->isChecked() && plan.background);
}

void BuilderPanel::showMessage(const QString &text)
{
	message_->setText(text);
	message_->setVisible(true);
	QTimer::singleShot(tokens::kCreatedMessageMs, message_, [this, text] {
		if (message_->text() == text)
			message_->setVisible(false);
	});
}

void BuilderPanel::fileIntoStack(const std::vector<std::string> &uuids)
{
	const std::string stack = stack_->currentData().toString().toStdString();
	if (stack.empty() || !stackRow_->isVisible() || target() == Target::Portrait)
		return;
	for (const std::string &uuid : uuids)
		SceneDirectory::placeInStack(uuid, stack);
}

QString BuilderPanel::createScene()
{
	const QString key = gallery_->currentKey();
	if (key.isEmpty())
		return {};
	QString name = name_->text().trimmed();
	if (name.isEmpty())
		name = gallery_->currentLabel();
	builder::Options o = options();
	obs_canvas_t *canvas = targetCanvas();
	o.canvas = canvas;
	std::vector<std::string> uuids;
	const std::vector<QString> names =
		builder::create(builder::single(sources_->selectedSources(), key.toStdString(), name), o, &uuids);
	obs_canvas_release(canvas);
	if (names.empty())
		return {};
	fileIntoStack(uuids);
	lastCreated_ = QString::fromStdString(uuids.front());
	open_->setEnabled(true);
	showMessage(target() == Target::Both
			    ? T("Builder.CreatedBoth").arg(names.front(), dual::twinName(names.front()))
			    : T("Builder.Created").arg(names.front()));
	return names.front();
}

std::vector<QString> BuilderPanel::createShow()
{
	const double aspect = targetAspect();
	builder::Options o = options();
	obs_canvas_t *canvas = targetCanvas();
	o.canvas = canvas;
	const builder::Plan plan = builder::plan(
		sources_->selectedSources(), o,
		[aspect](const std::string &key) { return broadcastLabel(key, aspect); },
		[](const QString &source) { return T("Broadcast.SoloScene").arg(source); });
	std::vector<std::string> uuids;
	const std::vector<QString> names = builder::create(plan, o, &uuids);
	obs_canvas_release(canvas);
	if (!names.empty()) {
		fileIntoStack(uuids);
		lastCreated_ = QString::fromStdString(uuids.front());
		open_->setEnabled(true);
	}
	showMessage(T("Broadcast.ShowDone").arg(names.size()));
	return names;
}

} // namespace layouts::ui
