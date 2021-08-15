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
    my_path[++my_path_len] = '\0';

    char *tinybuild_private_dir = malloc(my_path_len + strlen(".tinybuild/") + 1);
    strcpy(tinybuild_private_dir, my_path);
    strcat(tinybuild_private_dir, ".tinybuild/");
    size_t tinybuild_private_dir_len = my_path_len + strlen(".tinybuild/");

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

    if (getenv("FROM") == NULL) {
        fputs("Must have FROM for base image, stored in .tinybuild/imagename\n", stderr);
        return 1;
    }

    char *base_image_path = malloc(tinybuild_private_dir_len + strlen(getenv("FROM")) + 1);
    memcpy(base_image_path, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(base_image_path + tinybuild_private_dir_len, getenv("FROM"), strlen(getenv("FROM")));
    struct stat base_image_stat;
    if (stat(base_image_path, &base_image_stat) != 0 || !S_ISDIR(base_image_stat.st_mode)) {
        fprintf(stderr, "Image does not exist at %s\n", base_image_path);
        return 1;
    }

    // great - we have a .tinybuild folder with a base image in it! now we can like, you know, make a container

    /*
    // take all environment variables that start with INSTALL, in alphabetical order
    char **install = get_vars_with_prefix(envp, "INSTALL");
    char **install_p = install;
    while (*install_p != NULL) {
        printf("Install %s\n", *install_p);
        ++install_p;
    }
    free(install);
    */

    free(my_path);
    free(base_image_path);
    free(tinybuild_private_dir);
    return 0;
}
