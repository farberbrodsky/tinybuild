#include "util.h"
#include <stdlib.h>
#include <sys/stat.h>
#include "md5/md5.h"

bool dir_exists(char *dirname) {
    struct stat dir_stat;
    if (stat(dirname, &dir_stat) != 0) return false;
    if (S_ISDIR(dir_stat.st_mode)) return true;
    return false;
}

int md5_file(FILE *f, char *buf) {
    MD5_CTX hash_obj;
    MD5_Init(&hash_obj);
    size_t read_buf_size;
    char read_buf[1024];
    while ((read_buf_size = fread(read_buf, 1, 1024, f)) != 0) {
        MD5_Update(&hash_obj, read_buf, read_buf_size);
    }
    unsigned char md5_result[16];
    MD5_Final(md5_result, &hash_obj);
    for (int i = 0; i < 16; i++) {
        unsigned int hex_char1 = md5_result[i] / 16;
        unsigned int hex_char2 = md5_result[i] % 16;
        if (hex_char1 < 10) hex_char1 += '0'; else hex_char1 += ('a' - 10);
        if (hex_char2 < 10) hex_char2 += '0'; else hex_char2 += ('a' - 10);
        buf[2 * i] = hex_char1;
        buf[2 * i + 1] = hex_char2;
    }
    buf[32] = '\0';
    return 0;
}

void random_hex(char *buf, size_t buf_size) {
    for (size_t i = 0; i < buf_size - 1; i++) {
        unsigned char x = rand() % 16;
        if (x < 10) x += '0'; else x += ('a' - 10);
        buf[i] = x;
    }
    buf[buf_size - 1] = '\0';
}
