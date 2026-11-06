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

#include "scenes-navigator.hpp"

#include "theme.hpp"
#include "widgets.hpp"

#include "obs/edit-context.hpp"
#include "obs/scene-directory.hpp"

#include <obs.hpp>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace layouts::ui {

namespace {

using core::nav::Node;
using core::nav::Tally;

enum Role {
	KindRole = Qt::UserRole + 1,
	UuidRole,
	CanvasRole,
	ColorRole,
	CountRole,
	TallyRole,
	FavoriteRole,
	RatioRole,
	KeyRole,
	CurrentRole,
	OpenRole, /* stacks: open in Stacks */
};

int kindOf(const QTreeWidgetItem *it)
{
	return it ? it->data(0, KindRole).toInt() : -1;
}

constexpr int kCanvas = (int)Node::Kind::Canvas;
constexpr int kStack = (int)Node::Kind::Stack;
constexpr int kScene = (int)Node::Kind::Scene;

class NavDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QSize s = QStyledItemDelegate::sizeHint(option, index);
		s.setHeight(tokens::kNavRowHeight);
		return s;
	}

	void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QStyleOptionViewItem o(option);
		initStyleOption(&o, index);
		const QPalette &pal = o.palette;
		const int kind = index.data(KindRole).toInt();
		const QRect r = o.rect;
		QColor text = pal.color(QPalette::Text);
		QColor dim = pal.color(QPalette::Disabled, QPalette::Text);
		p->save();
		p->setRenderHint(QPainter::Antialiasing);

		if (kind == kScene && index.data(CurrentRole).toBool()) {
			const QRectF hl = QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
			p->setPen(QPen(QColor::fromRgba(tokens::kSelection), 1));
			p->setBrush(QColor::fromRgba(tokens::kNavSelectedFill));
			p->drawRoundedRect(hl, tokens::kNavRowRadius, tokens::kNavRowRadius);
		}

		int x = r.left() + tokens::kNavRowPadX;
		int right = r.right() - tokens::kNavRowPadX;
		const int cy = r.center().y();

		/* Right side first: tally dot, then count or favorite star. */
		const Tally tally = (Tally)index.data(TallyRole).toInt();
		const bool open = (o.state & QStyle::State_Open) != 0;
		const bool showDot = tally != Tally::None && (kind == kScene || (kind == kStack && !open));
		if (showDot) {
			const int d = tokens::kTallyDot;
			p->setPen(Qt::NoPen);
			p->setBrush(QColor::fromRgba(tally == Tally::Program ? tokens::kTallyProgram
									     : tokens::kTallyPreview));
			p->drawEllipse(QRectF(right - d + 1, cy - d / 2.0, d, d));
			right -= d + tokens::kNavRowPadX;
		}

		if (kind == kCanvas) {
			QFont f = o.font;
			f.setPointSizeF(f.pointSizeF() * tokens::kSectionTitlePt);
			f.setBold(true);
			f.setLetterSpacing(QFont::PercentageSpacing, 105);
			p->setFont(f);
			p->setPen(dim);
			const QString name = index.data(Qt::DisplayRole).toString().toUpper();
			const QString ratio = index.data(RatioRole).toString();
			QFont rf = QFontDatabase::systemFont(QFontDatabase::FixedFont);
			rf.setPointSizeF(o.font.pointSizeF() * tokens::kRatioTagPt);
			const QFontMetrics rm(rf);
			const int tagW = ratio.isEmpty() ? 0 : rm.horizontalAdvance(ratio) + 8 + tokens::kNavRowPadX;
			const QFontMetrics fm(f);
			const int avail = std::max(0, right - x - tagW);
			const QString shown = fm.elidedText(name, Qt::ElideRight, avail);
			const QRect nameRect = p->boundingRect(QRect(x, r.top(), avail, r.height()),
							       Qt::AlignVCenter | Qt::AlignLeft, shown);
			p->drawText(QRect(x, r.top(), avail, r.height()), Qt::AlignVCenter | Qt::AlignLeft, shown);
			x += nameRect.width() + tokens::kNavRowPadX;
			if (!ratio.isEmpty() && x < right) {
				const QRect tag(x, cy - rm.height() / 2 - 1, rm.horizontalAdvance(ratio) + 8,
						rm.height() + 2);
				p->setPen(pal.color(QPalette::Mid));
				p->setBrush(Qt::NoBrush);
				p->drawRoundedRect(QRectF(tag).adjusted(0.5, 0.5, -0.5, -0.5), tokens::kRatioTagRadius,
						   tokens::kRatioTagRadius);
				p->setFont(rf);
				p->setPen(text);
				p->drawText(tag, Qt::AlignCenter, ratio);
			}
			p->restore();
			return;
		}

		if (kind == kStack) {
			const QString count = index.data(CountRole).toString();
			QFont cf = QFontDatabase::systemFont(QFontDatabase::FixedFont);
			cf.setPointSizeF(o.font.pointSizeF() * tokens::kRatioTagPt);
			const QFontMetrics cm(cf);
			p->setFont(cf);
			p->setPen(dim);
			const int cw = cm.horizontalAdvance(count);
			p->drawText(QRect(right - cw, r.top(), cw, r.height()), Qt::AlignVCenter | Qt::AlignRight,
				    count);
			right -= cw + tokens::kNavRowPadX;

			const QColor strip(index.data(ColorRole).toString());
			QPainterPath path;
			path.addRoundedRect(QRectF(x, cy - tokens::kNavStripHeight / 2.0, tokens::kNavStripWidth,
						   tokens::kNavStripHeight),
					    tokens::kNavStripRadius, tokens::kNavStripRadius);
			p->fillPath(path, strip.isValid() ? strip : dim);
			x += tokens::kNavStripWidth + tokens::kNavRowPadX;
			QFont f = o.font;
			f.setBold(true);
			p->setFont(f);
		} else {
			p->setFont(o.font);
			if (index.data(FavoriteRole).toBool()) {
				const QString star = QString::fromUtf8("\xE2\x98\x85");
				const QFontMetrics fm(o.font);
				const int sw = fm.horizontalAdvance(star);
				p->setPen(QColor::fromRgba(tokens::kNavFavorite));
				p->drawText(QRect(right - sw, r.top(), sw, r.height()), Qt::AlignVCenter, star);
				right -= sw + tokens::kNavRowPadX;
			}
		}
		p->setPen(text);
		const QFontMetrics fm(p->font());
		const int w = std::max(0, right - x);
		p->drawText(QRect(x, r.top(), w, r.height()), Qt::AlignVCenter | Qt::AlignLeft,
			    fm.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, w));
		p->restore();
	}
};

void forEachItem(QTreeWidgetItem *it, const std::function<void(QTreeWidgetItem *)> &fn)
{
	fn(it);
	for (int i = 0; i < it->childCount(); i++)
		forEachItem(it->child(i), fn);
}

} // namespace

ScenesNavigator::ScenesNavigator(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsScenesNavigator");
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(tokens::kRowSpacing);

	auto *head = new QWidget(this);
	auto *hl = new QHBoxLayout(head);
	hl->setContentsMargins(0, 0, 0, 0);
	hl->addWidget(sectionTitle(T("Scenes.Title"), head));
	hl->addStretch(1);
	add_ = iconButton("plus", T("Scenes.NewTooltip"), head);
	add_->setObjectName("LayoutsNewScene");
	hl->addWidget(add_);
	v->addWidget(head);

	search_ = new QLineEdit(this);
	search_->setObjectName("LayoutsScenesSearch");
	search_->setPlaceholderText(T("Scenes.Search"));
	search_->setClearButtonEnabled(true);
	search_->addAction(icon("search", this), QLineEdit::LeadingPosition);
	v->addWidget(search_);

	auto *seg = new QWidget(this);
	filter_ = segmented({T("Scenes.All"), "16:9", "9:16", QString::fromUtf8("\xE2\x98\x85")}, seg);
	filter_->button(0)->setChecked(true);
	filter_->button(0)->setToolTip(T("Scenes.FilterAll"));
	filter_->button(1)->setToolTip(T("Scenes.FilterLandscape"));
	filter_->button(2)->setToolTip(T("Scenes.FilterPortrait"));
	filter_->button(3)->setToolTip(T("Scenes.FilterFavorites"));
	v->addWidget(seg);

	tree_ = new QTreeWidget(this);
	tree_->setObjectName("LayoutsScenesTree");
	tree_->setHeaderHidden(true);
	tree_->setColumnCount(1);
	tree_->setRootIsDecorated(false);
	tree_->setIndentation(14);
	tree_->setUniformRowHeights(true);
	tree_->setFrameShape(QFrame::NoFrame);
	tree_->setSelectionMode(QAbstractItemView::NoSelection);
	tree_->setFocusPolicy(Qt::NoFocus);
	tree_->setItemDelegate(new NavDelegate(tree_));
	tree_->setToolTip(T("Scenes.Tooltip"));
	tree_->setMinimumHeight(60);
	v->addWidget(tree_, 1);

	connect(tree_, &QTreeWidget::itemClicked, this, &ScenesNavigator::onClicked);
	auto remember = [this](QTreeWidgetItem *it, bool expanded) {
		if (updating_ || kindOf(it) != kStack)
			return;
		core::nav::Filter f;
		f.query = search_->text().trimmed().toStdString();
		f.ratio = (core::nav::RatioFilter)filter_->checkedId();
		if (!f.active())
			expanded_[it->data(0, KeyRole).toString().toStdString()] = expanded;
	};
	connect(tree_, &QTreeWidget::itemExpanded, this, [remember](QTreeWidgetItem *it) { remember(it, true); });
	connect(tree_, &QTreeWidget::itemCollapsed, this, [remember](QTreeWidgetItem *it) { remember(it, false); });
	connect(search_, &QLineEdit::textChanged, this, &ScenesNavigator::rebuild);
	connect(filter_, &QButtonGroup::idClicked, this, &ScenesNavigator::rebuild);
	connect(add_, &QToolButton::clicked, this, [this] { newScene(); });

	SceneDirectory &dir = SceneDirectory::instance();
	connect(&dir, &SceneDirectory::changed, this, &ScenesNavigator::rebuild);
	connect(&dir, &SceneDirectory::talliesChanged, this, &ScenesNavigator::updateTallies);
	connect(&EditContext::instance(), &EditContext::sceneChanged, this, &ScenesNavigator::syncCurrent);
	rebuild();
}

void ScenesNavigator::refresh()
{
	SceneDirectory::instance().refreshNow();
	rebuild();
}

void ScenesNavigator::rebuild()
{
	updating_ = true;
	SceneDirectory &dir = SceneDirectory::instance();
	const bool stacks = dir.stacksPresent();
	filter_->button(3)->setEnabled(stacks);
	filter_->button(3)->setToolTip(stacks ? T("Scenes.FilterFavorites") : T("Scenes.FavoritesNeedStacks"));
	if (!stacks && filter_->checkedId() == 3)
		filter_->button(0)->setChecked(true);

	core::nav::Filter f;
	f.query = search_->text().trimmed().toStdString();
	f.ratio = (core::nav::RatioFilter)filter_->checkedId();
	const std::vector<Node> nodes = core::nav::filterTree(dir.tree(), f);

	tree_->clear();
	std::function<void(const Node &, QTreeWidgetItem *)> add = [&](const Node &n, QTreeWidgetItem *parent) {
		auto *it = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
		it->setText(0, QString::fromStdString(n.name));
		it->setData(0, KindRole, (int)n.kind);
		it->setData(0, UuidRole, QString::fromStdString(n.uuid));
		it->setData(0, CanvasRole, QString::fromStdString(n.canvas));
		it->setFlags(Qt::ItemIsEnabled);
		switch (n.kind) {
		case Node::Kind::Canvas:
			it->setData(0, RatioRole, QString::fromStdString(n.ratio));
			/* The main canvas under its localized name (OBS calls it "Main"). The section
			 * has no expand arrow (root not decorated) and always stays open. */
			if (n.main)
				it->setText(0, T("Scenes.MainCanvas"));
			break;
		case Node::Kind::Stack:
			it->setData(0, ColorRole, QString::fromStdString(n.color));
			it->setData(0, CountRole, QString::number(core::nav::sceneCount(n)));
			it->setData(0, KeyRole, QString::fromStdString(n.canvas + "/" + n.path));
			it->setData(0, OpenRole, !n.collapsed);
			it->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
			break;
		case Node::Kind::Scene:
			it->setData(0, FavoriteRole, n.favorite);
			it->setToolTip(0, QString::fromStdString(n.name));
			break;
		}
		for (const Node &c : n.children)
			add(c, it);
	};
	for (const Node &n : nodes)
		add(n, nullptr);

	/* Canvases always open; stacks as in Stacks, or as the user left them here. */
	for (int i = 0; i < tree_->topLevelItemCount(); i++) {
		forEachItem(tree_->topLevelItem(i), [&](QTreeWidgetItem *it) {
			if (kindOf(it) == kCanvas) {
				it->setExpanded(true);
			} else if (kindOf(it) == kStack) {
				bool open = true;
				if (!f.active()) {
					auto found = expanded_.find(it->data(0, KeyRole).toString().toStdString());
					open = found != expanded_.end() ? found->second
									: it->data(0, OpenRole).toBool();
				}
				it->setExpanded(open);
			}
		});
	}
	updating_ = false;
	updateTallies();
	syncCurrent();
}

void ScenesNavigator::updateTallies()
{
	const SceneDirectory &dir = SceneDirectory::instance();
	const std::set<std::string> &program = dir.programScenes();
	const std::set<std::string> &preview = dir.previewScenes();
	std::function<Tally(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *it) {
		Tally t = Tally::None;
		if (kindOf(it) == kScene) {
			const std::string uuid = it->data(0, UuidRole).toString().toStdString();
			t = program.count(uuid) ? Tally::Program : preview.count(uuid) ? Tally::Preview : Tally::None;
		} else {
			for (int i = 0; i < it->childCount(); i++) {
				const Tally c = visit(it->child(i));
				if (c == Tally::Program || (c == Tally::Preview && t == Tally::None))
					t = c;
			}
		}
		it->setData(0, TallyRole, (int)t);
		it->setToolTip(0, kindOf(it) == kScene && t == Tally::Program   ? T("Scenes.OnProgram")
				  : kindOf(it) == kScene && t == Tally::Preview ? T("Scenes.OnPreview")
										: it->text(0));
		return t;
	};
	for (int i = 0; i < tree_->topLevelItemCount(); i++)
		visit(tree_->topLevelItem(i));
	tree_->viewport()->update();
}

void ScenesNavigator::syncCurrent()
{
	EditContext &ctx = EditContext::instance();
	const QString uuid = ctx.sceneUuid();
	QTreeWidgetItem *current = nullptr;
	for (int i = 0; i < tree_->topLevelItemCount(); i++) {
		forEachItem(tree_->topLevelItem(i), [&](QTreeWidgetItem *it) {
			const bool on = kindOf(it) == kScene && !uuid.isEmpty() &&
					it->data(0, UuidRole).toString() == uuid;
			it->setData(0, CurrentRole, on);
			if (on)
				current = it;
		});
	}
	if (current) {
		/* The edited scene stays in view (its stack opens). */
		for (QTreeWidgetItem *p = current->parent(); p; p = p->parent()) {
			if (!p->isExpanded())
				p->setExpanded(true);
		}
		tree_->scrollToItem(current);
	}
	tree_->viewport()->update();
}

QTreeWidgetItem *ScenesNavigator::rowOf(const QString &uuid) const
{
	QTreeWidgetItem *found = nullptr;
	for (int i = 0; i < tree_->topLevelItemCount() && !found; i++) {
		forEachItem(tree_->topLevelItem(i), [&](QTreeWidgetItem *it) {
			if (!found && kindOf(it) == kScene && it->data(0, UuidRole).toString() == uuid)
				found = it;
		});
	}
	return found;
}

void ScenesNavigator::onClicked(QTreeWidgetItem *item, int)
{
	if (updating_ || kindOf(item) != kScene)
		return;
	const QString uuid = item->data(0, UuidRole).toString();
	/* After the click: switching scenes rebuilds rows. Nothing changes on air. */
	QTimer::singleShot(0, this, [uuid] {
		OBSSourceAutoRelease src = obs_get_source_by_uuid(uuid.toUtf8().constData());
		if (src)
			EditContext::instance().pin(src);
	});
}

QString ScenesNavigator::newScene()
{
	EditContext &ctx = EditContext::instance();
	obs_canvas_t *canvas = ctx.sceneSource() ? obs_source_get_canvas(ctx.sceneSource()) : nullptr;
	if (!canvas)
		canvas = obs_get_main_canvas();
	QString name;
	for (int i = 1;; i++) {
		name = T("Scenes.NewName").arg(i);
		OBSSourceAutoRelease existing = obs_get_source_by_name(name.toUtf8().constData());
		if (!existing)
			break;
	}
	obs_scene_t *scene = canvas ? obs_canvas_scene_create(canvas, name.toUtf8().constData()) : nullptr;
	obs_canvas_release(canvas);
	if (!scene)
		return {};
	const std::string uuid = obs_source_get_uuid(obs_scene_get_source(scene));
	/* In the stack of the edited scene, when Stacks files it. */
	if (ctx.sceneSource()) {
		std::string stack;
		for (const Node *n : SceneDirectory::instance().locate(ctx.sceneUuid().toStdString())) {
			if (n->kind == Node::Kind::Stack && n->main)
				stack = n->path;
		}
		if (!stack.empty())
			SceneDirectory::placeInStack(uuid, stack);
	}
	ctx.pin(obs_scene_get_source(scene));
	obs_scene_release(scene);
	return name;
}

} // namespace layouts::ui
