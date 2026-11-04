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

/* The scene being edited and a UI-thread snapshot of its items.
 *
 * By default Layouts follows OBS: the preview scene in Studio Mode, the program
 * scene otherwise. The Canvas dock can pin any scene, including scenes of other
 * OBS 33 canvases. libobs signals (any thread) only set a dirty flag and post a
 * coalesced refresh to the UI thread; the snapshot is rebuilt lazily. */

#include "core/item-model.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>
#include <vector>

namespace layouts {

struct ItemEntry {
	OBSSceneItem item;
	OBSSceneItem group; /* containing group, null for top-level items */
	int64_t id = 0;
	int64_t groupId = 0;
	core::ItemModel model;
	core::Affine parent; /* parent space -> canvas */
	core::VisualFrame frame;
	bool locked = false;
	bool visible = true;
	bool selected = false;
	bool isGroup = false;
	QString name;
	QString typeName;
};

enum class LiveState {
	Off,     /* not visible in the output */
	Program, /* shown on the program output, no output running */
	OnAir,   /* shown on the program while streaming, recording, replay buffer or virtual camera runs */
};

class EditContext : public QObject {
	Q_OBJECT

public:
	static EditContext &instance();

	/* ---- scene ---- */
	obs_source_t *sceneSource() const { return scene_; }
	obs_scene_t *scene() const;
	QString sceneName() const;
	QString sceneUuid() const;
	bool following() const { return following_; }
	void follow();                       /* follow OBS (preview/program) */
	void pin(obs_source_t *sceneSource); /* edit this scene */
	uint32_t canvasWidth() const;
	uint32_t canvasHeight() const;
	core::Rect canvasRect() const { return {0, 0, (double)canvasWidth(), (double)canvasHeight()}; }
	/* True when the canvas can show the main output texture instead of rendering the
	 * scene again (no Studio Mode, scene is the program scene of the main canvas). */
	bool isProgramOfMainCanvas() const;

	/* ---- items ---- */
	const std::vector<ItemEntry> &items(); /* render order: bottom first, group children after their group */
	const ItemEntry *find(int64_t id, int64_t groupId);
	std::vector<const ItemEntry *> selection(); /* in selection order */
	const ItemEntry *topmostAt(const core::Vec2 &world, bool deep);

	void select(const std::vector<const ItemEntry *> &items, bool additive);
	void toggle(const ItemEntry *item);
	void clearSelection();

	/* ---- live safety ---- */
	LiveState liveState() const;
	/* False when edits are refused (block-on-air policy); shows the reason. */
	bool canEdit();

	/* Marks the snapshot dirty (called from any thread). */
	void markDirty();

	/* Ghost frames shown on the Canvas before an operation is applied (presets,
	 * auto layout). World space. Empty = no preview. */
	void setPreview(const std::vector<core::Quad> &quads);
	const std::vector<core::Quad> &preview() const { return preview_; }

	void handleFrontendEvent(enum obs_frontend_event event);
	void shutdown();

signals:
	void sceneChanged();
	void itemsChanged(); /* transforms, selection, list */
	void liveStateChanged();
	void editRefused(const QString &reason);

private slots:
	void refresh();

private:
	EditContext();
	void setScene(obs_source_t *source);
	void connectSignals();
	void disconnectSignals();
	void rebuild();
	void retire();
	obs_source_t *followedScene() const;

	OBSSource scene_;
	bool following_ = true;
	/* The snapshot is replaced, never mutated: pointers handed out during a call
	 * stay valid until control returns to the event loop, even if a libobs signal
	 * marks the snapshot dirty meanwhile. */
	std::shared_ptr<std::vector<ItemEntry>> items_ = std::make_shared<std::vector<ItemEntry>>();
	std::vector<std::shared_ptr<std::vector<ItemEntry>>> retired_;
	bool releasePosted_ = false;
	std::vector<core::Quad> preview_;
	std::vector<std::pair<int64_t, int64_t>> selectionOrder_; /* (groupId, id): ids are per scene */
	std::atomic_bool dirty_{true};
	std::atomic_bool posted_{false};
	std::vector<std::unique_ptr<OBSSignal>> signals_;
	std::vector<OBSSource> groupScenes_;
	QTimer liveTimer_;
	LiveState lastLive_ = LiveState::Off;
};

} // namespace layouts
