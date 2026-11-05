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

#include "style.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

namespace layouts::core {

double styleRadius(const Style &s, double w, double h)
{
	const double shortSide = std::max(0.0, std::min(std::fabs(w), std::fabs(h)));
	double r = 0;
	switch (s.radiusMode) {
	case RadiusMode::Auto:
		r = std::clamp(shortSide * 0.04, 6.0, 48.0);
		break;
	case RadiusMode::Pixels:
		r = std::max(0.0, s.radiusPx);
		break;
	case RadiusMode::Round:
		return shortSide / 2;
	case RadiusMode::None:
		return 0;
	}
	/* A corner can never be larger than half the short side. */
	return std::min(r, shortSide / 2);
}

/* ------------------------------------------------------------ JSON */

namespace {

const char *radiusName(RadiusMode m)
{
	switch (m) {
	case RadiusMode::Pixels:
		return "pixels";
	case RadiusMode::Round:
		return "round";
	case RadiusMode::None:
		return "none";
	case RadiusMode::Auto:
		break;
	}
	return "auto";
}

const char *shadowName(ShadowPreset p)
{
	switch (p) {
	case ShadowPreset::Soft:
		return "soft";
	case ShadowPreset::Hard:
		return "hard";
	case ShadowPreset::Custom:
		return "custom";
	case ShadowPreset::None:
		break;
	}
	return "none";
}

std::string num(double v)
{
	if (!std::isfinite(v))
		v = 0;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.10g", v);
	return buf;
}

/* A tiny JSON value: enough for the style object (numbers, booleans, strings, arrays). */
struct Value {
	enum Type { Null, Bool, Number, String, Array, Object } type = Null;
	bool b = false;
	double n = 0;
	std::string s;
	std::vector<Value> a;
	std::map<std::string, Value> o;
};

class Parser {
public:
	explicit Parser(const std::string &text) : t(text) {}

	bool parse(Value &out)
	{
		if (!value(out, 0))
			return false;
		ws();
		return i == t.size();
	}

private:
	const std::string &t;
	size_t i = 0;

	void ws()
	{
		while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r'))
			i++;
	}
	bool peek(char c) const { return i < t.size() && t[i] == c; }
	bool lit(const char *word)
	{
		const std::string w(word);
		if (t.compare(i, w.size(), w) != 0)
			return false;
		i += w.size();
		return true;
	}
	bool string(std::string &out)
	{
		if (!peek('"'))
			return false;
		i++;
		while (i < t.size() && t[i] != '"') {
			char c = t[i++];
			if (c == '\\') {
				if (i >= t.size())
					return false;
				const char e = t[i++];
				switch (e) {
				case 'n':
					c = '\n';
					break;
				case 't':
					c = '\t';
					break;
				case 'r':
					c = '\r';
					break;
				case 'b':
					c = '\b';
					break;
				case 'f':
					c = '\f';
					break;
				case 'u':
					/* Keys and values of a style are ASCII: other characters are not kept. */
					if (i + 4 > t.size())
						return false;
					i += 4;
					c = '?';
					break;
				default:
					c = e;
				}
			}
			out.push_back(c);
		}
		if (i >= t.size())
			return false;
		i++;
		return true;
	}
	bool object(Value &v, int depth)
	{
		i++;
		v.type = Value::Object;
		ws();
		if (peek('}')) {
			i++;
			return true;
		}
		for (;;) {
			ws();
			std::string key;
			if (!string(key))
				return false;
			ws();
			if (!peek(':'))
				return false;
			i++;
			Value item;
			if (!value(item, depth + 1))
				return false;
			v.o[key] = std::move(item);
			ws();
			if (peek(',')) {
				i++;
				continue;
			}
			if (peek('}')) {
				i++;
				return true;
			}
			return false;
		}
	}
	bool array(Value &v, int depth)
	{
		i++;
		v.type = Value::Array;
		ws();
		if (peek(']')) {
			i++;
			return true;
		}
		for (;;) {
			Value item;
			if (!value(item, depth + 1))
				return false;
			v.a.push_back(std::move(item));
			ws();
			if (peek(',')) {
				i++;
				continue;
			}
			if (peek(']')) {
				i++;
				return true;
			}
			return false;
		}
	}
	bool value(Value &v, int depth)
	{
		if (depth > 16)
			return false;
		ws();
		if (i >= t.size())
			return false;
		if (peek('{'))
			return object(v, depth);
		if (peek('['))
			return array(v, depth);
		if (peek('"')) {
			v.type = Value::String;
			return string(v.s);
		}
		if (lit("true")) {
			v.type = Value::Bool;
			v.b = true;
			return true;
		}
		if (lit("false")) {
			v.type = Value::Bool;
			return true;
		}
		if (lit("null"))
			return true;
		const char *start = t.c_str() + i;
		char *end = nullptr;
		const double n = std::strtod(start, &end);
		if (end == start)
			return false;
		i += (size_t)(end - start);
		v.type = Value::Number;
		v.n = n;
		return true;
	}
};

const Value *member(const Value &o, const char *key, Value::Type type)
{
	auto it = o.o.find(key);
	return it != o.o.end() && it->second.type == type ? &it->second : nullptr;
}

void readNumber(const Value &o, const char *key, double &out)
{
	if (const Value *v = member(o, key, Value::Number); v && std::isfinite(v->n))
		out = v->n;
}

void readColor(const Value &o, const char *key, uint32_t &out)
{
	if (const Value *v = member(o, key, Value::Number); v && v->n >= 0 && v->n <= 4294967295.0)
		out = (uint32_t)v->n;
}

void readBool(const Value &o, const char *key, bool &out)
{
	if (const Value *v = member(o, key, Value::Bool))
		out = v->b;
}

} // namespace

std::string toJson(const Style &s)
{
	std::string j = "{\"radiusMode\":\"";
	j += radiusName(s.radiusMode);
	j += "\",\"radiusPx\":" + num(s.radiusPx);
	j += ",\"border\":";
	j += s.border ? "true" : "false";
	j += ",\"borderWidth\":" + num(s.borderWidth);
	j += ",\"borderColor\":" + std::to_string(s.borderColor);
	j += ",\"shadow\":\"";
	j += shadowName(s.shadow);
	j += "\",\"shadowBlur\":" + num(s.shadowBlur);
	j += ",\"shadowOffsetX\":" + num(s.shadowOffsetX);
	j += ",\"shadowOffsetY\":" + num(s.shadowOffsetY);
	j += ",\"shadowOpacity\":" + num(s.shadowOpacity);
	j += ",\"shadowColor\":" + std::to_string(s.shadowColor);
	j += ",\"fadeShadowInTransition\":";
	j += s.fadeShadowInTransition ? "true" : "false";
	if (!s.polygon.empty()) {
		j += ",\"polygon\":[";
		for (size_t k = 0; k < s.polygon.size(); k++) {
			if (k)
				j += ",";
			j += "[" + num(s.polygon[k].x) + "," + num(s.polygon[k].y) + "]";
		}
		j += "]";
	}
	j += "}";
	return j;
}

bool fromJson(const std::string &json, Style &out)
{
	Value root;
	if (!Parser(json).parse(root) || root.type != Value::Object)
		return false;
	Style s;
	if (const Value *m = member(root, "radiusMode", Value::String)) {
		if (m->s == "pixels")
			s.radiusMode = RadiusMode::Pixels;
		else if (m->s == "round")
			s.radiusMode = RadiusMode::Round;
		else if (m->s == "none")
			s.radiusMode = RadiusMode::None;
	}
	readNumber(root, "radiusPx", s.radiusPx);
	readBool(root, "border", s.border);
	readNumber(root, "borderWidth", s.borderWidth);
	readColor(root, "borderColor", s.borderColor);
	if (const Value *p = member(root, "shadow", Value::String)) {
		if (p->s == "soft")
			s.shadow = ShadowPreset::Soft;
		else if (p->s == "hard")
			s.shadow = ShadowPreset::Hard;
		else if (p->s == "custom")
			s.shadow = ShadowPreset::Custom;
	}
	readNumber(root, "shadowBlur", s.shadowBlur);
	readNumber(root, "shadowOffsetX", s.shadowOffsetX);
	readNumber(root, "shadowOffsetY", s.shadowOffsetY);
	readNumber(root, "shadowOpacity", s.shadowOpacity);
	readColor(root, "shadowColor", s.shadowColor);
	readBool(root, "fadeShadowInTransition", s.fadeShadowInTransition);
	if (const Value *poly = member(root, "polygon", Value::Array)) {
		for (const Value &pt : poly->a) {
			if (pt.type == Value::Array && pt.a.size() == 2 && pt.a[0].type == Value::Number &&
			    pt.a[1].type == Value::Number && std::isfinite(pt.a[0].n) && std::isfinite(pt.a[1].n))
				s.polygon.push_back({pt.a[0].n, pt.a[1].n});
		}
	}
	out = std::move(s);
	return true;
}

} // namespace layouts::core
