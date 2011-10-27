#include <gtest/gtest.h>
#include <string.h>
#include "local_config.h"

TEST(config_reader, local_config_test)
{
#if 0
	config_t * config;
	int rv = read_users_local_config(config);
	ASSERT_EQ(CONFIG_TRUE, rv);
#endif
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

