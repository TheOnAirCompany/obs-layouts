#include "test.hpp"

#include "broadcast.hpp"
#include "failover.hpp"

using namespace layouts::core;

TEST("failover: family replaces numbers")
{
	CHECK(templateFamily("Screen1Cam3Strip") == "Screen#Cam#Strip");
	CHECK(templateFamily("Cam4Grid") == "Cam#Grid");
}

TEST("failover: a lost camera keeps the family and the order")
{
	const FailoverChoice c = chooseFallback("Screen1Cam3Strip", {"Host", "Guest1", "Guest2", "Slides"}, {"Guest1"});
	CHECK(c.ok);
	CHECK(c.key == "Screen1Cam2Strip");
	CHECK((c.assignment == SlotAssignment{"Host", "Guest2", "Slides"}));
}

TEST("failover: no template of the family, first one in display order")
{
	const FailoverChoice c = chooseFallback("Cam3Columns", {"Host", "A", "B"}, {"B"});
	CHECK(c.ok);
	CHECK(c.key == templatesFor(2, 0).front().key);
	CHECK((c.assignment == SlotAssignment{"Host", "A"}));
}

TEST("failover: host lost, the next guest takes the first slot")
{
	const FailoverChoice c = chooseFallback("Cam2Split", {"Host", "A"}, {"Host"});
	CHECK(c.ok);
	CHECK(c.key == "Cam1Full");
	CHECK((c.assignment == SlotAssignment{"A"}));
}

TEST("failover: a lost screen")
{
	const FailoverChoice c = chooseFallback("Screen1Cam1Side", {"Host", "Slides"}, {"Slides"});
	CHECK(c.ok);
	CHECK(c.key == "Cam1Full");
	CHECK((c.assignment == SlotAssignment{"Host"}));
}

TEST("failover: two sources lost at once")
{
	const FailoverChoice c = chooseFallback("Cam4Grid", {"H", "A", "B", "C"}, {"A", "C"});
	CHECK(c.ok);
	CHECK(c.key == templatesFor(2, 0).front().key);
	CHECK((c.assignment == SlotAssignment{"H", "B"}));
}

TEST("failover: nothing lost gives the original back")
{
	const FailoverChoice c = chooseFallback("Cam3OneTwo", {"H", "A", "B"}, {"Other"});
	CHECK(c.ok);
	CHECK(c.key == "Cam3OneTwo");
	CHECK((c.assignment == SlotAssignment{"H", "A", "B"}));
}

TEST("failover: nothing left or unknown key")
{
	CHECK(!chooseFallback("Cam1Full", {"H"}, {"H"}).ok);
	CHECK(!chooseFallback("NoSuchLayout", {"H"}, {"H"}).ok);
	/* One camera and two screens: the 0.5 templates cover it now. */
	CHECK(chooseFallback("Screen2Cam2Grid", {"H", "A", "S1", "S2"}, {"A"}).ok);
}
