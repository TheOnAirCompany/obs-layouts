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

#include "theme.hpp"
#include "widgets.hpp"

#include "obs/edit-context.hpp"

#include <obs-frontend-api.h>

#include <QDropEvent>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace layouts::ui {

namespace {

enum Column { kName = 0, kLock = 1, kEye = 2 };
enum Role { kIdRole = Qt::UserRole, kGroupRole, kIsGroupRole };

QString kindIcon(obs_source_t *src)
{
	if (!src)
		return "square-dashed";
	if (obs_source_is_group(src))
		return "folder";
	if (obs_source_is_scene(src))
		return "clapperboard";
	const char *id = obs_source_get_unversioned_id(src);
	const uint32_t flags = obs_source_get_output_flags(src);
	if (!(flags & OBS_SOURCE_VIDEO))
		return "audio-lines";
	if (!id)
		return "square-dashed";
	auto has = [id](const char *part) {
		return strstr(id, part) != nullptr;
	};
	if (has("av_capture") || has("avcapture") || has("dshow") || has("v4l2") || has("decklink") ||
	    has("ndi_source") || has("aja"))
		return "video";
	if (has("display_capture") || has("monitor_capture") || has("screen_capture") || has("xshm") ||
	    has("desktop-capture"))
		return "monitor";
	if (has("window_capture") || has("game_capture") || has("window-capture"))
		return "app-window";
	if (has("image_source") || has("slideshow"))
		return "image";
	if (has("text_"))
		return "type";
	if (has("browser"))
		return "globe";
	if (has("ffmpeg_source") || has("vlc_source") || has("media"))
		return "film";
	if (has("color_source"))
		return "square";
	return "square-dashed";
}

/* ---- stacking order with undo: libobs transform states do not carry the order ---- */

obs_scene_t *orderScene(obs_sceneitem_t *item)
{
	return obs_sceneitem_get_scene(item); /* the scene, or the group's own scene */
}

/* {"scene": uuid of the top scene, "group": group name or "", "ids": bottom -> top} */
std::string orderState(obs_scene_t *top, const char *group, obs_scene_t *scene)
{
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "scene", obs_source_get_uuid(obs_scene_get_source(top)));
	obs_data_set_string(d, "group", group ? group : "");
	OBSDataArrayAutoRelease ids = obs_data_array_create();
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
			OBSDataAutoRelease e = obs_data_create();
			obs_data_set_int(e, "id", obs_sceneitem_get_id(item));
			obs_data_array_push_back((obs_data_array_t *)p, e);
			return true;
		},
		ids.Get());
	obs_data_set_array(d, "ids", ids);
	return obs_data_get_json(d);
}

void applyOrderState(const char *json)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json);
	OBSSourceAutoRelease src = obs_get_source_by_uuid(obs_data_get_string(d, "scene"));
	obs_scene_t *scene = obs_scene_from_source(src);
	if (!scene)
		return;
	const char *group = obs_data_get_string(d, "group");
	if (group && *group) {
		obs_sceneitem_t *g = obs_scene_get_group(scene, group);
		scene = g ? obs_sceneitem_group_get_scene(g) : nullptr;
		if (!scene)
			return;
	}
	OBSDataArrayAutoRelease ids = obs_data_get_array(d, "ids");
	std::vector<obs_sceneitem_t *> order;
	for (size_t i = 0; i < obs_data_array_count(ids); i++) {
		OBSDataAutoRelease e = obs_data_array_item(ids, i);
		if (obs_sceneitem_t *item = obs_scene_find_sceneitem_by_id(scene, obs_data_get_int(e, "id")))
			order.push_back(item);
	}
	if (!order.empty())
		obs_scene_reorder_items(scene, order.data(), order.size());
	EditContext::instance().markDirty();
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
		QTreeWidgetItem *parentBefore = moved->parent();
		QTreeWidget::dropEvent(e);
		/* Qt has moved the row: keep it only inside its own scene or group. */
		QTreeWidgetItem *parentAfter = moved->parent();
		if (parentAfter != parentBefore) {
			panel_->signature_.clear();
			panel_->rebuild();
			return;
		}
		const int index = parentAfter ? parentAfter->indexOfChild(moved) : indexOfTopLevelItem(moved);
		panel_->reorder(moved, parentAfter, index);
	}

private:
	LayersPanel *panel_;
};

LayersPanel::LayersPanel(QWidget *parent) : QWidget(parent)
{
	setObjectName("LayoutsLayersPanel");
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(tokens::kPanelMargin, tokens::kPanelMargin, 0, tokens::kPanelMargin);
	v->setSpacing(tokens::kRowSpacing);

	v->addWidget(sectionTitle(T("Layers.Title"), this));

	tree_ = new LayerTree(this);
	tree_->setColumnCount(3);
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
		sig += QString("|%1:%2:%3:%4:%5").arg(e.groupId).arg(e.id).arg(e.visible).arg(e.locked).arg(e.name);
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
	dim.setAlphaF(0.35);
	tree_->clear();
	std::map<int64_t, QTreeWidgetItem *> groups;
	const std::vector<ItemEntry> &items = EditContext::instance().items();
	/* items are in render order (bottom first): prepend so the front is on top. */
	for (const ItemEntry &e : items) {
		auto *row = new QTreeWidgetItem();
		row->setText(kName, e.name);
		row->setData(kName, kIdRole, (qlonglong)e.id);
		row->setData(kName, kGroupRole, (qlonglong)e.groupId);
		row->setData(kName, kIsGroupRole, e.isGroup);
		row->setIcon(kName, icon(kindIcon(obs_sceneitem_get_source(e.item)), e.visible ? text : dim));
		row->setIcon(kLock, icon(e.locked ? "lock" : "lock-open", e.locked ? text : dim));
		row->setIcon(kEye, icon(e.visible ? "eye" : "eye-off", e.visible ? dim : text));
		row->setToolTip(kLock, T(e.locked ? "Layers.Unlock" : "Layers.Lock"));
		row->setToolTip(kEye, T(e.visible ? "Layers.Hide" : "Layers.Show"));
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
	updating_ = false;
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
	if (column != kLock && column != kEye)
		return;
	EditContext &ctx = EditContext::instance();
	const ItemEntry *e =
		ctx.find(row->data(kName, kIdRole).toLongLong(), row->data(kName, kGroupRole).toLongLong());
	if (!e || !ctx.canEdit())
		return;
	if (column == kLock)
		obs_sceneitem_set_locked(e->item, !e->locked);
	else
		obs_sceneitem_set_visible(e->item, !e->visible);
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
	if (!name.isEmpty() && name != e->name && !taken)
		obs_source_set_name(obs_sceneitem_get_source(e->item), name.toUtf8().constData());
	/* Refused or unchanged: the row shows the real name again. */
	signature_.clear();
	QMetaObject::invokeMethod(this, &LayersPanel::refresh, Qt::QueuedConnection);
}

void LayersPanel::reorder(QTreeWidgetItem *moved, QTreeWidgetItem *parentAfter, int indexAfter)
{
	EditContext &ctx = EditContext::instance();
	const ItemEntry *e =
		ctx.find(moved->data(kName, kIdRole).toLongLong(), moved->data(kName, kGroupRole).toLongLong());
	const int siblings = parentAfter ? parentAfter->childCount() : tree_->topLevelItemCount();
	if (e && ctx.canEdit() && ctx.scene()) {
		obs_scene_t *top = ctx.scene();
		obs_scene_t *scene = orderScene(e->item);
		const char *group =
			e->groupId != 0 && e->group ? obs_source_get_name(obs_sceneitem_get_source(e->group)) : nullptr;
		const std::string before = orderState(top, group, scene);
		/* Rows run front to back; OBS positions count from the back. */
		obs_sceneitem_set_order_position(e->item, siblings - 1 - indexAfter);
		const std::string after = orderState(top, group, scene);
		if (after != before) {
			const QString name = T("Undo.Format").arg(T("Undo.Order"), ctx.sceneName());
			obs_frontend_add_undo_redo_action(name.toUtf8().constData(), applyOrderState, applyOrderState,
							  before.c_str(), after.c_str(), false);
		}
	}
	signature_.clear();
	ctx.markDirty();
	QMetaObject::invokeMethod(this, &LayersPanel::refresh, Qt::QueuedConnection);
}

} // namespace layouts::ui
