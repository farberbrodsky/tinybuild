#include "util.h"
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "envparse.h"

int main(int argc, char *argv[], char *envp[]) {
    // find my path
    size_t my_path_size = 64;
    char *my_path = malloc(my_path_size);
    ssize_t my_path_read_size = readlink("/proc/self/exe", my_path, my_path_size);
    while (my_path_read_size >= my_path_size) {
        // try again with a larger buffer
        free(my_path);
        my_path = malloc(my_path_size *= 2);
        my_path_read_size = readlink("/proc/self/exe", my_path, my_path_size);
    }

    // remove whatever's after the last /
    size_t my_path_len = my_path_read_size;
    while (my_path[my_path_len] != '/') my_path_len--;
    my_path[my_path_len] = '\0';

    char *tinybuild_private_dir = malloc(my_path_len + 12);
    strcpy(tinybuild_private_dir, my_path);
    strcat(tinybuild_private_dir, ".tinybuild/");

    if (mkdir(tinybuild_private_dir, 0700) != 0) {  // create the .tinybuild directory and only my user can access it
        switch (errno) {
            case 0:
            case EEXIST:
                break;
            case EACCES:
                fprintf(stderr, "Can't create private directory at %s, no access\n", my_path);
                return 1;
            default:
                AWFUL_ERROR;
                return 1;
        }
    }

    free(my_path);
    free(tinybuild_private_dir);

    printf("FROM is %s\n", getenv("FROM"));
    // take all environment variables that start with INSTALL, in alphabetical order
    char **install = get_vars_with_prefix(envp, "INSTALL");
    char **install_p = install;
    while (*install_p != NULL) {
        printf("Install %s\n", *install_p);
        ++install_p;
    }
    free(install);
    return 0;
}
