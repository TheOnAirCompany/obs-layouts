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

#include "json.hpp"

#include <cstdlib>

namespace layouts::core {

namespace {

class Parser {
public:
	explicit Parser(const std::string &t) : t_(t) {}

	bool parse(JsonValue &out)
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
	bool value(JsonValue &v, int depth)
	{
		if (depth > 32)
			return false;
		ws();
		if (i_ >= t_.size())
			return false;
		const char c = t_[i_];
		if (c == '{') {
			i_++;
			v.type = JsonValue::Object;
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
				JsonValue child;
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
			v.type = JsonValue::Array;
			ws();
			if (i_ < t_.size() && t_[i_] == ']') {
				i_++;
				return true;
			}
			for (;;) {
				JsonValue child;
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
			v.type = JsonValue::String;
			return str(v.s);
		}
		if (lit("true")) {
			v.type = JsonValue::Bool;
			v.b = true;
			return true;
		}
		if (lit("false")) {
			v.type = JsonValue::Bool;
			return true;
		}
		if (lit("null"))
			return true;
		const char *begin = t_.c_str() + i_;
		char *end = nullptr;
		v.n = std::strtod(begin, &end);
		if (end == begin)
			return false;
		v.type = JsonValue::Number;
		i_ += (size_t)(end - begin);
		return true;
	}

	const std::string &t_;
	size_t i_ = 0;
};

} // namespace

bool parseJson(const std::string &text, JsonValue &out)
{
	out = JsonValue();
	return Parser(text).parse(out);
}

} // namespace layouts::core
