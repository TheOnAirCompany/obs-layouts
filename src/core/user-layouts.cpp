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

#include "user-layouts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <sstream>

namespace layouts::core {

/* ------------------------------------------------------------ model */

namespace {

int countKind(const UserLayout &l, SlotKind k)
{
	return (int)std::count_if(l.slots.begin(), l.slots.end(), [k](const Slot &s) { return s.kind == k; });
}

int kindOrder(const Slot &s)
{
	if (s.kind == SlotKind::Camera)
		return s.host ? 0 : 1;
	return s.kind == SlotKind::Screen ? 2 : 3;
}

} // namespace

int UserLayout::cameras() const
{
	return countKind(*this, SlotKind::Camera);
}

int UserLayout::screens() const
{
	return countKind(*this, SlotKind::Screen);
}

int UserLayout::chats() const
{
	return countKind(*this, SlotKind::Chat);
}

std::vector<Slot> normalizeSlots(const std::vector<Slot> &world, const Rect &canvas)
{
	std::vector<Slot> out;
	if (!(canvas.w > 0) || !(canvas.h > 0))
		return out;
	for (Slot s : world) {
		s.cell = {(s.cell.x - canvas.x) / canvas.w, (s.cell.y - canvas.y) / canvas.h, s.cell.w / canvas.w,
			  s.cell.h / canvas.h};
		if (s.cell.w > 0 && s.cell.h > 0)
			out.push_back(s);
	}
	/* Reading order: rows first (a small tolerance so tiles of one row stay together). */
	std::stable_sort(out.begin(), out.end(), [](const Slot &a, const Slot &b) {
		if (kindOrder(a) != kindOrder(b))
			return kindOrder(a) < kindOrder(b);
		if (std::fabs(a.cell.y - b.cell.y) > 0.02)
			return a.cell.y < b.cell.y;
		return a.cell.x < b.cell.x;
	});
	return out;
}

std::vector<Slot> buildUserLayout(const UserLayout &layout, const Rect &canvas)
{
	std::vector<Slot> out;
	if (!(canvas.w > 0) || !(canvas.h > 0))
		return out;
	for (Slot s : layout.slots) {
		s.cell = {canvas.x + s.cell.x * canvas.w, canvas.y + s.cell.y * canvas.h, s.cell.w * canvas.w,
			  s.cell.h * canvas.h};
		out.push_back(s);
	}
	return out;
}

/* ------------------------------------------------------------ JSON writer */

namespace {

std::string quote(const std::string &s)
{
	std::string o = "\"";
	for (unsigned char c : s) {
		switch (c) {
		case '"':
			o += "\\\"";
			break;
		case '\\':
			o += "\\\\";
			break;
		case '\n':
			o += "\\n";
			break;
		case '\r':
			o += "\\r";
			break;
		case '\t':
			o += "\\t";
			break;
		default:
			if (c < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof(buf), "\\u%04x", c);
				o += buf;
			} else {
				o += (char)c;
			}
		}
	}
	return o + "\"";
}

std::string num(double v)
{
	if (!std::isfinite(v))
		v = 0;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.6g", v);
	return buf;
}

const char *kindKey(SlotKind k)
{
	switch (k) {
	case SlotKind::Screen:
		return "screen";
	case SlotKind::Chat:
		return "chat";
	default:
		return "camera";
	}
}

const char *shapeKey(SlotShape s)
{
	switch (s) {
	case SlotShape::Round:
		return "round";
	case SlotShape::Polygon:
		return "polygon";
	default:
		return "rect";
	}
}

std::string layoutJson(const UserLayout &l, const std::string &indent)
{
	std::ostringstream o;
	o << indent << "{\"key\": " << quote(l.key) << ", \"name\": " << quote(l.name)
	  << ", \"aspect\": " << num(l.aspect) << ", \"slots\": [";
	for (size_t i = 0; i < l.slots.size(); i++) {
		const Slot &s = l.slots[i];
		o << (i ? "," : "") << "\n"
		  << indent << "  {\"kind\": \"" << kindKey(s.kind) << "\", \"x\": " << num(s.cell.x)
		  << ", \"y\": " << num(s.cell.y) << ", \"w\": " << num(s.cell.w) << ", \"h\": " << num(s.cell.h)
		  << ", \"rotation\": " << num(s.rotation) << ", \"shape\": \"" << shapeKey(s.shape) << "\"";
		if (!s.polygon.empty()) {
			o << ", \"polygon\": [";
			for (size_t p = 0; p < s.polygon.size(); p++)
				o << (p ? ", " : "") << "[" << num(s.polygon[p].x) << ", " << num(s.polygon[p].y)
				  << "]";
			o << "]";
		}
		o << ", \"frame\": " << (s.frame ? "true" : "false")
		  << ", \"ticker\": " << (s.ticker ? "true" : "false") << ", \"host\": " << (s.host ? "true" : "false")
		  << "}";
	}
	o << "\n" << indent << "]}";
	return o.str();
}

} // namespace

std::string userLayoutsToJson(const std::vector<UserLayout> &layouts)
{
	std::string o = "{\"format\": \"";
	o += kUserLayoutFormat;
	o += "\", \"layouts\": [";
	for (size_t i = 0; i < layouts.size(); i++)
		o += (i ? ",\n" : "\n") + layoutJson(layouts[i], "  ");
	o += "\n]}\n";
	return o;
}

/* ------------------------------------------------------------ JSON reader */

namespace {

struct Value {
	enum Type { Null, Bool, Number, String, Array, Object } type = Null;
	bool b = false;
	double n = 0;
	std::string s;
	std::vector<Value> items;
	std::vector<std::pair<std::string, Value>> fields;

	const Value *get(const std::string &k) const
	{
		for (const auto &f : fields)
			if (f.first == k)
				return &f.second;
		return nullptr;
	}
	double number(const std::string &k, double def) const
	{
		const Value *v = get(k);
		return v && v->type == Number && std::isfinite(v->n) ? v->n : def;
	}
	bool boolean(const std::string &k, bool def) const
	{
		const Value *v = get(k);
		return v && v->type == Bool ? v->b : def;
	}
	std::string string(const std::string &k, const std::string &def = {}) const
	{
		const Value *v = get(k);
		return v && v->type == String ? v->s : def;
	}
};

class Parser {
public:
	explicit Parser(const std::string &t) : t_(t) {}

	bool parse(Value &out)
	{
		if (!value(out, 0))
			return false;
		ws();
		return i_ == t_.size();
	}

private:
	void ws()
	{
		while (i_ < t_.size() && (t_[i_] == ' ' || t_[i_] == '\n' || t_[i_] == '\r' || t_[i_] == '\t'))
			i_++;
	}
	bool lit(const char *w)
	{
		const size_t n = std::char_traits<char>::length(w);
		if (t_.compare(i_, n, w) != 0)
			return false;
		i_ += n;
		return true;
	}
	static void utf8(std::string &o, unsigned cp)
	{
		if (cp < 0x80) {
			o += (char)cp;
		} else if (cp < 0x800) {
			o += (char)(0xC0 | (cp >> 6));
			o += (char)(0x80 | (cp & 0x3F));
		} else if (cp < 0x10000) {
			o += (char)(0xE0 | (cp >> 12));
			o += (char)(0x80 | ((cp >> 6) & 0x3F));
			o += (char)(0x80 | (cp & 0x3F));
		} else {
			o += (char)(0xF0 | (cp >> 18));
			o += (char)(0x80 | ((cp >> 12) & 0x3F));
			o += (char)(0x80 | ((cp >> 6) & 0x3F));
			o += (char)(0x80 | (cp & 0x3F));
		}
	}
	bool hex4(unsigned &cp)
	{
		if (i_ + 4 > t_.size())
			return false;
		cp = 0;
		for (int k = 0; k < 4; k++) {
			const char c = t_[i_++];
			cp <<= 4;
			if (c >= '0' && c <= '9')
				cp |= (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f')
				cp |= (unsigned)(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F')
				cp |= (unsigned)(c - 'A' + 10);
			else
				return false;
		}
		return true;
	}
	bool str(std::string &o)
	{
		if (i_ >= t_.size() || t_[i_] != '"')
			return false;
		i_++;
		while (i_ < t_.size()) {
			const char c = t_[i_++];
			if (c == '"')
				return true;
			if (c != '\\') {
				o += c;
				continue;
			}
			if (i_ >= t_.size())
				return false;
			const char e = t_[i_++];
			switch (e) {
			case '"':
			case '\\':
			case '/':
				o += e;
				break;
			case 'b':
				o += '\b';
				break;
			case 'f':
				o += '\f';
				break;
			case 'n':
				o += '\n';
				break;
			case 'r':
				o += '\r';
				break;
			case 't':
				o += '\t';
				break;
			case 'u': {
				unsigned cp = 0;
				if (!hex4(cp))
					return false;
				if (cp >= 0xD800 && cp < 0xDC00 && lit("\\u")) {
					unsigned lo = 0;
					if (!hex4(lo))
						return false;
					cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
				}
				utf8(o, cp);
				break;
			}
			default:
				return false;
			}
		}
		return false;
	}
	bool value(Value &v, int depth)
	{
		if (depth > 32)
			return false;
		ws();
		if (i_ >= t_.size())
			return false;
		const char c = t_[i_];
		if (c == '{') {
			i_++;
			v.type = Value::Object;
			ws();
			if (i_ < t_.size() && t_[i_] == '}') {
				i_++;
				return true;
			}
			for (;;) {
				ws();
				std::string k;
				if (!str(k))
					return false;
				ws();
				if (i_ >= t_.size() || t_[i_++] != ':')
					return false;
				Value child;
				if (!value(child, depth + 1))
					return false;
				v.fields.emplace_back(std::move(k), std::move(child));
				ws();
				if (i_ < t_.size() && t_[i_] == ',') {
					i_++;
					continue;
				}
				if (i_ < t_.size() && t_[i_] == '}') {
					i_++;
					return true;
				}
				return false;
			}
		}
		if (c == '[') {
			i_++;
			v.type = Value::Array;
			ws();
			if (i_ < t_.size() && t_[i_] == ']') {
				i_++;
				return true;
			}
			for (;;) {
				Value child;
				if (!value(child, depth + 1))
					return false;
				v.items.push_back(std::move(child));
				ws();
				if (i_ < t_.size() && t_[i_] == ',') {
					i_++;
					continue;
				}
				if (i_ < t_.size() && t_[i_] == ']') {
					i_++;
					return true;
				}
				return false;
			}
		}
		if (c == '"') {
			v.type = Value::String;
			return str(v.s);
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
		const char *begin = t_.c_str() + i_;
		char *end = nullptr;
		v.n = std::strtod(begin, &end);
		if (end == begin)
			return false;
		v.type = Value::Number;
		i_ += (size_t)(end - begin);
		return true;
	}

	const std::string &t_;
	size_t i_ = 0;
};

bool readSlot(const Value &v, Slot &s)
{
	if (v.type != Value::Object)
		return false;
	const std::string kind = v.string("kind");
	if (kind == "camera")
		s.kind = SlotKind::Camera;
	else if (kind == "screen")
		s.kind = SlotKind::Screen;
	else if (kind == "chat")
		s.kind = SlotKind::Chat;
	else
		return false;
	s.cell = {v.number("x", 0), v.number("y", 0), v.number("w", 0), v.number("h", 0)};
	if (!(s.cell.w > 0) || !(s.cell.h > 0) || s.cell.w > 100 || s.cell.h > 100 || std::fabs(s.cell.x) > 100 ||
	    std::fabs(s.cell.y) > 100)
		return false;
	s.rotation = v.number("rotation", 0);
	const std::string shape = v.string("shape", "rect");
	s.shape = shape == "round" ? SlotShape::Round : shape == "polygon" ? SlotShape::Polygon : SlotShape::Rect;
	if (const Value *poly = v.get("polygon"); poly && poly->type == Value::Array) {
		for (const Value &p : poly->items) {
			if (p.type == Value::Array && p.items.size() >= 2 && p.items[0].type == Value::Number &&
			    p.items[1].type == Value::Number)
				s.polygon.push_back({p.items[0].n, p.items[1].n});
		}
	}
	if (s.shape == SlotShape::Polygon && s.polygon.size() < 3) {
		s.shape = SlotShape::Rect;
		s.polygon.clear();
	}
	s.frame = v.boolean("frame", false);
	s.ticker = v.boolean("ticker", false);
	s.host = s.kind == SlotKind::Camera && v.boolean("host", false);
	return true;
}

} // namespace

bool userLayoutsFromJson(const std::string &json, std::vector<UserLayout> &out, std::string *error)
{
	auto fail = [error](const char *why) {
		if (error)
			*error = why;
		return false;
	};
	Value root;
	if (!Parser(json).parse(root) || root.type != Value::Object)
		return fail("not JSON");
	const std::string format = root.string("format");
	/* "layouts-user-layout/1" and later minor revisions of version 1. */
	if (format.rfind("layouts-user-layout/", 0) != 0)
		return fail("unknown format");
	if (std::atoi(format.c_str() + std::char_traits<char>::length("layouts-user-layout/")) != 1)
		return fail("unsupported version");
	const Value *list = root.get("layouts");
	if (!list || list->type != Value::Array)
		return fail("no layouts");
	for (const Value &lv : list->items) {
		if (lv.type != Value::Object)
			continue;
		UserLayout l;
		l.key = lv.string("key");
		if (!isUserKey(l.key))
			l.key.clear(); /* a new key is given when registered */
		l.name = lv.string("name");
		l.aspect = lv.number("aspect", 16.0 / 9.0);
		if (!(l.aspect > 0.05 && l.aspect < 20))
			l.aspect = 16.0 / 9.0;
		if (const Value *slots = lv.get("slots"); slots && slots->type == Value::Array) {
			for (const Value &sv : slots->items) {
				Slot s{SlotKind::Camera, {}};
				if (readSlot(sv, s))
					l.slots.push_back(s);
			}
		}
		if (!l.slots.empty())
			out.push_back(std::move(l));
	}
	return true;
}

/* ------------------------------------------------------------ registry */

UserLayoutRegistry &UserLayoutRegistry::instance()
{
	static UserLayoutRegistry r;
	return r;
}

const UserLayout *UserLayoutRegistry::find(const std::string &key) const
{
	for (const UserLayout &l : layouts_)
		if (l.key == key)
			return &l;
	return nullptr;
}

std::string UserLayoutRegistry::newKey() const
{
	static std::mt19937_64 rng{std::random_device{}()};
	for (;;) {
		char buf[24];
		std::snprintf(buf, sizeof(buf), "user:%08llx", (unsigned long long)(rng() & 0xFFFFFFFFull));
		if (!find(buf))
			return buf;
	}
}

std::string UserLayoutRegistry::uniqueName(const std::string &base) const
{
	auto taken = [this](const std::string &n) {
		return std::any_of(layouts_.begin(), layouts_.end(), [&n](const UserLayout &l) { return l.name == n; });
	};
	if (!taken(base))
		return base;
	for (int i = 2;; i++) {
		const std::string n = base + " (" + std::to_string(i) + ")";
		if (!taken(n))
			return n;
	}
}

std::string UserLayoutRegistry::add(UserLayout layout)
{
	layout.key = newKey();
	layouts_.push_back(std::move(layout));
	return layouts_.back().key;
}

bool UserLayoutRegistry::rename(const std::string &key, const std::string &name)
{
	for (UserLayout &l : layouts_)
		if (l.key == key) {
			l.name = name;
			return true;
		}
	return false;
}

bool UserLayoutRegistry::remove(const std::string &key)
{
	const auto it =
		std::find_if(layouts_.begin(), layouts_.end(), [&key](const UserLayout &l) { return l.key == key; });
	if (it == layouts_.end())
		return false;
	layouts_.erase(it);
	return true;
}

int UserLayoutRegistry::merge(const std::vector<UserLayout> &incoming)
{
	int added = 0;
	for (UserLayout l : incoming) {
		if (l.slots.empty())
			continue;
		if (const UserLayout *same = l.key.empty() ? nullptr : find(l.key)) {
			if (layoutJson(*same, "") == layoutJson(l, ""))
				continue; /* already here */
			l.key.clear();
		}
		const bool nameTaken = std::any_of(layouts_.begin(), layouts_.end(),
						   [&l](const UserLayout &o) { return o.name == l.name; });
		if (l.name.empty() || nameTaken)
			l.name = uniqueName(l.name.empty() ? std::string("Layout") : l.name);
		if (l.key.empty())
			l.key = newKey();
		layouts_.push_back(std::move(l));
		added++;
	}
	return added;
}

} // namespace layouts::core
