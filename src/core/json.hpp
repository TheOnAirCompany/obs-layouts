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

/* A small JSON reader (no dependency): enough for the user layouts files and the
 * organization that Stacks for OBS publishes. Objects keep their field order. */

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace layouts::core {

struct JsonValue {
	enum Type { Null, Bool, Number, String, Array, Object } type = Null;
	bool b = false;
	double n = 0;
	std::string s;
	std::vector<JsonValue> items;
	std::vector<std::pair<std::string, JsonValue>> fields;

	const JsonValue *get(const std::string &k) const
	{
		for (const auto &f : fields)
			if (f.first == k)
				return &f.second;
		return nullptr;
	}
	double number(const std::string &k, double def) const
	{
		const JsonValue *v = get(k);
		return v && v->type == Number && std::isfinite(v->n) ? v->n : def;
	}
	bool boolean(const std::string &k, bool def) const
	{
		const JsonValue *v = get(k);
		return v && v->type == Bool ? v->b : def;
	}
	std::string string(const std::string &k, const std::string &def = {}) const
	{
		const JsonValue *v = get(k);
		return v && v->type == String ? v->s : def;
	}
};

/* Parses a whole document; false on any syntax error or trailing text. */
bool parseJson(const std::string &text, JsonValue &out);

} // namespace layouts::core
