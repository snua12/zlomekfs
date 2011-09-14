#include <gtest/gtest.h>
#include <string.h>
#include "alloc-pool.h"

TEST(alloc_pool_test, alloc_pool)
{
  alloc_pool pool = create_alloc_pool ("google-test", 16, 10, NULL);
  ASSERT_TRUE(pool != NULL) << "Failed to create alloc pool.";

  free_alloc_pool (pool);
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

