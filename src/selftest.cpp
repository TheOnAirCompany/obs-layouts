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
#include "obs/edit-context.hpp"
#include "obs/obs-convert.hpp"
#include "obs/store.hpp"
#include "ui/canvas-dock.hpp"
#include "ui/canvas-view.hpp"
#include "ui/ruler.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QMainWindow>
#include <QKeyEvent>
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
	void run()
	{
		if (next >= steps.size()) {
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

	/* Preview frames are shown and cleared. */
	commands::showPreview(commands::flowFrames(fs, true));
	R->check(ctx.preview().size() == 3, "preview-shown");
	commands::showPreview({});
	R->check(ctx.preview().empty(), "preview-cleared");

	/* Hovering the presets section of the Inspector previews on the Canvas. */
	selectNames({"A", "B"});
	emit ctx.itemsChanged(); /* let the Inspector list the presets for 2 items */
	if (QWidget *sec = static_cast<QWidget *>(obs_frontend_get_main_window())
				   ->findChild<QWidget *>("LayoutsPresetSection")) {
		QEvent enter(QEvent::Enter);
		QApplication::sendEvent(sec, &enter);
		const size_t shown = ctx.preview().size();
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
	sendMouse(v, QEvent::MouseButtonPress, h, Qt::LeftButton);
	for (int i = 1; i <= 10; i++)
		sendMouse(v, QEvent::MouseMove, h + QPointF(d * i / 10.0, 0), Qt::LeftButton, Qt::ControlModifier);
	sendMouse(v, QEvent::MouseButtonRelease, h + QPointF(d, 0), Qt::NoButton, Qt::ControlModifier);
	ctx.markDirty();
	const core::Rect b1 = box("A").united(box("B"));
	const double k = (b0.w + 100) / b0.w;
	R->check(std::fabs(b1.x - b0.x) < 0.6 && std::fabs(b1.w - b0.w * k) < 1.5 && std::fabs(b1.h - b0.h * k) < 1.5,
		 "multi-resize-keeps-ratio", rectStr(b0) + " -> " + rectStr(b1));
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
	obs_source_t *s = obs_scene_get_source(obs_scene_create("Layouts Clean"));
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
	if (auto *d = qobject_cast<QDockWidget *>(g_dock->parentWidget()))
		d->setVisible(visible);
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

void runSelfTest(ui::CanvasDock *dock)
{
	static Runner runner;
	R = &runner;
	g_dock = dock;
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
			selectNames({"Guest camera", "Slides"});
			if (auto *d = qobject_cast<QDockWidget *>(g_dock->parentWidget()))
				d->setGeometry(0, 560, 820, 440);
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
		R->add(300, [] { testLayoutsV02(); });
		R->add(300, [] { testCropMode(); });
		R->add(300, [] { testGroup(); });
		R->add(500, [] { testGroupCheck(); });
	}
	if (enabled("dock")) {
		/* Docking re-parents the native surface: the display must come back. */
		static uint64_t frames0 = 0;
		R->add(300, [] {
			auto *d = qobject_cast<QDockWidget *>(g_dock->parentWidget());
			auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			d->setFloating(false);
			main->addDockWidget(Qt::BottomDockWidgetArea, d);
			d->show();
		});
		R->add(1500, [] { frames0 = g_dock->view()->frames(); });
		R->add(1000, [] {
			const uint64_t f = g_dock->view()->frames() - frames0;
			R->check(f > 10, "canvas-renders-when-docked", QString::number(f));
			auto *d = qobject_cast<QDockWidget *>(g_dock->parentWidget());
			d->setFloating(true);
			d->resize(900, 600);
		});
		R->add(1500, [] { frames0 = g_dock->view()->frames(); });
		R->add(1000, [] {
			const uint64_t f = g_dock->view()->frames() - frames0;
			R->check(f > 10, "canvas-renders-when-floating-again", QString::number(f));
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
