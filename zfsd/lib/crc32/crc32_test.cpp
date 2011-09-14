#include <gtest/gtest.h>
#include <string.h>
#include "crc32.h"

TEST(crc32_test, compute_crc32)
{
  const char buffer[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  size_t bufferLen = strlen(buffer);
  unsigned int string_crc = crc32_string (buffer); 
  unsigned int buffer_crc = crc32_buffer (buffer, bufferLen);
  unsigned int updated_crc = crc32_update(0, buffer, bufferLen);

  ASSERT_EQ(0xabf77822, string_crc) << "Function crc32_string has failed.";
  ASSERT_EQ(string_crc, buffer_crc) << "Function crc32_buffer has failed." ; 
  ASSERT_EQ(string_crc, updated_crc) << "Function crc32_update has failed." ;
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

