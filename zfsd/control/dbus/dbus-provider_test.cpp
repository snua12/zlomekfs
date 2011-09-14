#include <gtest/gtest.h>
#include <string.h>
#include "dbus-provider.h"

TEST(dbus_provider_test, add_listener)
{

  struct dbus_state_holder_def prov;
  dbus_name_add_t  add_name = (dbus_name_add_t) 0x1;
  dbus_name_release_t rel_name = (dbus_name_release_t)0x2;
  dbus_message_handler_t handle = (dbus_message_handler_t)0x3;

  int ret =  dbus_provider_init (&prov);
  ASSERT_TRUE(ret) << "failed to initialize provider struct";
  ret = dbus_provider_add_listener (&prov, add_name, rel_name, handle);
  ASSERT_TRUE(ret) << "_add_listener has failed";
  ASSERT_EQ(prov.listener_count, 1) << "wrong listener count";
  ASSERT_TRUE(prov.listeners[0].handle_message == handle) << "invalid handler set";
  ASSERT_TRUE(prov.listeners[0].add_name == add_name) << "invalid add function set";
  ASSERT_TRUE(prov.listeners[0].release_name == rel_name) << "invalid release function set";
}
	
int main(int argc, char **argv) 
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

