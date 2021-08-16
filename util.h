#ifndef UTIL_H
#define UTIL_H
#include <stdbool.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define AWFUL_ERROR do { fprintf(stderr, "Unrecoverable error at %s in %s:%d\n", __FUNCTION__, __FILE__, __LINE__); } while(0)
bool dir_exists(char *dirname);
#endif
