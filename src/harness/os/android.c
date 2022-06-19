// Based on https://gist.github.com/jvranish/4441299

#include "harness/os.h"

#if defined(dethrace_stdio_vfs_aliased)
#error "stdio functions aliased to vfs functions")
#endif

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

uint32_t OS_GetTime() {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
}

void OS_Sleep(int delay_ms) {
    struct timespec ts;
    ts.tv_sec = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000;
    nanosleep(&ts, &ts);
}

int OS_IsDirectory(const char* path) {
    struct stat statbuf;

    if (stat(path, &statbuf) != 0) {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

os_diriter* OS_OpenDir(char* path) {
    DIR* diriter = opendir(path);
    if (diriter == NULL) {
        return NULL;
    }
    return (os_diriter*)diriter;
}

char* OS_GetNextFileInDirectory(os_diriter* diriter) {
    DIR* pDir;
    struct dirent* entry;

    pDir = (DIR*)diriter;
    if (pDir == NULL) {
        return NULL;
    }
    while ((entry = readdir(pDir)) != NULL) {
        if (entry->d_type == DT_REG) {
            return entry->d_name;
        }
    }
    closedir(pDir);
    return NULL;
}

void OS_Basename(char* path, char* base) {
    strcpy(base, basename(path));
}

int OS_IsDebuggerPresent() {
    return 0;
}

void OS_InstallSignalHandler(char* program_name) {
}
