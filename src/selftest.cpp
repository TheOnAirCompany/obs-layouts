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

/* In-OBS self-test. Compiled only with -DLAYOUTS_SELFTEST=ON and run only when the
 * LAYOUTS_SELFTEST environment variable is set. It creates disposable scene
 * collections, drives the real code against the real libobs / frontend API and
 * logs "[selftest] PASS|FAIL <name> <detail>". Never part of a release build.
 * Test-only shortcuts (finding the Undo action in the main window) live here and
 * nowhere else. */

#include "core/expression.hpp"
#include "obs/commands.hpp"
#include "obs/constraints.hpp"
#include "obs/edit-session.hpp"
#include "obs/corners.hpp"
#include "obs/regie.hpp"
#include "obs/item-style.hpp"
#include "obs/builder.hpp"
#include "obs/roles.hpp"
#include "obs/edit-context.hpp"
#include "obs/hotkeys.hpp"
#include "obs/obs-convert.hpp"
#include "obs/store.hpp"
#include "ui/broadcast-gallery.hpp"
#include "ui/builder-panel.hpp"
#include "ui/canvas-dock.hpp"
#include "ui/canvas-view.hpp"
#include "ui/inspector-dock.hpp"
#include "ui/layers-panel.hpp"
#include "ui/layouts-window.hpp"
#include "ui/role-ui.hpp"
#include "ui/scenes-list.hpp"
#include "ui/ruler.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QKeyEvent>
#include <QListWidget>
#include <QTreeWidget>
#include <QMouseEvent>
#include <QTimer>

#include <cstring>
#include <functional>
#include <random>
#include <vector>

namespace layouts {

namespace {

struct Runner {
	std::vector<std::pair<int, std::function<void()>>> steps;
	size_t next = 0;
	int pass = 0;
	int fail = 0;

	void check(bool ok, const char *name, const QString &detail = QString())
	{
		(ok ? pass : fail)++;
		obs_log(ok ? LOG_INFO : LOG_WARNING, "[selftest] %s %s %s", ok ? "PASS" : "FAIL", name,
			detail.toUtf8().constData());
	}
	void add(int delayMs, std::function<void()> fn) { steps.emplace_back(delayMs, std::move(fn)); }
	std::function<void()> onDone; /* drops the references the tests keep, while OBS runs */
	void run()
	{
		if (next >= steps.size()) {
			if (onDone)
				onDone();
			obs_log(LOG_INFO, "[selftest] DONE pass=%d fail=%d", pass, fail);
			if (getenv("LAYOUTS_SELFTEST_QUIT"))
				QTimer::singleShot(500, [] {
					static_cast<QMainWindow *>(obs_frontend_get_main_window())->close();
				});
			return;
		}
		auto &s = steps[next++];
		QTimer::singleShot(s.first, [this, fn = s.second] {
			fn();
			run();
		});
	}
};

Runner *R = nullptr;
ui::CanvasDock *g_dock = nullptr;
ui::LayoutsWindow *g_window = nullptr;

OBSSourceAutoRelease colorSource(const char *name, uint32_t w, uint32_t h, uint32_t abgr)
{
	OBSDataAutoRelease s = obs_data_create();
	obs_data_set_int(s, "width", w);
	obs_data_set_int(s, "height", h);
	obs_data_set_int(s, "color", abgr);
	return obs_source_create("color_source", name, s, nullptr);
}

obs_sceneitem_t *addColor(obs_scene_t *scene, const char *name, uint32_t w, uint32_t h, uint32_t abgr, double x,
			  double y)
{
	OBSSourceAutoRelease src = colorSource(name, w, h, abgr);
	obs_sceneitem_t *item = obs_scene_add(scene, src);
	vec2 p;
	vec2_set(&p, (float)x, (float)y);
	obs_sceneitem_set_pos(item, &p);
	return item;
}

const ItemEntry *entry(const char *name)
{
	for (const ItemEntry &e : EditContext::instance().items()) {
		if (e.name == name)
			return &e;
	}
	return nullptr;
}

core::Rect box(const char *name)
{
	const ItemEntry *e = entry(name);
	return e ? e->frame.quad.bounds() : core::Rect();
}

void selectNames(std::initializer_list<const char *> names)
{
	std::vector<const ItemEntry *> list;
	for (const char *n : names) {
		if (const ItemEntry *e = entry(n))
			list.push_back(e);
	}
	EditContext::instance().select(list, false);
}

QAction *findAction(const QKeySequence &seq)
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	for (QAction *a : main->findChildren<QAction *>()) {
		if (a->shortcut() == seq)
			return a;
	}
	return nullptr;
}

void resetItem(const char *name, double x, double y)
{
	const ItemEntry *e = entry(name);
	if (!e)
		return;
	obs_transform_info info = {};
	obs_sceneitem_get_info2(e->item, &info);
	vec2_set(&info.pos, (float)x, (float)y);
	vec2_set(&info.scale, 1, 1);
	info.rot = 0;
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_type = OBS_BOUNDS_NONE;
	obs_sceneitem_set_info2(e->item, &info);
	obs_sceneitem_crop c = {};
	obs_sceneitem_set_crop(e->item, &c);
	EditContext::instance().markDirty();
}

core::Expression expr(const char *s, double base = 1920)
{
	return core::parseExpression(s, base);
}

QString rectStr(const core::Rect &r)
{
	return QString("(%1,%2 %3x%4)").arg(r.x).arg(r.y).arg(r.w).arg(r.h);
}

void sendMouse(QWidget *w, QEvent::Type type, const QPointF &local, Qt::MouseButtons buttons,
	       Qt::KeyboardModifiers mods = Qt::NoModifier)
{
	const QPointF global = w->mapToGlobal(local);
	QMouseEvent e(type, local, global, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, buttons, mods);
	QApplication::sendEvent(w, &e);
}

/* ------------------------------------------------------------------ tests */

void testGeometryFidelity()
{
	obs_scene_t *scene = EditContext::instance().scene();
	obs_sceneitem_t *item = addColor(scene, "Geo", 640, 360, 0xFF3366CC, 0, 0);

	struct Case {
		const char *name;
		float px, py, sx, sy, rot;
		uint32_t align;
		obs_bounds_type bt;
		float bx, by;
		uint32_t balign;
		bool cropBounds;
		obs_sceneitem_crop crop;
	};
	const Case cases[] = {
		{"plain", 100, 50, 0.5f, 0.5f, 0, OBS_ALIGN_LEFT | OBS_ALIGN_TOP, OBS_BOUNDS_NONE, 0, 0, 0, false, {}},
		{"rot-center", 960, 540, 1.2f, 1.2f, 33, OBS_ALIGN_CENTER, OBS_BOUNDS_NONE, 0, 0, 0, false, {}},
		{"flip-br",
		 800,
		 600,
		 -1.0f,
		 0.7f,
		 0,
		 OBS_ALIGN_RIGHT | OBS_ALIGN_BOTTOM,
		 OBS_BOUNDS_NONE,
		 0,
		 0,
		 0,
		 false,
		 {}},
		{"crop",
		 10,
		 20,
		 1.3f,
		 1.3f,
		 0,
		 OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		 OBS_BOUNDS_NONE,
		 0,
		 0,
		 0,
		 false,
		 {30, 5, 10, 7}},
		{"inner",
		 300,
		 300,
		 1,
		 1,
		 0,
		 OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		 OBS_BOUNDS_SCALE_INNER,
		 500,
		 500,
		 OBS_ALIGN_CENTER,
		 false,
		 {}},
		{"outer-crop",
		 300,
		 300,
		 1,
		 1,
		 0,
		 OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		 OBS_BOUNDS_SCALE_OUTER,
		 500,
		 300,
		 OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		 true,
		 {}},
		{"stretch-flip",
		 400,
		 100,
		 -1,
		 1,
		 0,
		 OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		 OBS_BOUNDS_STRETCH,
		 640,
		 360,
		 OBS_ALIGN_CENTER,
		 false,
		 {}},
		{"width-rot",
		 900,
		 500,
		 1,
		 1,
		 -45,
		 OBS_ALIGN_CENTER,
		 OBS_BOUNDS_SCALE_TO_WIDTH,
		 800,
		 100,
		 OBS_ALIGN_CENTER,
		 false,
		 {}},
		{"maxonly",
		 50,
		 50,
		 1,
		 1,
		 0,
		 OBS_ALIGN_LEFT | OBS_ALIGN_TOP,
		 OBS_BOUNDS_MAX_ONLY,
		 100,
		 100,
		 OBS_ALIGN_CENTER,
		 false,
		 {}},
		{"flip-both-180",
		 700,
		 400,
		 2,
		 -2,
		 180,
		 OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM,
		 OBS_BOUNDS_NONE,
		 0,
		 0,
		 0,
		 false,
		 {}},
	};

	for (const Case &c : cases) {
		obs_transform_info info = {};
		vec2_set(&info.pos, c.px, c.py);
		vec2_set(&info.scale, c.sx, c.sy);
		info.rot = c.rot;
		info.alignment = c.align;
		info.bounds_type = c.bt;
		vec2_set(&info.bounds, c.bx, c.by);
		info.bounds_alignment = c.balign;
		info.crop_to_bounds = c.cropBounds;
		obs_sceneitem_set_info2(item, &info);
		obs_sceneitem_set_crop(item, &c.crop);
		/* set_crop defers the matrix update to the next tick: apply it now. */
		obs_sceneitem_force_update_transform(item);

		matrix4 obsBox, obsDraw;
		obs_sceneitem_get_box_transform(item, &obsBox);
		obs_sceneitem_get_draw_transform(item, &obsDraw);

		obs_transform_info back = {};
		obs_sceneitem_crop crop = {};
		obs_sceneitem_get_info2(item, &back);
		obs_sceneitem_get_crop(item, &crop);
		core::ItemModel m;
		m.t = fromObs(back, crop);
		m.sourceWidth = 640;
		m.sourceHeight = 360;
		const core::ItemGeometry g = core::computeGeometry(m);
		const core::Affine ob = fromMatrix(obsBox), od = fromMatrix(obsDraw);

		double err = 0;
		for (core::Vec2 u : {core::Vec2{0, 0}, core::Vec2{1, 0}, core::Vec2{1, 1}, core::Vec2{0, 1}}) {
			err = std::max(err, (ob.map(u) - g.box.map(u)).length());
			const core::Vec2 px{u.x * 640, u.y * 360};
			err = std::max(err, (od.map(px) - g.draw.map(px)).length());
		}
		R->check(err < 0.01, "geometry-fidelity", QString("%1 max error %2 px").arg(c.name).arg(err));
	}
	obs_sceneitem_remove(item);
}

void setupScene()
{
	obs_scene_t *scene = EditContext::instance().scene();
	addColor(scene, "A", 400, 200, 0xFF4444FF, 100, 100);
	addColor(scene, "B", 300, 300, 0xFF44FF44, 700, 400);
	addColor(scene, "C", 200, 100, 0xFFFF4444, 1300, 250);
	obs_sceneitem_t *d = addColor(scene, "D", 100, 100, 0xFF00FFFF, 1500, 700);
	obs_sceneitem_t *e = addColor(scene, "E", 120, 80, 0xFFFF00FF, 1700, 800);
	obs_sceneitem_t *group = obs_scene_add_group2(scene, "G", true);
	obs_sceneitem_group_add_item(group, d);
	obs_sceneitem_group_add_item(group, e);
	EditContext::instance().markDirty();
}

void testCommands()
{
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	resetItem("C", 1300, 250);
	EditContext &ctx = EditContext::instance();
	ctx.markDirty();
	ctx.items();

	selectNames({"A", "B", "C"});
	R->check(ctx.selection().size() == 3, "select-three", QString::number(ctx.selection().size()));

	commands::setAlignTarget(core::AlignTarget::Selection);
	commands::align(core::AlignEdge::Left);
	ctx.markDirty();
	R->check(box("A").x == 100 && box("B").x == 100 && box("C").x == 100, "align-left",
		 rectStr(box("A")) + rectStr(box("B")) + rectStr(box("C")));

	commands::align(core::AlignEdge::Bottom);
	ctx.markDirty();
	const double bottom = box("B").bottom();
	R->check(box("A").bottom() == bottom && box("C").bottom() == bottom, "align-bottom", QString::number(bottom));

	/* Spread horizontally then distribute spacing. */
	resetItem("A", 0, 100);
	resetItem("B", 450, 400);
	resetItem("C", 1500, 250);
	ctx.markDirty();
	selectNames({"A", "B", "C"});
	commands::distribute(core::Axis::X, true);
	ctx.markDirty();
	const double g1 = box("B").left() - box("A").right();
	const double g2 = box("C").left() - box("B").right();
	R->check(std::fabs(g1 - g2) < 0.6, "distribute-spacing", QString("%1 vs %2").arg(g1).arg(g2));

	commands::spaceExactly(core::Axis::X, 24);
	ctx.markDirty();
	R->check(std::fabs(box("B").left() - box("A").right() - 24) < 0.6 &&
			 std::fabs(box("C").left() - box("B").right() - 24) < 0.6,
		 "space-exactly-24", rectStr(box("A")) + rectStr(box("B")) + rectStr(box("C")));

	commands::setAlignTarget(core::AlignTarget::KeyObject);
	if (const ItemEntry *b = entry("B"))
		commands::setKeyItem(b->groupId, b->id);
	commands::sameSize(true, true);
	ctx.markDirty();
	R->check(std::fabs(box("A").w - 300) < 0.6 && std::fabs(box("C").h - 300) < 0.6, "same-size-key",
		 rectStr(box("A")) + rectStr(box("C")));
	commands::setAlignTarget(core::AlignTarget::Selection);

	/* Field edits with expressions. */
	selectNames({"A"});
	Store::instance().settings().refPoint = 0;
	commands::setField(commands::Field::X, expr("1920 / 2"));
	ctx.markDirty();
	R->check(box("A").x == 960, "field-x-expression", rectStr(box("A")));
	commands::setField(commands::Field::X, expr("+20"));
	ctx.markDirty();
	R->check(box("A").x == 980, "field-x-relative", rectStr(box("A")));
	Store::instance().settings().refPoint = 8; /* bottom right */
	commands::setField(commands::Field::X, expr("1920"));
	ctx.markDirty();
	R->check(std::fabs(box("A").right() - 1920) < 0.6, "field-x-refpoint", rectStr(box("A")));
	Store::instance().settings().refPoint = 0;

	Store::instance().settings().ratioLock = true;
	const double ratio = box("A").w / box("A").h;
	commands::setField(commands::Field::W, expr("600"));
	ctx.markDirty();
	R->check(std::fabs(box("A").w - 600) < 0.6 && std::fabs(box("A").w / box("A").h - ratio) < 0.01,
		 "field-w-ratio", rectStr(box("A")));
	Store::instance().settings().ratioLock = false;

	resetItem("A", 100, 100);
	ctx.markDirty();
	selectNames({"A"});
	if (!entry("A"))
		return;
	const core::Vec2 c0 = entry("A")->frame.center();
	commands::rotateBy(90);
	ctx.markDirty();
	const core::Vec2 c1 = entry("A") ? entry("A")->frame.center() : core::Vec2();
	R->check(entry("A") && (c0 - c1).length() < 0.6 && std::fabs(entry("A")->frame.rotation - 90) < 0.01,
		 "rotate-90-center", QString("%1,%2 -> %3,%4").arg(c0.x).arg(c0.y).arg(c1.x).arg(c1.y));

	resetItem("A", 100, 100);
	ctx.markDirty();
	selectNames({"A"});
	commands::setField(commands::Field::CropLeft, expr("50"));
	ctx.markDirty();
	R->check(box("A").x == 150 && box("A").w == 350, "crop-keeps-content", rectStr(box("A")));

	resetItem("A", 100, 100);
	ctx.markDirty();
	selectNames({"A"});
	commands::fit(core::FitMode::Fit);
	ctx.markDirty();
	R->check(std::fabs(box("A").w - 1920) < 0.6 && std::fabs(box("A").centerY() - 540) < 0.6, "fit-canvas",
		 rectStr(box("A")));

	/* Copy / paste position and size from B to C. */
	resetItem("A", 100, 100);
	ctx.markDirty();
	selectNames({"B"});
	commands::copyProperties();
	selectNames({"C"});
	commands::pasteProperties(commands::PasteSize | commands::PastePosition);
	ctx.markDirty();
	R->check(box("C") == box("B"), "paste-properties", rectStr(box("B")) + rectStr(box("C")));
	resetItem("C", 1300, 250);
	ctx.markDirty();
}

void testUndo()
{
	EditContext &ctx = EditContext::instance();
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	ctx.markDirty();
	selectNames({"A", "B"});
	commands::align(core::AlignEdge::Right);
	ctx.markDirty();
	const core::Rect after = box("A");
	QAction *undo = findAction(QKeySequence(QKeySequence::Undo));
	QAction *redo = findAction(QKeySequence(QKeySequence::Redo));
	R->check(undo && redo, "undo-actions-found");
	if (!undo || !redo)
		return;
	undo->trigger();
	ctx.markDirty();
	R->check(box("A").x == 100, "undo-restores", rectStr(box("A")));
	redo->trigger();
	ctx.markDirty();
	R->check(box("A") == after, "redo-reapplies", rectStr(box("A")));

	/* A whole drag is one undo entry: drive the canvas with mouse events. */
	resetItem("A", 100, 100);
	ctx.markDirty();
	selectNames({"A"});
	ui::CanvasView *v = g_dock->view();
	const QPointF start = v->worldToView(box("A").center());
	sendMouse(v, QEvent::MouseButtonPress, start, Qt::LeftButton);
	for (int i = 1; i <= 10; i++)
		sendMouse(v, QEvent::MouseMove, start + QPointF(i * 10, 0), Qt::LeftButton, Qt::ControlModifier);
	sendMouse(v, QEvent::MouseButtonRelease, start + QPointF(100, 0), Qt::NoButton, Qt::ControlModifier);
	ctx.markDirty();
	const double moved = box("A").x;
	R->check(moved > 100, "canvas-drag-moves", QString::number(moved));
	undo->trigger();
	ctx.markDirty();
	R->check(box("A").x == 100, "canvas-drag-single-undo", QString::number(box("A").x));
}

void testCanvasSnapAndRuler()
{
	EditContext &ctx = EditContext::instance();
	ui::CanvasView *v = g_dock->view();
	Store &store = Store::instance();
	resetItem("A", 100, 100);
	ctx.markDirty();
	store.guides().clearScene(ctx.sceneUuid().toStdString());
	/* Guide 3 canvas px to the right of where A's right edge lands after the drag. */
	const double target = 100 + 400 + 200 + 3;
	store.guides().add(ctx.sceneUuid().toStdString(), ctx.sceneName().toStdString(), core::Axis::X, target, false);
	store.settings().snap.enabled = true;
	store.settings().snap.guides = true;
	store.notifyGuidesChanged();
	selectNames({"A"});

	const QPointF start = v->worldToView(box("A").center());
	const double dx = 200 * v->zoom();
	sendMouse(v, QEvent::MouseButtonPress, start, Qt::LeftButton);
	for (int i = 1; i <= 10; i++)
		sendMouse(v, QEvent::MouseMove, start + QPointF(dx * i / 10.0, 0), Qt::LeftButton);
	sendMouse(v, QEvent::MouseButtonRelease, start + QPointF(dx, 0), Qt::NoButton);
	ctx.markDirty();
	R->check(std::fabs(box("A").right() - target) < 0.01, "canvas-snap-to-guide",
		 QString("right %1 target %2 zoom %3").arg(box("A").right()).arg(target).arg(v->zoom()));

	/* Drag from the top ruler into the canvas: creates a horizontal guide. */
	const size_t before = store.guides().sceneGuides(ctx.sceneUuid().toStdString()).size();
	ui::RulerWidget *ruler = nullptr;
	for (ui::RulerWidget *r : g_dock->findChildren<ui::RulerWidget *>()) {
		if (r->height() < r->width())
			ruler = r;
	}
	if (ruler) {
		const QPointF rp(ruler->width() / 2.0, ruler->height() / 2.0);
		sendMouse(ruler, QEvent::MouseButtonPress, rp, Qt::LeftButton);
		const QPointF inView =
			ruler->mapFromGlobal(v->mapToGlobal(QPointF(v->width() / 2.0, v->height() / 2.0)));
		sendMouse(ruler, QEvent::MouseMove, inView, Qt::LeftButton, Qt::ControlModifier);
		sendMouse(ruler, QEvent::MouseButtonRelease, inView, Qt::NoButton, Qt::ControlModifier);
	}
	const size_t afterCount = store.guides().sceneGuides(ctx.sceneUuid().toStdString()).size();
	R->check(ruler && afterCount == before + 1, "ruler-drag-creates-guide",
		 QString("%1 -> %2").arg(before).arg(afterCount));

	/* Guide edits are one OBS undo entry each. */
	QAction *undo = findAction(QKeySequence(QKeySequence::Undo));
	QAction *redo = findAction(QKeySequence(QKeySequence::Redo));
	if (undo && redo) {
		undo->trigger();
		const size_t afterUndo = store.guides().sceneGuides(ctx.sceneUuid().toStdString()).size();
		redo->trigger();
		const size_t afterRedo = store.guides().sceneGuides(ctx.sceneUuid().toStdString()).size();
		R->check(afterUndo == before && afterRedo == before + 1, "guide-undo-redo",
			 QString("undo %1, redo %2").arg(afterUndo).arg(afterRedo));
	}
}

void testLayoutsV02()
{
	EditContext &ctx = EditContext::instance();
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	resetItem("C", 1300, 250);
	ctx.markDirty();

	/* Auto layout: horizontal, gap 30, inside the canvas, vertically centered. */
	selectNames({"A", "B", "C"});
	core::FlowSpec fs;
	fs.direction = core::FlowDirection::Horizontal;
	fs.gap = 30;
	fs.padding = 40;
	fs.crossAlign = core::FlowAlign::Center;
	commands::autoLayout(fs, true);
	ctx.markDirty();
	R->check(box("A").x == 40 && std::fabs(box("B").x - (40 + 400 + 30)) < 0.6 &&
			 std::fabs(box("C").x - (40 + 400 + 30 + 300 + 30)) < 0.6 &&
			 std::fabs(box("B").centerY() - 540) < 0.6,
		 "auto-layout-horizontal", rectStr(box("A")) + rectStr(box("B")) + rectStr(box("C")));

	/* Grid of 2 columns in the selection. */
	fs.direction = core::FlowDirection::Grid;
	fs.columns = 2;
	fs.gap = 10;
	fs.padding = 0;
	fs.crossAlign = core::FlowAlign::Start;
	commands::autoLayout(fs, false);
	ctx.markDirty();
	R->check(std::fabs(box("C").y - (box("A").y + 300 + 10)) < 0.6, "auto-layout-grid",
		 rectStr(box("A")) + rectStr(box("B")) + rectStr(box("C")));

	/* A stray preview (nothing hovered) goes away at the next selection change. */
	for (const char *name : {"LayoutsAutoLayoutSection", "LayoutsPresetSection"}) {
		if (QWidget *sec = g_window->findChild<QWidget *>(name)) {
			QEvent leave(QEvent::Leave); /* the real cursor may sit over the window */
			QApplication::sendEvent(sec, &leave);
		}
	}
	commands::showPreview(commands::flowFrames(fs, true));
	emit ctx.itemsChanged();
	R->check(ctx.preview().empty(), "stray-preview-cleared", QString::number(ctx.preview().size()));

	/* Hovering the presets section of the Inspector previews on the Canvas. */
	selectNames({"A", "B"});
	emit ctx.itemsChanged(); /* let the Inspector list the presets for 2 items */
	if (QWidget *sec = g_window->findChild<QWidget *>("LayoutsPresetSection")) {
		QEvent enter(QEvent::Enter);
		QApplication::sendEvent(sec, &enter);
		const size_t shown = ctx.preview().size();
		/* Selection cleared while hovering: the section hides, the preview must go. */
		ctx.clearSelection();
		emit ctx.itemsChanged();
		R->check(shown == 2 && ctx.preview().empty(), "preview-cleared-when-selection-cleared",
			 QString::number(ctx.preview().size()));
		selectNames({"A", "B"});
		emit ctx.itemsChanged();
		QApplication::sendEvent(sec, &enter);
		QEvent leave(QEvent::Leave);
		QApplication::sendEvent(sec, &leave);
		R->check(shown == 2 && ctx.preview().empty(), "preset-hover-preview", QString::number(shown));
	} else {
		R->check(false, "preset-hover-preview", "section not found");
	}

	/* Presets. */
	resetItem("B", 700, 400);
	ctx.markDirty();
	selectNames({"B"});
	core::PresetSpec ps;
	ps.margin = 40;
	ps.cornerScale = 0.25;
	commands::applyPreset(core::Preset::CornerBottomRight, ps);
	ctx.markDirty();
	R->check(std::fabs(box("B").right() - 1880) < 0.6 && std::fabs(box("B").bottom() - 1040) < 0.6 &&
			 std::fabs(box("B").w - 480) < 0.6,
		 "preset-corner", rectStr(box("B")));
	resetItem("A", 0, 0);
	resetItem("B", 0, 0);
	ctx.markDirty();
	selectNames({"A", "B"});
	ps.gap = 20;
	commands::applyPreset(core::Preset::SplitScreen, ps);
	ctx.markDirty();
	R->check(box("A").right() < box("B").left() && std::fabs(box("A").w - 910) < 0.6 &&
			 std::fabs(box("A").w / box("A").h - 2.0) < 0.01,
		 "preset-split-keeps-ratio", rectStr(box("A")) + rectStr(box("B")));

	/* Multi-selection resize with the bottom-right handle, mouse driven. */
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	ctx.markDirty();
	selectNames({"A", "B"});
	ui::CanvasView *v = g_dock->view();
	v->rebuildOverlay();
	const core::Rect b0 = box("A").united(box("B"));
	const QPointF h = v->worldToView({b0.right(), b0.bottom()});
	const double d = 100 * v->zoom(); /* +100 canvas px on x */
	/* Handles follow the ratio lock (0.5): locked here, so the corner keeps the ratio. */
	const bool lockBefore = Store::instance().settings().ratioLock;
	Store::instance().settings().ratioLock = true;
	sendMouse(v, QEvent::MouseButtonPress, h, Qt::LeftButton);
	for (int i = 1; i <= 10; i++)
		sendMouse(v, QEvent::MouseMove, h + QPointF(d * i / 10.0, 0), Qt::LeftButton, Qt::ControlModifier);
	sendMouse(v, QEvent::MouseButtonRelease, h + QPointF(d, 0), Qt::NoButton, Qt::ControlModifier);
	ctx.markDirty();
	Store::instance().settings().ratioLock = lockBefore;
	const core::Rect b1 = box("A").united(box("B"));
	/* Locked corner: the pointer is projected on the diagonal (Figma), the box grows and
	 * keeps its ratio, the opposite corner stays. */
	R->check(std::fabs(b1.x - b0.x) < 0.6 && std::fabs(b1.y - b0.y) < 0.6 && b1.w > b0.w + 10 &&
			 std::fabs(b1.w / b1.h - b0.w / b0.h) < 0.01,
		 "multi-resize-keeps-ratio", rectStr(b0) + " -> " + rectStr(b1));
}

/* Roles: saved on the source, kept when renamed, undoable, suggested from the name. */
void testRoles()
{
	EditContext &ctx = EditContext::instance();
	const ItemEntry *a = entry("A");
	obs_source_t *src = a ? obs_sceneitem_get_source(a->item) : nullptr;
	R->check(src && roles::explicitRole(src) == core::Role::None && roles::role(src) == core::Role::None,
		 "roles-unset-by-default");
	if (!src)
		return;
	roles::setRole({src}, core::Role::Camera);
	obs_source_set_name(src, "A renamed");
	const bool kept = roles::role(src) == core::Role::Camera;
	obs_source_set_name(src, "A");
	ctx.markDirty();
	R->check(kept, "roles-survive-rename");
	roles::setRole({src}, core::Role::Host);
	R->check(roles::role(src) == core::Role::Host && roles::sceneCount(src) >= 1, "roles-host",
		 QString::number(roles::sceneCount(src)));
	QAction *undo = findAction(QKeySequence(QKeySequence::Undo));
	if (undo)
		undo->trigger(); /* Host -> Camera */
	if (undo)
		undo->trigger(); /* Camera -> unset */
	ctx.markDirty();
	R->check(undo && roles::explicitRole(src) == core::Role::None, "roles-undo",
		 core::roleKey(roles::explicitRole(src)));
	OBSSourceAutoRelease guest = colorSource("CAM _ GUEST 1", 64, 64, 0xFFFFFFFF);
	OBSSourceAutoRelease bg = colorSource("BG _ DYNAMIQUE", 64, 64, 0xFFFFFFFF);
	R->check(roles::role(guest) == core::Role::Camera && roles::role(bg) == core::Role::Background,
		 "roles-suggested-from-name");
	obs_source_remove(guest);
	obs_source_remove(bg);
}

/* 0.5: chat sources, suggested from their name and fitted whole in chat slots. */
void testChatRole()
{
	OBSSourceAutoRelease chat = colorSource("TCHAT _ TWITCH", 400, 900, 0xFFFFFFFF);
	R->check(roles::role(chat) == core::Role::Chat && roles::explicitRole(chat) == core::Role::None,
		 "chat-suggested-from-name", core::roleKey(roles::role(chat)));
	obs_source_remove(chat);

	core::ItemModel m;
	m.sourceWidth = 400;
	m.sourceHeight = 900;
	const core::Rect cell{1400, 0, 520, 1080};
	const core::ItemTransform column = commands::slotTransform(m, core::SlotKind::Chat, cell, false);
	const core::ItemTransform ticker = commands::slotTransform(m, core::SlotKind::Chat, cell, true);
	R->check(column.boundsType == core::BoundsType::ScaleInner && column.bounds.x == cell.w &&
			 column.bounds.y == cell.h && column.pos.x == cell.x && column.pos.y == cell.y &&
			 column.boundsAlign == (uint32_t)core::kAlignTop && column.crop.left == 0 &&
			 column.crop.top == 0 && !column.cropToBounds,
		 "chat-fits-top-of-column");
	R->check(ticker.boundsType == core::BoundsType::ScaleInner &&
			 ticker.boundsAlign == (uint32_t)core::kAlignCenter,
		 "chat-centered-in-ticker");
}

/* 0.4: types in Layers (filter), in broadcast layouts (BG / CG never in a tile) and the
 * Builder (scenes made from typed sources, background at the back, graphics on top). */
obs_source_t *sourceOf(const char *name)
{
	const ItemEntry *e = entry(name);
	return e ? obs_sceneitem_get_source(e->item) : nullptr;
}

void testTypes()
{
	EditContext &ctx = EditContext::instance();
	obs_source_t *a = sourceOf("A"), *b = sourceOf("B"), *c = sourceOf("C");
	if (!a || !b || !c) {
		R->check(false, "types-setup");
		return;
	}
	roles::setRole({a}, core::Role::Host);
	roles::setRole({b}, core::Role::Camera);
	roles::setRole({c}, core::Role::Screen);
	ctx.markDirty();
	ui::LayersPanel *layers = g_window->layers();
	layers->refresh();
	layers->filter()->select(false, core::Role::Camera);
	QStringList shown;
	for (int i = 0; i < layers->tree()->topLevelItemCount(); i++) {
		if (!layers->tree()->topLevelItem(i)->isHidden())
			shown << layers->tree()->topLevelItem(i)->text(0);
	}
	const bool noDrag = layers->tree()->dragDropMode() == QAbstractItemView::NoDragDrop;
	layers->filter()->select(true);
	int all = 0;
	for (int i = 0; i < layers->tree()->topLevelItemCount(); i++)
		all += layers->tree()->topLevelItem(i)->isHidden() ? 0 : 1;
	R->check(shown == QStringList{"B"} && noDrag && all == layers->tree()->topLevelItemCount(),
		 "layers-filter-by-type", shown.join(','));

	/* A background and a logo in the scene: never placed in a tile. */
	obs_scene_t *scene = ctx.scene();
	obs_sceneitem_t *bgItem = addColor(scene, "BG _ TEST", 1280, 720, 0xFF202020, 0, 0);
	obs_sceneitem_t *cgItem = addColor(scene, "LOGO _ TEST", 200, 80, 0xFFFFFFFF, 1680, 40);
	ctx.markDirty();
	selectNames({"A", "B", "BG _ TEST", "LOGO _ TEST"});
	R->check(commands::selectionKinds(commands::KindMode::Auto).size() == 2, "broadcast-skips-bg-and-cg",
		 QString::number(commands::selectionKinds(commands::KindMode::Auto).size()));

	/* Builder window mode lists the sources of the collection. */
	g_window->setMode(ui::LayoutsWindow::Mode::Builder);
	g_window->builderSources()->refresh();
	const int rows = g_window->builderSources()->tree()->topLevelItemCount();
	R->check(g_window->builderSources()->isVisible() && rows == (int)roles::videoSources().size(),
		 "builder-lists-collection-sources", QString::number(rows));
	/* Select sources: the gallery of Scene mode shows the layouts that fit them, and
	 * "New scene" makes the chosen one with the background and the logo. */
	ui::BuilderSources *bs = g_window->builderSources();
	bs->selectNames({"A", "B", "C", "BG _ TEST", "LOGO _ TEST"});
	ui::BuilderPanel *bp = g_window->builderPanel();
	const int expected = (int)core::templatesFor(2, 1).size();
	R->check(bs->selectedSources().size() == 5 && bp->gallery()->count() == expected,
		 "builder-gallery-for-selection",
		 QString("%1 templates, %2 expected").arg(bp->gallery()->count()).arg(expected));
	const QString made = bp->createScene();
	OBSSourceAutoRelease madeScene = obs_get_source_by_name(made.toUtf8().constData());
	int madeItems = 0;
	if (obs_scene_t *ms = madeScene ? obs_scene_from_source(madeScene) : nullptr)
		obs_scene_enum_items(
			ms,
			[](obs_scene_t *, obs_sceneitem_t *, void *p) {
				(*static_cast<int *>(p))++;
				return true;
			},
			&madeItems);
	R->check(madeScene && madeItems == 5 && ctx.sceneSource() == madeScene.Get(), "builder-new-scene-from-gallery",
		 made + QString(" %1 items").arg(madeItems));
	if (madeScene)
		obs_source_remove(madeScene);
	ctx.follow();

	/* Plan and create from five typed sources. */
	builder::Options o;
	o.spec.gap = 8;
	const std::vector<OBSSource> picked{a, b, c, obs_sceneitem_get_source(bgItem),
					    obs_sceneitem_get_source(cgItem)};
	const builder::Plan plan = builder::plan(
		picked, o, [](const std::string &key) { return QString("T ") + QString::fromStdString(key); },
		[](const QString &n) { return QString("Solo ") + n; });
	size_t layouts = 0;
	for (const core::BroadcastTemplate &t : core::templatesFor(2, 1))
		layouts += t.cameras + t.screens >= 2 ? 1 : 0;
	R->check(plan.scenes.size() == 3 + layouts && plan.cameras.size() == 2 && plan.cameras[0].Get() == a &&
			 plan.background.Get() == obs_sceneitem_get_source(bgItem) && plan.graphics.size() == 1,
		 "builder-plan", QString::number(plan.scenes.size()));
	const std::vector<QString> names = builder::create(plan, o);
	bool layered = names.size() == plan.scenes.size();
	QString why;
	for (const QString &n : names) {
		OBSSourceAutoRelease src = obs_get_source_by_name(n.toUtf8().constData());
		obs_scene_t *sc = src ? obs_scene_from_source(src) : nullptr;
		std::vector<obs_sceneitem_t *> items;
		if (sc)
			obs_scene_enum_items(
				sc,
				[](obs_scene_t *, obs_sceneitem_t *it, void *p) {
					static_cast<std::vector<obs_sceneitem_t *> *>(p)->push_back(it);
					return true;
				},
				&items);
		const bool ok = items.size() >= 3 &&
				obs_sceneitem_get_source(items.front()) == obs_sceneitem_get_source(bgItem) &&
				obs_sceneitem_get_bounds_type(items.front()) == OBS_BOUNDS_SCALE_OUTER &&
				obs_sceneitem_get_source(items.back()) == obs_sceneitem_get_source(cgItem) &&
				obs_sceneitem_locked(items[1]);
		vec2 cgPos = {};
		if (!items.empty())
			obs_sceneitem_get_pos(items.back(), &cgPos);
		if (!ok || std::fabs(cgPos.x - 1680) > 0.5 || std::fabs(cgPos.y - 40) > 0.5) {
			layered = false;
			why = n;
		}
		if (src)
			obs_source_remove(src);
	}
	R->check(layered, "builder-bg-back-cg-top-locked", why);

	/* Back to the scene; the test sources go away and the types are cleared. */
	g_window->setMode(ui::LayoutsWindow::Mode::Scene);
	obs_sceneitem_remove(bgItem);
	obs_sceneitem_remove(cgItem);
	roles::setRole({a, b, c}, core::Role::None);
	ctx.follow();
	ctx.markDirty();
	R->check(g_window->layers()->isVisible(), "builder-back-to-scene-mode");
}

/* Scenes above Layers: a click edits another scene, nothing changes on air. */
void testScenesList()
{
	EditContext &ctx = EditContext::instance();
	ctx.follow();
	obs_scene_t *extra = obs_scene_create("Layouts Other Scene");
	OBSSourceAutoRelease extraSource = obs_source_get_ref(obs_scene_get_source(extra));
	obs_scene_release(extra);
	ui::ScenesList *list = g_window->layers()->scenes();
	list->refresh();
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	const QString programName = QString::fromUtf8(obs_source_get_name(program));
	QListWidgetItem *other = nullptr, *onAir = nullptr;
	for (int i = 1; i < list->count(); i++) {
		if (list->item(i)->text() == programName)
			onAir = list->item(i);
		else if (!other && !list->item(i)->text().contains(QString::fromUtf8("\xE2\x80\xBA")))
			other = list->item(i);
	}
	/* A fresh collection in Studio Mode can have nothing on the program yet. */
	R->check(other && (!program || (onAir && !onAir->toolTip().isEmpty())), "scenes-list-marks-program",
		 QString("%1 rows, program '%2'").arg(list->count()).arg(programName));
	if (!other) {
		obs_source_remove(extraSource);
		return;
	}
	const QString otherName = other->text();
	emit list->itemClicked(other); /* the list is rebuilt after the click */
	QApplication::processEvents();
	OBSSourceAutoRelease programAfter = obs_frontend_get_current_scene();
	R->check(!ctx.following() && ctx.sceneName() == otherName && programAfter.Get() == program.Get(),
		 "scenes-list-edits-scene", ctx.sceneName());
	emit list->itemClicked(list->item(0));
	QApplication::processEvents();
	/* Follow OBS edits the preview scene in Studio Mode, the program otherwise. */
	OBSSourceAutoRelease followed = obs_frontend_preview_program_mode_active()
						? obs_frontend_get_current_preview_scene()
						: obs_frontend_get_current_scene();
	R->check(ctx.following() && followed && ctx.sceneName() == QString::fromUtf8(obs_source_get_name(followed)),
		 "scenes-list-follow-obs", ctx.sceneName());
	obs_source_remove(extraSource);
}

/* Frame modes: W and H change the shape of a 16:9 camera without distorting it. */
void testFrameModes()
{
	EditContext &ctx = EditContext::instance();
	ViewSettings &vs = Store::instance().settings();
	const bool lock = vs.ratioLock;
	const int refPoint = vs.refPoint;
	vs.ratioLock = false;
	vs.refPoint = 4; /* center */
	obs_sceneitem_t *item = addColor(ctx.scene(), "Frame cam", 1920, 1080, 0xFF808080, 100, 100);
	vec2 half;
	vec2_set(&half, 0.5f, 0.5f);
	obs_sceneitem_set_scale(item, &half);
	ctx.markDirty();
	selectNames({"Frame cam"});
	const core::Vec2 c0 = entry("Frame cam")->frame.center();
	commands::setField(commands::Field::W, core::parseExpression("540", 1920));
	ctx.markDirty();
	const ItemEntry *e = entry("Frame cam");
	R->check(e && commands::frameMode(e->item) == core::FrameMode::Fill && std::fabs(e->frame.width - 540) < 0.6 &&
			 std::fabs(e->frame.height - 540) < 0.6 && e->model.t.crop.left == 420 &&
			 e->model.t.crop.right == 420 && std::fabs(e->model.t.scale.x - 0.5) < 1e-6 &&
			 std::fabs(e->model.t.scale.y - 0.5) < 1e-6 && std::fabs(e->frame.center().x - c0.x) < 0.6 &&
			 std::fabs(e->frame.center().y - c0.y) < 0.6,
		 "frame-fill-square-not-distorted",
		 e ? rectStr(box("Frame cam")) +
				 QString(" crop %1/%2").arg(e->model.t.crop.left).arg(e->model.t.crop.right)
		   : "no item");

	/* Back to 16:9 with the ratio button: the largest 16:9 inside the square. */
	commands::setRatio(16.0 / 9.0);
	ctx.markDirty();
	e = entry("Frame cam");
	R->check(e && std::fabs(e->frame.width - 540) < 0.6 && std::fabs(e->frame.height - 303.75) < 0.6 &&
			 e->model.t.crop.left == 0 && e->model.t.crop.right == 0,
		 "frame-ratio-button", e ? rectStr(box("Frame cam")) : "no item");
	commands::setRatio(1.0);
	ctx.markDirty();
	e = entry("Frame cam");
	const double side = e ? e->frame.width : 0;
	R->check(e && std::fabs(e->frame.width - e->frame.height) < 0.6 && e->model.t.crop.left > 0,
		 "frame-ratio-square", e ? rectStr(box("Frame cam")) : "no item");

	/* Fit: the whole picture in the same square (bounding box). */
	commands::setFrameMode(core::FrameMode::Fit);
	ctx.markDirty();
	e = entry("Frame cam");
	R->check(e && commands::frameMode(e->item) == core::FrameMode::Fit &&
			 e->model.t.boundsType == core::BoundsType::ScaleInner && e->model.t.crop == core::Crop() &&
			 std::fabs(e->frame.width - side) < 0.6 && std::fabs(e->frame.height - side) < 0.6,
		 "frame-fit-bounds", e ? rectStr(box("Frame cam")) : "no item");

	/* Stretch: as in OBS, the image is distorted. */
	resetItem("Frame cam", 100, 100);
	obs_sceneitem_set_scale(item, &half);
	ctx.markDirty();
	commands::setFrameMode(core::FrameMode::Stretch);
	ctx.markDirty();
	commands::setField(commands::Field::W, core::parseExpression("540", 1920));
	ctx.markDirty();
	e = entry("Frame cam");
	R->check(e && e->model.t.crop == core::Crop() && std::fabs(e->model.t.scale.x - 0.28125) < 1e-6 &&
			 std::fabs(e->model.t.scale.y - 0.5) < 1e-6,
		 "frame-stretch-distorts", e ? QString("%1 x %2").arg(e->model.t.scale.x).arg(e->model.t.scale.y) : "");

	obs_sceneitem_remove(item);
	vs.ratioLock = lock;
	vs.refPoint = refPoint;
	ctx.markDirty();
}

void unlockAll(std::initializer_list<const char *> names)
{
	for (const char *n : names) {
		if (const ItemEntry *e = entry(n)) {
			obs_sceneitem_set_locked(e->item, false);
			setConstraints(e->item, core::HConstraint::Left, core::VConstraint::Top);
		}
	}
	EditContext::instance().markDirty();
}

void testBroadcast()
{
	EditContext &ctx = EditContext::instance();
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	resetItem("C", 1300, 250);
	ctx.markDirty();

	/* Color sources count as screens when detected, as cameras when forced. */
	selectNames({"A", "B"});
	int screens = 0;
	for (core::SlotKind k : commands::selectionKinds(commands::KindMode::Auto))
		screens += k == core::SlotKind::Screen;
	R->check(screens == 2, "broadcast-detects-screens", QString::number(screens));

	/* Duo split, cameras: each half filled edge to edge (cropped), then locked. */
	core::BroadcastSpec spec;
	spec.gap = 8;
	commands::applyBroadcast("Cam2Split", spec, commands::KindMode::AllCameras, true);
	ctx.markDirty();
	const core::Rect a = box("A"), b = box("B");
	const double half = (1920 - 8) / 2.0;
	R->check(std::fabs(a.x) < 0.6 && std::fabs(a.y) < 0.6 && std::fabs(a.w - half) < 1.0 &&
			 std::fabs(a.h - 1080) < 1.0 && std::fabs(b.x - (half + 8)) < 1.0 &&
			 std::fabs(b.right() - 1920) < 1.0 && std::fabs(b.h - 1080) < 1.0,
		 "broadcast-duo-fills-halves", rectStr(a) + rectStr(b));
	const ItemEntry *ea = entry("A");
	obs_sceneitem_crop crop = {};
	if (ea)
		obs_sceneitem_get_crop(ea->item, &crop);
	core::HConstraint h;
	core::VConstraint v;
	if (ea)
		getConstraints(ea->item, h, v);
	R->check(ea && crop.left > 0 && crop.right > 0 && obs_sceneitem_locked(ea->item) &&
			 h == core::HConstraint::Scale && v == core::VConstraint::Scale,
		 "broadcast-camera-cropped-and-locked", QString("crop %1/%2").arg(crop.left).arg(crop.right));

	/* Same items as screens, re-applied without the lock: never cropped, whole content
	 * inside the half, and freed again. */
	commands::applyBroadcast("Screen2Split", spec, commands::KindMode::AllScreens, false);
	ctx.markDirty();
	obs_sceneitem_crop crop2 = {};
	if ((ea = entry("A")))
		obs_sceneitem_get_crop(ea->item, &crop2);
	const core::Rect a2 = box("A");
	R->check(ea && crop2.left == 0 && crop2.right == 0 && crop2.top == 0 && crop2.bottom == 0 &&
			 std::fabs(a2.w / a2.h - 2.0) < 0.01 && a2.right() <= half + 0.6 &&
			 !obs_sceneitem_locked(ea->item),
		 "broadcast-screen-fit-uncropped-unlocked", rectStr(a2));
	if (ea)
		getConstraints(ea->item, h, v);
	R->check(ea && h == core::HConstraint::Left && v == core::VConstraint::Top,
		 "broadcast-unlock-resets-constraints");

	/* A mirrored webcam stays mirrored in its slot, and still fills it. */
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	ctx.markDirty();
	selectNames({"A"});
	commands::flip(core::Axis::X);
	ctx.markDirty();
	selectNames({"A", "B"});
	commands::applyBroadcast("Cam2Split", spec, commands::KindMode::AllCameras, false);
	ctx.markDirty();
	vec2 mirrored = {};
	if ((ea = entry("A")))
		obs_sceneitem_get_scale(ea->item, &mirrored);
	const core::Rect a3 = box("A");
	R->check(ea && mirrored.x < 0 && mirrored.y > 0 && std::fabs(a3.x) < 0.6 && std::fabs(a3.w - half) < 1.0 &&
			 std::fabs(a3.h - 1080) < 1.0,
		 "broadcast-keeps-flip", rectStr(a3) + QString(" sx %1").arg(mirrored.x));

	/* The Inspector lists the templates for the selection. */
	Store::instance().settings().broadcastKinds = 1;
	selectNames({"A", "B", "C"});
	emit ctx.itemsChanged();
	auto *gallery = g_window->inspector()->findChild<QListWidget *>();
	R->check(gallery && gallery->count() == (int)core::templatesFor(3, 0, 0, ui::canvasAspect() < 1.0).size(),
		 "broadcast-gallery-three-guests", gallery ? QString::number(gallery->count()) : "no gallery");
	Store::instance().settings().broadcastKinds = 0;
	emit ctx.itemsChanged();
	R->check(gallery && gallery->count() == (int)core::templatesFor(0, 3, 0, ui::canvasAspect() < 1.0).size(),
		 "broadcast-gallery-three-screens", gallery ? QString::number(gallery->count()) : "no gallery");
}

/* Swap (0.5): a locked Duo, the right camera dropped on the left one on the canvas. */
void testSwap()
{
	EditContext &ctx = EditContext::instance();
	obs_sceneitem_t *a = addColor(ctx.scene(), "CAM Swap L", 1920, 1080, 0xFF3060C0, 0, 0);
	obs_sceneitem_t *b = addColor(ctx.scene(), "CAM Swap R", 1080, 1920, 0xFFC06030, 0, 0);
	const core::ItemModel ma{commands::slotTransform({{}, 1920, 1080}, core::SlotKind::Camera, {0, 0, 960, 1080}),
				 1920, 1080};
	const core::ItemModel mb{commands::slotTransform({{}, 1080, 1920}, core::SlotKind::Camera, {960, 0, 960, 1080}),
				 1080, 1920};
	applyChangesAtomically(ctx.scene(), {{OBSSceneItem(a), ma.t}, {OBSSceneItem(b), mb.t}});
	commands::lockItem(a);
	commands::lockItem(b);
	{
		OBSDataAutoRelease priv = obs_sceneitem_get_private_settings(a);
		obs_data_set_string(priv, "layouts_style", "{\"border\":true}");
	}
	const int orderA = obs_sceneitem_get_order_position(a), orderB = obs_sceneitem_get_order_position(b);
	ctx.markDirty();
	selectNames({"CAM Swap R"});

	/* Drag R onto L with the mouse: one release, one swap. */
	ui::CanvasView *v = g_dock->view();
	const QPointF start = v->worldToView(box("CAM Swap R").center());
	const QPointF end = v->worldToView(box("CAM Swap L").center());
	sendMouse(v, QEvent::MouseButtonPress, start, Qt::LeftButton);
	for (int i = 1; i <= 10; i++)
		sendMouse(v, QEvent::MouseMove, start + (end - start) * (i / 10.0), Qt::LeftButton);
	sendMouse(v, QEvent::MouseButtonRelease, end, Qt::NoButton);
	ctx.markDirty();

	const core::Rect l = box("CAM Swap L"), r = box("CAM Swap R");
	R->check(std::fabs(l.x - 960) < 0.6 && std::fabs(l.w - 960) < 0.6 && std::fabs(r.x) < 0.6 &&
			 std::fabs(r.w - 960) < 0.6 && std::fabs(r.h - 1080) < 0.6,
		 "swap-drag-exchanges-cells", rectStr(l) + " / " + rectStr(r));
	R->check(obs_sceneitem_locked(a) && obs_sceneitem_locked(b), "swap-keeps-lock");
	R->check(obs_sceneitem_get_order_position(a) == orderB && obs_sceneitem_get_order_position(b) == orderA,
		 "swap-exchanges-order");
	{
		OBSDataAutoRelease pa = obs_sceneitem_get_private_settings(a);
		OBSDataAutoRelease pb = obs_sceneitem_get_private_settings(b);
		R->check(!obs_data_has_user_value(pa, "layouts_style") &&
				 QString::fromUtf8(obs_data_get_string(pb, "layouts_style")).contains("border"),
			 "swap-style-stays-with-slot");
	}

	QAction *undo = findAction(QKeySequence(QKeySequence::Undo));
	if (undo) {
		undo->trigger();
		ctx.markDirty();
		R->check(std::fabs(box("CAM Swap L").x) < 0.6 && std::fabs(box("CAM Swap R").x - 960) < 0.6 &&
				 obs_sceneitem_get_order_position(a) == orderA,
			 "swap-one-undo-step", rectStr(box("CAM Swap L")));
	}
	unlockAll({"CAM Swap L", "CAM Swap R"});
	obs_sceneitem_remove(a);
	obs_sceneitem_remove(b);
	ctx.markDirty();
}

void testBroadcastScene()
{
	EditContext &ctx = EditContext::instance();
	ctx.follow();
	const QString before = ctx.sceneName();
	selectNames({"A", "B", "C"});
	core::BroadcastSpec spec;
	const QString name = commands::createBroadcastScene("Cam3Columns", "Three columns", spec,
							    commands::KindMode::AllCameras, true);
	OBSSourceAutoRelease src = obs_get_source_by_name(name.toUtf8().constData());
	obs_scene_t *scene = src ? obs_scene_from_source(src) : nullptr;
	int count = 0;
	if (scene)
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
				(*(int *)p) += obs_sceneitem_locked(item) ? 1 : 0;
				return true;
			},
			&count);
	OBSSourceAutoRelease original = obs_get_source_by_name(before.toUtf8().constData());
	int originalLocked = 0;
	if (obs_scene_t *os = original ? obs_scene_from_source(original) : nullptr)
		obs_scene_enum_items(
			os,
			[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
				(*(int *)p) += obs_sceneitem_locked(item) ? 1 : 0;
				return true;
			},
			&originalLocked);
	R->check(scene && count == 3 && ctx.sceneName() == name && originalLocked == 0, "broadcast-new-scene",
		 name + " locked " + QString::number(count));
	if (src)
		obs_source_remove(src);
	ctx.pin(original);
	ctx.markDirty();
}

void testShowScenes()
{
	EditContext &ctx = EditContext::instance();
	ctx.follow();
	const QString edited = ctx.sceneName();
	selectNames({"A", "B", "C"});
	core::BroadcastSpec spec;
	const int n = commands::createShowScenes(
		spec, commands::KindMode::AllCameras, true,
		[](const std::string &key) { return QString("Show ") + QString::fromStdString(key); },
		[](const QString &source) { return QString("Solo ") + source; });
	/* 3 close-ups + three columns + host + 2. */
	int found = 0, locked = 0;
	for (const char *name : {"Solo A", "Solo B", "Solo C", "Show Cam3Columns", "Show Cam3OneTwo"}) {
		OBSSourceAutoRelease src = obs_get_source_by_name(name);
		obs_scene_t *scene = src ? obs_scene_from_source(src) : nullptr;
		if (!scene)
			continue;
		found++;
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
				(*(int *)p) += obs_sceneitem_locked(item) ? 1 : 0;
				return true;
			},
			&locked);
		obs_source_remove(src);
	}
	/* The other layouts for three guests were created too: remove them. */
	const std::vector<core::BroadcastTemplate> trio = core::templatesFor(3, 0, 0, ui::canvasAspect() < 1.0);
	for (const core::BroadcastTemplate &t : trio) {
		OBSSourceAutoRelease src = obs_get_source_by_name(("Show " + t.key).c_str());
		if (src)
			obs_source_remove(src);
	}
	R->check(n == 3 + (int)trio.size() && found == 5 && locked == 3 + 3 + 3 && ctx.sceneName() == edited,
		 "broadcast-show-scenes", QString("created %1 found %2 locked %3").arg(n).arg(found).arg(locked));

	/* A group is never shared with another scene. */
	selectNames({"G"});
	const QString groupScene = commands::createBroadcastScene("Screen1Full", "Group only", spec,
								  commands::KindMode::AllScreens, false);
	OBSSourceAutoRelease leaked = obs_get_source_by_name("Group only");
	R->check(groupScene.isEmpty() && !leaked && ctx.sceneName() == edited, "broadcast-new-scene-skips-groups",
		 groupScene);
	if (leaked)
		obs_source_remove(leaked);
}

QTreeWidgetItem *layerRow(const char *name)
{
	QTreeWidget *tree = g_window->layers()->tree();
	for (QTreeWidgetItemIterator it(tree); *it; ++it) {
		if ((*it)->text(0) == name)
			return *it;
	}
	return nullptr;
}

QString topLevelOrder()
{
	QTreeWidget *tree = g_window->layers()->tree();
	QStringList names;
	for (int i = 0; i < tree->topLevelItemCount(); i++)
		names << tree->topLevelItem(i)->text(0);
	return names.join(',');
}

void testLayers()
{
	EditContext &ctx = EditContext::instance();
	ui::LayersPanel *layers = g_window->layers();
	layers->refresh();
	QTreeWidget *tree = layers->tree();
	QTreeWidgetItem *group = layerRow("G");
	/* Front on top, as libobs stacks them (enumeration runs from the back). */
	QStringList inGroup;
	if (const ItemEntry *g = entry("G"))
		obs_sceneitem_group_enum_items(
			g->item,
			[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
				((QStringList *)p)
					->prepend(
						QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))));
				return true;
			},
			&inGroup);
	QStringList rows;
	for (int i = 0; group && i < group->childCount(); i++)
		rows << group->child(i)->text(0);
	R->check(topLevelOrder() == "G,C,B,A" && rows.size() == 2 && rows == inGroup, "layers-front-on-top",
		 topLevelOrder() + " / " + rows.join(',') + " vs " + inGroup.join(','));

	/* Canvas -> layers. */
	selectNames({"C"});
	emit ctx.itemsChanged();
	R->check(tree->selectedItems().size() == 1 && tree->selectedItems()[0]->text(0) == "C",
		 "layers-follow-canvas-selection");

	/* Layers -> canvas (and OBS). */
	tree->clearSelection();
	if (QTreeWidgetItem *b = layerRow("B"))
		b->setSelected(true);
	if (QTreeWidgetItem *d = layerRow("D"))
		d->setSelected(true);
	std::vector<const ItemEntry *> sel = ctx.selection();
	QStringList names;
	for (const ItemEntry *e : sel)
		names << e->name;
	names.sort();
	R->check(names.join(',') == "B,D", "layers-select-sources", names.join(','));

	/* Eye column hides and shows. */
	QTreeWidgetItem *c = layerRow("C");
	emit tree->itemClicked(c, 3); /* eye column */
	const bool hidden = entry("C") && !entry("C")->visible;
	emit tree->itemClicked(layerRow("C"), 3);
	R->check(hidden && entry("C") && entry("C")->visible, "layers-eye-toggles");

	/* Reorder: A to the front, one undo entry. */
	layers->reorder(layerRow("A"), nullptr, 0);
	layers->refresh();
	const QString moved = topLevelOrder();
	QAction *undo = findAction(QKeySequence(QKeySequence::Undo));
	if (undo)
		undo->trigger();
	ctx.markDirty();
	layers->refresh();
	R->check(moved == "A,G,C,B" && topLevelOrder() == "G,C,B,A", "layers-reorder-undo",
		 moved + " then " + topLevelOrder());

	/* Hiding is undoable. */
	emit tree->itemClicked(layerRow("C"), 3);
	const bool hid = entry("C") && !entry("C")->visible;
	if (undo)
		undo->trigger();
	ctx.markDirty();
	R->check(hid && entry("C") && entry("C")->visible, "layers-eye-undo");

	/* Into a group and back out, the image stays in place; undo and redo. */
	const core::Rect b0 = box("B");
	layers->refresh();
	layers->reorder(layerRow("B"), layerRow("G"), 0);
	ctx.markDirty();
	layers->refresh();
	const ItemEntry *g = entry("G");
	const ItemEntry *b = entry("B");
	const bool movedIn = g && b && b->groupId == g->id;
	const bool kept = std::fabs(box("B").x - b0.x) < 0.6 && std::fabs(box("B").y - b0.y) < 0.6;
	const QString inside = topLevelOrder();
	const QString movedRect = rectStr(b0) + "->" + rectStr(box("B"));
	if (undo)
		undo->trigger();
	ctx.markDirty();
	layers->refresh();
	const bool backOut = entry("B") && entry("B")->groupId == 0 && topLevelOrder() == "G,C,B,A";
	QAction *redo = findAction(QKeySequence(QKeySequence::Redo));
	if (redo)
		redo->trigger();
	ctx.markDirty();
	const bool redone = entry("B") && entry("G") && entry("B")->groupId == entry("G")->id;
	if (undo)
		undo->trigger();
	ctx.markDirty();
	layers->refresh();
	/* Renaming from the list is undoable. */
	if (QTreeWidgetItem *rowA = layerRow("A"))
		rowA->setText(0, "A renamed");
	const bool renamed = entry("A renamed") != nullptr;
	if (undo)
		undo->trigger();
	ctx.markDirty();
	layers->refresh();
	R->check(renamed && entry("A") && layerRow("A"), "layers-rename-undo");

	R->check(movedIn && kept && inside == "G,C,A" && backOut && redone && entry("B")->groupId == 0 &&
			 std::fabs(box("B").x - b0.x) < 0.6,
		 "layers-group-move-undo",
		 QString("in %1 kept %2 order %3 back %4 redo %5 %6")
			 .arg(movedIn)
			 .arg(kept)
			 .arg(inside)
			 .arg(backOut)
			 .arg(redone)
			 .arg(movedRect));
}

void testCropMode()
{
	EditContext &ctx = EditContext::instance();
	resetItem("A", 100, 100);
	ctx.markDirty();
	selectNames({"A"});
	ui::CanvasView *v = g_dock->view();
	v->setFocus();
	QKeyEvent kc(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, "c");
	QApplication::sendEvent(v, &kc);
	v->rebuildOverlay();
	/* Drag the left-middle handle 50 canvas px inwards: crop 50 source px on the left. */
	const core::Rect b = box("A");
	const QPointF h = v->worldToView({b.left(), b.centerY()});
	const double d = 50 * v->zoom();
	sendMouse(v, QEvent::MouseButtonPress, h, Qt::LeftButton);
	for (int i = 1; i <= 10; i++)
		sendMouse(v, QEvent::MouseMove, h + QPointF(d * i / 10.0, 0), Qt::LeftButton, Qt::ControlModifier);
	sendMouse(v, QEvent::MouseButtonRelease, h + QPointF(d, 0), Qt::NoButton, Qt::ControlModifier);
	ctx.markDirty();
	const ItemEntry *a = entry("A");
	R->check(a && a->model.t.crop.left == 50 && std::fabs(box("A").x - 150) < 0.6 &&
			 std::fabs(box("A").w - 350) < 0.6,
		 "crop-mode-drag", rectStr(box("A")) + QString(" crop.left=%1").arg(a ? a->model.t.crop.left : -1));
	QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
	QApplication::sendEvent(v, &esc);
	resetItem("A", 100, 100);
	ctx.markDirty();
}

void testHotkeys()
{
	const bool grid = Store::instance().settings().grid;
	const bool ok = hotkeys::trigger("layouts.toggle_grid");
	R->check(ok && Store::instance().settings().grid != grid, "hotkey-toggle-grid");
	hotkeys::trigger("layouts.toggle_grid");
	resetItem("A", 100, 100);
	resetItem("B", 700, 400);
	EditContext::instance().markDirty();
	selectNames({"A", "B"});
	commands::setAlignTarget(core::AlignTarget::Selection);
	hotkeys::trigger("layouts.align_left");
	EditContext::instance().markDirty();
	R->check(box("B").x == 100, "hotkey-align-left", rectStr(box("B")));
	resetItem("B", 700, 400);
	EditContext::instance().markDirty();
}

void testGroup()
{
	EditContext &ctx = EditContext::instance();
	ctx.markDirty();
	const ItemEntry *d = entry("D");
	R->check(d && d->groupId != 0, "group-child-listed");
	if (!d)
		return;
	ctx.select({d}, false);
	Store::instance().settings().refPoint = 0;
	commands::setField(commands::Field::X, expr("500"));
	commands::setField(commands::Field::Y, expr("100"));
}

void testGroupCheck()
{
	EditContext::instance().markDirty();
	const core::Rect b = box("D");
	R->check(std::fabs(b.x - 500) < 0.6 && std::fabs(b.y - 100) < 0.6, "group-child-world-position", rectStr(b));
}

void testGuidesSaved()
{
	EditContext &ctx = EditContext::instance();
	Store::instance().guides().add(ctx.sceneUuid().toStdString(), ctx.sceneName().toStdString(), core::Axis::Y, 333,
				       false);
	Store::instance().guides().add("", "", core::Axis::X, 777, true);
	obs_frontend_save();
}

QString g_testCollection;

void switchAway()
{
	char *cur = obs_frontend_get_current_scene_collection();
	g_testCollection = QString::fromUtf8(cur);
	bfree(cur);
	obs_frontend_add_scene_collection("Layouts Selftest Other");
}

void checkAwayEmpty()
{
	R->check(Store::instance().guides().globalGuides().empty(), "guides-per-collection",
		 QString::number(Store::instance().guides().globalGuides().size()));
	obs_frontend_set_current_scene_collection(g_testCollection.toUtf8().constData());
}

void checkGuidesRestored()
{
	EditContext &ctx = EditContext::instance();
	ctx.follow();
	const auto &g = Store::instance().guides();
	bool scene333 = false;
	for (const auto &x : g.sceneGuides(ctx.sceneUuid().toStdString()))
		scene333 = scene333 || (x.axis == core::Axis::Y && x.pos == 333);
	R->check(scene333, "scene-guides-restored", ctx.sceneName());
	R->check(g.globalGuides().size() == 1 && g.globalGuides()[0].pos == 777, "global-guides-restored",
		 QString::number(g.globalGuides().size()));
}

OBSSourceAutoRelease g_cleanScene;

void prepareCleanOutput()
{
	obs_scene_t *sScene = obs_scene_create("Layouts Clean");
	obs_source_t *s = obs_scene_get_source(sScene);
	g_cleanScene = s;
	addColor(obs_scene_from_source(s), "Clean BG", 1920, 1080, 0xFF303030, 0, 0);
	obs_frontend_set_current_scene(s);
	ViewSettings &v = Store::instance().settings();
	v.grid = true;
	v.guides = true;
	v.safeAreas = true;
	v.centerLines = true;
	Store::instance().guides().add(obs_source_get_uuid(s), "Layouts Clean", core::Axis::X, 960, false);
	Store::instance().guides().add(obs_source_get_uuid(s), "Layouts Clean", core::Axis::Y, 540, false);
	Store::instance().notifySettingsChanged(false);
}

void takeShot()
{
	obs_frontend_take_screenshot();
}

void checkCleanOutput()
{
	char *path = obs_frontend_get_last_screenshot();
	const QString p = QString::fromUtf8(path ? path : "");
	bfree(path);
	QImage img(p);
	R->check(!img.isNull(), "screenshot-taken", p);
	if (img.isNull())
		return;
	int bad = 0;
	for (int y = 0; y < img.height(); y += 4) {
		for (int x = 0; x < img.width(); x += 4) {
			const QColor c = img.pixelColor(x, y);
			if (std::abs(c.red() - 0x30) > 3 || std::abs(c.green() - 0x30) > 3 ||
			    std::abs(c.blue() - 0x30) > 3)
				bad++;
		}
	}
	const QColor center = img.pixelColor(960, 540);
	R->check(bad == 0, "tools-never-in-output",
		 QString("%1 off pixels, center %2, %3x%4")
			 .arg(bad)
			 .arg(center.name())
			 .arg(img.width())
			 .arg(img.height()));
	QFile::remove(p);
}

/* Rounded corners: a cropped item keeps round corners on the program output. */
OBSSourceAutoRelease g_cornerScene;
OBSSourceAutoRelease g_beforeCorners;
OBSSourceAutoRelease g_cornerSceneB;

void prepareCorners()
{
	EditContext &ctx = EditContext::instance();
	g_beforeCorners = obs_frontend_get_current_scene();
	obs_scene_t *sScene = obs_scene_create("Layouts Corners");
	obs_source_t *s = obs_scene_get_source(sScene);
	g_cornerScene = s;
	/* 800x600 white, cropped 100 left / 50 top: visible 700x550 at (200, 265). */
	obs_sceneitem_t *item = addColor(obs_scene_from_source(s), "Rounded", 800, 600, 0xFFFFFFFF, 200, 265);
	obs_sceneitem_crop crop = {100, 50, 0, 0};
	obs_sceneitem_set_crop(item, &crop);
	/* 400x200 at half size, not cropped: visible 200x100 at (1300, 100). */
	obs_sceneitem_t *half = addColor(obs_scene_from_source(s), "Rounded half", 400, 200, 0xFFFFFFFF, 1300, 100);
	vec2 halfScale;
	vec2_set(&halfScale, 0.5f, 0.5f);
	obs_sceneitem_set_scale(half, &halfScale);
	ctx.pin(s);
	ctx.markDirty();

	selectNames({"Rounded"});
	corners::setRadius(core::parseExpression("40", 0));
	ctx.markDirty();
	const ItemEntry *e = entry("Rounded");
	R->check(e && std::fabs(corners::radius(*e) - 40) < 0.01, "corners-radius-set",
		 e ? QString::number(corners::radius(*e)) : "no item");

	/* Half size: the field still shows canvas pixels, the source keeps 80 px. */
	selectNames({"Rounded half"});
	corners::setRadius(core::parseExpression("20", 0));
	ctx.markDirty();
	const ItemEntry *h = entry("Rounded half");
	OBSSourceAutoRelease filter =
		obs_source_get_filter_by_name(obs_sceneitem_get_source(half), obs_module_text("Corners.FilterName"));
	OBSDataAutoRelease fs = filter ? obs_source_get_settings(filter) : nullptr;
	OBSDataArrayAutoRelease perScene = fs ? obs_data_get_array(fs, "scenes") : nullptr;
	OBSDataAutoRelease entry0 = perScene ? obs_data_array_item(perScene, 0) : nullptr;
	const double stored = entry0 ? obs_data_get_double(entry0, "radius") : -1;
	R->check(h && std::fabs(corners::radius(*h) - 20) < 0.01 && std::fabs(stored - 40) < 0.01 &&
			 obs_data_get_double(fs, "radius") == 0.0,
		 "corners-canvas-pixels", QString::number(stored));
	/* On air once set, as in a show: Studio Mode puts a copy of the scene on the program. */
	obs_frontend_set_current_scene(s);
}

void checkCorners()
{
	char *path = obs_frontend_get_last_screenshot();
	const QString p = QString::fromUtf8(path ? path : "");
	bfree(path);
	QImage img(p);
	R->check(!img.isNull(), "corners-screenshot", p);
	if (img.isNull())
		return;
	auto lum = [&](int x, int y) {
		return qGray(img.pixel(x, y));
	};
	/* Visible rect 200..900 x 265..815; the corners are cut, the edges are not. */
	const int cut = std::max({lum(201, 266), lum(898, 266), lum(201, 813), lum(898, 813)});
	const int kept = std::min({lum(550, 266), lum(201, 540), lum(898, 540), lum(550, 813), lum(240, 305)});
	R->check(cut < 40 && kept > 215, "corners-follow-crop",
		 QString("corner max %1, edge min %2").arg(cut).arg(kept));
	/* Half-size item, 200x100 at (1300, 100), 20 px corners. */
	const int cutHalf = std::max({lum(1300, 100), lum(1499, 100), lum(1300, 199), lum(1499, 199)});
	const int keptHalf = std::min({lum(1400, 100), lum(1300, 150), lum(1320, 120), lum(1400, 150)});
	R->check(cutHalf < 40 && keptHalf > 215, "corners-scaled-item",
		 QString("corner max %1, edge min %2").arg(cutHalf).arg(keptHalf));
	if (!getenv("LAYOUTS_SELFTEST_KEEP_SHOTS"))
		QFile::remove(p);

	/* The same source in another scene keeps square corners: the radius belongs to the scene. */
	EditContext &ctx = EditContext::instance();
	obs_scene_t *otherScene = obs_scene_create("Layouts Corners B");
	obs_source_t *other = obs_scene_get_source(otherScene);
	g_cornerSceneB = other;
	OBSSourceAutoRelease halfSource = obs_get_source_by_name("Rounded half");
	obs_sceneitem_t *again = obs_scene_add(obs_scene_from_source(other), halfSource);
	vec2 pos, halfScale;
	vec2_set(&pos, 1300, 100);
	vec2_set(&halfScale, 0.5f, 0.5f);
	obs_sceneitem_set_pos(again, &pos);
	obs_sceneitem_set_scale(again, &halfScale);
	obs_frontend_set_current_scene(other);
	ctx.pin(other);
	ctx.markDirty();
	const ItemEntry *h = entry("Rounded half");
	R->check(h && corners::radius(*h) == 0.0, "corners-per-scene-value",
		 h ? QString::number(corners::radius(*h)) : "no item");
}

void checkCornersOtherScene()
{
	char *path = obs_frontend_get_last_screenshot();
	const QString p = QString::fromUtf8(path ? path : "");
	bfree(path);
	QImage img(p);
	const int corner = img.isNull() ? -1 : qGray(img.pixel(1300, 100));
	R->check(corner > 215, "corners-square-in-other-scene", QString("corner %1").arg(corner));
	if (!getenv("LAYOUTS_SELFTEST_KEEP_SHOTS"))
		QFile::remove(p);

	/* Undo puts the square corners back in the first scene (the filter goes away). */
	EditContext &ctx = EditContext::instance();
	ctx.pin(g_cornerScene);
	ctx.markDirty();
	QAction *undo = findAction(QKeySequence(QKeySequence::Undo));
	if (undo)
		undo->trigger();
	ctx.markDirty();
	const ItemEntry *h = entry("Rounded half");
	OBSSourceAutoRelease halfSource = obs_get_source_by_name("Rounded half");
	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(halfSource, obs_module_text("Corners.FilterName"));
	R->check(undo && h && corners::radius(*h) == 0.0 && !filter, "corners-undo",
		 h ? QString::number(corners::radius(*h)) : "no item");

	/* The next groups expect the scene that was on the program. */
	if (g_beforeCorners) {
		obs_frontend_set_current_scene(g_beforeCorners);
		ctx.pin(g_beforeCorners);
	}
	g_beforeCorners = nullptr;
}

/* Corners from Layouts 0.3 were for every scene: editing one scene keeps the others. */
void testCornersMigration()
{
	EditContext &ctx = EditContext::instance();
	OBSSourceAutoRelease legacy = colorSource("Legacy", 400, 200, 0xFFFFFFFF);
	obs_scene_add(obs_scene_from_source(g_cornerScene), legacy);
	obs_scene_add(obs_scene_from_source(g_cornerSceneB), legacy);
	OBSDataAutoRelease fs = obs_data_create();
	obs_data_set_double(fs, "radius", 30);
	OBSSourceAutoRelease filter = obs_source_create_private(corners::kFilterId, "Legacy corners", fs);
	obs_source_filter_add(legacy, filter);
	ctx.pin(g_cornerScene);
	ctx.markDirty();
	selectNames({"Legacy"});
	corners::setRadius(core::parseExpression("10", 0));
	OBSDataAutoRelease after = obs_source_get_settings(filter);
	OBSDataArrayAutoRelease list = obs_data_get_array(after, "scenes");
	double inA = -1, inB = -1;
	for (size_t i = 0; i < obs_data_array_count(list); i++) {
		OBSDataAutoRelease it = obs_data_array_item(list, i);
		const std::string token = obs_data_get_string(it, "scene");
		if (token == obs_source_get_uuid(g_cornerScene))
			inA = obs_data_get_double(it, "radius");
		if (token == obs_source_get_uuid(g_cornerSceneB))
			inB = obs_data_get_double(it, "radius");
	}
	R->check(obs_data_get_double(after, "radius") == 0.0 && std::fabs(inA - 10) < 0.01 &&
			 std::fabs(inB - 30) < 0.01,
		 "corners-legacy-split-per-scene", QString("A %1, B %2").arg(inA).arg(inB));
	obs_source_remove(legacy);
	ctx.follow();
	ctx.markDirty();
}

/* Move Transition (exeldro) draws each source itself during the transition: the corners
 * must stay. Runs only when the Move Transition plugin is loaded. */

/* ---- 0.5 styles: round, diagonal mask, border, shadow (program pixels) ---- */
OBSSourceAutoRelease g_shapeScene;
OBSSourceAutoRelease g_beforeShapes;

void prepareShapes()
{
	EditContext &ctx = EditContext::instance();
	g_beforeShapes = obs_frontend_get_current_scene();
	obs_scene_t *scene = obs_scene_create("Layouts Shapes");
	g_shapeScene = obs_scene_get_source(scene);
	addColor(scene, "Shapes BG", 1920, 1080, 0xFF808080, 0, 0);
	obs_sceneitem_t *round = addColor(scene, "Shape round", 400, 400, 0xFFFFFFFF, 100, 100);
	obs_sceneitem_t *diag = addColor(scene, "Shape diagonal", 400, 400, 0xFFFFFFFF, 700, 100);
	obs_sceneitem_t *shadow = addColor(scene, "Shape shadow", 400, 400, 0xFFFFFFFF, 1300, 100);
	obs_sceneitem_t *border = addColor(scene, "Shape border", 400, 200, 0xFFFFFFFF, 100, 700);
	/* Cropped (OBS draws it through its own texture): 600x400, crop 100 left / 100 top,
	 * visible 500x300 at (1300, 640). */
	obs_sceneitem_t *cropped = addColor(scene, "Shape cropped shadow", 600, 400, 0xFFFFFFFF, 1300, 640);
	obs_sceneitem_crop cc = {100, 100, 0, 0};
	obs_sceneitem_set_crop(cropped, &cc);
	ctx.pin(g_shapeScene);
	ctx.markDirty();
	itemstyle::edit({round}, [](core::Style &st) { st.radiusMode = core::RadiusMode::Round; });
	itemstyle::edit({diag}, [](core::Style &st) {
		st.radiusMode = core::RadiusMode::None;
		st.polygon = {{0, 0}, {0.6, 0}, {0.4, 1}, {0, 1}};
	});
	itemstyle::edit({shadow}, [](core::Style &st) {
		st.radiusMode = core::RadiusMode::None;
		st.shadow = core::ShadowPreset::Hard;
	});
	itemstyle::edit({cropped}, [](core::Style &st) {
		st.radiusMode = core::RadiusMode::None;
		st.shadow = core::ShadowPreset::Hard;
	});
	itemstyle::edit({border}, [](core::Style &st) {
		st.radiusMode = core::RadiusMode::None;
		st.border = true;
		st.borderWidth = 8;
		st.borderColor = 0xFFFF0000;
	});
	obs_frontend_set_current_scene(g_shapeScene);
}

void checkShapes()
{
	char *path = obs_frontend_get_last_screenshot();
	const QString p = QString::fromUtf8(path ? path : "");
	bfree(path);
	QImage img(p);
	R->check(!img.isNull(), "shapes-screenshot", p);
	if (img.isNull())
		return;
	auto lum = [&](int x, int y) {
		return qGray(img.pixel(x, y));
	};
	/* Round: outside the circle (near the corner) the grey background shows. */
	R->check(lum(140, 140) < 160 && lum(300, 300) > 230, "style-round-mask",
		 QString("corner %1 center %2").arg(lum(140, 140)).arg(lum(300, 300)));
	/* Diagonal: right of the slanted edge is cut, left of it is kept. */
	R->check(lum(1060, 300) < 160 && lum(740, 300) > 230, "style-diagonal-mask",
		 QString("cut %1 kept %2").arg(lum(1060, 300)).arg(lum(740, 300)));
	/* Hard shadow: darker than the background just below the item. */
	R->check(lum(1500, 504) < 110 && lum(1500, 300) > 230, "style-shadow-under-item",
		 QString("below %1 item %2").arg(lum(1500, 504)).arg(lum(1500, 300)));
	/* Border: red inside the edge, white in the middle. */
	const QRgb edge = img.pixel(103, 800);
	R->check(qRed(edge) > 200 && qGreen(edge) < 80 && lum(300, 800) > 230, "style-border-inside",
		 QString("edge %1,%2,%3").arg(qRed(edge)).arg(qGreen(edge)).arg(qBlue(edge)));
	/* Cropped item: the shadow is drawn in the scene below it, the item stays intact. */
	R->check(lum(1550, 944) < 110 && lum(1550, 790) > 230 && lum(1290, 790) > 110, "style-shadow-cropped-item",
		 QString("below %1 item %2 left %3").arg(lum(1550, 944)).arg(lum(1550, 790)).arg(lum(1290, 790)));
	if (!getenv("LAYOUTS_SELFTEST_KEEP_SHOTS"))
		QFile::remove(p);
	if (g_beforeShapes)
		obs_frontend_set_current_scene(g_beforeShapes);
}

/* ---- 0.5 Switcher + lost camera failover, end to end on the program ---- */
OBSSourceAutoRelease g_plateau;
OBSSourceAutoRelease g_beforeRegie;

obs_sceneitem_t *programItem(const char *name)
{
	/* What is really on air (the private copy in Studio Mode with duplication). */
	OBSSource scene = commands::onAirScene();
	obs_scene_t *sc = scene ? obs_scene_from_source(scene) : nullptr;
	return sc ? obs_scene_find_source(sc, name) : nullptr;
}

double programItemWidth(const char *name)
{
	obs_sceneitem_t *item = programItem(name);
	if (!item)
		return -1;
	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	obs_bounds_type bt = obs_sceneitem_get_bounds_type(item);
	if (bt != OBS_BOUNDS_NONE) {
		vec2 b;
		obs_sceneitem_get_bounds(item, &b);
		return b.x;
	}
	obs_sceneitem_crop c;
	obs_sceneitem_get_crop(item, &c);
	return (obs_source_get_width(obs_sceneitem_get_source(item)) - c.left - c.right) * scale.x;
}

/* obs-websocket, as Stream Deck / Companion call it: CallVendorRequest on vendor "layouts".
 * Returns the response JSON ("" when obs-websocket is not loaded). */
std::string websocketVendorCall(const char *requestType, obs_data_t *requestData)
{
	proc_handler_t *global = obs_get_proc_handler();
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_t *ph = nullptr;
	if (global && proc_handler_call(global, "obs_websocket_api_get_ph", &cd))
		ph = (proc_handler_t *)calldata_ptr(&cd, "ph");
	calldata_free(&cd);
	if (!ph)
		return "";
	OBSDataAutoRelease req = obs_data_create();
	obs_data_set_string(req, "vendorName", "layouts");
	obs_data_set_string(req, "requestType", requestType);
	if (requestData)
		obs_data_set_obj(req, "requestData", requestData);
	calldata_init(&cd);
	calldata_set_string(&cd, "request_type", "CallVendorRequest");
	calldata_set_string(&cd, "request_data", obs_data_get_json(req)); /* JSON text */
	std::string out;
	if (proc_handler_call(ph, "call_request", &cd)) {
		struct Response {
			unsigned int status;
			char *comment;
			char *data;
		};
		auto *r = (Response *)calldata_ptr(&cd, "response");
		if (r) {
			out = r->data ? r->data : "";
			bfree(r->comment);
			bfree(r->data);
			bfree(r);
		}
	}
	calldata_free(&cd);
	return out;
}

void prepareRegie()
{
	g_beforeRegie = obs_frontend_get_current_scene();
	obs_scene_t *scene = obs_scene_create("Layouts Plateau");
	g_plateau = obs_scene_get_source(scene);
	/* Image sources: removing the file drops the picture like an unplugged capture. */
	const QString file = QDir::temp().filePath("layouts-selftest-cam.png");
	QImage pic(640, 360, QImage::Format_RGB32);
	pic.fill(Qt::red);
	pic.save(file);
	auto image = [&](const char *name) {
		OBSDataAutoRelease st = obs_data_create();
		obs_data_set_string(st, "file", file.toUtf8().constData());
		OBSSourceAutoRelease src = obs_source_create("image_source", name, st, nullptr);
		return obs_scene_add(scene, src);
	};
	obs_sceneitem_t *a = image("REGIE CAM 1");
	obs_sceneitem_t *b = image("REGIE CAM 2");
	roles::setRole({obs_sceneitem_get_source(a), obs_sceneitem_get_source(b)}, core::Role::Camera);
	Regie &r = Regie::instance();
	r.setPlateau("Layouts Plateau");
	r.refresh();
	bool hasDuo = false;
	for (const core::BroadcastTemplate &t : r.layouts())
		hasDuo = hasDuo || t.key == "Cam2Split";
	R->check(r.sources().size() == 2 && hasDuo, "regie-reads-plateau",
		 QString("%1 sources, %2 layouts").arg(r.sources().size()).arg(r.layouts().size()));
	const bool set = r.setLayout("Cam2Split");
	R->check(set && r.preview().key == "Cam2Split" && r.preview().tiles.size() == 2, "regie-layout-in-preview",
		 QString::fromStdString(r.preview().key));
	R->check(r.studioMode() && r.take(), "regie-cut", r.studioMode() ? "" : "studio mode off");
	/* Stream Deck / Companion path: obs-websocket CallVendorRequest. */
	const std::string state = websocketVendorCall("GetState", nullptr);
	R->check(state.find("Cam2Split") != std::string::npos, "websocket-getstate",
		 QString::fromStdString(state.substr(0, 160)));
	const std::string list = websocketVendorCall("ListLayouts", nullptr);
	R->check(list.find("Cam1Full") != std::string::npos, "websocket-listlayouts",
		 QString::number(list.size()) + " bytes");
}

void checkRegieProgram()
{
	const double w1 = programItemWidth("REGIE CAM 1"), w2 = programItemWidth("REGIE CAM 2");
	R->check(Regie::instance().program().key == "Cam2Split" && w1 > 900 && w1 < 980 && w2 > 900 && w2 < 980,
		 "regie-program-duo", QString("%1 / %2").arg(w1).arg(w2));
	/* CAM 2 stops sending video (a capture unplugged): width 0. */
	OBSSourceAutoRelease src = obs_get_source_by_name("REGIE CAM 2");
	OBSDataAutoRelease st = obs_data_create();
	obs_data_set_string(st, "file", "/nonexistent/layouts-selftest-cam.png");
	obs_source_update(src, st);
}

void checkFailoverLost()
{
	obs_sceneitem_t *lost = programItem("REGIE CAM 2");
	const double w1 = programItemWidth("REGIE CAM 1");
	R->check(lost && !obs_sceneitem_visible(lost) && w1 > 1900, "failover-drops-to-solo",
		 QString("cam2 %1, cam1 width %2")
			 .arg(lost && obs_sceneitem_visible(lost) ? "visible" : "hidden")
			 .arg(w1));
	OBSSourceAutoRelease src = obs_get_source_by_name("REGIE CAM 2");
	OBSDataAutoRelease st = obs_data_create();
	obs_data_set_string(st, "file", QDir::temp().filePath("layouts-selftest-cam.png").toUtf8().constData());
	obs_source_update(src, st);
}

void checkFailoverBack()
{
	obs_sceneitem_t *back = programItem("REGIE CAM 2");
	const double w1 = programItemWidth("REGIE CAM 1");
	R->check(back && obs_sceneitem_visible(back) && w1 > 900 && w1 < 980, "failover-restores-layout",
		 QString("cam1 width %1").arg(w1));
	Regie::instance().setPlateau(QString());
	/* Leave the program as the next tests expect it. */
	if (g_beforeRegie)
		obs_frontend_set_current_scene(g_beforeRegie);
}

OBSSourceAutoRelease g_move, g_savedTransition, g_moveA, g_moveB;

bool moveAvailable()
{
	return obs_source_get_display_name("move_transition") != nullptr;
}

void prepareMoveCorners()
{
	EditContext &ctx = EditContext::instance();
	OBSSourceAutoRelease white = colorSource("Moving", 800, 600, 0xFFFFFFFF);
	for (auto *slot : {&g_moveA, &g_moveB}) {
		obs_scene_t *scene = obs_scene_create(slot == &g_moveA ? "Move A" : "Move B");
		*slot = obs_source_get_ref(obs_scene_get_source(scene));
		obs_sceneitem_t *item = obs_scene_add(scene, white);
		obs_sceneitem_crop crop = {100, 50, 0, 0};
		obs_sceneitem_set_crop(item, &crop);
		vec2 pos;
		vec2_set(&pos, 200, 265);
		obs_sceneitem_set_pos(item, &pos);
		obs_scene_release(scene);
		ctx.pin(*slot);
		ctx.markDirty();
		selectNames({"Moving"});
		corners::setRadius(core::parseExpression("40", 0));
	}
	g_savedTransition = obs_get_output_source(0);
	g_move = obs_source_create_private("move_transition", "Layouts move test", nullptr);
	obs_set_output_source(0, g_move);
	corners::watchProgram();
	obs_transition_set(g_move, g_moveA);
	obs_transition_start(g_move, OBS_TRANSITION_MODE_AUTO, 4000, g_moveB);
}

void checkMoveCorners()
{
	char *path = obs_frontend_get_last_screenshot();
	const QString p = QString::fromUtf8(path ? path : "");
	bfree(path);
	QImage img(p);
	const bool mid = g_move && obs_transition_is_active(g_move);
	const int corner = img.isNull() ? -1 : qGray(img.pixel(201, 266));
	const int edge = img.isNull() ? -1 : qGray(img.pixel(550, 266));
	R->check(mid && corner >= 0 && corner < 40 && edge > 215, "corners-during-move-transition",
		 QString("active %1, corner %2, edge %3").arg(mid).arg(corner).arg(edge));
	if (!getenv("LAYOUTS_SELFTEST_KEEP_SHOTS"))
		QFile::remove(p);
	obs_set_output_source(0, g_savedTransition);
	corners::watchProgram();
	g_move = nullptr;
	g_savedTransition = nullptr;
	obs_source_remove(g_moveA);
	obs_source_remove(g_moveB);
	g_moveA = nullptr;
	g_moveB = nullptr;
	EditContext::instance().follow();
}

void liveProgramCheck()
{
	EditContext &ctx = EditContext::instance();
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	ctx.pin(program);
	obs_log(LOG_INFO, "[selftest] INFO program=%s active=%d showing=%d studio=%d edited=%s",
		obs_source_get_name(program), obs_source_active(program), obs_source_showing(program),
		obs_frontend_preview_program_mode_active(), ctx.sceneName().toUtf8().constData());
	R->check(ctx.liveState() == LiveState::Program, "live-program-state", QString::number((int)ctx.liveState()));
	obs_frontend_recording_start();
}

void liveOnAirCheck()
{
	EditContext &ctx = EditContext::instance();
	obs_log(LOG_INFO, "[selftest] INFO recording=%d active=%d edited=%s", obs_frontend_recording_active(),
		obs_source_active(ctx.sceneSource()), ctx.sceneName().toUtf8().constData());
	R->check(ctx.liveState() == LiveState::OnAir, "live-onair-state", QString::number((int)ctx.liveState()));
	Store::instance().settings().blockOnAir = true;
	selectNames({"Clean BG"});
	const core::Rect before = box("Clean BG");
	const bool applied = commands::nudge(10, 0);
	ctx.markDirty();
	R->check(!applied && box("Clean BG") == before, "live-block-refuses", rectStr(box("Clean BG")));
	Store::instance().settings().blockOnAir = false;
	const bool applied2 = commands::nudge(10, 0);
	ctx.markDirty();
	R->check(applied2 && box("Clean BG").x == before.x + 10, "live-allow-edits", rectStr(box("Clean BG")));
	commands::nudge(-10, 0);
	obs_frontend_recording_stop();
	obs_frontend_set_preview_program_mode(true);
}

obs_canvas_t *g_vertical = nullptr;

void testMultiCanvas()
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	ovi.base_width = 1080;
	ovi.base_height = 1920;
	ovi.output_width = 1080;
	ovi.output_height = 1920;
	g_vertical = obs_frontend_add_canvas("Layouts Vertical", &ovi, PROGRAM);
	R->check(g_vertical != nullptr, "canvas-created");
	if (!g_vertical)
		return;
	obs_scene_t *vs = obs_canvas_scene_create(g_vertical, "Vertical Scene");
	addColor(vs, "V1", 300, 300, 0xFF8080FF, 10, 10);
	EditContext::instance().pin(obs_scene_get_source(vs));
	EditContext &ctx = EditContext::instance();
	R->check(ctx.canvasWidth() == 1080 && ctx.canvasHeight() == 1920, "canvas-size-1080x1920",
		 QString("%1x%2").arg(ctx.canvasWidth()).arg(ctx.canvasHeight()));
	selectNames({"V1"});
	commands::align(core::AlignEdge::Right);
	commands::align(core::AlignEdge::Bottom);
	ctx.markDirty();
	R->check(box("V1").right() == 1080 && box("V1").bottom() == 1920, "align-in-vertical-canvas",
		 rectStr(box("V1")));

	/* Broadcast layouts on a portrait canvas: the duo stacks top and bottom. */
	addColor(vs, "V2", 640, 360, 0xFF80FF80, 400, 400);
	ctx.markDirty();
	selectNames({"V1", "V2"});
	core::BroadcastSpec spec;
	spec.gap = 0;
	commands::applyBroadcast("Cam2Split", spec, commands::KindMode::AllCameras, false);
	ctx.markDirty();
	const core::Rect a = box("V1"), b = box("V2");
	R->check(std::fabs(a.w - 1080) < 1 && std::fabs(a.h - 960) < 1 && std::fabs(b.y - 960) < 1 &&
			 std::fabs(b.bottom() - 1920) < 1,
		 "broadcast-portrait-duo-stacks", rectStr(a) + rectStr(b));
	obs_scene_release(vs);
}

void cleanupCanvas()
{
	EditContext::instance().follow();
	if (g_vertical) {
		obs_frontend_remove_canvas(g_vertical);
		obs_canvas_release(g_vertical);
		g_vertical = nullptr;
	}
}

obs_canvas_t *g_cc = nullptr;
OBSSourceAutoRelease g_ccScene;

void constraintsSetup()
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	ovi.base_width = ovi.output_width = 1920;
	ovi.base_height = ovi.output_height = 1080;
	g_cc = obs_frontend_add_canvas("Layouts Constraints", &ovi, PROGRAM);
	R->check(g_cc != nullptr, "constraints-canvas-created");
	if (!g_cc)
		return;
	obs_scene_t *s = obs_canvas_scene_create(g_cc, "Constraints Scene");
	obs_sceneitem_t *r = addColor(s, "KR", 160, 120, 0xFF00AAFF, 1720, 920);
	obs_sceneitem_t *l = addColor(s, "KL", 1840, 100, 0xFFAA00FF, 40, 40);
	addColor(s, "KU", 200, 200, 0xFFFFAA00, 860, 440);
	setConstraints(r, core::HConstraint::Right, core::VConstraint::Bottom);
	setConstraints(l, core::HConstraint::LeftRight, core::VConstraint::Top);
	g_ccScene = obs_scene_get_source(s); /* takes the create reference */
	ConstraintWatcher::instance().recordSizes();
}

void constraintsReset()
{
	if (!g_cc)
		return;
	obs_video_info ovi = {};
	obs_canvas_get_video_info(g_cc, &ovi);
	ovi.base_width = ovi.output_width = 1280;
	ovi.base_height = ovi.output_height = 720;
	R->check(obs_canvas_reset_video(g_cc, &ovi), "constraints-canvas-reset");
}

core::Rect itemRect(const char *name)
{
	obs_sceneitem_t *it = obs_scene_find_source(obs_scene_from_source(g_ccScene), name);
	if (!it)
		return {};
	obs_transform_info info = {};
	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_info2(it, &info);
	obs_sceneitem_get_crop(it, &crop);
	core::ItemModel m;
	m.t = fromObs(info, crop);
	obs_source_t *src = obs_sceneitem_get_source(it);
	m.sourceWidth = obs_source_get_width(src);
	m.sourceHeight = obs_source_get_height(src);
	return core::visualFrame(m).quad.bounds();
}

void constraintsCheck()
{
	if (!g_cc)
		return;
	const core::Rect r = itemRect("KR"), l = itemRect("KL"), u = itemRect("KU");
	R->check(std::fabs(r.right() - 1240) < 1 && std::fabs(r.bottom() - 680) < 1 && std::fabs(r.w - 160) < 1,
		 "constraint-right-bottom", rectStr(r));
	R->check(std::fabs(l.x - 40) < 1 && std::fabs(l.right() - 1240) < 1 && std::fabs(l.y - 40) < 1,
		 "constraint-left-right-top", rectStr(l));
	/* Unconstrained: OBS's own behavior (center offset and size follow the height ratio). */
	const core::Rect expected =
		core::obsRelativeMapping(core::Rect{860, 440, 200, 200}, 1920, 1080, 1280, 720, true);
	R->check(std::fabs(u.x - expected.x) < 1 && std::fabs(u.w - expected.w) < 1, "constraint-none-is-obs-default",
		 rectStr(u) + " expected " + rectStr(expected));
	g_ccScene = nullptr;
	obs_frontend_remove_canvas(g_cc);
	obs_canvas_release(g_cc);
	g_cc = nullptr;
}

/* ---- performance ---- */

OBSSourceAutoRelease g_perfScene;
struct Probe {
	uint64_t frames0 = 0;
	uint32_t total0 = 0, lagged0 = 0;
	std::vector<uint64_t> renderNs;
	std::vector<uint64_t> frameNs;
};
Probe g_probe;

void perfScene(int count)
{
	const QString name = QString("Layouts Perf %1").arg(count);
	obs_scene_t *scene = obs_scene_create(name.toUtf8().constData());
	std::mt19937 rng(42);
	std::uniform_real_distribution<double> px(0, 1800), py(0, 1000);
	for (int i = 0; i < count; i++) {
		const QString n = QString("P%1-%2").arg(count).arg(i);
		obs_sceneitem_t *it = addColor(scene, n.toUtf8().constData(), 120, 80,
					       0xFF000000 | (uint32_t)(rng() & 0xFFFFFF), px(rng), py(rng));
		obs_sceneitem_set_rot(it, (float)(i % 7) * 5.0f);
	}
	g_perfScene = obs_scene_get_source(scene);
	obs_source_get_ref(g_perfScene);
	obs_scene_release(scene);
	obs_frontend_set_current_scene(g_perfScene);
	EditContext::instance().follow();
	g_probe = Probe();
	g_probe.frames0 = g_dock->view()->frames();
	g_probe.total0 = obs_get_total_frames();
	g_probe.lagged0 = obs_get_lagged_frames();
}

void perfSample()
{
	g_probe.renderNs.push_back(g_dock->view()->lastRenderNs());
	g_probe.frameNs.push_back(obs_get_average_frame_time_ns());
}

void perfReport(const char *label)
{
	auto avg = [](const std::vector<uint64_t> &v) {
		uint64_t s = 0;
		for (uint64_t x : v)
			s += x;
		return v.empty() ? 0.0 : (double)s / v.size() / 1e6;
	};
	auto mx = [](const std::vector<uint64_t> &v) {
		uint64_t m = 0;
		for (uint64_t x : v)
			m = std::max(m, x);
		return m / 1e6;
	};
	const uint64_t frames = g_dock->view()->frames() - g_probe.frames0;
	const uint32_t lagged = obs_get_lagged_frames() - g_probe.lagged0;
	const uint32_t total = obs_get_total_frames() - g_probe.total0;
	obs_log(LOG_INFO,
		"[selftest] PERF %s canvas_draw_avg_ms=%.3f canvas_draw_max_ms=%.3f obs_frame_avg_ms=%.3f canvas_frames=%llu obs_frames=%u lagged=%u",
		label, avg(g_probe.renderNs), mx(g_probe.renderNs), avg(g_probe.frameNs), (unsigned long long)frames,
		total, lagged);
	R->check(lagged == 0, "perf-no-lagged-frames", QString("%1 lagged=%2").arg(label).arg(lagged));
}

void setCanvasVisible(bool visible)
{
	if (visible)
		g_window->open();
	else
		g_window->hide();
}

void addPerfSteps(int count)
{
	R->add(500, [count] { perfScene(count); });
	for (int i = 0; i < 10; i++)
		R->add(200, [] { perfSample(); });
	R->add(10, [count] {
		const QByteArray l = QString("items=%1 canvas=visible").arg(count).toUtf8();
		perfReport(l.constData());
		setCanvasVisible(false);
		g_probe = Probe();
		g_probe.total0 = obs_get_total_frames();
		g_probe.lagged0 = obs_get_lagged_frames();
		g_probe.frames0 = g_dock->view()->frames();
	});
	for (int i = 0; i < 10; i++)
		R->add(200, [] { g_probe.frameNs.push_back(obs_get_average_frame_time_ns()); });
	R->add(10, [count] {
		const QByteArray l = QString("items=%1 canvas=hidden").arg(count).toUtf8();
		perfReport(l.constData());
		R->check(g_dock->view()->frames() == g_probe.frames0, "canvas-idle-when-hidden",
			 QString("%1 frames while hidden").arg(g_dock->view()->frames() - g_probe.frames0));
		setCanvasVisible(true);
	});
}

} // namespace

static bool enabled(const char *group)
{
	/* LAYOUTS_SELFTEST_ONLY=a,b runs only those groups (development aid). */
	const char *only = getenv("LAYOUTS_SELFTEST_ONLY");
	if (!only || !*only)
		return strcmp(group, "bare") != 0 && strcmp(group, "rawcanvas") != 0 && strcmp(group, "demo") != 0;
	return QString::fromUtf8(only).split(',').contains(QString::fromUtf8(group));
}

void runSelfTest(ui::LayoutsWindow *window)
{
	static Runner runner;
	R = &runner;
	runner.onDone = [] {
		/* Released here, not by static destructors after libobs is gone. */
		g_cleanScene = nullptr;
		g_cornerScene = nullptr;
		g_beforeCorners = nullptr;
		g_cornerSceneB = nullptr;
		g_move = nullptr;
		g_savedTransition = nullptr;
		g_moveA = nullptr;
		g_moveB = nullptr;
		g_ccScene = nullptr;
		g_perfScene = nullptr;
		g_shapeScene = nullptr;
		g_beforeShapes = nullptr;
		g_plateau = nullptr;
		g_beforeRegie = nullptr;
	};
	g_window = window;
	g_dock = window->canvas();
	obs_log(LOG_INFO, "[selftest] START");
	if (!getenv("LAYOUTS_SELFTEST_HIDDEN"))
		setCanvasVisible(true);

	if (enabled("demo")) {
		/* A realistic scene left open for manual testing and screenshots. */
		R->add(0, [] { obs_frontend_add_scene_collection("Layouts Demo"); });
		R->add(1500, [] {
			obs_frontend_set_preview_program_mode(false);
			EditContext::instance().follow();
			obs_scene_t *s = EditContext::instance().scene();
			addColor(s, "Background", 1920, 1080, 0xFF2A1F1A, 0, 0);
			addColor(s, "Camera", 1280, 720, 0xFF6B4F3A, 96, 96);
			addColor(s, "Guest camera", 480, 270, 0xFF3A5A6B, 1440, 96);
			addColor(s, "Slides", 480, 270, 0xFF4A3A6B, 1440, 420);
			addColor(s, "Lower third", 1100, 120, 0xFF2B6BE5, 96, 880);
			addColor(s, "Logo", 160, 160, 0xFF27C4F5, 1664, 840);
			if (obs_sceneitem_t *bg = obs_scene_find_source(s, "Background"))
				obs_sceneitem_set_locked(bg, true);
			const std::string uuid = EditContext::instance().sceneUuid().toStdString();
			const std::string name = EditContext::instance().sceneName().toStdString();
			Store &st = Store::instance();
			st.guides().add(uuid, name, core::Axis::X, 96, false);
			st.guides().add(uuid, name, core::Axis::X, 1824, false);
			st.guides().add(uuid, name, core::Axis::Y, 96, false);
			st.guides().add(uuid, name, core::Axis::Y, 984, true);
			st.settings().gridKind = GridKind::Columns;
			st.settings().grid = getenv("LAYOUTS_DEMO_GRID") != nullptr;
			st.notifyGuidesChanged();
			st.notifySettingsChanged(false);
			obs_frontend_save();
		});
		R->add(500, [] {
			if (getenv("LAYOUTS_DEMO_CROP")) {
				selectNames({"Camera"});
				QKeyEvent kc(QEvent::KeyPress, Qt::Key_C, Qt::NoModifier, "c");
				QApplication::sendEvent(g_dock->view(), &kc);
				g_window->setGeometry(0, 380, 1200, 620);
				return;
			}
			if (const char *grab = getenv("LAYOUTS_DEMO_TYPES")) {
				/* 0.4: a show named the way people name it, in Scene mode then Builder mode. */
				const std::pair<const char *, const char *> names[] = {
					{"Camera", "CAM _ VALENTIN"},       {"Guest camera", "CAM _ GUEST 1"},
					{"Slides", "SCREEN _ RIGHT"},       {"Background", "BG _ DYNAMIQUE"},
					{"Logo", "LOGO _ \xC3\x89MISSION"},
				};
				for (const auto &[from, to] : names) {
					OBSSourceAutoRelease src = obs_get_source_by_name(from);
					if (src)
						obs_source_set_name(src, to);
				}
				OBSSourceAutoRelease host = obs_get_source_by_name("CAM _ VALENTIN");
				roles::setRole({host}, core::Role::Host);
				EditContext::instance().markDirty();
				selectNames({"CAM _ GUEST 1"});
				g_window->setGeometry(0, 120, 1400, 820);
				const QString base = QString::fromUtf8(grab);
				QTimer::singleShot(1500, [base] {
					g_window->grab().save(base + "-scene.png");
					g_window->setMode(ui::LayoutsWindow::Mode::Builder);
					g_window->builderSources()->selectNames({"BG _ DYNAMIQUE", "CAM _ VALENTIN",
										 "CAM _ GUEST 1", "SCREEN _ RIGHT"});
				});
				QTimer::singleShot(3000, [base] {
					g_window->grab().save(base + "-builder.png");
					g_window->layers()->filter()->select(false, core::Role::Camera);
					g_window->setMode(ui::LayoutsWindow::Mode::Scene);
				});
				QTimer::singleShot(4500, [base] { g_window->grab().save(base + "-filter.png"); });
				return;
			}
			if (getenv("LAYOUTS_DEMO_BROADCAST")) {
				/* Three guests: the Broadcast layouts gallery, first template previewed. */
				Store::instance().settings().broadcastKinds = 1;
				selectNames({"Camera", "Guest camera", "Slides"});
				if (getenv("LAYOUTS_DEMO_PORTRAIT")) {
					obs_video_info ovi = {};
					obs_get_video_info(&ovi);
					ovi.base_width = ovi.output_width = 1080;
					ovi.base_height = ovi.output_height = 1920;
					g_vertical = obs_frontend_add_canvas("Vertical", &ovi, PROGRAM);
					obs_scene_t *vs = obs_canvas_scene_create(g_vertical, "Vertical Scene");
					addColor(vs, "Host", 1280, 720, 0xFF6B4F3A, 0, 0);
					addColor(vs, "Guest 1", 1280, 720, 0xFF3A5A6B, 0, 800);
					addColor(vs, "Guest 2", 1280, 720, 0xFF4A3A6B, 0, 1400);
					EditContext::instance().pin(obs_scene_get_source(vs));
					obs_scene_release(vs);
					selectNames({"Host", "Guest 1", "Guest 2"});
				}
				g_window->setGeometry(0, 120, 1400, 900);
				QTimer::singleShot(800, [] {
					core::BroadcastSpec spec;
					commands::showPreview(commands::broadcastFrames(
						"Cam3Columns", spec, commands::KindMode::AllCameras));
				});
				if (const char *grab = getenv("LAYOUTS_DEMO_GRAB"))
					QTimer::singleShot(2000, [path = QString::fromUtf8(grab)] {
						g_window->grab().save(path);
					});
				return;
			}
			selectNames({"Guest camera", "Slides"});
			if (getenv("LAYOUTS_DEMO_PREVIEW")) {
				core::PresetSpec ps;
				commands::showPreview(commands::presetFrames(core::Preset::SplitScreen, ps));
			}
			g_window->setGeometry(0, 380, 1200, 620);
		});
		R->run();
		return;
	}
	if (enabled("bare")) {
		/* Plugin loaded, docks shown, nothing else: leak baseline. */
		R->add(2000, [] {});
		R->run();
		return;
	}
	R->add(0, [] { obs_frontend_add_scene_collection("Layouts Selftest"); });
	R->add(1500, [] {
		EditContext::instance().follow();
		R->check(EditContext::instance().scene() != nullptr, "scene-available",
			 EditContext::instance().sceneName());
		setupScene();
	});
	if (enabled("core")) {
		R->add(300, [] { testGeometryFidelity(); });
		R->add(300, [] { testCommands(); });
		R->add(300, [] { testUndo(); });
		R->add(300, [] { testCanvasSnapAndRuler(); });
		R->add(300, [] { testLayers(); });
		R->add(300, [] { testLayoutsV02(); });
		R->add(300, [] { testRoles(); });
		R->add(300, [] { testChatRole(); });
		R->add(300, [] { testBroadcast(); });
		R->add(300, [] { testSwap(); });
		R->add(300, [] { testBroadcastScene(); });
		R->add(300, [] { testShowScenes(); });
		R->add(300, [] { testTypes(); });
		R->add(300, [] { testScenesList(); });
		R->add(300, [] { testFrameModes(); });
		R->add(300, [] { unlockAll({"A", "B", "C"}); });
		R->add(300, [] { testCropMode(); });
		R->add(300, [] { testHotkeys(); });
		R->add(300, [] { testGroup(); });
		R->add(500, [] { testGroupCheck(); });
	}
	if (enabled("window")) {
		/* The Layouts window is a regular window holding the Canvas and the Inspector. */
		static uint64_t frames0 = 0;
		R->add(300, [] {
			R->check(g_window->isWindow() && g_dock->window() == g_window &&
					 g_window->inspector()->window() == g_window &&
					 g_window->layers()->window() == g_window,
				 "window-holds-canvas-and-inspector");
			g_window->hide();
		});
		R->add(800, [] { frames0 = g_dock->view()->frames(); });
		R->add(800, [] {
			R->check(g_dock->view()->frames() == frames0, "window-hidden-no-render");
			g_window->open();
		});
		R->add(1200, [] { frames0 = g_dock->view()->frames(); });
		R->add(1000, [] {
			R->check(g_dock->view()->frames() - frames0 > 10, "window-reopened-renders",
				 QString::number(g_dock->view()->frames() - frames0));
			g_window->showMaximized();
		});
		R->add(1500, [] { frames0 = g_dock->view()->frames(); });
		R->add(1000, [] {
			R->check(g_window->isMaximized() && g_dock->view()->frames() - frames0 > 10,
				 "window-maximized-renders", QString::number(g_dock->view()->frames() - frames0));
			g_window->showNormal();
		});
	}
	if (enabled("collections")) {
		R->add(200, [] { testGuidesSaved(); });
		R->add(500, [] { switchAway(); });
		R->add(1500, [] { checkAwayEmpty(); });
		R->add(1500, [] { checkGuidesRestored(); });
	}
	if (enabled("output")) {
		R->add(500, [] { prepareCleanOutput(); });
		R->add(1000, [] { takeShot(); });
		R->add(1500, [] { checkCleanOutput(); });
	}
	if (enabled("regie")) {
		R->add(500, [] { prepareRegie(); });
		R->add(1500, [] { checkRegieProgram(); });
		R->add(3000, [] { checkFailoverLost(); });
		R->add(3500, [] { checkFailoverBack(); });
	}
	if (enabled("corners")) {
		R->add(500, [] { prepareCorners(); });
		R->add(1000, [] { takeShot(); });
		R->add(1500, [] { checkCorners(); });
		R->add(1000, [] { takeShot(); });
		R->add(1500, [] { checkCornersOtherScene(); });
		R->add(300, [] { testCornersMigration(); });
		R->add(300, [] { prepareShapes(); });
		R->add(1200, [] { takeShot(); });
		R->add(1500, [] { checkShapes(); });
		if (moveAvailable()) {
			R->add(300, [] { prepareMoveCorners(); });
			R->add(1800, [] { takeShot(); });
			R->add(800, [] { checkMoveCorners(); });
		}
	}
	if (enabled("live")) {
		R->add(300, [] {
			/* Studio Mode with scene duplication renders a private copy on the program:
			 * edits to the original are not on air (state Off, checked here first). */
			OBSSourceAutoRelease program = obs_frontend_get_current_scene();
			EditContext::instance().pin(program);
			if (obs_frontend_preview_program_mode_active())
				R->check(EditContext::instance().liveState() == LiveState::Off,
					 "live-studio-duplicate-is-off");
			obs_frontend_set_preview_program_mode(false);
		});
		R->add(800, [] { liveProgramCheck(); });
		R->add(2500, [] { liveOnAirCheck(); });
	}
	if (enabled("rawcanvas")) {
		/* Same libobs calls as the canvas test, without Layouts (leak attribution). */
		R->add(500, [] {
			obs_video_info ovi = {};
			obs_get_video_info(&ovi);
			ovi.base_width = ovi.output_width = 1080;
			ovi.base_height = ovi.output_height = 1920;
			obs_canvas_t *c = obs_frontend_add_canvas("Raw Vertical", &ovi, PROGRAM);
			obs_scene_t *vs = obs_canvas_scene_create(c, "Raw Scene");
			addColor(vs, "RV1", 300, 300, 0xFF8080FF, 10, 10);
			obs_scene_release(vs);
			obs_frontend_remove_canvas(c);
			obs_canvas_release(c);
		});
	}
	if (enabled("canvas")) {
		R->add(1500, [] { testMultiCanvas(); });
		R->add(800, [] { cleanupCanvas(); });
		R->add(500, [] { constraintsSetup(); });
		R->add(800, [] { constraintsReset(); });
		R->add(1000, [] { constraintsCheck(); });
	}
	if (enabled("perf")) {
		for (int n : {10, 50, 100, 250})
			addPerfSteps(n);
	}
	R->add(100, [] {
		/* Release test refs while libobs is alive. */
		EditContext::instance().follow();
		g_cleanScene = nullptr;
		g_perfScene = nullptr;
	});
	R->run();
}

} // namespace layouts
