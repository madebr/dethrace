#include "realfs.h"
#include "vfs_private.h"

#include "harness/os.h"
#include "harness/trace.h"

#if defined(_WIN32)
#include <io.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct REALFS_FILE {
    struct vfs_state* state;
    FILE* file;
} REALFS_FILE;

struct realfs_handle {
    char path[512];
};

#define STATE_REALFS_HANDLE(STATE) ((struct realfs_handle*)((STATE)->handle))

static char* REALFS_GetNextFileInDirectory(struct vfs_state*, struct vfs_diriter_handle* diriter) {
    os_diriter* osdiriter;

    osdiriter = (os_diriter*)diriter;
    return OS_GetNextFileInDirectory(osdiriter);
}

static struct vfs_diriter_handle* REALFS_OpenDir(struct vfs_state* state, char* path) {
    char full_path[512];
    os_diriter* osdiriter;
    strcpy(full_path, STATE_REALFS_HANDLE(state)->path);
    strcat(full_path, "/");
    strcat(full_path, path);

    osdiriter = OS_OpenDir(full_path);
    return (struct vfs_diriter_handle*)osdiriter;
}

static int REALFS_access(struct vfs_state* state, const char* pathname, int mode) {
    char full_path[512];
    strcpy(full_path, STATE_REALFS_HANDLE(state)->path);
    strcat(full_path, "/");
    strcat(full_path, pathname);

    return access(full_path, mode);
}

static int REALFS_chdir(struct vfs_state* state, const char* path) {
    NOT_IMPLEMENTED();
    //    size_t pathLen = strlen(path);
    //    if (pathLen >= 4 && strcasecmp(&path[rootlen-4], ".zip") == 0) {
    //        if (strcmp(gZipPath, path) == 0) {
    //            return
    //        }
    //    }
}

int REALFS_fgetc(struct vfs_state* state, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return fgetc(f->file);
}

char* REALFS_fgets(struct vfs_state* state, char* s, int size, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return fgets(s, size, f->file);
}

static VFILE* REALFS_fopen(struct vfs_state* state, const char* path, const char* mode) {
    char full_path[512];
    strcpy(full_path, STATE_REALFS_HANDLE(state)->path);
    strcat(full_path, "/");
    strcat(full_path, path);

    FILE* f = fopen(full_path, mode);
    if (f == NULL) {
        return NULL;
    }
    REALFS_FILE* realfs_file = malloc(sizeof(REALFS_FILE));
    realfs_file->file = f;
    realfs_file->state = state;
    return (VFILE*)realfs_file;
}

static int REALFS_fclose(struct vfs_state* state, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return fclose(f->file);
}

static int REALFS_feof(struct vfs_state* state, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return feof(f->file);
}

static int REALFS_fputs(struct vfs_state* state, const char* s, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return fputs(s, f->file);
}

static size_t REALFS_fread(struct vfs_state*, void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return fread(ptr, size, nmemb, f->file);
}

static int REALFS_fseek(struct vfs_state*, VFILE* stream, long offset, int whence) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return fseek(f->file, offset, whence);
}

static long REALFS_ftell(struct vfs_state*, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return ftell(f->file);
}

static void REALFS_rewind(struct vfs_state*, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return rewind(f->file);
}

static int REALFS_ungetc(struct vfs_state* state, int c, VFILE* stream) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return ungetc(c, f->file);
}

static int REALFS_vfprintf(struct vfs_state*, VFILE *stream, const char *format, va_list ap) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return vfprintf(f->file, format, ap);
}

static int REALFS_vfscanf(struct vfs_state*, VFILE *stream, const char *format, va_list ap) {
    REALFS_FILE* f = (REALFS_FILE*)stream;
    return vfscanf(f->file, format, ap);
}

const struct vfs_functions gREALFS_Functions = {
    REALFS_GetNextFileInDirectory,
    REALFS_OpenDir,
    REALFS_access,
    REALFS_chdir,
    REALFS_fclose,
    REALFS_feof,
    REALFS_fgetc,
    REALFS_fgets,
    REALFS_fopen,
    REALFS_fputs,
    REALFS_fread,
    REALFS_fseek,
    REALFS_ftell,
    REALFS_rewind,
    REALFS_ungetc,
    REALFS_vfprintf,
    REALFS_vfscanf,
};

struct vfs_state* REALFS_InitPath(const char* path) {
    struct stat statbuf;

    int result = stat(path, &statbuf);
    if (result != 0) {
        LOG_WARN("Cannot open %s (%s)", path, strerror(errno));
        return NULL;
    }


    struct vfs_state* newState = malloc(sizeof(struct vfs_state));
    newState->functions = &gREALFS_Functions;
    struct realfs_handle* handle = malloc(sizeof(struct realfs_handle));
    strcpy(handle->path, path);
    newState->handle = handle;
    return newState;
}

//extern const struct vfs_functions gREALFS_Functions;

//return OS_fopen(pathname, mode);
