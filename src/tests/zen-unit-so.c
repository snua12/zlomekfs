
#include "zen-unit.h"

ZEN_TEST(so_pass)
{
	ZEN_ASSERT(TRUE, "this should not be printed");

	return TRUE;
}

ZEN_TEST(so_fail)
{
	ZEN_ASSERT(FALSE, "this should be printed");

	return TRUE;
}
