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

#include "layers-panel.hpp"

#include "role-ui.hpp"
#include "scenes-list.hpp"
#include "theme.hpp"
#include "widgets.hpp"

#include "obs/edit-context.hpp"
#include "obs/roles.hpp"

#include <obs-frontend-api.h>

#include <QDropEvent>
#include <QHeaderView>
#include <QLabel>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace layouts::ui {

namespace {

enum Column { kName = 0, kType = 1, kLock = 2, kEye = 3 };
enum Role { kIdRole = Qt::UserRole, kGroupRole, kIsGroupRole, kRoleRole };

/* ---- undoable layer edits ----
 * Each undo entry names the item by the top scene's uuid, its id and its source's uuid
 * (ids are kept when an item enters or leaves a group). libobs transform states do not
 * carry the stacking order, visibility or grouping, so these entries are our own. */

obs_sceneitem_t *findIn(obs_scene_t *scene, int64_t id, const char *sourceUuid)
{
	struct Find {
		int64_t id;
		const char *uuid;
		obs_sceneitem_t *out;
	} f{id, sourceUuid, nullptr};
	if (!scene)
		return nullptr;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			auto *f = (Find *)p;
			if (obs_sceneitem_get_id(item) == f->id &&
			    strcmp(obs_source_get_uuid(obs_sceneitem_get_source(item)), f->uuid) == 0) {
				f->out = item;
				return false;
			}
			return true;
		},
		&f);
	return f.out;
}

/* The scene that holds the items: the top scene, or the inner scene of a group of it. */
obs_scene_t *container(obs_scene_t *top, const char *group, obs_sceneitem_t **groupItem = nullptr)
{
	if (groupItem)
		*groupItem = nullptr;
	if (!group || !*group)
		return top;
	obs_sceneitem_t *g = top ? obs_scene_get_group(top, group) : nullptr;
	if (groupItem)
		*groupItem = g;
	return g ? obs_sceneitem_group_get_scene(g) : nullptr;
}

OBSSourceAutoRelease topScene(obs_data_t *d)
{
	return obs_get_source_by_uuid(obs_data_get_string(d, "scene"));
}

/* ---- stacking order and grouping, as OBS's own Sources list does it ----
 * obs_scene_reorder_items2 takes the whole scene from back to front, each group
 * followed by its own items; it keeps every image in place when an item enters or
 * leaves a group. Undo entries hold that list as {"scene", "items": [{id, source,
 * group}]}, the items named by id and source uuid. */

using OrderList = std::vector<obs_sceneitem_order_info>;

void appendGroupChildren(OrderList &out, obs_sceneitem_t *group)
{
	struct Ctx {
		OrderList *out;
		obs_sceneitem_t *group;
	} c{&out, group};
	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			auto *c = (Ctx *)p;
			c->out->push_back({c->group, item});
			return true;
		},
		&c);
}

OrderList currentOrder(obs_scene_t *top)
{
	OrderList out;
	obs_scene_enum_items(
		top,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			auto *out = (OrderList *)p;
			out->push_back({nullptr, item});
			if (obs_sceneitem_is_group(item))
				appendGroupChildren(*out, item);
			return true;
		},
		&out);
	return out;
}

OBSDataAutoRelease orderData(obs_scene_t *top, const OrderList &order)
{
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "scene", obs_source_get_uuid(obs_scene_get_source(top)));
	OBSDataArrayAutoRelease items = obs_data_array_create();
	for (const obs_sceneitem_order_info &o : order) {
		OBSDataAutoRelease e = obs_data_create();
		obs_data_set_int(e, "id", obs_sceneitem_get_id(o.item));
		obs_data_set_string(e, "source", obs_source_get_uuid(obs_sceneitem_get_source(o.item)));
		obs_data_set_string(e, "group", o.group ? obs_source_get_name(obs_sceneitem_get_source(o.group)) : "");
		obs_data_array_push_back(items, e);
	}
	obs_data_set_array(d, "items", items);
	return d;
}

bool sameOrder(const OrderList &a, const OrderList &b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); i++) {
		if (a[i].item != b[i].item || a[i].group != b[i].group)
			return false;
	}
	return true;
}

void applyOrder(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSSourceAutoRelease src = topScene(d);
	obs_scene_t *top = obs_scene_from_source(src);
	if (!top)
		return;
	/* Every item of the scene and of its groups, wherever it is now. */
	const OrderList now = currentOrder(top);
	auto lookup = [&now](int64_t id, const char *uuid) -> obs_sceneitem_t * {
		for (const obs_sceneitem_order_info &o : now) {
			if (obs_sceneitem_get_id(o.item) == id &&
			    strcmp(obs_source_get_uuid(obs_sceneitem_get_source(o.item)), uuid) == 0)
				return o.item;
		}
		return nullptr;
	};
	OBSDataArrayAutoRelease items = obs_data_get_array(d, "items");
	OrderList order;
	for (size_t i = 0; i < obs_data_array_count(items); i++) {
		OBSDataAutoRelease e = obs_data_array_item(items, i);
		obs_sceneitem_t *item = lookup(obs_data_get_int(e, "id"), obs_data_get_string(e, "source"));
		const char *group = obs_data_get_string(e, "group");
		obs_sceneitem_t *g = group && *group ? obs_scene_get_group(top, group) : nullptr;
		if (!item || (group && *group && !g))
			return; /* the scene changed since: leave it alone */
		order.push_back({g, item});
	}
	if (order.size() == now.size())
		obs_scene_reorder_items2(top, order.data(), order.size());
	EditContext::instance().markDirty();
}

/* {"scene", "group", "id", "source", "flag": "visible"|"locked", "value"}. */
void applyFlag(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSSourceAutoRelease src = topScene(d);
	obs_sceneitem_t *item = findIn(container(obs_scene_from_source(src), obs_data_get_string(d, "group")),
				       obs_data_get_int(d, "id"), obs_data_get_string(d, "source"));
	if (!item)
		return;
	const bool value = obs_data_get_bool(d, "value");
	if (strcmp(obs_data_get_string(d, "flag"), "visible") == 0)
		obs_sceneitem_set_visible(item, value);
	else
		obs_sceneitem_set_locked(item, value);
	EditContext::instance().markDirty();
}

/* {"source": uuid, "name"}. */
void applyName(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSSourceAutoRelease src = obs_get_source_by_uuid(obs_data_get_string(d, "source"));
	if (src)
		obs_source_set_name(src, obs_data_get_string(d, "name"));
	EditContext::instance().markDirty();
}

QString groupName(const ItemEntry &e)
{
	return e.groupId != 0 && e.group ? QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(e.group)))
					 : QString();
}

void addUndo(const QString &what, void (*undo)(const char *), void (*redo)(const char *), obs_data_t *before,
	     obs_data_t *after)
{
	const QString name = T("Undo.Format").arg(what, EditContext::instance().sceneName());
	obs_frontend_add_undo_redo_action(name.toUtf8().constData(), undo, redo, obs_data_get_json(before),
					  obs_data_get_json(after), false);
}

} // namespace

/* Tree with drag and drop limited to reordering among siblings. */
class LayerTree : public QTreeWidget {
public:
	LayerTree(LayersPanel *panel) : QTreeWidget(panel), panel_(panel) {}

protected:
	void dropEvent(QDropEvent *e) override
	{
		QTreeWidgetItem *moved = currentItem();
		if (!moved || selectedItems().size() > 1) { /* one row at a time */
			e->ignore();
			return;
		}
		QTreeWidget::dropEvent(e);
		/* Qt has moved the row; reorder() applies it (or puts a group back). */
		QTreeWidgetItem *parentAfter = moved->parent();
		const int index = parentAfter ? parentAfter->indexOfChild(moved) : indexOfTopLevelItem(moved);
		panel_->reorder(moved, parentAfter, index);
	}

private:
	LayersPanel *panel_;
};

LayersPanel::LayersPanel(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsLayersPanel");
	/* Scenes on top (which scene is edited), Layers below, resizable. */
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *split = new QSplitter(Qt::Vertical, this);
	split->setChildrenCollapsible(false);
	outer->addWidget(split);

	auto *top = new QWidget(split);
	auto *tv = new QVBoxLayout(top);
	tv->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, 0, 0);
	tv->setSpacing(tokens::kRowSpacing);
	tv->addWidget(sectionTitle(T("Scenes.Title"), top));
	scenes_ = new ScenesList(top);
	scenes_->setMinimumHeight(60);
	tv->addWidget(scenes_, 1);

	auto *body = new QWidget(split);
	auto *v = new QVBoxLayout(body);
	v->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, 0, tokens::kPanelMargin);
	v->setSpacing(tokens::kRowSpacing);
	split->addWidget(top);
	split->addWidget(body);
	split->setStretchFactor(0, 0);
	split->setStretchFactor(1, 1);
	split->setSizes({170, 600});

	v->addWidget(sectionTitle(T("Layers.Title"), this));
	filter_ = new RoleFilterBar(this);
	v->addWidget(filter_);

	tree_ = new LayerTree(this);
	tree_->setColumnCount(4);
	tree_->setHeaderHidden(true);
	tree_->setRootIsDecorated(true);
	tree_->setIndentation(14);
	tree_->setUniformRowHeights(true);
	tree_->setFrameShape(QFrame::NoFrame);
	tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	tree_->setDragDropMode(QAbstractItemView::InternalMove);
	tree_->setDefaultDropAction(Qt::MoveAction);
	tree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
	tree_->setIconSize(QSize(tokens::kIconSize, tokens::kIconSize));
	tree_->header()->setStretchLastSection(false);
	tree_->header()->setSectionResizeMode(kName, QHeaderView::Stretch);
	tree_->header()->setSectionResizeMode(kType, QHeaderView::Fixed);
	tree_->header()->resizeSection(kType, rolePillSize().width() + 8);
	tree_->setItemDelegateForColumn(kType, new PillDelegate(tree_));
	tree_->header()->setSectionResizeMode(kLock, QHeaderView::Fixed);
	tree_->header()->setSectionResizeMode(kEye, QHeaderView::Fixed);
	tree_->header()->resizeSection(kLock, tokens::kIconButton);
	tree_->header()->resizeSection(kEye, tokens::kIconButton);
	tree_->setToolTip(T("Layers.Tooltip"));
	v->addWidget(tree_, 1);

	empty_ = new QLabel(T("Layers.Empty"), this);
	empty_->setWordWrap(true);
	v->addWidget(empty_);
	v->addStretch(0);

	EditContext &ctx = EditContext::instance();
	connect(&ctx, &EditContext::itemsChanged, this, &LayersPanel::refresh);
	connect(&ctx, &EditContext::sceneChanged, this, [this] {
		collapsed_.clear();
		signature_.clear();
		refresh();
	});
	connect(filter_, &RoleFilterBar::changed, this, &LayersPanel::applyFilter);
	connect(tree_, &QTreeWidget::itemSelectionChanged, this, &LayersPanel::onUserSelection);
	connect(tree_, &QTreeWidget::itemClicked, this, &LayersPanel::onClicked);
	connect(tree_, &QTreeWidget::itemChanged, this, &LayersPanel::onRenamed);
	connect(tree_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *row) {
		if (!updating_)
			collapsed_.insert(row->data(kName, kIdRole).toLongLong());
	});
	connect(tree_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *row) {
		if (!updating_)
			collapsed_.erase(row->data(kName, kIdRole).toLongLong());
	});
	refresh();
}

QTreeWidget *LayersPanel::tree() const
{
	return tree_;
}

void LayersPanel::refresh()
{
	if (updating_)
		return;
	/* Only rebuild when the list itself changed (not on every transform). */
	QString sig = EditContext::instance().sceneUuid();
	for (const ItemEntry &e : EditContext::instance().items())
		sig += QString("|%1:%2:%3:%4:%5:%6")
			       .arg(e.groupId)
			       .arg(e.id)
			       .arg(e.visible)
			       .arg(e.locked)
			       .arg(e.name)
			       .arg((int)roles::role(obs_sceneitem_get_source(e.item)));
	if (sig != signature_) {
		signature_ = sig;
		rebuild();
	}
	syncSelection();
}

void LayersPanel::rebuild()
{
	updating_ = true;
	const QColor text = palette().color(QPalette::WindowText);
	QColor dim = text;
	dim.setAlphaF(0.35f);
	tree_->clear();
	std::map<int64_t, QTreeWidgetItem *> groups;
	std::map<core::Role, int> counts;
	std::set<obs_source_t *> counted; /* a source twice in the scene counts once */
	const std::vector<ItemEntry> &items = EditContext::instance().items();
	/* items are in render order (bottom first): prepend so the front is on top. */
	for (const ItemEntry &e : items) {
		auto *row = new QTreeWidgetItem();
		row->setText(kName, e.name);
		row->setData(kName, kIdRole, (qlonglong)e.id);
		row->setData(kName, kGroupRole, (qlonglong)e.groupId);
		row->setData(kName, kIsGroupRole, e.isGroup);
		row->setIcon(kName, icon(sourceIcon(obs_sceneitem_get_source(e.item)), e.visible ? text : dim));
		row->setIcon(kLock, icon(e.locked ? "lock" : "lock-open", e.locked ? text : dim));
		row->setIcon(kEye, icon(e.visible ? "eye" : "eye-off", e.visible ? dim : text));
		row->setToolTip(kLock, T(e.locked ? "Layers.Unlock" : "Layers.Lock"));
		row->setToolTip(kEye, T(e.visible ? "Layers.Hide" : "Layers.Show"));
		if (!e.isGroup) {
			obs_source_t *src = obs_sceneitem_get_source(e.item);
			const core::Role r = roles::role(src);
			row->setData(kName, kRoleRole, (int)r);
			row->setIcon(kType, rolePill(r));
			row->setToolTip(kType, T("Layers.Type").arg(roleName(r)));
			if (counted.insert(src).second)
				counts[r]++;
		}
		if (!e.visible)
			row->setForeground(kName, dim);
		Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable |
				      Qt::ItemIsDragEnabled;
		if (e.isGroup)
			flags |= Qt::ItemIsDropEnabled;
		row->setFlags(flags);
		if (e.groupId != 0 && groups.count(e.groupId))
			groups[e.groupId]->insertChild(0, row);
		else
			tree_->insertTopLevelItem(0, row);
		if (e.isGroup)
			groups[e.id] = row;
	}
	for (auto &[id, row] : groups)
		row->setExpanded(!collapsed_.count(id));
	/* The invisible root accepts drops between top-level rows. */
	tree_->invisibleRootItem()->setFlags(Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
	tree_->setVisible(!items.empty());
	empty_->setVisible(items.empty());
	filter_->setCounts(counts, (int)counted.size());
	filter_->setVisible(!items.empty());
	updating_ = false;
	applyFilter();
}

void LayersPanel::applyFilter()
{
	const bool filtering = filter_->filtering();
	auto accepted = [this](QTreeWidgetItem *row) {
		return filter_->accepts((core::Role)row->data(kName, kRoleRole).toInt());
	};
	for (int i = 0; i < tree_->topLevelItemCount(); i++) {
		QTreeWidgetItem *top = tree_->topLevelItem(i);
		if (top->data(kName, kIsGroupRole).toBool()) {
			/* A group stays while one of its sources matches. */
			bool any = false;
			for (int c = 0; c < top->childCount(); c++) {
				const bool on = !filtering || accepted(top->child(c));
				top->child(c)->setHidden(!on);
				any = any || on;
			}
			top->setHidden(filtering && !any);
			if (filtering && any)
				top->setExpanded(true);
		} else {
			top->setHidden(filtering && !accepted(top));
		}
	}
	/* Reordering a filtered list would move rows past hidden ones: off while filtering. */
	tree_->setDragDropMode(filtering ? QAbstractItemView::NoDragDrop : QAbstractItemView::InternalMove);
}

void LayersPanel::syncSelection()
{
	updating_ = true;
	QTreeWidgetItem *first = nullptr;
	std::set<std::pair<int64_t, int64_t>> selected;
	for (const ItemEntry &e : EditContext::instance().items()) {
		if (e.selected)
			selected.insert({e.groupId, e.id});
	}
	for (QTreeWidgetItemIterator it(tree_); *it; ++it) {
		QTreeWidgetItem *row = *it;
		const bool on = selected.count(
			{row->data(kName, kGroupRole).toLongLong(), row->data(kName, kIdRole).toLongLong()});
		if (row->isSelected() != on)
			row->setSelected(on);
		if (on && !first)
			first = row;
	}
	if (first)
		tree_->scrollToItem(first);
	updating_ = false;
}

void LayersPanel::onUserSelection()
{
	if (updating_)
		return;
	EditContext &ctx = EditContext::instance();
	std::vector<const ItemEntry *> list;
	for (QTreeWidgetItem *row : tree_->selectedItems()) {
		if (const ItemEntry *e =
			    ctx.find(row->data(kName, kIdRole).toLongLong(), row->data(kName, kGroupRole).toLongLong()))
			list.push_back(e);
	}
	updating_ = true;
	if (list.empty())
		ctx.clearSelection();
	else
		ctx.select(list, false);
	updating_ = false;
}

void LayersPanel::onClicked(QTreeWidgetItem *row, int column)
{
	EditContext &ctx = EditContext::instance();
	if (column == kType && !row->data(kName, kIsGroupRole).toBool()) {
		/* The type of the clicked source, or of every selected source when it is selected. */
		std::vector<obs_source_t *> sources;
		const QList<QTreeWidgetItem *> rows = row->isSelected() ? tree_->selectedItems()
									: QList<QTreeWidgetItem *>{row};
		for (QTreeWidgetItem *r : rows) {
			const ItemEntry *e =
				ctx.find(r->data(kName, kIdRole).toLongLong(), r->data(kName, kGroupRole).toLongLong());
			obs_source_t *src = e && !e->isGroup ? obs_sceneitem_get_source(e->item) : nullptr;
			if (src && std::find(sources.begin(), sources.end(), src) == sources.end())
				sources.push_back(src);
		}
		const QRect cell = tree_->visualItemRect(row);
		const int x = tree_->header()->sectionPosition(kType);
		chooseRole(sources, tree_->viewport()->mapToGlobal(QPoint(x, cell.bottom())), this);
		return;
	}
	if (column != kLock && column != kEye)
		return;
	const ItemEntry *e =
		ctx.find(row->data(kName, kIdRole).toLongLong(), row->data(kName, kGroupRole).toLongLong());
	if (!e || !ctx.canEdit() || !ctx.sceneSource())
		return;
	const bool lock = column == kLock;
	const bool before = lock ? e->locked : e->visible;
	OBSSceneItem item = e->item;
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "scene", obs_source_get_uuid(ctx.sceneSource()));
	obs_data_set_string(d, "group", groupName(*e).toUtf8().constData());
	obs_data_set_int(d, "id", e->id);
	obs_data_set_string(d, "source", obs_source_get_uuid(obs_sceneitem_get_source(item)));
	obs_data_set_string(d, "flag", lock ? "locked" : "visible");
	OBSDataAutoRelease after = obs_data_create();
	obs_data_apply(after, d);
	obs_data_set_bool(d, "value", before);
	obs_data_set_bool(after, "value", !before);
	if (lock)
		obs_sceneitem_set_locked(item, !before);
	else
		obs_sceneitem_set_visible(item, !before);
	const char *what = lock ? (before ? "Layers.Unlock" : "Layers.Lock") : (before ? "Layers.Hide" : "Layers.Show");
	addUndo(T(what), applyFlag, applyFlag, d, after);
	ctx.markDirty();
}

void LayersPanel::onRenamed(QTreeWidgetItem *row, int column)
{
	if (updating_ || column != kName)
		return;
	EditContext &ctx = EditContext::instance();
	const ItemEntry *e =
		ctx.find(row->data(kName, kIdRole).toLongLong(), row->data(kName, kGroupRole).toLongLong());
	if (!e)
		return;
	const QString name = row->text(kName).trimmed();
	OBSSourceAutoRelease taken = obs_get_source_by_name(name.toUtf8().constData());
	obs_source_t *source = obs_sceneitem_get_source(e->item);
	if (!name.isEmpty() && name != e->name && !taken) {
		OBSDataAutoRelease before = obs_data_create(), after = obs_data_create();
		obs_data_set_string(before, "source", obs_source_get_uuid(source));
		obs_data_set_string(before, "name", e->name.toUtf8().constData());
		obs_data_apply(after, before);
		obs_data_set_string(after, "name", name.toUtf8().constData());
		obs_source_set_name(source, name.toUtf8().constData());
		addUndo(T("Undo.Rename"), applyName, applyName, before, after);
	}
	ctx.markDirty(); /* names live in the snapshot */
	/* Refused or unchanged: the row shows the real name again. */
	signature_.clear();
	QMetaObject::invokeMethod(this, &LayersPanel::refresh, Qt::QueuedConnection);
}

void LayersPanel::reorder(QTreeWidgetItem *moved, QTreeWidgetItem *parentAfter, int indexAfter)
{
	EditContext &ctx = EditContext::instance();
	obs_scene_t *top = ctx.scene();
	/* Place the row (a drop has already done it). OBS has no nested groups. */
	QTreeWidgetItem *parentBefore = moved->parent();
	if (moved->data(kName, kIsGroupRole).toBool() && (parentAfter || parentBefore)) {
		signature_.clear(); /* puts the rows back */
		rebuild();
		syncSelection();
		return;
	}
	const int indexBefore = parentBefore ? parentBefore->indexOfChild(moved) : tree_->indexOfTopLevelItem(moved);
	if (parentBefore != parentAfter || indexBefore != indexAfter) {
		if (parentBefore)
			parentBefore->takeChild(indexBefore);
		else
			tree_->takeTopLevelItem(indexBefore);
		const int count = parentAfter ? parentAfter->childCount() : tree_->topLevelItemCount();
		const int at = std::clamp(indexAfter, 0, count);
		if (parentAfter)
			parentAfter->insertChild(at, moved);
		else
			tree_->insertTopLevelItem(at, moved);
	}

	if (top && ctx.canEdit()) {
		/* The scene, back to front, as the rows now show it (rows run front to back). */
		auto itemOf = [&ctx](QTreeWidgetItem *row) -> obs_sceneitem_t * {
			const ItemEntry *e = ctx.find(row->data(kName, kIdRole).toLongLong(),
						      row->data(kName, kGroupRole).toLongLong());
			return e ? e->item.Get() : nullptr;
		};
		OrderList order;
		bool complete = true;
		for (int i = tree_->topLevelItemCount() - 1; i >= 0; i--) {
			QTreeWidgetItem *row = tree_->topLevelItem(i);
			obs_sceneitem_t *item = itemOf(row);
			complete = complete && item;
			if (!item)
				continue;
			order.push_back({nullptr, item});
			for (int k = row->childCount() - 1; k >= 0; k--) {
				obs_sceneitem_t *child = itemOf(row->child(k));
				complete = complete && child;
				if (child)
					order.push_back({item, child});
			}
		}
		const OrderList before = currentOrder(top);
		if (complete && order.size() == before.size() && !sameOrder(order, before)) {
			bool regrouped = false;
			for (const obs_sceneitem_order_info &o : order) {
				for (const obs_sceneitem_order_info &b : before)
					regrouped = regrouped || (b.item == o.item && b.group != o.group);
			}
			OBSDataAutoRelease undoData = orderData(top, before);
			OBSDataAutoRelease redoData = orderData(top, order);
			obs_scene_reorder_items2(top, order.data(), order.size());
			addUndo(T(regrouped ? "Undo.Group" : "Undo.Order"), applyOrder, applyOrder, undoData, redoData);
		}
	}
	signature_.clear();
	ctx.markDirty();
	QMetaObject::invokeMethod(this, &LayersPanel::refresh, Qt::QueuedConnection);
}

} // namespace layouts::ui
