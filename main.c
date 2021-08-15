#include <stdio.h>
#include <stdlib.h>
#include "envparse.h"

int main(int argc, char *argv[], char *envp[]) {
    printf("FROM is %s\n", getenv("FROM"));
    // take all environment variables that start with INSTALL, in alphabetical order
    char **install = get_vars_with_prefix(envp, "INSTALL");
    while (*install != NULL) {
        printf("Install %s\n", *install);
        ++install;
    }
    free(install);
}
