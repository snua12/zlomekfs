#ifndef CONFIG_LIMITS_H
#define CONFIG_LIMITS_H

#include"system.h"

#ifdef BUFSIZ
#define LINE_SIZE BUFSIZ
#else
#define LINE_SIZE 2048
#endif

#endif
