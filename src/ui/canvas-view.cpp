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

#include "canvas-view.hpp"

#include "theme.hpp"

#include "core/expression.hpp"
#include "obs/commands.hpp"
#include "obs/store.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#endif

#include <QApplication>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlatformSurfaceEvent>
#include <QScreen>
#include <QWheelEvent>
#include <QWindow>

#include <cmath>

namespace layouts::ui {

namespace {

constexpr double kMinZoom = 0.02;
constexpr double kMaxZoom = 64.0;

const core::RefPoint kHandleRefs[8] = {{0, 0}, {0.5, 0}, {1, 0}, {1, 0.5}, {1, 1}, {0.5, 1}, {0, 1}, {0, 0.5}};

bool toGsWindow(QWindow *window, gs_window &gswindow)
{
#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
	return true;
#elif __APPLE__
	gswindow.view = (id)window->winId();
	return true;
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		return true;
	case OBS_NIX_PLATFORM_WAYLAND:
		gswindow.display = (void *)window->winId();
		return gswindow.display != nullptr;
	default:
		return false;
	}
#endif
}

void setColor(gs_eparam_t *param, uint32_t argb)
{
	vec4 c;
	vec4_set(&c, ((argb >> 16) & 0xFF) / 255.0f, ((argb >> 8) & 0xFF) / 255.0f, (argb & 0xFF) / 255.0f,
		 ((argb >> 24) & 0xFF) / 255.0f);
	gs_effect_set_vec4(param, &c);
}

void drawRect(gs_vertbuffer_t *quad, float x, float y, float w, float h, uint32_t argb)
{
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	setColor(gs_effect_get_param_by_name(solid, "color"), argb);
	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.0f);
	gs_matrix_scale3f(w, h, 1.0f);
	gs_load_vertexbuffer(quad);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw(GS_TRISTRIP, 0, 0);
	gs_matrix_pop();
}

void drawLine(gs_vertbuffer_t *quad, float x0, float y0, float x1, float y1, float t, uint32_t argb)
{
	const float dx = x1 - x0, dy = y1 - y0;
	const float len = std::sqrt(dx * dx + dy * dy);
	if (len <= 0.0f)
		return;
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	setColor(gs_effect_get_param_by_name(solid, "color"), argb);
	gs_matrix_push();
	gs_matrix_translate3f(x0, y0, 0.0f);
	gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, std::atan2(dy, dx));
	gs_matrix_translate3f(0.0f, -t / 2.0f, 0.0f);
	gs_matrix_scale3f(len, t, 1.0f);
	gs_load_vertexbuffer(quad);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw(GS_TRISTRIP, 0, 0);
	gs_matrix_pop();
}

QString fmt(double v)
{
	return QString::fromStdString(core::formatNumber(v, 1));
}

double dot(const core::Vec2 &a, const core::Vec2 &b)
{
	return a.x * b.x + a.y * b.y;
}

} // namespace

/* =================================================================== setup */

CanvasView::CanvasView(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	setMinimumSize(160, 90);
	setAccessibleName(T("Canvas.Title"));

	connect(windowHandle(), &QWindow::visibleChanged, this, [this](bool visible) {
		if (visible) {
			createDisplay();
			if (display_) {
				const QSize sz = size() * devicePixelRatioF();
				obs_display_resize(display_, sz.width(), sz.height());
			}
		}
		updateDisplayEnabled();
	});
	connect(windowHandle(), &QWindow::screenChanged, this, [this](QScreen *) {
		if (display_) {
			const QSize sz = size() * devicePixelRatioF();
			obs_display_resize(display_, sz.width(), sz.height());
			obs_display_update_color_space(display_);
		}
		rebuildOverlay();
	});

	struct SurfaceFilter : QObject {
		CanvasView *v;
		explicit SurfaceFilter(CanvasView *view) : QObject(view), v(view) {}
		bool eventFilter(QObject *o, QEvent *e) override
		{
			if (e->type() == QEvent::PlatformSurface &&
			    static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() ==
				    QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
				v->destroyDisplay();
			return QObject::eventFilter(o, e);
		}
	};
	windowHandle()->installEventFilter(new SurfaceFilter(this));

	EditContext &ctx = EditContext::instance();
	connect(&ctx, &EditContext::sceneChanged, this, &CanvasView::onSceneChanged);
	connect(&ctx, &EditContext::itemsChanged, this, &CanvasView::rebuildOverlay);
	connect(&ctx, &EditContext::liveStateChanged, this, &CanvasView::rebuildOverlay);
	connect(&Store::instance(), &Store::guidesChanged, this, &CanvasView::rebuildOverlay);
	connect(&Store::instance(), &Store::settingsChanged, this, &CanvasView::rebuildOverlay);
	onSceneChanged();
}

CanvasView::~CanvasView()
{
	destroying_ = true;
	destroyDisplay();
	setShowing(nullptr);
}

double CanvasView::dpr() const
{
	return devicePixelRatioF();
}

void CanvasView::createDisplay()
{
	if (display_ || destroying_ || !windowHandle() || !windowHandle()->isExposed())
		return;
	const QSize sz = size() * dpr();
	gs_init_data info = {};
	info.cx = (uint32_t)std::max(1, sz.width());
	info.cy = (uint32_t)std::max(1, sz.height());
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
	if (!toGsWindow(windowHandle(), info.window))
		return;
	/* obs_display wants 0xAABBGGRR. */
	const uint32_t argb = tokens::kOutside;
	const uint32_t abgr = (argb & 0xFF00FF00u) | ((argb >> 16) & 0xFFu) | ((argb & 0xFFu) << 16);
	display_ = obs_display_create(&info, abgr);
	if (!display_)
		return;
	obs_display_add_draw_callback(display_, &CanvasView::drawCallback, this);
	updateDisplayEnabled();
	rebuildOverlay();
}

void CanvasView::destroyDisplay()
{
	if (!display_)
		return;
	obs_display_remove_draw_callback(display_, &CanvasView::drawCallback, this);
	obs_display_destroy(display_);
	display_ = nullptr;

	obs_enter_graphics();
	if (unitQuad_) {
		gs_vertexbuffer_destroy(unitQuad_);
		unitQuad_ = nullptr;
	}
	for (auto &[k, v] : textures_)
		gs_texture_destroy(v.first);
	textures_.clear();
	obs_leave_graphics();
}

void CanvasView::updateDisplayEnabled()
{
	const bool visible = isVisible() && windowHandle() && windowHandle()->isVisible();
	/* A hidden Canvas owns no display at all: nothing is rendered, and no OpenGL
	 * surface outlives its native view (macOS can tear windows down first when the
	 * app is terminated). The display is created again when the Canvas shows. */
	if (!visible)
		destroyDisplay();
	else
		createDisplay();
	if (display_)
		obs_display_set_enabled(display_, visible);
	setShowing(visible ? EditContext::instance().sceneSource() : nullptr);
}

void CanvasView::setShowing(obs_source_t *source)
{
	/* Sources of a scene that is not on the program or preview would not be
	 * "showing" (some stop rendering). Count as a viewer, like a projector does. */
	if (source == showing_.Get())
		return;
	if (showing_)
		obs_source_dec_showing(showing_);
	showing_ = source;
	if (showing_)
		obs_source_inc_showing(showing_);
}

void CanvasView::paintEvent(QPaintEvent *)
{
	createDisplay();
}

void CanvasView::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	createDisplay();
	if (display_ && isVisible()) {
		const QSize sz = size() * dpr();
		obs_display_resize(display_, sz.width(), sz.height());
	}
	if (autoFit_)
		zoomFit();
	else
		applyViewChange();
}

void CanvasView::moveEvent(QMoveEvent *e)
{
	QWidget::moveEvent(e);
	if (display_)
		obs_display_update_color_space(display_);
}

void CanvasView::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	createDisplay();
	updateDisplayEnabled();
}

void CanvasView::hideEvent(QHideEvent *e)
{
	QWidget::hideEvent(e);
	updateDisplayEnabled();
}

void CanvasView::onSceneChanged()
{
	cancelInteraction();
	EditContext &ctx = EditContext::instance();
	updateDisplayEnabled();
	if (ctx.canvasWidth() != lastCanvasW_ || ctx.canvasHeight() != lastCanvasH_) {
		lastCanvasW_ = ctx.canvasWidth();
		lastCanvasH_ = ctx.canvasHeight();
		autoFit_ = true;
		zoomFit();
	} else {
		rebuildOverlay();
	}
}

/* ==================================================================== view */

QPointF CanvasView::worldToView(const core::Vec2 &w) const
{
	return {pan_.x() + w.x * zoom_, pan_.y() + w.y * zoom_};
}

core::Vec2 CanvasView::viewToWorld(const QPointF &p) const
{
	return {(p.x() - pan_.x()) / zoom_, (p.y() - pan_.y()) / zoom_};
}

void CanvasView::applyViewChange()
{
	rebuildOverlay();
	emit viewChanged();
}

void CanvasView::zoomAt(double z, const QPointF &anchor)
{
	z = std::clamp(z, kMinZoom, kMaxZoom);
	const core::Vec2 w = viewToWorld(anchor);
	zoom_ = z;
	pan_ = QPointF(anchor.x() - w.x * zoom_, anchor.y() - w.y * zoom_);
	autoFit_ = false;
	applyViewChange();
}

void CanvasView::zoomIn()
{
	zoomAt(zoom_ * 1.25, QPointF(width() / 2.0, height() / 2.0));
}

void CanvasView::zoomOut()
{
	zoomAt(zoom_ / 1.25, QPointF(width() / 2.0, height() / 2.0));
}

void CanvasView::zoom100()
{
	const core::Rect c = EditContext::instance().canvasRect();
	zoom_ = 1.0;
	pan_ = QPointF((width() - c.w) / 2.0, (height() - c.h) / 2.0);
	autoFit_ = false;
	applyViewChange();
}

void CanvasView::zoomFit()
{
	const core::Rect c = EditContext::instance().canvasRect();
	const double margin = 24.0;
	if (c.w <= 0 || c.h <= 0 || width() <= 2 * margin || height() <= 2 * margin) {
		applyViewChange();
		return;
	}
	zoom_ = std::clamp(std::min((width() - 2 * margin) / c.w, (height() - 2 * margin) / c.h), kMinZoom, kMaxZoom);
	pan_ = QPointF((width() - c.w * zoom_) / 2.0, (height() - c.h * zoom_) / 2.0);
	autoFit_ = true;
	applyViewChange();
}

void CanvasView::zoomSelection()
{
	const auto b = selectionBounds();
	if (!b || b->w <= 0 || b->h <= 0) {
		zoomFit();
		return;
	}
	const double margin = 48.0;
	zoom_ = std::clamp(std::min((width() - 2 * margin) / b->w, (height() - 2 * margin) / b->h), kMinZoom, kMaxZoom);
	pan_ = QPointF(width() / 2.0 - b->centerX() * zoom_, height() / 2.0 - b->centerY() * zoom_);
	autoFit_ = false;
	applyViewChange();
}

std::optional<core::Rect> CanvasView::selectionBounds()
{
	std::optional<core::Rect> r;
	for (const ItemEntry *e : EditContext::instance().selection()) {
		const core::Rect b = e->frame.quad.bounds();
		r = r ? r->united(b) : b;
	}
	return r;
}

/* ============================================================== rendering */

void CanvasView::drawCallback(void *param, uint32_t cx, uint32_t cy)
{
	static_cast<CanvasView *>(param)->render(cx, cy);
}

void CanvasView::render(uint32_t cx, uint32_t cy)
{
	std::shared_ptr<const RenderSnapshot> s;
	{
		std::lock_guard<std::mutex> lock(snapMutex_);
		s = snapshot_;
	}
	if (!s || s->zoom <= 0)
		return;
	const uint64_t t0 = os_gettime_ns();

	if (!unitQuad_) {
		gs_render_start(true);
		gs_vertex2f(0.0f, 0.0f);
		gs_vertex2f(1.0f, 0.0f);
		gs_vertex2f(0.0f, 1.0f);
		gs_vertex2f(1.0f, 1.0f);
		unitQuad_ = gs_render_save();
	}

	gs_viewport_push();
	gs_projection_push();
	gs_matrix_push();
	gs_matrix_identity();
	gs_blend_state_push();
	gs_enable_blending(true);
	gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

	gs_set_viewport(0, 0, (int)cx, (int)cy);

	/* ---- scene, in canvas coordinates ---- */
	const float l = (float)(-s->panX / s->zoom);
	const float t = (float)(-s->panY / s->zoom);
	const float r = (float)((cx - s->panX) / s->zoom);
	const float b = (float)((cy - s->panY) / s->zoom);
	gs_ortho(l, r, t, b, -100.0f, 100.0f);
	drawRect(unitQuad_, 0.0f, 0.0f, (float)s->canvasW, (float)s->canvasH, tokens::kCanvasBackdrop);

	gs_blend_state_push();
	gs_reset_blend_state();
	if (s->mainTexture) {
		obs_render_main_texture_src_color_only();
	} else if (s->scene) {
		OBSSourceAutoRelease src = obs_weak_source_get_source(s->scene);
		if (src)
			obs_source_video_render(src);
	}
	gs_load_vertexbuffer(nullptr);
	gs_blend_state_pop();

	/* ---- overlays, in device pixels ---- */
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

	/* Hide what overflows the canvas. */
	const float x0 = (float)s->panX, y0 = (float)s->panY;
	const float x1 = (float)(s->panX + s->canvasW * s->zoom), y1 = (float)(s->panY + s->canvasH * s->zoom);
	const float W = (float)cx, H = (float)cy;
	if (y0 > 0)
		drawRect(unitQuad_, 0, 0, W, y0, s->outside);
	if (y1 < H)
		drawRect(unitQuad_, 0, y1, W, H - y1, s->outside);
	if (x0 > 0)
		drawRect(unitQuad_, 0, std::max(0.0f, y0), x0, std::min(H, y1) - std::max(0.0f, y0), s->outside);
	if (x1 < W)
		drawRect(unitQuad_, x1, std::max(0.0f, y0), W - x1, std::min(H, y1) - std::max(0.0f, y0), s->outside);

	for (const OverlayPrim &p : s->prims) {
		switch (p.kind) {
		case OverlayPrim::Kind::Rect:
			drawRect(unitQuad_, p.x0, p.y0, p.x1, p.y1, p.argb);
			break;
		case OverlayPrim::Kind::Line:
			drawLine(unitQuad_, p.x0, p.y0, p.x1, p.y1, p.thickness, p.argb);
			break;
		case OverlayPrim::Kind::RectOutline: {
			const float th = p.thickness;
			drawRect(unitQuad_, p.x0, p.y0, p.x1, th, p.argb);
			drawRect(unitQuad_, p.x0, p.y0 + p.y1 - th, p.x1, th, p.argb);
			drawRect(unitQuad_, p.x0, p.y0 + th, th, p.y1 - 2 * th, p.argb);
			drawRect(unitQuad_, p.x0 + p.x1 - th, p.y0 + th, th, p.y1 - 2 * th, p.argb);
			break;
		}
		}
	}

	/* ---- text labels (premultiplied textures) ---- */
	const uint64_t frame = frames_.load();
	if (!s->labels.empty()) {
		gs_effect_t *def = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		gs_eparam_t *image = gs_effect_get_param_by_name(def, "image");
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		for (const OverlayLabel &lb : s->labels) {
			const std::string key = lb.key.toStdString();
			auto it = textures_.find(key);
			if (it == textures_.end()) {
				const uint8_t *data = lb.image.constBits();
				gs_texture_t *tex = gs_texture_create(
					(uint32_t)lb.image.width(), (uint32_t)lb.image.height(), GS_BGRA, 1, &data, 0);
				if (!tex)
					continue;
				it = textures_.emplace(key, std::make_pair(tex, frame)).first;
			}
			it->second.second = frame;
			gs_effect_set_texture(image, it->second.first);
			gs_matrix_push();
			gs_matrix_translate3f(std::round(lb.x), std::round(lb.y), 0.0f);
			while (gs_effect_loop(def, "Draw"))
				gs_draw_sprite(it->second.first, 0, (uint32_t)lb.image.width(),
					       (uint32_t)lb.image.height());
			gs_matrix_pop();
		}
	}
	/* Drop label textures unused for a while. */
	if (textures_.size() > 48) {
		for (auto it = textures_.begin(); it != textures_.end();) {
			if (frame - it->second.second > 120) {
				gs_texture_destroy(it->second.first);
				it = textures_.erase(it);
			} else {
				++it;
			}
		}
	}

	gs_blend_state_pop();
	gs_matrix_pop();
	gs_projection_pop();
	gs_viewport_pop();

	frames_++;
	lastRenderNs_ = os_gettime_ns() - t0;
}

QImage CanvasView::labelImage(const QString &text, QRgb background)
{
	const QString key = text + '|' + QString::number(background, 16) + '|' + QString::number(dpr());
	auto it = labelCache_.constFind(key);
	if (it != labelCache_.constEnd())
		return it.value();

	QFont f = font();
	f.setPointSizeF(tokens::kLabelFontPt);
	f.setBold(true);
	const QFontMetrics fm(f);
	const double r = dpr();
	const int w = (int)std::ceil((fm.horizontalAdvance(text) + 2 * tokens::kLabelPadX) * r);
	const int h = (int)std::ceil((fm.height() + 2 * tokens::kLabelPadY) * r);
	QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing);
	p.setRenderHint(QPainter::TextAntialiasing);
	p.scale(r, r);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor::fromRgba(background));
	p.drawRoundedRect(QRectF(0, 0, w / r, h / r), 3, 3);
	p.setPen(Qt::white);
	p.setFont(f);
	p.drawText(QRectF(0, 0, w / r, h / r), Qt::AlignCenter, text);
	p.end();

	if (labelCache_.size() > 256)
		labelCache_.clear();
	labelCache_.insert(key, img);
	return img;
}

void CanvasView::addLabel(RenderSnapshot &s, const QString &text, QRgb bg, float cx, float cy, bool above)
{
	const QImage img = labelImage(text, bg);
	OverlayLabel l;
	l.key = text + '|' + QString::number(bg, 16) + '|' + QString::number(dpr());
	l.image = img;
	l.x = cx - img.width() / 2.0f;
	l.y = above ? cy - img.height() - 4 * (float)dpr() : cy;
	s.labels.push_back(std::move(l));
}

void CanvasView::rebuildOverlay()
{
	EditContext &ctx = EditContext::instance();
	const ViewSettings &vs = Store::instance().settings();
	const double r = dpr();
	auto s = std::make_shared<RenderSnapshot>();
	s->canvasW = ctx.canvasWidth();
	s->canvasH = ctx.canvasHeight();
	s->zoom = zoom_ * r;
	s->panX = pan_.x() * r;
	s->panY = pan_.y() * r;
	s->outside = tokens::kOutside;
	if (ctx.sceneSource()) {
		s->scene = OBSGetWeakRef(ctx.sceneSource());
		s->mainTexture = ctx.isProgramOfMainCanvas();
	}

	const float W = (float)(width() * r), H = (float)(height() * r);
	const float one = (float)std::max(1.0, std::round(r));
	auto dev = [&](const core::Vec2 &w) {
		return QPointF((pan_.x() + w.x * zoom_) * r, (pan_.y() + w.y * zoom_) * r);
	};
	auto rect = [&](float x, float y, float w, float h, uint32_t c) {
		s->prims.push_back({OverlayPrim::Kind::Rect, x, y, w, h, 0, c});
	};
	auto outline = [&](float x, float y, float w, float h, float t, uint32_t c) {
		s->prims.push_back({OverlayPrim::Kind::RectOutline, x, y, w, h, t, c});
	};
	auto line = [&](const QPointF &a, const QPointF &b, float t, uint32_t c) {
		s->prims.push_back(
			{OverlayPrim::Kind::Line, (float)a.x(), (float)a.y(), (float)b.x(), (float)b.y(), t, c});
	};
	auto vline = [&](double worldX, float y0, float y1, float t, uint32_t c) {
		const float x = (float)std::floor(dev({worldX, 0}).x());
		rect(x, y0, t, y1 - y0, c);
	};
	auto hline = [&](double worldY, float x0, float x1, float t, uint32_t c) {
		const float y = (float)std::floor(dev({0, worldY}).y());
		rect(x0, y, x1 - x0, t, c);
	};
	auto quad = [&](const core::Quad &q, float t, uint32_t c) {
		for (int i = 0; i < 4; i++)
			line(dev(q.p[i]), dev(q.p[(i + 1) % 4]), t, c);
	};
	auto worldRect = [&](const core::Rect &wr, float t, uint32_t c) {
		const QPointF a = dev({wr.left(), wr.top()}), b = dev({wr.right(), wr.bottom()});
		outline((float)std::floor(a.x()), (float)std::floor(a.y()),
			(float)(std::floor(b.x()) - std::floor(a.x()) + t),
			(float)(std::floor(b.y()) - std::floor(a.y()) + t), t, c);
	};

	const core::Rect canvas = ctx.canvasRect();
	const QPointF c0 = dev({0, 0}), c1 = dev({canvas.w, canvas.h});

	if (ctx.sceneSource()) {
		/* ---- grid ---- */
		if (vs.grid) {
			if (vs.gridKind == GridKind::Square) {
				const double spacing = vs.square.size * zoom_;
				if (spacing >= 6.0) {
					const core::Vec2 a = viewToWorld({0, 0}),
							 b = viewToWorld({(double)width(), (double)height()});
					for (double x : core::squareLines(vs.square, core::Axis::X, std::max(0.0, a.x),
									  std::min(canvas.w, b.x)))
						vline(x, (float)std::max(0.0, c0.y()),
						      (float)std::min((double)H, c1.y()), one, tokens::kGrid);
					for (double y : core::squareLines(vs.square, core::Axis::Y, std::max(0.0, a.y),
									  std::min(canvas.h, b.y)))
						hline(y, (float)std::max(0.0, c0.x()),
						      (float)std::min((double)W, c1.x()), one, tokens::kGrid);
				}
			} else {
				const bool cols = vs.gridKind == GridKind::Columns;
				for (const core::Track &tr :
				     core::computeTracks(cols ? vs.columns : vs.rows, cols ? canvas.w : canvas.h)) {
					if (cols) {
						const QPointF a = dev({tr.start, 0}), b = dev({tr.end, canvas.h});
						rect((float)a.x(), (float)a.y(), (float)(b.x() - a.x()),
						     (float)(b.y() - a.y()), tokens::kTrackFill);
					} else {
						const QPointF a = dev({0, tr.start}), b = dev({canvas.w, tr.end});
						rect((float)a.x(), (float)a.y(), (float)(b.x() - a.x()),
						     (float)(b.y() - a.y()), tokens::kTrackFill);
					}
				}
			}
		}

		/* ---- safe areas ---- */
		if (vs.safeAreas) {
			for (double pct : {0.035, 0.05}) {
				const double mx = canvas.w * pct, my = canvas.h * pct;
				worldRect(core::Rect::fromEdges(mx, my, canvas.w - mx, canvas.h - my), one,
					  tokens::kSafeArea);
			}
			const double m43 = canvas.w * 0.1625;
			vline(m43, (float)c0.y(), (float)c1.y(), one, tokens::kCenterLine);
			vline(canvas.w - m43, (float)c0.y(), (float)c1.y(), one, tokens::kCenterLine);
		}
		if (vs.customMarginX > 0 || vs.customMarginY > 0) {
			worldRect(core::Rect::fromEdges(vs.customMarginX, vs.customMarginY, canvas.w - vs.customMarginX,
							canvas.h - vs.customMarginY),
				  one, tokens::kSafeArea);
		}
		if (vs.centerLines) {
			vline(canvas.w / 2.0, (float)c0.y(), (float)c1.y(), one, tokens::kCenterLine);
			hline(canvas.h / 2.0, (float)c0.x(), (float)c1.x(), one, tokens::kCenterLine);
		}

		/* ---- items ---- */
		const std::vector<ItemEntry> &items = ctx.items();
		const std::vector<const ItemEntry *> sel = ctx.selection();
		if (vs.outlines) {
			for (const ItemEntry &e : items) {
				if (e.visible && e.groupId == 0 && !e.selected)
					quad(e.frame.quad, one, tokens::kOutline);
			}
		}
		if (mode_ == Mode::Idle || mode_ == Mode::Guide) {
			if (const ItemEntry *h = ctx.find(hover_.second, hover_.first)) {
				if (!h->selected)
					quad(h->frame.quad, 2 * one, tokens::kHover);
			}
		}
		const int key = sel.size() > 1 && commands::alignTarget() == core::AlignTarget::KeyObject
					? commands::keyIndex()
					: -1;
		for (size_t i = 0; i < sel.size(); i++) {
			const ItemEntry *e = sel[i];
			quad(e->frame.quad, ((int)i == key ? 3 : 1) * one,
			     e->locked ? tokens::kLocked : tokens::kSelection);
		}
		if (sel.size() > 1) {
			if (const auto b = selectionBounds())
				worldRect(*b, one, tokens::kSelection);
		}

		/* ---- guides ---- */
		if (vs.guides) {
			const std::string uuid = ctx.sceneUuid().toStdString();
			const uint32_t base = vs.guideColor.rgb() & 0x00FFFFFF;
			for (const auto &ref : Store::instance().guides().visibleGuides(uuid)) {
				const bool active = (mode_ == Mode::Guide && ref.guide.id == guideId_);
				uint32_t c = base | (ref.global ? 0x99000000u : 0xFF000000u);
				if (ref.guide.locked)
					c = base | 0x66000000u;
				const float t = active ? 2 * one : one;
				if (ref.guide.axis == core::Axis::X)
					vline(ref.guide.pos, 0, H, t, c);
				else
					hline(ref.guide.pos, 0, W, t, c);
			}
		}
		if (mode_ == Mode::Guide && guideIsNew_ && !guideOutside_) {
			const uint32_t c = (vs.guideColor.rgb() & 0x00FFFFFF) | 0xFF000000u;
			if (guideAxis_ == core::Axis::X)
				vline(guidePos_, 0, H, 2 * one, c);
			else
				hline(guidePos_, 0, W, 2 * one, c);
		}
		if (mode_ == Mode::Guide && !guideOutside_) {
			const QPointF p = dev(guideAxis_ == core::Axis::X ? core::Vec2{guidePos_, cursorWorld_.y}
									  : core::Vec2{cursorWorld_.x, guidePos_});
			addLabel(*s, (guideAxis_ == core::Axis::X ? "X " : "Y ") + fmt(guidePos_), vs.guideColor.rgba(),
				 (float)p.x() + (guideAxis_ == core::Axis::X ? 40 * (float)r : 0),
				 (float)p.y() - (guideAxis_ == core::Axis::Y ? 4 * (float)r : 0), true);
		}

		/* ---- snapping feedback ---- */
		auto snapLine = [&](const core::SnapLine &sl) {
			if (sl.axis == core::Axis::X)
				line(dev({sl.pos, sl.from}), dev({sl.pos, sl.to}), one, tokens::kSmartGuide);
			else
				line(dev({sl.from, sl.pos}), dev({sl.to, sl.pos}), one, tokens::kSmartGuide);
		};
		if (mode_ == Mode::Move || mode_ == Mode::Resize) {
			for (const core::SnapLine &sl : snap_.lines)
				snapLine(sl);
			for (const core::SpacingHint &h : snap_.spacings) {
				for (const auto &seg : h.segments) {
					const core::Vec2 a = h.axis == core::Axis::X ? core::Vec2{seg.start, seg.cross}
										     : core::Vec2{seg.cross, seg.start};
					const core::Vec2 b = h.axis == core::Axis::X ? core::Vec2{seg.end, seg.cross}
										     : core::Vec2{seg.cross, seg.end};
					line(dev(a), dev(b), one, tokens::kSmartGuide);
					const QPointF m = (dev(a) + dev(b)) / 2.0;
					addLabel(*s, fmt(h.gap), tokens::kSmartGuide, (float)m.x(), (float)m.y(), true);
				}
			}
		}
		if (valueSnap_ && (mode_ == Mode::Guide || mode_ == Mode::Resize))
			snapLine(*valueSnap_);

		/* ---- measurements (Alt) ---- */
		if (altHeld_ && mode_ == Mode::Idle && !sel.empty()) {
			const auto selBox = selectionBounds();
			core::Rect other = canvas;
			const ItemEntry *h = ctx.find(hover_.second, hover_.first);
			if (h && !h->selected)
				other = h->frame.quad.bounds();
			else if (h && h->selected)
				other = canvas;
			if (selBox) {
				if (h && !h->selected)
					worldRect(other, one, tokens::kMeasure);
				for (const core::Measure &m : core::measureBetween(*selBox, other)) {
					const core::Vec2 a = m.axis == core::Axis::X ? core::Vec2{m.start, m.cross}
										     : core::Vec2{m.cross, m.start};
					const core::Vec2 b = m.axis == core::Axis::X ? core::Vec2{m.end, m.cross}
										     : core::Vec2{m.cross, m.end};
					line(dev(a), dev(b), one, tokens::kMeasure);
					const QPointF mid = (dev(a) + dev(b)) / 2.0;
					addLabel(*s, fmt(m.value()), tokens::kMeasure, (float)mid.x(),
						 (float)mid.y() -
							 (m.axis == core::Axis::X ? 2 * (float)r : -6 * (float)r),
						 m.axis == core::Axis::X);
				}
			}
		}

		/* ---- rubber band ---- */
		if (mode_ == Mode::Rubber && dragStarted_) {
			const core::Rect rb = core::Rect::fromEdges(std::min(pressWorld_.x, rubberEnd_.x),
								    std::min(pressWorld_.y, rubberEnd_.y),
								    std::max(pressWorld_.x, rubberEnd_.x),
								    std::max(pressWorld_.y, rubberEnd_.y));
			const QPointF a = dev({rb.left(), rb.top()}), b = dev({rb.right(), rb.bottom()});
			rect((float)a.x(), (float)a.y(), (float)(b.x() - a.x()), (float)(b.y() - a.y()),
			     tokens::kRubberFill);
			outline((float)a.x(), (float)a.y(), (float)(b.x() - a.x()), (float)(b.y() - a.y()), one,
				tokens::kSelection);
		}

		/* ---- handles and size label ---- */
		if (sel.size() == 1 && !sel.front()->locked) {
			const ItemEntry *e = sel.front();
			const float hs = (float)(tokens::kHandleSize * r);
			for (const core::RefPoint &hr : kHandleRefs) {
				const QPointF p = dev(e->frame.point(hr));
				rect((float)p.x() - hs / 2, (float)p.y() - hs / 2, hs, hs, 0xFFFFFFFF);
				outline((float)p.x() - hs / 2, (float)p.y() - hs / 2, hs, hs, one, tokens::kSelection);
			}
		}
		if (!sel.empty() && mode_ != Mode::Guide && mode_ != Mode::Rubber) {
			const auto b = selectionBounds();
			if (b) {
				QString text;
				if (sel.size() == 1)
					text = fmt(sel.front()->frame.width) + QString::fromUtf8(" \xC3\x97 ") +
					       fmt(sel.front()->frame.height);
				else
					text = fmt(b->w) + QString::fromUtf8(" \xC3\x97 ") + fmt(b->h);
				if (mode_ == Mode::Rotate && sel.size() == 1)
					text = fmt(core::worldRotation(sel.front()->model, sel.front()->parent)) +
					       QString::fromUtf8("\xC2\xB0");
				const QPointF p = dev({b->centerX(), b->bottom()});
				addLabel(*s, text, tokens::kSelection, (float)p.x(), (float)p.y() + 8 * (float)r,
					 false);
			}
		}

		/* ---- live frame ---- */
		if (ctx.liveState() == LiveState::OnAir)
			outline((float)c0.x() - 2 * one, (float)c0.y() - 2 * one, (float)(c1.x() - c0.x()) + 4 * one,
				(float)(c1.y() - c0.y()) + 4 * one, 2 * one, tokens::kLiveFrame);
	}

	{
		std::lock_guard<std::mutex> lock(snapMutex_);
		snapshot_ = std::move(s);
	}
}

/* ============================================================ hit testing */

const ItemEntry *CanvasView::singleEditable()
{
	const auto sel = EditContext::instance().selection();
	if (sel.size() != 1 || sel.front()->locked)
		return nullptr;
	return sel.front();
}

int CanvasView::handleAt(const QPointF &p, const ItemEntry &e) const
{
	for (int i = 0; i < 8; i++) {
		const QPointF h = worldToView(e.frame.point(kHandleRefs[i]));
		if (std::hypot(h.x() - p.x(), h.y() - p.y()) <= tokens::kHandleHit)
			return i;
	}
	/* Rotation zone: just outside a corner. */
	const core::Vec2 w = viewToWorld(p);
	if (!e.frame.quad.contains(w)) {
		for (int i : {0, 2, 4, 6}) {
			const QPointF h = worldToView(e.frame.point(kHandleRefs[i]));
			if (std::hypot(h.x() - p.x(), h.y() - p.y()) <= tokens::kRotateZone)
				return 8;
		}
	}
	return -1;
}

std::optional<uint64_t> CanvasView::guideAt(const QPointF &p) const
{
	const ViewSettings &vs = Store::instance().settings();
	if (!vs.guides)
		return std::nullopt;
	const std::string uuid = EditContext::instance().sceneUuid().toStdString();
	std::optional<uint64_t> best;
	double bestD = tokens::kGuideHit + 0.01;
	for (const auto &ref : Store::instance().guides().visibleGuides(uuid)) {
		const QPointF g = worldToView({ref.guide.pos, ref.guide.pos});
		const double d = ref.guide.axis == core::Axis::X ? std::fabs(g.x() - p.x()) : std::fabs(g.y() - p.y());
		if (d < bestD) {
			bestD = d;
			best = ref.guide.id;
		}
	}
	return best;
}

void CanvasView::updateHoverCursor(const QPointF &p)
{
	EditContext &ctx = EditContext::instance();
	if (spaceHeld_) {
		setCursor(Qt::OpenHandCursor);
		return;
	}
	if (const ItemEntry *e = singleEditable()) {
		const int h = handleAt(p, *e);
		if (h >= 0) {
			static const Qt::CursorShape shapes[8] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor,
								  Qt::SizeBDiagCursor, Qt::SizeHorCursor,
								  Qt::SizeFDiagCursor, Qt::SizeVerCursor,
								  Qt::SizeBDiagCursor, Qt::SizeHorCursor};
			setCursor(h == 8 ? Qt::CrossCursor : shapes[h]);
			return;
		}
	}
	if (const auto g = guideAt(p)) {
		const auto ref = Store::instance().guides().find(*g);
		if (ref && !ref->guide.locked) {
			setCursor(ref->guide.axis == core::Axis::X ? Qt::SplitHCursor : Qt::SplitVCursor);
			return;
		}
	}
	(void)ctx;
	unsetCursor();
}

/* ============================================================= interaction */

core::SnapContext CanvasView::snapContext(bool excludeSelection)
{
	EditContext &ctx = EditContext::instance();
	const ViewSettings &vs = Store::instance().settings();
	core::SnapContext c;
	c.canvas = ctx.canvasRect();
	for (const auto &ref : Store::instance().guides().visibleGuides(ctx.sceneUuid().toStdString())) {
		(ref.guide.axis == core::Axis::X ? c.guideX : c.guideY).push_back(ref.guide.pos);
	}
	if (vs.gridKind == GridKind::Square)
		c.square = vs.square;
	else if (vs.gridKind == GridKind::Columns)
		c.trackX = core::trackEdges(core::computeTracks(vs.columns, c.canvas.w));
	else
		c.trackY = core::trackEdges(core::computeTracks(vs.rows, c.canvas.h));

	for (const ItemEntry &e : ctx.items()) {
		if (!e.visible || (excludeSelection && e.selected))
			continue;
		if (excludeSelection && e.groupId != 0) {
			const ItemEntry *g = ctx.find(e.groupId, 0);
			if (g && g->selected)
				continue;
		}
		if (e.isGroup && excludeSelection) {
			/* a group whose child is being dragged is not a target */
			bool childSelected = false;
			for (const ItemEntry &c2 : ctx.items())
				childSelected = childSelected || (c2.groupId == e.id && c2.selected);
			if (childSelected)
				continue;
		}
		c.others.push_back(e.frame.quad.bounds());
	}
	return c;
}

void CanvasView::mousePressEvent(QMouseEvent *e)
{
	setFocus(Qt::MouseFocusReason);
	EditContext &ctx = EditContext::instance();
	const QPointF p = e->position();
	pressView_ = p;
	pressWorld_ = viewToWorld(p);
	dragStarted_ = false;

	if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && spaceHeld_)) {
		mode_ = Mode::Pan;
		panStart_ = p;
		panOrigin_ = pan_;
		setCursor(Qt::ClosedHandCursor);
		return;
	}
	if (e->button() != Qt::LeftButton || !ctx.sceneSource())
		return QWidget::mousePressEvent(e);

	const Qt::KeyboardModifiers mods = e->modifiers();

	/* Handles of the single selected item. */
	if (const ItemEntry *item = singleEditable()) {
		const int h = handleAt(p, *item);
		if (h >= 0) {
			dragItems_ = {*item};
			handle_ = h;
			mode_ = h == 8 ? Mode::Rotate : Mode::Resize;
			if (mode_ == Mode::Rotate) {
				rotCenter_ = item->frame.center();
				rotStartAngle_ = core::radToDeg(
					std::atan2(pressWorld_.y - rotCenter_.y, pressWorld_.x - rotCenter_.x));
				rotStartRot_ = core::worldRotation(item->model, item->parent);
			}
			return;
		}
	}

	/* Guides. */
	if (const auto g = guideAt(p)) {
		const auto ref = Store::instance().guides().find(*g);
		if (ref && !ref->guide.locked) {
			mode_ = Mode::Guide;
			guideId_ = *g;
			guideIsNew_ = false;
			guideAxis_ = ref->guide.axis;
			guidePos_ = ref->guide.pos;
			guideOutside_ = false;
			return;
		}
	}

	/* Items. Ctrl/Cmd+click selects inside groups. */
	const bool deep = mods & Qt::ControlModifier;
	const ItemEntry *hit = ctx.topmostAt(pressWorld_, deep);
	if (hit) {
		if (mods & Qt::ShiftModifier) {
			ctx.toggle(hit);
			if (!hit->selected) {
				/* just deselected: no drag */
				mode_ = Mode::Idle;
				return;
			}
		} else if (!hit->selected) {
			ctx.select({hit}, false);
		}
		mode_ = Mode::Move;
		return;
	}

	/* Empty canvas: rubber band. */
	rubberAdditive_ = mods & Qt::ShiftModifier;
	rubberBase_.clear();
	if (rubberAdditive_) {
		for (const ItemEntry *s : ctx.selection())
			rubberBase_.emplace_back(s->groupId, s->id);
	} else {
		ctx.clearSelection();
	}
	rubberEnd_ = pressWorld_;
	mode_ = Mode::Rubber;
}

void CanvasView::startMove(const QPointF &)
{
	EditContext &ctx = EditContext::instance();
	dragItems_.clear();
	std::optional<core::Rect> u;
	for (const ItemEntry *e : ctx.selection()) {
		if (e->locked)
			continue;
		/* A child of a selected group moves with the group. */
		if (e->groupId) {
			const ItemEntry *g = ctx.find(e->groupId, 0);
			if (g && g->selected)
				continue;
		}
		dragItems_.push_back(*e);
		const core::Rect b = e->frame.quad.bounds();
		u = u ? u->united(b) : b;
	}
	dragUnion_ = u.value_or(core::Rect());
	session_ = std::make_unique<EditSession>();
	if (dragItems_.empty() || !session_->begin(T("Undo.Move"))) {
		session_.reset();
		dragItems_.clear();
		mode_ = Mode::Idle;
	}
}

void CanvasView::updateMove(const QPointF &p, Qt::KeyboardModifiers mods)
{
	if (!session_)
		return;
	const core::Vec2 w = viewToWorld(p);
	core::Vec2 delta = w - pressWorld_;
	if (mods & Qt::ShiftModifier) {
		if (std::fabs(delta.x) >= std::fabs(delta.y))
			delta.y = 0;
		else
			delta.x = 0;
	}
	/* Whole canvas pixels at normal zoom, finer when zoomed in. */
	const double step = zoom_ >= 4.0 ? 0.5 : 1.0;
	delta.x = std::round(delta.x / step) * step;
	delta.y = std::round(delta.y / step) * step;

	snap_ = core::SnapResult();
	core::SnapSettings ss = Store::instance().settings().snap;
	if (!(mods & Qt::ControlModifier) && ss.enabled) {
		snap_ = core::snapMove(dragUnion_.translated(delta), snapContext(true), ss, ss.thresholdPx / zoom_,
				       !(mods & Qt::ShiftModifier) || delta.x != 0,
				       !(mods & Qt::ShiftModifier) || delta.y != 0);
		delta += snap_.delta;
	}

	std::vector<Change> changes;
	for (const ItemEntry &e : dragItems_)
		changes.push_back({e.item, core::moveBy(e.model, e.parent, delta)});
	session_->apply(changes);
	rebuildOverlay();
}

void CanvasView::updateResize(const QPointF &p, Qt::KeyboardModifiers mods)
{
	if (!session_ || dragItems_.empty())
		return;
	const ItemEntry &e = dragItems_.front();
	const core::VisualFrame &f = e.frame;
	if (f.width <= 0 || f.height <= 0)
		return;
	const core::Vec2 ex = (f.quad.p[1] - f.quad.p[0]) / f.width;
	const core::Vec2 ey = (f.quad.p[3] - f.quad.p[0]) / f.height;
	const core::Vec2 w = viewToWorld(p);
	const core::Vec2 d = w - f.quad.p[0];
	double u = dot(d, ex), v = dot(d, ey);

	const core::RefPoint hr = kHandleRefs[handle_];
	const bool fromCenter = mods & Qt::AltModifier;
	const double ax = fromCenter ? 0.5 : 1.0 - hr.fx;
	const double ay = fromCenter ? 0.5 : 1.0 - hr.fy;

	/* Snap the moving edges when the item is axis aligned. */
	valueSnap_.reset();
	snap_ = core::SnapResult();
	const bool aligned = std::fabs(std::remainder(f.rotation, 90.0)) < 0.01 && std::fabs(f.rotation) < 0.01;
	core::SnapSettings ss = Store::instance().settings().snap;
	if (aligned && ss.enabled && !(mods & Qt::ControlModifier)) {
		const core::SnapContext ctx = snapContext(true);
		const double th = ss.thresholdPx / zoom_;
		const core::Rect b = f.quad.bounds();
		if (hr.fx != 0.5) {
			const core::ValueSnap vx =
				core::snapValue(f.quad.p[0].x + u, core::Axis::X, ctx, ss, th, b.top(), b.bottom());
			if (vx.snapped) {
				u = vx.value - f.quad.p[0].x;
				if (vx.line)
					snap_.lines.push_back(*vx.line);
			}
		}
		if (hr.fy != 0.5) {
			const core::ValueSnap vy =
				core::snapValue(f.quad.p[0].y + v, core::Axis::Y, ctx, ss, th, b.left(), b.right());
			if (vy.snapped) {
				v = vy.value - f.quad.p[0].y;
				if (vy.line)
					snap_.lines.push_back(*vy.line);
			}
		}
	}

	double nw = f.width, nh = f.height;
	if (hr.fx != 0.5) {
		const double anchorU = ax * f.width;
		nw = fromCenter ? 2.0 * std::fabs(u - anchorU) : std::fabs(u - anchorU);
	}
	if (hr.fy != 0.5) {
		const double anchorV = ay * f.height;
		nh = fromCenter ? 2.0 * std::fabs(v - anchorV) : std::fabs(v - anchorV);
	}
	/* Corners keep the aspect ratio like OBS; Shift frees it. */
	const bool corner = hr.fx != 0.5 && hr.fy != 0.5;
	if (corner && !(mods & Qt::ShiftModifier)) {
		const double kx = nw / f.width, ky = nh / f.height;
		const double k = std::max(kx, ky);
		nw = f.width * k;
		nh = f.height * k;
	}
	nw = std::max(1.0, std::round(nw));
	nh = std::max(1.0, std::round(nh));

	session_->apply({{e.item, core::setWorldSize(e.model, e.parent, nw, nh, core::RefPoint{ax, ay})}});
	rebuildOverlay();
}

void CanvasView::updateRotate(const QPointF &p, Qt::KeyboardModifiers mods)
{
	if (!session_ || dragItems_.empty())
		return;
	const ItemEntry &e = dragItems_.front();
	const core::Vec2 w = viewToWorld(p);
	const double a = core::radToDeg(std::atan2(w.y - rotCenter_.y, w.x - rotCenter_.x));
	double rot = rotStartRot_ + (a - rotStartAngle_);
	if (mods & Qt::ShiftModifier) {
		rot = std::round(rot / 15.0) * 15.0;
	} else if (!(mods & Qt::ControlModifier)) {
		const double n = std::round(rot / 90.0) * 90.0;
		if (std::fabs(rot - n) < 2.0)
			rot = n;
	}
	rot = std::round(rot * 10.0) / 10.0;
	session_->apply({{e.item, core::setWorldRotation(e.model, e.parent, rot)}});
	rebuildOverlay();
}

void CanvasView::updateGuide(const QPointF &p, Qt::KeyboardModifiers mods)
{
	const core::Vec2 w = viewToWorld(p);
	guideOutside_ = guideAxis_ == core::Axis::X ? p.x() < 0 : p.y() < 0;
	double pos = std::round(guideAxis_ == core::Axis::X ? w.x : w.y);
	valueSnap_.reset();
	core::SnapSettings ss = Store::instance().settings().snap;
	ss.guides = false;
	if (ss.enabled && !(mods & Qt::ControlModifier)) {
		const core::Rect c = EditContext::instance().canvasRect();
		const core::ValueSnap v = core::snapValue(pos, guideAxis_, snapContext(false), ss,
							  ss.thresholdPx / zoom_,
							  guideAxis_ == core::Axis::X ? c.top() : c.left(),
							  guideAxis_ == core::Axis::X ? c.bottom() : c.right());
		if (v.snapped) {
			pos = v.value;
			valueSnap_ = v.line;
		}
	}
	guidePos_ = pos;
	if (!guideIsNew_ && guideId_)
		Store::instance().guides().move(guideId_, pos);
	cursorWorld_ = w;
	rebuildOverlay();
	emit cursorChanged();
}

void CanvasView::finishGuide(const QPointF &p)
{
	EditContext &ctx = EditContext::instance();
	const bool outside = guideAxis_ == core::Axis::X ? p.x() < 0 : p.y() < 0;
	core::GuideDocument &doc = Store::instance().guides();
	if (guideIsNew_) {
		if (!outside && ctx.sceneSource())
			doc.add(ctx.sceneUuid().toStdString(), ctx.sceneName().toStdString(), guideAxis_, guidePos_,
				false);
	} else if (outside) {
		doc.remove(guideId_);
	}
	mode_ = Mode::Idle;
	guideId_ = 0;
	guideIsNew_ = false;
	valueSnap_.reset();
	Store::instance().notifyGuidesChanged();
	obs_frontend_save(); /* guides live in the collection */
}

void CanvasView::updateRubber(const QPointF &p)
{
	EditContext &ctx = EditContext::instance();
	rubberEnd_ = viewToWorld(p);
	const core::Rect rb =
		core::Rect::fromEdges(std::min(pressWorld_.x, rubberEnd_.x), std::min(pressWorld_.y, rubberEnd_.y),
				      std::max(pressWorld_.x, rubberEnd_.x), std::max(pressWorld_.y, rubberEnd_.y));
	std::vector<const ItemEntry *> hits;
	for (const ItemEntry &e : ctx.items()) {
		if (!e.visible || e.groupId != 0)
			continue;
		const bool inBase = std::find(rubberBase_.begin(), rubberBase_.end(),
					      std::make_pair(e.groupId, e.id)) != rubberBase_.end();
		if (inBase || e.frame.quad.intersects(rb))
			hits.push_back(&e);
	}
	ctx.select(hits, false);
	rebuildOverlay();
}

void CanvasView::cancelInteraction()
{
	if (session_) {
		session_->cancel();
		session_.reset();
	}
	if (mode_ == Mode::Guide && !guideIsNew_ && guideId_)
		Store::instance().notifyGuidesChanged();
	mode_ = Mode::Idle;
	dragItems_.clear();
	snap_ = core::SnapResult();
	valueSnap_.reset();
	dragStarted_ = false;
	unsetCursor();
}

void CanvasView::mouseMoveEvent(QMouseEvent *e)
{
	const QPointF p = e->position();
	const core::Vec2 w = viewToWorld(p);
	cursorWorld_ = w;
	cursorInside_ = true;
	altHeld_ = e->modifiers() & Qt::AltModifier;
	EditContext &ctx = EditContext::instance();

	if (mode_ == Mode::Pan) {
		pan_ = panOrigin_ + (p - panStart_);
		autoFit_ = false;
		applyViewChange();
		return;
	}

	if (mode_ != Mode::Idle && !dragStarted_) {
		if (std::hypot(p.x() - pressView_.x(), p.y() - pressView_.y()) < tokens::kDragThreshold)
			return;
		dragStarted_ = true;
		if (mode_ == Mode::Move) {
			startMove(p);
		} else if (mode_ == Mode::Resize || mode_ == Mode::Rotate) {
			session_ = std::make_unique<EditSession>();
			if (!session_->begin(T(mode_ == Mode::Resize ? "Undo.Size" : "Undo.Rotation"))) {
				session_.reset();
				mode_ = Mode::Idle;
			}
		}
	}

	switch (mode_) {
	case Mode::Move:
		updateMove(p, e->modifiers());
		break;
	case Mode::Resize:
		updateResize(p, e->modifiers());
		break;
	case Mode::Rotate:
		updateRotate(p, e->modifiers());
		break;
	case Mode::Guide:
		updateGuide(p, e->modifiers());
		break;
	case Mode::Rubber:
		updateRubber(p);
		break;
	default: {
		const ItemEntry *h = ctx.topmostAt(w, e->modifiers() & Qt::ControlModifier);
		const std::pair<int64_t, int64_t> key = h ? std::make_pair(h->groupId, h->id)
							  : std::make_pair(int64_t(-1), int64_t(-1));
		if (key != hover_ || altHeld_) {
			hover_ = key;
			rebuildOverlay();
		}
		updateHoverCursor(p);
		break;
	}
	}
	emit cursorChanged();
}

void CanvasView::mouseReleaseEvent(QMouseEvent *e)
{
	if (mode_ == Mode::Pan) {
		mode_ = Mode::Idle;
		updateHoverCursor(e->position());
		return;
	}
	if (mode_ == Mode::Guide) {
		if (dragStarted_ || guideIsNew_)
			finishGuide(e->position());
		else
			mode_ = Mode::Idle;
	}
	if (session_) {
		session_->commit();
		session_.reset();
	}
	mode_ = Mode::Idle;
	dragItems_.clear();
	snap_ = core::SnapResult();
	valueSnap_.reset();
	dragStarted_ = false;
	rebuildOverlay();
	updateHoverCursor(e->position());
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *e)
{
	EditContext &ctx = EditContext::instance();
	const core::Vec2 w = viewToWorld(e->position());
	if (const auto g = guideAt(e->position())) {
		const auto ref = Store::instance().guides().find(*g);
		if (!ref)
			return;
		bool ok = false;
		const QString text = QInputDialog::getText(this, T("Guide.SetPosition"), T("Guide.Position"),
							   QLineEdit::Normal, fmt(ref->guide.pos), &ok);
		if (ok) {
			const core::Rect c = ctx.canvasRect();
			const core::Expression ex =
				core::parseExpression(text.toStdString(), ref->guide.axis == core::Axis::X ? c.w : c.h);
			double v = 0;
			if (ex.apply(ref->guide.pos, v)) {
				Store::instance().guides().move(*g, v);
				Store::instance().notifyGuidesChanged();
				obs_frontend_save();
			}
		}
		return;
	}
	/* Double-click enters a group: select the child under the cursor. */
	const ItemEntry *top = ctx.topmostAt(w, false);
	if (top && top->isGroup) {
		if (const ItemEntry *child = ctx.topmostAt(w, true))
			ctx.select({child}, false);
		return;
	}
	if (top)
		obs_frontend_open_sceneitem_edit_transform(top->item);
}

void CanvasView::leaveEvent(QEvent *)
{
	cursorInside_ = false;
	hover_ = {-1, -1};
	rebuildOverlay();
	emit cursorChanged();
}

void CanvasView::wheelEvent(QWheelEvent *e)
{
	if (e->modifiers() & Qt::ControlModifier) {
		const double steps = e->angleDelta().y() / 120.0;
		if (steps != 0.0)
			zoomAt(zoom_ * std::pow(1.2, steps), e->position());
	} else {
		QPointF d = e->pixelDelta();
		if (d.isNull())
			d = QPointF(e->angleDelta()) / 3.0;
		if (e->modifiers() & Qt::ShiftModifier && d.x() == 0)
			d = QPointF(d.y(), 0);
		pan_ += d;
		autoFit_ = false;
		applyViewChange();
	}
	e->accept();
}

bool CanvasView::event(QEvent *e)
{
	if (e->type() == QEvent::NativeGesture) {
		auto *g = static_cast<QNativeGestureEvent *>(e);
		if (g->gestureType() == Qt::ZoomNativeGesture) {
			zoomAt(zoom_ * (1.0 + g->value()), g->position());
			return true;
		}
	}
	if (e->type() == QEvent::ShortcutOverride) {
		/* Keep arrow keys and Escape for the canvas while it has focus. */
		auto *k = static_cast<QKeyEvent *>(e);
		switch (k->key()) {
		case Qt::Key_Left:
		case Qt::Key_Right:
		case Qt::Key_Up:
		case Qt::Key_Down:
		case Qt::Key_Escape:
		case Qt::Key_Space:
			e->accept();
			return true;
		default:
			break;
		}
	}
	return QWidget::event(e);
}

void CanvasView::keyPressEvent(QKeyEvent *e)
{
	EditContext &ctx = EditContext::instance();
	const bool shift = e->modifiers() & Qt::ShiftModifier;
	switch (e->key()) {
	case Qt::Key_Space:
		if (!e->isAutoRepeat()) {
			spaceHeld_ = true;
			setCursor(Qt::OpenHandCursor);
		}
		return;
	case Qt::Key_Alt:
		altHeld_ = true;
		rebuildOverlay();
		return;
	case Qt::Key_Escape:
		if (mode_ != Mode::Idle)
			cancelInteraction();
		else
			ctx.clearSelection();
		rebuildOverlay();
		return;
	case Qt::Key_Left:
		commands::nudge(shift ? -10 : -1, 0);
		return;
	case Qt::Key_Right:
		commands::nudge(shift ? 10 : 1, 0);
		return;
	case Qt::Key_Up:
		commands::nudge(0, shift ? -10 : -1);
		return;
	case Qt::Key_Down:
		commands::nudge(0, shift ? 10 : 1);
		return;
	case Qt::Key_0:
	case Qt::Key_ParenRight:
		if (shift) {
			zoom100();
			return;
		}
		break;
	case Qt::Key_1:
	case Qt::Key_Exclam:
		if (shift) {
			zoomFit();
			return;
		}
		break;
	case Qt::Key_2:
	case Qt::Key_At:
		if (shift) {
			zoomSelection();
			return;
		}
		break;
	case Qt::Key_Plus:
	case Qt::Key_Equal:
		zoomIn();
		return;
	case Qt::Key_Minus:
		zoomOut();
		return;
	case Qt::Key_R:
		if (shift) {
			Store::instance().settings().rulers = !Store::instance().settings().rulers;
			Store::instance().notifySettingsChanged();
			return;
		}
		break;
	case Qt::Key_G:
		if (shift) {
			Store::instance().settings().grid = !Store::instance().settings().grid;
			Store::instance().notifySettingsChanged();
			return;
		}
		break;
	case Qt::Key_Semicolon:
		if (e->modifiers() & Qt::ControlModifier) {
			Store::instance().settings().guides = !Store::instance().settings().guides;
			Store::instance().notifySettingsChanged();
			return;
		}
		break;
	default:
		break;
	}
	QWidget::keyPressEvent(e);
}

void CanvasView::keyReleaseEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
		spaceHeld_ = false;
		if (mode_ != Mode::Pan)
			unsetCursor();
		return;
	}
	if (e->key() == Qt::Key_Alt) {
		altHeld_ = false;
		rebuildOverlay();
		return;
	}
	if (e->key() >= Qt::Key_Left && e->key() <= Qt::Key_Down)
		return; /* nudges are merged by OBS's repeatable undo */
	QWidget::keyReleaseEvent(e);
}

/* ============================================================ rulers API */

void CanvasView::beginGuideDrag(core::Axis axis, const QPoint &globalPos)
{
	if (!EditContext::instance().sceneSource())
		return;
	cancelInteraction();
	mode_ = Mode::Guide;
	guideIsNew_ = true;
	guideId_ = 0;
	guideAxis_ = axis;
	dragStarted_ = true;
	updateGuide(mapFromGlobal(QPointF(globalPos)), QGuiApplication::keyboardModifiers());
}

void CanvasView::guideDragMove(const QPoint &globalPos)
{
	if (mode_ == Mode::Guide)
		updateGuide(mapFromGlobal(QPointF(globalPos)), QGuiApplication::keyboardModifiers());
}

void CanvasView::guideDragEnd(const QPoint &globalPos)
{
	if (mode_ == Mode::Guide)
		finishGuide(mapFromGlobal(QPointF(globalPos)));
	rebuildOverlay();
}

std::optional<std::pair<core::Axis, double>> CanvasView::draggedGuide() const
{
	if (mode_ != Mode::Guide || guideOutside_)
		return std::nullopt;
	return std::make_pair(guideAxis_, guidePos_);
}

/* ========================================================== context menu */

void CanvasView::contextMenuEvent(QContextMenuEvent *e)
{
	EditContext &ctx = EditContext::instance();
	if (!ctx.sceneSource())
		return;
	QMenu menu(this);
	const QPointF p = e->pos();
	const core::Vec2 w = viewToWorld(p);

	if (const auto g = guideAt(p)) {
		const auto ref = Store::instance().guides().find(*g);
		if (ref) {
			const uint64_t id = *g;
			menu.addAction(T("Guide.SetPosition"), this, [this, id, ref] {
				bool ok = false;
				const QString text = QInputDialog::getText(this, T("Guide.SetPosition"),
									   T("Guide.Position"), QLineEdit::Normal,
									   fmt(ref->guide.pos), &ok);
				if (!ok)
					return;
				const core::Rect c = EditContext::instance().canvasRect();
				const core::Expression ex = core::parseExpression(
					text.toStdString(), ref->guide.axis == core::Axis::X ? c.w : c.h);
				double v = 0;
				if (ex.apply(ref->guide.pos, v)) {
					Store::instance().guides().move(id, v);
					Store::instance().notifyGuidesChanged();
					obs_frontend_save();
				}
			});
			menu.addAction(ref->guide.locked ? T("Guide.Unlock") : T("Guide.Lock"), this, [id, ref] {
				Store::instance().guides().setLocked(id, !ref->guide.locked);
				Store::instance().notifyGuidesChanged();
				obs_frontend_save();
			});
			menu.addAction(ref->global ? T("Guide.MakeScene") : T("Guide.MakeGlobal"), this, [id, ref] {
				EditContext &c = EditContext::instance();
				Store::instance().guides().setGlobal(id, c.sceneUuid().toStdString(),
								     c.sceneName().toStdString(), !ref->global);
				Store::instance().notifyGuidesChanged();
				obs_frontend_save();
			});
			menu.addAction(T("Guide.Delete"), this, [id] {
				Store::instance().guides().remove(id);
				Store::instance().notifyGuidesChanged();
				obs_frontend_save();
			});
			menu.addSeparator();
		}
	}

	const ItemEntry *hit = ctx.topmostAt(w, false);
	if (hit && !hit->selected)
		ctx.select({hit}, false);
	if (!ctx.selection().empty()) {
		menu.addAction(T("Inspector.CopyProps"), this, [] { commands::copyProperties(); });
		QMenu *paste = menu.addMenu(T("Inspector.PasteProps"));
		paste->setEnabled(commands::canPaste());
		const std::pair<const char *, unsigned> pastes[] = {
			{"Paste.All", commands::PasteAll},   {"Paste.Position", commands::PastePosition},
			{"Paste.Size", commands::PasteSize}, {"Paste.Rotation", commands::PasteRotation},
			{"Paste.Crop", commands::PasteCrop}, {"Paste.Bounds", commands::PasteBounds},
		};
		for (const auto &[key, mask] : pastes) {
			const unsigned m = mask;
			paste->addAction(T(key), this, [m] { commands::pasteProperties(m); });
		}
		menu.addAction(T("Canvas.ZoomSelection"), this, [this] { zoomSelection(); });
		menu.addSeparator();
	}

	const std::string uuid = ctx.sceneUuid().toStdString();
	const std::string name = ctx.sceneName().toStdString();
	const double gx = std::round(w.x), gy = std::round(w.y);
	menu.addAction(T("Guide.AddVertical").arg(fmt(gx)), this, [uuid, name, gx] {
		Store::instance().guides().add(uuid, name, core::Axis::X, gx, false);
		Store::instance().notifyGuidesChanged();
		obs_frontend_save();
	});
	menu.addAction(T("Guide.AddHorizontal").arg(fmt(gy)), this, [uuid, name, gy] {
		Store::instance().guides().add(uuid, name, core::Axis::Y, gy, false);
		Store::instance().notifyGuidesChanged();
		obs_frontend_save();
	});
	menu.addAction(T("Guide.AddCenter"), this, [uuid, name] {
		const core::Rect c = EditContext::instance().canvasRect();
		Store::instance().guides().add(uuid, name, core::Axis::X, c.w / 2.0, false);
		Store::instance().guides().add(uuid, name, core::Axis::Y, c.h / 2.0, false);
		Store::instance().notifyGuidesChanged();
		obs_frontend_save();
	});
	menu.addAction(T("Guide.ClearScene"), this, [uuid] {
		Store::instance().guides().clearScene(uuid);
		Store::instance().notifyGuidesChanged();
		obs_frontend_save();
	});
	menu.addAction(T("Guide.ClearGlobal"), this, [] {
		Store::instance().guides().globalGuidesMutable().clear();
		Store::instance().notifyGuidesChanged();
		obs_frontend_save();
	});
	menu.exec(e->globalPos());
}

} // namespace layouts::ui
