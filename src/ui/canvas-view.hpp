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

/* Layouts Canvas: an editing surface with its own obs_display.
 *
 * The edited scene is drawn with public libobs calls only (like an OBS projector):
 * the main output texture when the scene is the program scene of the main canvas,
 * obs_source_video_render() otherwise. Rulers, guides, grid, selection, snapping
 * feedback and measurements are drawn in the display's draw callback, so they can
 * never reach the video output. The native OBS preview is left untouched. */

#include "core/snapping.hpp"
#include "obs/edit-context.hpp"
#include "obs/edit-session.hpp"

#include <obs.hpp>

#include <QHash>
#include <QImage>
#include <QPointer>
#include <QWidget>

#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace layouts::ui {

struct OverlayPrim {
	enum class Kind { Rect, Line, RectOutline } kind;
	float x0, y0, x1, y1; /* device px; Rect: x0,y0 = top-left, x1,y1 = size */
	float thickness;
	uint32_t argb;
};

struct OverlayLabel {
	QString key;
	QImage image; /* premultiplied ARGB32, device px */
	float x, y;   /* device px, top-left */
};

struct RenderSnapshot {
	OBSWeakSource scene;
	bool mainTexture = false;
	double canvasW = 0, canvasH = 0;
	double zoom = 1;           /* device px per canvas px */
	double panX = 0, panY = 0; /* device px of the canvas origin */
	uint32_t outside = 0;
	std::vector<OverlayPrim> prims;
	std::vector<OverlayLabel> labels;
};

class CanvasView : public QWidget {
	Q_OBJECT

public:
	explicit CanvasView(QWidget *parent = nullptr);
	~CanvasView() override;

	/* ---- view ---- */
	double zoom() const { return zoom_; } /* logical px per canvas px */
	QPointF worldToView(const core::Vec2 &w) const;
	core::Vec2 viewToWorld(const QPointF &p) const;
	void zoomAt(double z, const QPointF &anchor);
	void zoomIn();
	void zoomOut();
	void zoomFit();
	void zoom100();
	void zoomSelection();

	core::Vec2 cursorWorld() const { return cursorWorld_; }
	bool cursorInside() const { return cursorInside_; }
	/* Selection range on each axis (for the rulers), empty if none. */
	std::optional<core::Rect> selectionBounds();

	/* ---- guides created from the rulers ---- */
	void beginGuideDrag(core::Axis axis, const QPoint &globalPos);
	void guideDragMove(const QPoint &globalPos);
	void guideDragEnd(const QPoint &globalPos);
	std::optional<std::pair<core::Axis, double>> draggedGuide() const;

	QPaintEngine *paintEngine() const override { return nullptr; }

	/* For the in-OBS self-test / performance probe. */
	uint64_t lastRenderNs() const { return lastRenderNs_.load(); }
	uint64_t frames() const { return frames_.load(); }

signals:
	void viewChanged();
	void cursorChanged();

public slots:
	void rebuildOverlay();

protected:
	void paintEvent(QPaintEvent *) override;
	void resizeEvent(QResizeEvent *) override;
	void moveEvent(QMoveEvent *) override;
	void showEvent(QShowEvent *) override;
	void hideEvent(QHideEvent *) override;
	bool event(QEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void keyReleaseEvent(QKeyEvent *e) override;
	void leaveEvent(QEvent *) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

private:
	enum class Mode { Idle, Pan, Rubber, Move, Resize, ResizeMulti, Rotate, Guide };

	void createDisplay();
	void destroyDisplay();
	void updateDisplayEnabled();
	void setShowing(obs_source_t *source);
	static void drawCallback(void *param, uint32_t cx, uint32_t cy);
	void render(uint32_t cx, uint32_t cy);

	void onSceneChanged();
	void applyViewChange();
	double dpr() const;

	/* hit testing (view coordinates) */
	int handleAt(const QPointF &p, const ItemEntry &e) const; /* -1 none, 0..7 handles, 8 rotate */
	std::optional<uint64_t> guideAt(const QPointF &p) const;
	const ItemEntry *singleEditable();
	/* Bounds of a multi-selection that can be resized (2+ items, one unlocked). */
	std::optional<core::Rect> multiResizable();
	int handleAtRect(const QPointF &p, const core::Rect &r) const; /* -1 none, 0..7 */
	void startResizeMulti();
	void updateResizeMulti(const QPointF &p, Qt::KeyboardModifiers mods);
	void updateHoverCursor(const QPointF &p);

	/* interactions */
	void startMove(const QPointF &p);
	void updateMove(const QPointF &p, Qt::KeyboardModifiers mods);
	void updateResize(const QPointF &p, Qt::KeyboardModifiers mods);
	/* The inspector ratio lock, inverted while Shift is held. */
	static bool resizeKeepsRatio(Qt::KeyboardModifiers mods);
	bool refreshResizeForModifier(QKeyEvent *e);
	void updateCrop(const QPointF &p);
	bool canCrop();
	void updateRotate(const QPointF &p, Qt::KeyboardModifiers mods);
	void updateGuide(const QPointF &p, Qt::KeyboardModifiers mods);
	void finishGuide(const QPointF &p);
	void updateRubber(const QPointF &p);
	void cancelInteraction();
	core::SnapContext snapContext(bool excludeSelection);

	QImage labelImage(const QString &text, QRgb background);
	void addLabel(RenderSnapshot &s, const QString &text, QRgb bg, float cx, float cy, bool above);

	/* display */
	obs_display_t *display_ = nullptr;
	bool destroying_ = false;
	OBSSource showing_;
	std::mutex snapMutex_;
	std::shared_ptr<const RenderSnapshot> snapshot_;
	gs_vertbuffer_t *unitQuad_ = nullptr;
	std::unordered_map<std::string, std::pair<gs_texture_t *, uint64_t>> textures_; /* graphics thread */
	std::atomic<uint64_t> lastRenderNs_{0};
	std::atomic<uint64_t> frames_{0};
	QHash<QString, QImage> labelCache_;

	/* view */
	double zoom_ = 0.5;
	QPointF pan_{20, 20};
	bool autoFit_ = true;
	uint32_t lastCanvasW_ = 0, lastCanvasH_ = 0;

	/* interaction state */
	Mode mode_ = Mode::Idle;
	bool dragStarted_ = false;
	QPointF pressView_;
	core::Vec2 pressWorld_;
	std::vector<ItemEntry> dragItems_;
	core::Rect dragUnion_;
	std::unique_ptr<EditSession> session_;
	int handle_ = -1;
	bool resizeLocked_ = false; /* the running resize keeps the ratio (badge) */
	double rotStartAngle_ = 0, rotStartRot_ = 0;
	core::Vec2 rotCenter_;
	uint64_t guideId_ = 0;
	bool guideIsNew_ = false;
	core::Axis guideAxis_ = core::Axis::X;
	double guidePos_ = 0;
	bool guideOutside_ = false;
	core::Vec2 rubberEnd_;
	bool rubberAdditive_ = false;
	std::vector<std::pair<int64_t, int64_t>> rubberBase_;
	core::SnapResult snap_;
	/* Swap (0.5): the item pressed, and the item it would swap with on release. A locked
	 * item does not move but can still be dropped on another slot. */
	OBSSceneItem swapSource_;
	OBSSceneItem swapTarget_;
	bool swapOnly_ = false;
	void updateSwapTarget(const core::Vec2 &world, Qt::KeyboardModifiers mods);
	std::optional<core::SnapLine> valueSnap_;
	std::pair<int64_t, int64_t> hover_{-1, -1};
	bool spaceHeld_ = false;
	bool altHeld_ = false;
	bool cropMode_ = false; /* C: handles crop the single selected item */
	bool panning_ = false;
	QPointF panStart_;
	QPointF panOrigin_;
	core::Vec2 cursorWorld_;
	bool cursorInside_ = false;
};

} // namespace layouts::ui
