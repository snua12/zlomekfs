#include <gtest/gtest.h>
#include <string.h>
#include "formatter-api.h"


// this test is based on original ZEN_TEST (formatter_to_name)
TEST(fromatter_test, formatter_to_name)
{
  ASSERT_EQ(formatter_for_name (RAW_FORMATTER_NAME), &raw_formatter) << "bad formatter returned for " RAW_FORMATTER_NAME;
  ASSERT_EQ(formatter_for_name (USER_READABLE_FORMATTER_NAME), &user_readable_formatter) << "bad formatter returned for " USER_READABLE_FORMATTER_NAME;
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

