#ifndef NAMESPACES_H
#define NAMESPACES_H
// Finds the list of my uids in /etc/sub[ug]id, calls unshare and maps them with /proc/self/[ug]id_map. returns 0 if successful
int enter_user_namespace();
#endif
