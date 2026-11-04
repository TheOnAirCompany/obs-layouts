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

/* Math input for numeric fields. A small recursive-descent parser: no code is ever
 * executed. Grammar (whitespace allowed between tokens):
 *
 *   input    := '=' expr                 absolute, explicit (allows "=-10")
 *             | relop expr               relative to the current value
 *             | expr                     absolute
 *   relop    := '+' | '-' | '*' | 'x' | '/'
 *   expr     := term (('+' | '-') term)*
 *   term     := unary (('*' | 'x' | '/') unary)*
 *   unary    := ('+' | '-') unary | postfix
 *   postfix  := primary '%'?
 *   primary  := number | '(' expr ')'
 *   number   := digits [('.' | ',') digits] | ('.' | ',') digits
 *
 * An optional unit suffix "px" or "°" / "deg" at the very end is ignored.
 * "n%" means n percent of the field's percent base (canvas width for X and W, canvas
 * height for Y and H, 360 for rotation, source size for crop). After a '*', 'x' or '/'
 * relative operator, "n%" is a plain fraction: "*50%" halves the value.
 * Division by zero, overflow and malformed input are errors. */

#include <string>
#include <string_view>

namespace layouts::core {

struct Expression {
	enum class Mode { Absolute, Add, Subtract, Multiply, Divide };

	bool ok = false;
	Mode mode = Mode::Absolute;
	double operand = 0.0;
	std::string error;

	bool isRelative() const { return mode != Mode::Absolute; }
	/* Result for a given current value. Returns false on division by zero. */
	bool apply(double current, double &out) const;
};

Expression parseExpression(std::string_view text, double percentBase);

/* Formats a value for display: up to `decimals` decimals, trailing zeros removed,
 * "-0" shown as "0". */
std::string formatNumber(double v, int decimals = 2);

} // namespace layouts::core
