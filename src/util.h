#ifndef UTIL_H
#define UTIL_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdbool.h>
#define AWFUL_ERROR do { fprintf(stderr, "Unrecoverable error at %s in %s:%d\n", __FUNCTION__, __FILE__, __LINE__); } while(0)
bool dir_exists(char *dirname);
// Calculates MD5 hash of a given file, into a given 33-character buffer.
// Returns 0 if successful
int md5_file(FILE *f, char *buf);
// Converts a 16-byte MD5 hash into a 33-character hex version.
void md5_hex(unsigned char *original, char *hex);
// Writes buf_size - 1 random hex digits to the buffer and a \0
void random_hex(char *buf, size_t buf_size);
// Fork and run /usr/bin/cp -r, returns 0 if successful. Also create folder if necessary
int copy_recursive(char *src, char *dst);
int remove_recursive(char *path);
#endif
