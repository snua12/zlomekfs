#include <gtest/gtest.h>
#include <string.h>
#include "memory.h"

TEST(memory_test, memory)
{
  //  ASSERT_EQ(0, rv) << "Failed to computed memory, invalid result!";
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

