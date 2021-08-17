#include "util.h"
#include <time.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include "envparse.h"
#include "namespaces.h"

static int default_main(char *tinybuild_private_dir, size_t tinybuild_private_dir_len) {
    if (getenv("FROM") == NULL) {
        fputs("Must have FROM for base image, stored in .tinybuild/imagename, or use subcommands: untar-img\n", stderr);
        return 1;
    }

    char *base_image_path = malloc(tinybuild_private_dir_len + strlen(getenv("FROM")) + 1);
    memcpy(base_image_path, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(base_image_path + tinybuild_private_dir_len, getenv("FROM"), strlen(getenv("FROM")) + 1);
    if (!dir_exists(base_image_path)) {
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

    if (enter_user_namespace() != 0) return 1;
    // create random mount point, like .tinybuild/mount_[0-9a-f]{6}
    char *mount_dir = malloc(tinybuild_private_dir_len + strlen("mount_") + 7);
    memcpy(mount_dir, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(mount_dir + tinybuild_private_dir_len, "mount_", strlen("mount_"));
    do {
        char random_buf[7];
        random_hex(random_buf, 7);
        memcpy(mount_dir + tinybuild_private_dir_len + strlen("mount_"), random_buf, 7);
    } while (dir_exists(mount_dir));

    // fork so we remove the mount point when we're done
    char *bash_argv[] = {"/bin/bash", NULL};
    pid_t p = fork();
    if (p == 0) {
        if (enter_file_namespace(base_image_path, mount_dir) != 0) exit(1);
        execv("/bin/bash", bash_argv);
        exit(1);  // failed
    }

    int wstatus = 0;
    do {
        waitpid(p, &wstatus, 0);
    } while (!WIFEXITED(wstatus));
    rmdir(mount_dir);
    free(base_image_path);
    return WEXITSTATUS(wstatus);
}


static int untar_main(char *tinybuild_private_dir, size_t tinybuild_private_dir_len, int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s untar-img x.tar image_name\n", argv[0]);
        return 2;
    }

    char *tar_name = argv[2];
    char *image_name = argv[3];
    if (strchr(image_name, '-') != NULL) {
        fprintf(stderr, "Image name can't have a - in it.\n");
        return 1;
    }
    size_t image_name_len = strlen(image_name);
    char *image_dir = malloc(tinybuild_private_dir_len + image_name_len + 1);
    memcpy(image_dir, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(image_dir + tinybuild_private_dir_len, image_name, image_name_len);
    image_dir[tinybuild_private_dir_len + image_name_len] = '\0';

    if (dir_exists(image_dir)) {
        puts("Image exists, removing...\n");
        char *rm_argv[] = {"/bin/rm", "-rf", image_dir, NULL};
        int wstatus;
        pid_t rm_pid = fork(); if (rm_pid == 0) execv("/bin/rm", rm_argv); waitpid(rm_pid, &wstatus, 0);
        if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
            fprintf(stderr, "Couldn't remove\n");
            return 1;
        }
    }
    if (mkdir(image_dir, 0700) != 0) return 1;
    // run tar xvf as the target user
    if (enter_user_namespace() != 0) return 1;
    puts("starting tar xvf...\n");
    char *tar_argv[] = {"/usr/bin/tar", "-xvf", tar_name, "-C", image_dir, NULL};
    pid_t p = fork();
    if (p == 0) {
        execv("/usr/bin/tar", tar_argv);
        fprintf(stderr, "failed, tar is not in /usr/bin/tar or not executable\n");
        exit(1);
    } else {
        // calculate md5 hash, then wait
        FILE *f = fopen(tar_name, "r");
        if (f == NULL) {
            fprintf(stderr, "%s does not exist or could not be opened\n", tar_name);
            return 2;
        }
        char md5_buf[33];
        md5_file(f, md5_buf);

        int wstatus = 0;
        do {
            waitpid(p, &wstatus, 0);
        } while (!WIFEXITED(wstatus));
        if (WEXITSTATUS(wstatus) != 0) {
            return WEXITSTATUS(wstatus);
        }

        puts("Calculating tar md5...\n");

        // write the md5 to .tinybuild/md5-{imagename}
        char *md5_f_path = malloc(tinybuild_private_dir_len + strlen("md5-") + image_name_len + 1);
        memcpy(md5_f_path, tinybuild_private_dir, tinybuild_private_dir_len);
        memcpy(md5_f_path + tinybuild_private_dir_len, "md5-", strlen("md5-"));
        memcpy(md5_f_path + tinybuild_private_dir_len + strlen("md5-"), image_name, image_name_len + 1);
        FILE *md5_f = fopen(md5_f_path, "w");
        fwrite(md5_buf, 1, 32, md5_f);
        fclose(md5_f);
    }
    free(image_dir);
    puts("Done.\n");
    return 0;
}


int main(int argc, char *argv[], char *envp[]) {
    // initialize random seed
    unsigned int seed;
    FILE *urandom = fopen("/dev/urandom", "r");
    if (urandom == NULL) {
        srand(time(NULL) + getpid() * 1337);
    } else {
        fread(&seed, sizeof(unsigned int), 1, urandom);
        srand(seed);
        fclose(urandom);
    }

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

    if (argc <= 1) {
        // no arguments, trying to use a base image
        int code = default_main(tinybuild_private_dir, tinybuild_private_dir_len);
        if (code != 0) return code;
    } else if (argc >= 2 && strcmp(argv[1], "untar-img") == 0) {
        int code = untar_main(tinybuild_private_dir, tinybuild_private_dir_len, argc, argv);
        if (code != 0) return code;
    }
    free(my_path);
    free(tinybuild_private_dir);
    return 0;
}
