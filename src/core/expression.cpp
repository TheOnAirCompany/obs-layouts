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

#include "expression.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace layouts::core {

namespace {

constexpr size_t kMaxLength = 256;
constexpr int kMaxDepth = 32;
constexpr double kMaxMagnitude = 1e9;

struct ParseError : std::runtime_error {
	using std::runtime_error::runtime_error;
};

class Parser {
public:
	Parser(std::string_view s, double percentBase) : s_(s), base_(percentBase) {}

	double parseAll()
	{
		const double v = expr();
		skipWs();
		if (pos_ != s_.size())
			throw ParseError("unexpected character");
		return v;
	}

	void setPercentBase(double b) { base_ = b; }

private:
	std::string_view s_;
	size_t pos_ = 0;
	double base_;
	int depth_ = 0;

	void skipWs()
	{
		while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t'))
			pos_++;
	}

	bool peek(char c)
	{
		skipWs();
		return pos_ < s_.size() && s_[pos_] == c;
	}

	bool accept(char c)
	{
		if (peek(c)) {
			pos_++;
			return true;
		}
		return false;
	}

	static double check(double v)
	{
		if (!std::isfinite(v) || std::fabs(v) > kMaxMagnitude)
			throw ParseError("value out of range");
		return v;
	}

	double expr()
	{
		double v = term();
		for (;;) {
			if (accept('+'))
				v = check(v + term());
			else if (accept('-'))
				v = check(v - term());
			else
				return v;
		}
	}

	double term()
	{
		double v = unary();
		for (;;) {
			if (accept('*') || accept('x') || accept('X')) {
				v = check(v * unary());
			} else if (accept('/')) {
				const double d = unary();
				if (d == 0.0)
					throw ParseError("division by zero");
				v = check(v / d);
			} else {
				return v;
			}
		}
	}

	double unary()
	{
		if (++depth_ > kMaxDepth)
			throw ParseError("expression too deep");
		double v;
		if (accept('-'))
			v = -unary();
		else if (accept('+'))
			v = unary();
		else
			v = postfix();
		depth_--;
		return v;
	}

	double postfix()
	{
		double v = primary();
		if (accept('%'))
			v = check(v * base_ / 100.0);
		return v;
	}

	double primary()
	{
		if (accept('(')) {
			if (++depth_ > kMaxDepth)
				throw ParseError("expression too deep");
			const double v = expr();
			depth_--;
			if (!accept(')'))
				throw ParseError("missing )");
			return v;
		}
		return number();
	}

	double number()
	{
		skipWs();
		const size_t start = pos_;
		double intPart = 0.0;
		bool digits = false;
		while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') {
			intPart = intPart * 10.0 + (s_[pos_] - '0');
			digits = true;
			pos_++;
		}
		double frac = 0.0;
		if (pos_ < s_.size() && (s_[pos_] == '.' || s_[pos_] == ',')) {
			pos_++;
			double scale = 0.1;
			bool fracDigits = false;
			while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') {
				frac += (s_[pos_] - '0') * scale;
				scale /= 10.0;
				fracDigits = true;
				pos_++;
			}
			digits = digits || fracDigits;
		}
		if (!digits) {
			pos_ = start;
			throw ParseError("number expected");
		}
		return check(intPart + frac);
	}
};

std::string_view trim(std::string_view s)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
		s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
		s.remove_suffix(1);
	return s;
}

std::string_view stripUnit(std::string_view s)
{
	s = trim(s);
	for (std::string_view unit : {std::string_view("px"), std::string_view("PX"), std::string_view("deg"),
				      std::string_view("\xC2\xB0") /* ° */}) {
		if (s.size() > unit.size() && s.substr(s.size() - unit.size()) == unit) {
			s.remove_suffix(unit.size());
			return trim(s);
		}
	}
	return s;
}

} // namespace

Expression parseExpression(std::string_view text, double percentBase)
{
	Expression e;
	if (text.size() > kMaxLength) {
		e.error = "input too long";
		return e;
	}
	std::string_view s = stripUnit(text);
	if (s.empty()) {
		e.error = "empty";
		return e;
	}

	double base = percentBase;
	switch (s.front()) {
	case '=':
		e.mode = Expression::Mode::Absolute;
		s.remove_prefix(1);
		break;
	case '+':
		e.mode = Expression::Mode::Add;
		s.remove_prefix(1);
		break;
	case '-':
		e.mode = Expression::Mode::Subtract;
		s.remove_prefix(1);
		break;
	case '*':
	case 'x':
	case 'X':
		e.mode = Expression::Mode::Multiply;
		s.remove_prefix(1);
		base = 1.0;
		break;
	case '/':
		e.mode = Expression::Mode::Divide;
		s.remove_prefix(1);
		base = 1.0;
		break;
	default:
		break;
	}

	try {
		Parser p(s, base);
		e.operand = p.parseAll();
		if (e.mode == Expression::Mode::Divide && e.operand == 0.0)
			throw ParseError("division by zero");
		e.ok = true;
	} catch (const ParseError &err) {
		e.ok = false;
		e.error = err.what();
	}
	return e;
}

bool Expression::apply(double current, double &out) const
{
	if (!ok)
		return false;
	double v = operand;
	switch (mode) {
	case Mode::Absolute:
		v = operand;
		break;
	case Mode::Add:
		v = current + operand;
		break;
	case Mode::Subtract:
		v = current - operand;
		break;
	case Mode::Multiply:
		v = current * operand;
		break;
	case Mode::Divide:
		if (operand == 0.0)
			return false;
		v = current / operand;
		break;
	}
	if (!std::isfinite(v) || std::fabs(v) > kMaxMagnitude)
		return false;
	out = v;
	return true;
}

std::string formatNumber(double v, int decimals)
{
	if (!std::isfinite(v))
		return "0";
	decimals = std::max(0, std::min(decimals, 6));
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
	std::string s(buf);
	if (s.find('.') != std::string::npos) {
		while (!s.empty() && s.back() == '0')
			s.pop_back();
		if (!s.empty() && s.back() == '.')
			s.pop_back();
	}
	if (s == "-0")
		s = "0";
	return s;
}

} // namespace layouts::core
