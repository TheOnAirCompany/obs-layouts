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

#include "broadcast-inventive.hpp"

#include <algorithm>

namespace layouts::core {

namespace {

constexpr double kRefW = 1920.0;
constexpr double kRefH = 1080.0;

/* H = host camera, C = camera, S = screen, T = chat. */
enum K { H, C, S, T };

struct SlotDef {
	K kind;
	Rect cell; /* on the 1920 x 1080 reference */
	double rotation;
	SlotShape shape;
	std::vector<Vec2> polygon; /* cell-normalized */
	bool frame;
	bool ticker;
};

struct Def {
	const char *key;
	int cameras;
	int screens;
	int chats;
	std::vector<SlotDef> slots; /* back to front */
};

const std::vector<Def> &defs()
{
	static const std::vector<Def> list = {
		{"ChatCam",
		 1,
		 0,
		 1,
		 {
			 {H, {40.0, 135.0, 1440.0, 810.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {1500.0, 135.0, 380.0, 810.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatScreenCam",
		 1,
		 1,
		 1,
		 {
			 {S, {40.0, 40.0, 1400.0, 788.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {H, {1460.0, 40.0, 420.0, 236.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {1460.0, 292.0, 420.0, 748.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatDuo",
		 2,
		 0,
		 1,
		 {
			 {H, {40.0, 100.0, 704.0, 880.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {760.0, 100.0, 400.0, 880.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {1176.0, 100.0, 704.0, 880.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatCard",
		 1,
		 0,
		 1,
		 {
			 {H, {0.0, 0.0, 1920.0, 1080.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {60.0, 560.0, 420.0, 460.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatTicker",
		 1,
		 1,
		 1,
		 {
			 {S, {120.8889, 0.0, 1678.2222, 944.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {H, {1340.0, 640.0, 420.0, 236.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {0.0, 960.0, 1920.0, 120.0}, 0.0, SlotShape::Rect, {}, false, true},
		 }},
		{"ChatGuests",
		 4,
		 0,
		 1,
		 {
			 {H, {0.0, 123.75, 732.0, 408.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {748.0, 123.75, 732.0, 408.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {0.0, 548.0, 732.0, 408.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {748.0, 548.0, 732.0, 408.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {1496.0, 124.0, 424.0, 832.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatQuestions",
		 2,
		 0,
		 1,
		 {
			 {H, {40.0, 300.0, 480.0, 480.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {560.0, 60.0, 800.0, 960.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {1400.0, 300.0, 480.0, 480.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatVisio",
		 3,
		 1,
		 1,
		 {
			 {H, {20.0, 237.125, 340.0, 191.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {20.0, 444.375, 340.0, 191.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {20.0, 651.625, 340.0, 191.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {S, {376.0, 223.875, 1124.0, 632.25}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {1516.0, 20.0, 384.0, 1040.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"ChatPodcast",
		 2,
		 0,
		 1,
		 {
			 {H, {180.0, 40.0, 760.0, 760.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {980.0, 40.0, 760.0, 760.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {120.0, 840.0, 1680.0, 200.0}, 0.0, SlotShape::Rect, {}, false, true},
		 }},
		{"ChatGameplay",
		 1,
		 1,
		 1,
		 {
			 {S, {0.0, 0.0, 1920.0, 1080.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {H, {40.0, 740.0, 300.0, 300.0}, 0.0, SlotShape::Round, {}, false, false},
			 {T, {1520.0, 40.0, 360.0, 700.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"TiltedCards",
		 3,
		 0,
		 0,
		 {
			 {C, {300.0, 215.0, 520.0, 650.0}, -6.0, SlotShape::Rect, {}, false, false},
			 {C, {1100.0, 215.0, 520.0, 650.0}, 6.0, SlotShape::Rect, {}, false, false},
			 {H, {700.0, 195.0, 520.0, 650.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"Polaroid",
		 2,
		 0,
		 0,
		 {
			 {H, {250.0, 200.0, 640.0, 640.0}, -4.0, SlotShape::Rect, {}, true, false},
			 {C, {1030.0, 240.0, 640.0, 640.0}, 3.0, SlotShape::Rect, {}, true, false},
		 }},
		{"Bento4",
		 3,
		 1,
		 0,
		 {
			 {S, {40.0, 40.0, 1160.0, 652.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {H, {1216.0, 40.0, 664.0, 1000.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {40.0, 708.0, 572.0, 332.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {628.0, 708.0, 572.0, 332.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"Bento6",
		 4,
		 1,
		 1,
		 {
			 {H, {40.0, 40.0, 880.0, 620.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {S, {936.0, 40.0, 944.0, 531.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {40.0, 676.0, 432.0, 364.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {488.0, 676.0, 432.0, 364.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {T, {936.0, 587.0, 450.0, 453.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {1402.0, 587.0, 478.0, 453.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"Dock",
		 4,
		 1,
		 0,
		 {
			 {S, {0.0, 0.0, 1920.0, 1080.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {H, {570.0, 860.0, 180.0, 180.0}, 0.0, SlotShape::Round, {}, false, false},
			 {C, {770.0, 860.0, 180.0, 180.0}, 0.0, SlotShape::Round, {}, false, false},
			 {C, {970.0, 860.0, 180.0, 180.0}, 0.0, SlotShape::Round, {}, false, false},
			 {C, {1170.0, 860.0, 180.0, 180.0}, 0.0, SlotShape::Round, {}, false, false},
		 }},
		{"Diagonal",
		 2,
		 0,
		 0,
		 {
			 {H,
			  {0.0, 0.0, 1100.0, 1080.0},
			  0.0,
			  SlotShape::Polygon,
			  {{0.0, 0.0}, {1.0, 0.0}, {0.7455, 1.0}, {0.0, 1.0}},
			  false,
			  false},
			 {C,
			  {820.0, 0.0, 1100.0, 1080.0},
			  0.0,
			  SlotShape::Polygon,
			  {{0.2691, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0145, 1.0}},
			  false,
			  false},
		 }},
		{"CardStack",
		 4,
		 0,
		 0,
		 {
			 {H, {40.0, 180.0, 1280.0, 720.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {1380.0, 170.0, 480.0, 270.0}, 5.0, SlotShape::Rect, {}, false, false},
			 {C, {1400.0, 400.0, 480.0, 270.0}, -3.0, SlotShape::Rect, {}, false, false},
			 {C, {1370.0, 630.0, 480.0, 270.0}, 2.0, SlotShape::Rect, {}, false, false},
		 }},
		{"TiltedScreen",
		 1,
		 1,
		 0,
		 {
			 {S, {160.0, 140.0, 1360.0, 765.0}, -3.0, SlotShape::Rect, {}, false, false},
			 {H, {1480.0, 620.0, 360.0, 360.0}, 0.0, SlotShape::Round, {}, false, false},
		 }},
		{"Masonry",
		 5,
		 0,
		 0,
		 {
			 {H, {1272.0, 40.0, 608.0, 1000.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {40.0, 40.0, 600.0, 620.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {40.0, 676.0, 600.0, 364.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {656.0, 40.0, 600.0, 380.0}, 0.0, SlotShape::Rect, {}, false, false},
			 {C, {656.0, 436.0, 600.0, 604.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
		{"PortHole",
		 1,
		 1,
		 0,
		 {
			 {H, {80.0, 200.0, 680.0, 680.0}, 0.0, SlotShape::Round, {}, false, false},
			 {S, {820.0, 250.0, 1060.0, 596.0}, 0.0, SlotShape::Rect, {}, false, false},
		 }},
	};
	return list;
}

} // namespace

void appendInventiveTemplates(std::vector<BroadcastTemplate> &list)
{
	for (const Def &d : defs())
		list.push_back({d.key, d.cameras, d.screens, d.chats, true});
}

bool buildInventiveTemplate(const std::string &key, const Rect &canvas, const BroadcastSpec &spec,
			    std::vector<Slot> &out)
{
	const auto it = std::find_if(defs().begin(), defs().end(), [&](const Def &d) { return key == d.key; });
	if (it == defs().end())
		return false;
	if (!(canvas.w > 0) || !(canvas.h > 0))
		return true;
	const double m = std::max(0.0, std::min(spec.margin, std::min(canvas.w, canvas.h) / 4));
	const Rect r{canvas.x + m, canvas.y + m, canvas.w - 2 * m, canvas.h - 2 * m};
	const double sx = r.w / kRefW, sy = r.h / kRefH;
	for (const SlotDef &d : it->slots) {
		Slot s;
		s.kind = d.kind == S ? SlotKind::Screen : d.kind == T ? SlotKind::Chat : SlotKind::Camera;
		s.cell = {r.x + d.cell.x * sx, r.y + d.cell.y * sy, d.cell.w * sx, d.cell.h * sy};
		if (d.shape == SlotShape::Round) {
			/* Keep bubbles round on a canvas that is not 16:9. */
			const double side = std::min(d.cell.w, d.cell.h) * std::min(sx, sy);
			const double cx = s.cell.x + s.cell.w / 2, cy = s.cell.y + s.cell.h / 2;
			s.cell = {cx - side / 2, cy - side / 2, side, side};
		}
		s.rotation = d.rotation;
		s.shape = d.shape;
		s.polygon = d.polygon;
		s.frame = d.frame;
		s.ticker = d.ticker;
		s.host = d.kind == H;
		out.push_back(s);
	}
	return true;
}

} // namespace layouts::core
