#include "system.h"
#include <stdio.h>
#include "control.h"

int main(ATTRIBUTE_UNUSED int argc, ATTRIBUTE_UNUSED char * argv[])
{
  initialize_control_c();
  printf("Initialized\n");
  cleanup_control_c();
  return 0;
}

