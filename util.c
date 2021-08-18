#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
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
    md5_hex(md5_result, buf);
    return 0;
}

void md5_hex(unsigned char *original, char *hex) {
    for (int i = 0; i < 16; i++) {
        unsigned int hex_char1 = original[i] / 16;
        unsigned int hex_char2 = original[i] % 16;
        if (hex_char1 < 10) hex_char1 += '0'; else hex_char1 += ('a' - 10);
        if (hex_char2 < 10) hex_char2 += '0'; else hex_char2 += ('a' - 10);
        hex[2 * i] = hex_char1;
        hex[2 * i + 1] = hex_char2;
    }
    hex[32] = '\0';
}

void random_hex(char *buf, size_t buf_size) {
    for (size_t i = 0; i < buf_size - 1; i++) {
        unsigned char x = rand() % 16;
        if (x < 10) x += '0'; else x += ('a' - 10);
        buf[i] = x;
    }
    buf[buf_size - 1] = '\0';
}

int copy_recursive(char *src, char *dst) {
    pid_t p = fork();
    if (p == 0) {
        char *without_last_slash = strdup(dst);
        if (without_last_slash[strlen(without_last_slash) - 1] == '/') {
            without_last_slash[strlen(without_last_slash) - 1] = '\0';
        }
        *strrchr(without_last_slash, '/') = '\0';
        char *mkdir_argv[] = {"/usr/bin/mkdir", "-p", without_last_slash, NULL};
        execv("/usr/bin/mkdir", mkdir_argv);
        exit(1);
    }
    int wstatus;
    do {
        waitpid(p, &wstatus, 0);
    } while (!WIFEXITED(wstatus));
    if (WEXITSTATUS(wstatus) != 0) return WEXITSTATUS(wstatus);
    p = fork();
    if (p == 0) {
        char *cp_argv[] = {"/usr/bin/cp", "-r", src, dst, NULL};
        execv("/usr/bin/cp", cp_argv);
        exit(1);
    }
    do {
        waitpid(p, &wstatus, 0);
    } while (!WIFEXITED(wstatus));
    return WEXITSTATUS(wstatus);
}

int remove_recursive(char *path) {
    pid_t p = fork();
    if (p == 0) {
        char *rm_argv[] = {"/usr/bin/rm", "-rf", path, NULL};
        execv("/usr/bin/rm", rm_argv);
        exit(1);
    }
    int wstatus;
    do {
        waitpid(p, &wstatus, 0);
    } while (!WIFEXITED(wstatus));
    return WEXITSTATUS(wstatus);
}
