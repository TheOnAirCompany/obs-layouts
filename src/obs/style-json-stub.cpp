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

/* TODO: temporary core::Style JSON so the Style UI builds on its own. Delete this file
 * (and its CMake line) when src/core/style.cpp lands. */

#include "core/style.hpp"

#include <obs.hpp>

#include <algorithm>

namespace layouts::core {

namespace {

const char *radiusKey(RadiusMode m)
{
	switch (m) {
	case RadiusMode::Pixels:
		return "px";
	case RadiusMode::Round:
		return "round";
	case RadiusMode::None:
		return "none";
	default:
		return "auto";
	}
}

const char *shadowKey(ShadowPreset p)
{
	switch (p) {
	case ShadowPreset::Soft:
		return "soft";
	case ShadowPreset::Hard:
		return "hard";
	case ShadowPreset::Custom:
		return "custom";
	default:
		return "none";
	}
}

} // namespace

double styleRadius(const Style &s, double w, double h)
{
	const double shortSide = std::max(0.0, std::min(w, h));
	switch (s.radiusMode) {
	case RadiusMode::Pixels:
		return std::clamp(s.radiusPx, 0.0, shortSide / 2);
	case RadiusMode::Round:
		return shortSide / 2;
	case RadiusMode::None:
		return 0;
	default:
		return std::min(std::clamp(shortSide * 0.04, 6.0, 48.0), shortSide / 2);
	}
}

std::string toJson(const Style &s)
{
	OBSDataAutoRelease d = obs_data_create();
	obs_data_set_string(d, "radius", radiusKey(s.radiusMode));
	obs_data_set_double(d, "radiusPx", s.radiusPx);
	obs_data_set_bool(d, "border", s.border);
	obs_data_set_double(d, "borderWidth", s.borderWidth);
	obs_data_set_int(d, "borderColor", s.borderColor);
	obs_data_set_string(d, "shadow", shadowKey(s.shadow));
	obs_data_set_double(d, "shadowBlur", s.shadowBlur);
	obs_data_set_double(d, "shadowX", s.shadowOffsetX);
	obs_data_set_double(d, "shadowY", s.shadowOffsetY);
	obs_data_set_double(d, "shadowOpacity", s.shadowOpacity);
	obs_data_set_int(d, "shadowColor", s.shadowColor);
	obs_data_set_bool(d, "fadeShadow", s.fadeShadowInTransition);
	const char *json = obs_data_get_json(d);
	return json ? json : "{}";
}

bool fromJson(const std::string &json, Style &out)
{
	OBSDataAutoRelease d = obs_data_create_from_json(json.c_str());
	if (!d)
		return false;
	Style s;
	const std::string r = obs_data_get_string(d, "radius");
	s.radiusMode = r == "px"      ? RadiusMode::Pixels
		       : r == "round" ? RadiusMode::Round
		       : r == "none"  ? RadiusMode::None
				      : RadiusMode::Auto;
	s.radiusPx = obs_data_get_double(d, "radiusPx");
	s.border = obs_data_get_bool(d, "border");
	if (obs_data_has_user_value(d, "borderWidth"))
		s.borderWidth = obs_data_get_double(d, "borderWidth");
	if (obs_data_has_user_value(d, "borderColor"))
		s.borderColor = (uint32_t)obs_data_get_int(d, "borderColor");
	const std::string sh = obs_data_get_string(d, "shadow");
	s.shadow = sh == "soft"     ? ShadowPreset::Soft
		   : sh == "hard"   ? ShadowPreset::Hard
		   : sh == "custom" ? ShadowPreset::Custom
				    : ShadowPreset::None;
	if (obs_data_has_user_value(d, "shadowBlur"))
		s.shadowBlur = obs_data_get_double(d, "shadowBlur");
	if (obs_data_has_user_value(d, "shadowX"))
		s.shadowOffsetX = obs_data_get_double(d, "shadowX");
	if (obs_data_has_user_value(d, "shadowY"))
		s.shadowOffsetY = obs_data_get_double(d, "shadowY");
	if (obs_data_has_user_value(d, "shadowOpacity"))
		s.shadowOpacity = obs_data_get_double(d, "shadowOpacity");
	if (obs_data_has_user_value(d, "shadowColor"))
		s.shadowColor = (uint32_t)obs_data_get_int(d, "shadowColor");
	if (obs_data_has_user_value(d, "fadeShadow"))
		s.fadeShadowInTransition = obs_data_get_bool(d, "fadeShadow");
	out = s;
	return true;
}

} // namespace layouts::core
