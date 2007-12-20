
#include "zen-unit.h"

ZEN_TEST(bin_pass)
{
	ZEN_ASSERT(TRUE, "this should not be printed");

	return TRUE;
}

ZEN_TEST(bin_fail)
{
	ZEN_ASSERT(FALSE, "this should be printed");

	return TRUE;
}

int main (int argc, char ** argv)
{
	exit (1);
}
