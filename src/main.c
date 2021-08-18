#include <time.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include "util.h"
#include "md5/md5.h"
#include "envparse.h"
#include "namespaces.h"

static int default_main(char *tinybuild_private_dir, size_t tinybuild_private_dir_len, char *envp[]) {
    if (enter_user_namespace() != 0) return 1;  // we don't need anything from other users in this code

    if (getenv("FROM") == NULL) {
        fputs("Must have FROM for base image, stored in .tinybuild/imagename, or use subcommands: untar-img\n", stderr);
        return 1;
    }

    size_t base_image_path_len = tinybuild_private_dir_len + strlen(getenv("FROM"));
    char *base_image_path = malloc(base_image_path_len + 1);
    memcpy(base_image_path, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(base_image_path + tinybuild_private_dir_len, getenv("FROM"), strlen(getenv("FROM")) + 1);
    if (!dir_exists(base_image_path)) {
        fprintf(stderr, "Image does not exist at %s\n", base_image_path);
        return 1;
    }

    // read the current hash of the image
    char *base_image_hash_path = malloc(strlen("md5-") + base_image_path_len + 1);
    memcpy(base_image_hash_path, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(base_image_hash_path + tinybuild_private_dir_len, "md5-", strlen("md5-"));
    memcpy(base_image_hash_path + tinybuild_private_dir_len + strlen("md5-"), getenv("FROM"), strlen(getenv("FROM")) + 1);
    FILE *img_hash_f = fopen(base_image_hash_path, "r");
    if (img_hash_f == NULL) {
        fprintf(stderr, "Image hash does not exist at %s\n", base_image_hash_path);
        return 1;
    }
    char img_hash[33];
    fread(img_hash, 32, 1, img_hash_f);
    img_hash[32] = '\0';

    // take all environment variables that start with IMGCOPY and INSTALL, in alphabetical order
    // then create a hash of the copied files and the installation commands, which identifies the container
    // and see if one exists. if it does, great - run it, otherwise create this image
    MD5_CTX container_hash_obj;
    MD5_Init(&container_hash_obj);
    char **imgcopy = get_vars_with_prefix(envp, "IMGCOPY");
    char **install = get_vars_with_prefix(envp, "INSTALL");

    char **install_p = install;
    while (*install_p != NULL) {
        MD5_Update(&container_hash_obj, "-INSTALL-", strlen("-INSTALL-"));
        MD5_Update(&container_hash_obj, *install_p, strlen(*install_p));
        ++install_p;
    }
    char **imgcopy_p = imgcopy;
    while (*imgcopy_p != NULL) {
        MD5_Update(&container_hash_obj, "-IMGCOPY-", strlen("-IMGCOPY-"));
        MD5_Update(&container_hash_obj, *imgcopy_p, strlen(*imgcopy_p));
        ++imgcopy_p;
    }

    unsigned char container_hash[16];
    MD5_Final(container_hash, &container_hash_obj);
    char container_hash_hex[33];
    md5_hex(container_hash, container_hash_hex);

    // the path should be $(base_image_path)-$(img_hash)-$(container_hash_hex)/
    size_t expected_container_path_len = base_image_path_len + 1 + 32 + 1 + 32 + 1;
    char *expected_container_path = malloc(expected_container_path_len + 1);
    memcpy(expected_container_path, base_image_path, base_image_path_len);
    expected_container_path[base_image_path_len] = '-';
    memcpy(expected_container_path + base_image_path_len + 1, img_hash, 32);
    expected_container_path[base_image_path_len + 1 + 32] = '-';
    memcpy(expected_container_path + base_image_path_len + 1 + 32 + 1, container_hash_hex, 32);
    expected_container_path[base_image_path_len + 1 + 32 + 1 + 32] = '/';
    expected_container_path[base_image_path_len + 1 + 32 + 1 + 32 + 1] = '\0';
    printf("My image path is %s\n", expected_container_path);


    // create random mount point, like .tinybuild/mount_[0-9a-f]{6}/
    size_t mount_dir_len = tinybuild_private_dir_len + strlen("mount_") + 6 + 1;
    char *mount_dir = malloc(mount_dir_len + 1);
    memcpy(mount_dir, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(mount_dir + tinybuild_private_dir_len, "mount_", strlen("mount_"));
    do {
        char random_buf[7];
        random_hex(random_buf, 7);
        memcpy(mount_dir + tinybuild_private_dir_len + strlen("mount_"), random_buf, 7);
    } while (dir_exists(mount_dir));
    mount_dir[mount_dir_len - 1] = '/';
    mount_dir[mount_dir_len] = '\0';

    if (dir_exists(expected_container_path)) {
        puts("Using cached image...\n");
    } else {
        // we need to run all those copies and installation commands
        copy_recursive(base_image_path, expected_container_path);
        char **imgcopy_p = imgcopy;
        while (*imgcopy_p != NULL) {
            // split the imgcopy commands by the colon
            char *copy_cmd = *imgcopy_p;
            char *colon = strchr(copy_cmd, ':');
            // take whatever's after the colon
            if (colon == NULL || colon[1] == '\0') {
                fprintf(stderr, "IMGCOPY commands are in the following format: host_src:container_dst\nAnd p.s., if you have a colon in the filename, you're kinda screwed, just mv it with installation commands\n");
                return 1;
            }
            *colon = '\0';
            char *dst = &colon[1];
            size_t dst_len = strlen(dst);
            // copy_cmd is now the source, dst is the destination
            char *real_dst = malloc(expected_container_path_len + dst_len + 1);
            memcpy(real_dst, expected_container_path, expected_container_path_len);
            memcpy(real_dst + expected_container_path_len, dst, dst_len + 1);
            printf("Copying %s to %s\n", copy_cmd, real_dst);
            copy_recursive(copy_cmd, real_dst);
            free(real_dst);
            *colon = ':';
            ++imgcopy_p;
        }

        // run installation commands, one by one
        char **install_p = install;
        while (*install_p != NULL) {
            char *sh_argv[] = {"/bin/sh", "-c", *install_p, NULL};
            printf("Running %s...\n", *install_p);
            pid_t sh_p = fork();
            if (sh_p == 0) {
                if (enter_file_namespace(expected_container_path, mount_dir) != 0) exit(1337);
                execv("/bin/sh", sh_argv);
                exit(1337);
            } else {
                int wstatus = 0;
                do {
                    waitpid(sh_p, &wstatus, 0);
                } while (!WIFEXITED(wstatus));
                if (WEXITSTATUS(wstatus) != 0) {
                    if (WEXITSTATUS(wstatus) == 1337) {
                        fprintf(stderr, "Failed, perhaps /bin/sh does not exist?\n");
                    } else {
                        fprintf(stderr, "Failed with error code %d\n", WEXITSTATUS(wstatus));
                    }
                    rmdir(mount_dir);
                    remove_recursive(expected_container_path);
                    return WEXITSTATUS(wstatus);
                }
            }
            ++install_p;
        }
        puts("Image built!\n");
    }

    free(imgcopy);
    free(install);

    // create a temporary image, with the name image_xxxxx like mount_xxxxx
    const size_t temp_image_path_len = mount_dir_len;
    char *temp_image_path = malloc(temp_image_path_len + 1);
    memcpy(temp_image_path, mount_dir, mount_dir_len + 1);
    memcpy(temp_image_path + tinybuild_private_dir_len, "image_", strlen("image_"));
    if (copy_recursive(expected_container_path, temp_image_path) != 0) {
        fprintf(stderr, "Could not copy image\n");
        return 1;
    }

    // like IMGCOPY but for stuff that changes every run
    char **copy = get_vars_with_prefix(envp, "COPY");
    char **copy_p = copy;

    while (*copy_p != NULL) {
        // split the copy commands by the colon
        char *copy_cmd = *copy_p;
        char *colon = strchr(copy_cmd, ':');
        // take whatever's after the colon
        if (colon == NULL || colon[1] == '\0') {
            fprintf(stderr, "COPY commands are in the following format: host_src:container_dst\nAnd p.s., if you have a colon in the filename, you're kinda screwed, just mv it with installation commands\n");
            return 1;
        }
        *colon = '\0';
        char *dst = &colon[1];
        size_t dst_len = strlen(dst);
        // copy_cmd is now the source, dst is the destination
        char *real_dst = malloc(temp_image_path_len + dst_len + 1);
        memcpy(real_dst, temp_image_path, temp_image_path_len);
        memcpy(real_dst + temp_image_path_len, dst, dst_len + 1);
        printf("Copying %s to %s\n", copy_cmd, real_dst);
        copy_recursive(copy_cmd, real_dst);  // TODO check for a ..
        free(real_dst);
        *colon = ':';
        ++copy_p;
    }

    free(copy);

    // fork so we remove the mount point when we're done
    char *sh_argv[] = {"/bin/sh", NULL, NULL, NULL};
    if (getenv("EXEC") != NULL) {
        sh_argv[1] = "-c";
        sh_argv[2] = getenv("EXEC");
    }
    pid_t p = fork();
    if (p == 0) {
        if (enter_file_namespace(temp_image_path, mount_dir) != 0) exit(1);
        execv("/bin/sh", sh_argv);
        exit(1);  // failed
    }

    int wstatus = 0;
    do {
        waitpid(p, &wstatus, 0);
    } while (!WIFEXITED(wstatus));

    // copy resulting artifacts
    char **postcopy = get_vars_with_prefix(envp, "POSTCOPY");
    char **postcopy_p = postcopy;

    while (*postcopy_p != NULL) {
        // split the copy commands by the colon
        char *copy_cmd = *postcopy_p;
        char *colon = strchr(copy_cmd, ':');
        // take whatever's after the colon
        if (colon == NULL || colon[1] == '\0') {
            fprintf(stderr, "POSTCOPY commands are in the following format: container_src:host_dst\nAnd p.s., if you have a colon in the filename, you're kinda screwed, just mv it with installation commands\n");
            rmdir(mount_dir);
            remove_recursive(temp_image_path);
            return 1;
        }
        *colon = '\0';
        char *real_src = malloc(temp_image_path_len + strlen(copy_cmd) + 1);
        memcpy(real_src, temp_image_path, temp_image_path_len + 1);
        strcat(real_src, copy_cmd);
        char *dst = &colon[1];
        // real_src is now the source, dst is the destination
        printf("Copying %s to %s\n", real_src, dst);
        copy_recursive(real_src, dst);  // TODO check for a ..
        free(real_src);
        *colon = ':';
        ++postcopy_p;
    }

    free(postcopy);

    rmdir(mount_dir);
    free(mount_dir);
    remove_recursive(temp_image_path);
    free(temp_image_path);
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
    }
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

    char *md5_f_path = malloc(tinybuild_private_dir_len + strlen("md5-") + image_name_len + 1);
    memcpy(md5_f_path, tinybuild_private_dir, tinybuild_private_dir_len);
    memcpy(md5_f_path + tinybuild_private_dir_len, "md5-", strlen("md5-"));
    memcpy(md5_f_path + tinybuild_private_dir_len + strlen("md5-"), image_name, image_name_len + 1);
    // check if an md5 exists, and read it
    FILE *md5_f = fopen(md5_f_path, "r+");
    if (md5_f == NULL) {
        // write the first md5 to .tinybuild/md5-{imagename}
        md5_f = fopen(md5_f_path, "w");
        fwrite(md5_buf, 1, 32, md5_f);
    } else {
        // check if the new md5 is different
        char existing_md5[32] = {0};
        fread(existing_md5, 1, 32, md5_f);
        if (memcmp(md5_buf, existing_md5, 32) != 0) {
            // remove anything starting with image_name-
            size_t image_name_dash_len = image_name_len + 1;
            char *image_name_dash = malloc(image_name_len + 2);
            memcpy(image_name_dash, image_name, image_name_len);
            image_name_dash[image_name_len] = '-';
            image_name_dash[image_name_len + 1] = '\0';
            
            // new is different, delete dependent images
            fwrite(md5_buf, 32, 1, md5_f);
            DIR *dr = opendir(tinybuild_private_dir);
            struct dirent *de;
            while ((de = readdir(dr)) != NULL) {
                // de->d_name is the dir name
                if (strncmp(image_name_dash, de->d_name, image_name_dash_len) == 0) {
                    printf("REMOVE %s\n", de->d_name);
                    char *full_path = malloc(tinybuild_private_dir_len + strlen(de->d_name) + 1);
                    memcpy(full_path, tinybuild_private_dir, tinybuild_private_dir_len);
                    memcpy(full_path + tinybuild_private_dir_len, de->d_name, strlen(de->d_name) + 1);
                    remove_recursive(full_path);
                    free(full_path);
                }
            }
            free(image_name_dash);
        } else {
            // same hash, nothing to do
        }
    }
    fclose(md5_f);

    free(md5_f_path);
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
        int code = default_main(tinybuild_private_dir, tinybuild_private_dir_len, envp);
        if (code != 0) return code;
    } else if (argc >= 2 && strcmp(argv[1], "untar-img") == 0) {
        int code = untar_main(tinybuild_private_dir, tinybuild_private_dir_len, argc, argv);
        if (code != 0) return code;
    }
    free(my_path);
    free(tinybuild_private_dir);
    return 0;
}
