#include "envparse.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>  // delet dis

char **get_vars_with_prefix(char *envp[], char *prefix) {
    size_t result_len = 0;
    char **result = calloc(1, sizeof(char *));

    size_t prefix_len = strlen(prefix);
    while (*envp != NULL) {
        char *seperator = strchr(*envp, '=');
        *seperator = '\0';
        if (strncmp(*envp, prefix, prefix_len) == 0) {
            // insert sorted into result
            size_t i = 0;
            for (; i < result_len; i++) {
                if (strcmp(*envp, result[i]) < 0) {
                    // place right before this
                    break;
                }
            }
            // grow result by 1, and move everything from i onwards one slot forward, then place the new key
            result = realloc(result, sizeof(char *) * (++result_len + 1));
            memmove(&result[i + 1], &result[i], (result_len - i) * sizeof(char *));
            result[i] = *envp;
            result[result_len] = NULL;
        } else {
            *seperator = '=';
        }
        ++envp;
    }
    // restore the envp and set pointers to those of the values
    for (size_t i = 0; i < result_len; i++) {
        char *null_addr = result[i] + strlen(result[i]);
        *null_addr = '=';
        result[i] = null_addr + 1;
    }
    return result;
}
