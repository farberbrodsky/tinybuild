// gets a prefix
// returns a null-terminated array of environment variable values with that prefix, sorted by key
// will make the envp unstable while running, don't do multiple threads
char **get_vars_with_prefix(char *envp[], char *prefix);
