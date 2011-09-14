#include <gtest/gtest.h>
#include <string.h>
#include "memory.h"

TEST(memory_test, xstrconcat)
{
	char * s;
	s = xstrconcat("Hello ", "world", "!", NULL);
	ASSERT_STREQ("Hello world!", s);
	free(s);

	s = xstrconcat(NULL);
	ASSERT_STREQ("", s);
	free(s);
}

TEST(memory_test, memory)
{
  //  ASSERT_EQ(0, rv) << "Failed to computed memory, invalid result!";
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

