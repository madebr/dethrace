// Based on https://gist.github.com/jvranish/4441299

#include "harness/os.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

//FIXME: implement all calls!!

DIR* directory_iterator;

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

char* OS_GetFirstFileInDirectory(char* path) {
    directory_iterator = opendir(path);
    if (directory_iterator == NULL) {
        return NULL;
    }
    return OS_GetNextFileInDirectory();
}

char* OS_GetNextFileInDirectory(void) {
    struct dirent* entry;

    if (directory_iterator == NULL) {
        return NULL;
    }
    while ((entry = readdir(directory_iterator)) != NULL) {
        if (entry->d_type == DT_REG) {
            return entry->d_name;
        }
    }
    closedir(directory_iterator);
    directory_iterator = NULL;
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

FILE* OS_fopen(const char* pathname, const char* mode) {
    FILE* f = fopen(pathname, mode);
    if (f != NULL) {
        return f;
    }
    char buffer[512];
    char buffer2[512];
    strcpy(buffer, pathname);
    strcpy(buffer2, pathname);
    char* pDirName = dirname(buffer);
    char* pBaseName = basename(buffer2);
    DIR* pDir = opendir(pDirName);
    if (pDir == NULL) {
        return NULL;
    }
    for (struct dirent* pDirent = readdir(pDir); pDirent != NULL; pDirent = readdir(pDir)) {
        if (strcasecmp(pBaseName, pDirent->d_name) == 0) {
            strcat(pDirName, "/");
            strcat(pDirName, pDirent->d_name);
            return fopen(pDirName, mode);
        }
    }
    return NULL;
}
