#include "test.hpp"

#include "layout.hpp"

using namespace layouts::core;

namespace {

Frame fr(double x, double y, double w, double h)
{
	return Frame::fromRect(Rect{x, y, w, h});
}

} // namespace

TEST("layout: scale a selection")
{
	const std::vector<Frame> in = {fr(0, 0, 100, 100), fr(200, 100, 100, 100)};
	const auto out = scaleFrames(in, Rect{0, 0, 300, 200}, Rect{0, 0, 600, 200});
	CHECK_NEAR(out[0].rect().x, 0, 1e-9);
	CHECK_NEAR(out[0].width, 200, 1e-9);
	CHECK_NEAR(out[1].rect().x, 400, 1e-9);
	CHECK_NEAR(out[1].rect().right(), 600, 1e-9);
	CHECK_NEAR(out[1].height, 100, 1e-9);
}

TEST("layout: fit, fill and stretch in a cell")
{
	const Rect cell{0, 0, 400, 400};
	Frame f = fitInCell(16.0 / 9.0, cell, FitMode::Fit);
	CHECK_NEAR(f.width, 400, 1e-9);
	CHECK_NEAR(f.height, 225, 1e-9);
	CHECK_NEAR(f.center.y, 200, 1e-9);
	f = fitInCell(16.0 / 9.0, cell, FitMode::Fill);
	CHECK_NEAR(f.height, 400, 1e-9);
	CHECK_NEAR(f.width, 400 * 16.0 / 9.0, 1e-9);
	f = fitInCell(16.0 / 9.0, cell, FitMode::Stretch);
	CHECK_NEAR(f.width, 400, 0);
	CHECK_NEAR(f.height, 400, 0);
}

TEST("layout: horizontal flow packs in spatial order")
{
	const std::vector<Frame> in = {fr(500, 50, 100, 50), fr(0, 0, 200, 100)};
	FlowSpec s;
	s.gap = 20;
	s.padding = 10;
	s.crossAlign = FlowAlign::Center;
	const auto out = flow(in, Rect{0, 0, 1000, 200}, s);
	CHECK_NEAR(out[1].rect().x, 10, 1e-9);
	CHECK_NEAR(out[0].rect().x, 10 + 200 + 20, 1e-9);
	CHECK_NEAR(out[0].center.y, 100, 1e-9);
	CHECK_NEAR(out[0].width, 100, 0);
}

TEST("layout: vertical flow centered on the main axis, end aligned")
{
	const std::vector<Frame> in = {fr(0, 0, 100, 100), fr(0, 300, 50, 100)};
	FlowSpec s;
	s.direction = FlowDirection::Vertical;
	s.gap = 0;
	s.mainAlign = FlowAlign::Center;
	s.crossAlign = FlowAlign::End;
	const auto out = flow(in, Rect{0, 0, 400, 1000}, s);
	CHECK_NEAR(out[0].rect().y, 400, 1e-9);
	CHECK_NEAR(out[1].rect().y, 500, 1e-9);
	CHECK_NEAR(out[1].rect().right(), 400, 1e-9);
}

TEST("layout: fill sizing shares the main axis and keeps ratios")
{
	const std::vector<Frame> in = {fr(0, 0, 160, 90), fr(300, 0, 100, 100)};
	FlowSpec s;
	s.gap = 20;
	s.sizing = FlowSizing::Fill;
	const auto out = flow(in, Rect{0, 0, 1020, 600}, s);
	CHECK_NEAR(out[0].width, 500, 1e-9);
	CHECK_NEAR(out[0].height, 500 * 9.0 / 16.0, 1e-9);
	CHECK_NEAR(out[1].rect().x, 520, 1e-9);
}

TEST("layout: grid in reading order")
{
	const std::vector<Frame> in = {fr(300, 300, 100, 100), fr(0, 0, 100, 100), fr(300, 0, 100, 100),
				       fr(0, 310, 100, 100)};
	FlowSpec s;
	s.direction = FlowDirection::Grid;
	s.columns = 2;
	s.gap = 10;
	const auto out = flow(in, Rect{0, 0, 410, 500}, s);
	CHECK_NEAR(out[1].rect().x, 0, 1e-9);
	CHECK_NEAR(out[2].rect().x, 210, 1e-9);
	CHECK_NEAR(out[3].rect().y, 110, 1e-9);
	CHECK_NEAR(out[0].rect().x, 210, 1e-9);
	CHECK_NEAR(out[0].rect().y, 110, 1e-9);
}

TEST("layout: presets place items and keep ratios")
{
	const Rect canvas{0, 0, 1920, 1080};
	PresetSpec ps;
	ps.margin = 40;
	ps.gap = 20;
	ps.cornerScale = 0.25;
	auto out = applyPreset(Preset::CornerBottomRight, {fr(0, 0, 1280, 720)}, canvas, ps);
	CHECK_NEAR(out[0].width, 480, 1e-9);
	CHECK_NEAR(out[0].rect().right(), 1880, 1e-9);
	CHECK_NEAR(out[0].rect().bottom(), 1040, 1e-9);

	out = applyPreset(Preset::SplitScreen, {fr(0, 0, 1920, 1080), fr(0, 0, 1920, 1080)}, canvas, ps);
	CHECK_NEAR(out[0].width, (1840 - 20) / 2.0, 1e-9);
	CHECK_NEAR(out[1].rect().x, 40 + 910 + 20, 1e-9);
	CHECK_NEAR(out[0].aspect(), 16.0 / 9.0, 1e-9);

	out = applyPreset(Preset::PictureInPicture, {fr(0, 0, 1920, 1080), fr(0, 0, 640, 360)}, canvas, ps);
	CHECK_NEAR(out[0].width, 1920, 1e-9);
	CHECK_NEAR(out[1].rect().right(), 1880, 1e-9);

	out = applyPreset(Preset::GridTwoByTwo, {fr(0, 0, 16, 9), fr(0, 0, 16, 9), fr(0, 0, 16, 9), fr(0, 0, 16, 9)},
			  canvas, ps);
	CHECK_NEAR(out[3].center.x, 970 + 455, 1e-6); /* centered in its cell */
	CHECK_NEAR(out[3].center.y, 40 + 490 + 20 + 245, 1e-6);
	CHECK_NEAR(out[3].height, 490, 1e-6);

	CHECK(presetFromKey("SplitScreen") == Preset::SplitScreen);
	CHECK(!presetFromKey("Nope"));
}
