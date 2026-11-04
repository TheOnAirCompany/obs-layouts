#include "test.hpp"

#include "expression.hpp"

using namespace layouts::core;

namespace {

double eval(const char *s, double current = 100, double base = 1920)
{
	const Expression e = parseExpression(s, base);
	double out = -12345;
	if (!e.apply(current, out))
		return -12345;
	return out;
}

bool fails(const char *s)
{
	return !parseExpression(s, 1920).ok;
}

} // namespace

TEST("expr: absolute arithmetic")
{
	CHECK_NEAR(eval("1920 / 2"), 960, 1e-12);
	CHECK_NEAR(eval("1080 - 120"), 960, 1e-12);
	CHECK_NEAR(eval("(1 + 2) * 3"), 9, 1e-12);
	CHECK_NEAR(eval("2 * 3 + 4"), 10, 1e-12);
	CHECK_NEAR(eval("2 x 3"), 6, 1e-12);
	CHECK_NEAR(eval("425.68"), 425.68, 1e-12);
	CHECK_NEAR(eval("1,5"), 1.5, 1e-12);
	CHECK_NEAR(eval(".5"), 0.5, 1e-12);
	CHECK_NEAR(eval("  42  "), 42, 0);
}

TEST("expr: relative operators")
{
	CHECK(parseExpression("+20", 0).isRelative());
	CHECK_NEAR(eval("+20"), 120, 1e-12);
	CHECK_NEAR(eval("-10"), 90, 1e-12);
	CHECK_NEAR(eval("*1.5"), 150, 1e-12);
	CHECK_NEAR(eval("x2"), 200, 1e-12);
	CHECK_NEAR(eval("/4"), 25, 1e-12);
	CHECK_NEAR(eval("--5"), 105, 1e-12);
	CHECK_NEAR(eval("+ 10 * 2"), 120, 1e-12);
}

TEST("expr: explicit absolute negative values")
{
	CHECK(!parseExpression("=-10", 0).isRelative());
	CHECK_NEAR(eval("=-10"), -10, 1e-12);
	CHECK_NEAR(eval("(-10)"), -10, 1e-12);
	CHECK_NEAR(eval("0 - 10"), -10, 1e-12);
}

TEST("expr: percentages")
{
	CHECK_NEAR(eval("50%"), 960, 1e-9);
	CHECK_NEAR(eval("100% - 120", 0, 1080), 960, 1e-9);
	CHECK_NEAR(eval("+10%"), 100 + 192, 1e-9);
	CHECK_NEAR(eval("*50%"), 50, 1e-9);
}

TEST("expr: units are ignored")
{
	CHECK_NEAR(eval("10px"), 10, 0);
	CHECK_NEAR(eval("45\xC2\xB0"), 45, 0);
	CHECK_NEAR(eval("90 deg"), 90, 0);
}

TEST("expr: errors")
{
	CHECK(fails(""));
	CHECK(fails("abc"));
	CHECK(fails("1/0"));
	CHECK(fails("/0"));
	CHECK(fails("(1+2"));
	CHECK(fails("1 2"));
	CHECK(fails("1e999"));
	CHECK(fails("system(1)"));
	std::string deep(100, '(');
	CHECK(fails(deep.c_str()));
	std::string longInput(300, '1');
	CHECK(fails(longInput.c_str()));
	CHECK(fails("99999999999"));
}

TEST("expr: number formatting")
{
	CHECK(formatNumber(425.68) == "425.68");
	CHECK(formatNumber(389.0) == "389");
	CHECK(formatNumber(-0.0001) == "0");
	CHECK(formatNumber(1.005, 1) == "1");
	CHECK(formatNumber(12.5, 2) == "12.5");
}
