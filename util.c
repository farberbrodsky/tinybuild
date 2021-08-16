#include "util.h"
#include <sys/stat.h>

bool dir_exists(char *dirname) {
    struct stat dir_stat;
    if (stat(dirname, &dir_stat) != 0) return false;
    if (S_ISDIR(dir_stat.st_mode)) return true;
    return false;
}
