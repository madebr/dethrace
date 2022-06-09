// Based on https://gist.github.com/jvranish/4441299

extern "C" {

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
}

#include <emscripten.h>

extern "C" {


DIR* directory_iterator;

void OS_Init(void) {
    setenv("DETHRACE_ROOT_DIR", "/carmdemo.zip", 1);
//    EM_ASM(
//        var file_selector = document.createElement('input');
//        file_selector.setAttribute('type', 'file');
//        file_selector.setAttribute('onchange', 'open_file(event)');
//        file_selector.setAttribute('accept', '.7z,.zip'); // optional - limit accepted file types
//        file_selector.click();
//    );
}

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

}
