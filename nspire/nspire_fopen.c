/*
 * nspire_fopen.c - Linker-level fopen wrapper for TI-Nspire
 *
 * On TI-Nspire, ALL files must have .tns extension.
 * This uses the --wrap=fopen linker flag to intercept ALL
 * fopen() calls and automatically append ".tns" when needed.
 *
 * The linker renames:
 *   fopen() calls -> __wrap_fopen()  (our function)
 *   __real_fopen() calls -> the original fopen()
 */

#include <stdio.h>
#include <string.h>

/* Provided by the linker --wrap mechanism */
extern FILE *__real_fopen(const char *path, const char *mode);

FILE *__wrap_fopen(const char *path, const char *mode) {
    size_t len = strlen(path);
    int has_tns = (len >= 4 && strcmp(path + len - 4, ".tns") == 0);

    /* For write/append modes: always use .tns extension */
    if (!has_tns && (strchr(mode, 'w') || strchr(mode, 'a'))) {
        char tns_path[320];
        snprintf(tns_path, sizeof(tns_path), "%s.tns", path);
        return __real_fopen(tns_path, mode);
    }

    /* For read modes: try original path first, then with .tns */
    FILE *f = __real_fopen(path, mode);
    if (f) return f;

    if (!has_tns) {
        char tns_path[320];
        snprintf(tns_path, sizeof(tns_path), "%s.tns", path);
        f = __real_fopen(tns_path, mode);
    }
    return f;
}
