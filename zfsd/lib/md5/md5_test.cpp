#include <gtest/gtest.h>
#include <string.h>
#include "md5.h"

TEST(md5_test, compute_md5)
{
  unsigned const char buffer[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  size_t bufferLen = sizeof(buffer) - 1;
  MD5Context context;
  unsigned char digest[MD5_SIZE] = {0x43, 0x7b, 0xba, 0x8e, 0x0b, 0xf5, 0x83, 0x37, 0x67, 0x4f, 0x45, 0x39, 0xe7, 0x51, 0x86, 0xac};
  unsigned char digest_computed[MD5_SIZE];
//  char hext_digest[MD5_SIZE *2];

  MD5Init(&context);
  MD5Update (&context, buffer, bufferLen);
  MD5Final(digest_computed, &context);

  int rv = memcmp(digest, digest_computed, MD5_SIZE);

  ASSERT_EQ(0, rv) << "Failed to computed md5, invalid result!";
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

