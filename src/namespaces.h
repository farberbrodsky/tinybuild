#ifndef NAMESPACES_H
#define NAMESPACES_H
// Finds the list of my uids in /etc/sub[ug]id, calls unshare and maps them with /proc/self/[ug]id_map. returns 0 if successful
int enter_user_namespace();
// Creates a bind mount and sets the root to mountdst
int enter_file_namespace(char *mountsrc, char *mountdst);
// Tries to unshare PIDs and mount procfs to /proc
int enter_proc_namespace();
#endif
