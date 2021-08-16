#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "namespaces.h"
#include "util.h"
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

static char *get_name_of_uid(FILE *file, uid_t uid) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    char real_uid_str[21];  // len(str(2**64)) = 20
    snprintf(real_uid_str, 21, "%d", uid);

    while ((nread = getline(&line, &len, file)) != -1) {
        char *colon = strchr(line, ':');
        *colon = '\0';  // first part is the name
        // go for the next colon, after which is the username
        char *uid_str = strchr(colon + 1, ':') + 1;
        *strchr(uid_str, ':') = '\0';
        if (strcmp(uid_str, real_uid_str) == 0) {
            return line;
        }
    }
    free(line);
    return NULL;
}

static void read_subuid_or_subgid(FILE *file, char *name, uid_t *base, uid_t *amount) {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, file)) != -1) {
        // replace : with \0
        char *colon = strchr(line, ':');
        *colon = '\0';
        if (strcmp(name, line) == 0) {
            char *base_and_amount = ++colon;
            colon = strchr(base_and_amount, ':');
            *colon = '\0';
            *base = strtoul(base_and_amount, NULL, 10);
            *amount = strtoul(++colon, NULL, 0);
            break;
        }
    }
    free(line);
}

int enter_user_namespace() {
    // get my user and group
    FILE *passwd_file = fopen("/etc/passwd", "r");
    FILE *group_file = fopen("/etc/group", "r");

    uid_t my_uid = getuid();
    uid_t my_gid = getgid();

    char *my_name = get_name_of_uid(passwd_file, my_uid);
    fclose(passwd_file);
    char *my_group = get_name_of_uid(group_file, my_gid);
    fclose(group_file);

    if (my_name == NULL || my_group == NULL) {
        AWFUL_ERROR;
        fprintf(stderr, "invalid user or group\n");
        return 1;
    }

    // parse /etc/subuid and /etc/subgid
    FILE *subuid_file = fopen("/etc/subuid", "r");
    FILE *subgid_file = fopen("/etc/subgid", "r");
    
    // read line by line
    uid_t uid_base = 0;
    uid_t uid_amount = 0;
    uid_t gid_base = 0;
    uid_t gid_amount = 0;

    read_subuid_or_subgid(subuid_file, my_name, &uid_base, &uid_amount);
    fclose(subuid_file);
    read_subuid_or_subgid(subgid_file, my_name, &gid_base, &gid_amount);
    fclose(subgid_file);

    if (uid_amount == 0 || gid_amount == 0) {
        AWFUL_ERROR;
        fprintf(stderr, "user not in /etc/subuid or /etc/subgid\n");
        return 1;
    }

    // set the uid and gid maps using using /usr/bin/newuidmap
    int commfd1[2];  // communication for synchronization
    pipe(commfd1);
    int commfd2[2];
    pipe(commfd2);

    pid_t parent_pid = getpid();
    pid_t p1 = fork(), p2;
    if (p1 != 0) {
        p2 = fork();
    }
    if (p1 == 0 || p2 == 0) {
        char parent_pid_str[21], a[21], b[21], c[21];
        snprintf(parent_pid_str, 21, "%d", parent_pid);
        char *cmd;

        char buf;
        if (p1 == 0) {
            // responsible for uid
            cmd = "/usr/bin/newuidmap";
            close(commfd1[1]);  // close write end
            snprintf(a, 21, "%d", my_uid); snprintf(b, 21, "%d", uid_base); snprintf(c, 21, "%d", uid_amount);
            // wait for parent to send something
            read(commfd1[0], &buf, 1);
            close(commfd1[0]);
        } else {
            // responsible for gid
            cmd = "/usr/bin/newgidmap";
            close(commfd2[1]);  // close write end
            snprintf(a, 21, "%d", my_gid); snprintf(b, 21, "%d", gid_base); snprintf(c, 21, "%d", gid_amount);
            read(commfd2[0], &buf, 1);
            close(commfd2[0]);
        }

        char *argv[] = {cmd, parent_pid_str, "0", a, "1", "1", b, c, NULL};
        execv(cmd, argv);
        AWFUL_ERROR;
        exit(1);
    } else {
        close(commfd1[0]);  // close read end
        // great - we have our uid and gid ranges! we can unshare at this point
        if (unshare(CLONE_NEWUSER) != 0) {
            AWFUL_ERROR;
            fprintf(stderr, "could not unshare user namespace\n");
            return 1;
        }

        // notify children i'm ready
        char buf = 'A';
        write(commfd1[1], &buf, 1);
        close(commfd1[1]);
        write(commfd2[1], &buf, 1);
        close(commfd2[1]);

        int wstatus;
        waitpid(p1, &wstatus, 0);
        if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
            return 1;
        }
        waitpid(p2, &wstatus, 0);
        if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
            return 1;
        }
    }
    return 0;
}
