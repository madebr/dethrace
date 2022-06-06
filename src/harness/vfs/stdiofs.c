#include "stdiofs.h"
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

struct stdiofs_handle {
    char path[512];
};

#define STATE_STDIOFS_HANDLE(STATE) ((struct stdiofs_handle*)((STATE)->handle))
#define VFILE_FILE(VFILE) ((FILE*)((VFILE)->file))

static char* STDIOFS_GetNextFileInDirectory(struct vfs_state*, struct vfs_diriter_handle* diriter) {
    os_diriter* osdiriter;

    osdiriter = (os_diriter*)diriter;
    return OS_GetNextFileInDirectory(osdiriter);
}

static struct vfs_diriter_handle* STDIOFS_OpenDir(struct vfs_state* state, char* path) {
    char full_path[512];
    os_diriter* osdiriter;
    strcpy(full_path, STATE_STDIOFS_HANDLE(state)->path);
    strcat(full_path, "/");
    strcat(full_path, path);

    osdiriter = OS_OpenDir(full_path);
    return (struct vfs_diriter_handle*)osdiriter;
}

static int STDIOFS_access(struct vfs_state* state, const char* pathname, int mode) {
    char full_path[512];
    strcpy(full_path, STATE_STDIOFS_HANDLE(state)->path);
    strcat(full_path, "/");
    strcat(full_path, pathname);

    return access(full_path, mode);
}

static int STDIOFS_chdir(struct vfs_state* state, const char* path) {
    NOT_IMPLEMENTED();
    //    size_t pathLen = strlen(path);
    //    if (pathLen >= 4 && strcasecmp(&path[rootlen-4], ".zip") == 0) {
    //        if (strcmp(gZipPath, path) == 0) {
    //            return
    //        }
    //    }
}

int STDIOFS_fgetc(struct vfs_state* state, VFILE* stream) {
    return fgetc(VFILE_FILE(stream));
}

char* STDIOFS_fgets(struct vfs_state* state, char* s, int size, VFILE* stream) {
    return fgets(s, size, VFILE_FILE(stream));
}

static VFILE* STDIOFS_fopen(struct vfs_state* state, const char* path, const char* mode) {
    char full_path[512];
    strcpy(full_path, STATE_STDIOFS_HANDLE(state)->path);
    strcat(full_path, "/");
    strcat(full_path, path);

    FILE* file = fopen(full_path, mode);
    if (file == NULL) {
        return NULL;
    }
    VFILE* vfile = malloc(sizeof(VFILE));
    vfile->file = file;
    vfile->state = state;
    return vfile;
}

static int STDIOFS_fclose(struct vfs_state* state, VFILE* stream) {
    return fclose(VFILE_FILE(stream));
}

static int STDIOFS_feof(struct vfs_state* state, VFILE* stream) {
    return feof(VFILE_FILE(stream));
}

static int STDIOFS_fputs(struct vfs_state* state, const char* s, VFILE* stream) {
    return fputs(s, VFILE_FILE(stream));
}

static size_t STDIOFS_fread(struct vfs_state*, void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    return fread(ptr, size, nmemb, VFILE_FILE(stream));
}

static int STDIOFS_fseek(struct vfs_state*, VFILE* stream, long offset, int whence) {
    return fseek(VFILE_FILE(stream), offset, whence);
}

static long STDIOFS_ftell(struct vfs_state*, VFILE* stream) {
    return ftell(VFILE_FILE(stream));
}

static void STDIOFS_rewind(struct vfs_state*, VFILE* stream) {
    return rewind(VFILE_FILE(stream));
}

static int STDIOFS_ungetc(struct vfs_state* state, int c, VFILE* stream) {
    return ungetc(c, VFILE_FILE(stream));
}

static int STDIOFS_vfprintf(struct vfs_state*, VFILE *stream, const char *format, va_list ap) {
    return vfprintf(VFILE_FILE(stream), format, ap);
}

static int STDIOFS_vfscanf(struct vfs_state*, VFILE *stream, const char *format, va_list ap) {
    return vfscanf(VFILE_FILE(stream), format, ap);
}

const struct vfs_functions gSTDIOFS_Functions = {
    STDIOFS_GetNextFileInDirectory,
    STDIOFS_OpenDir,
    STDIOFS_access,
    STDIOFS_chdir,
    STDIOFS_fclose,
    STDIOFS_feof,
    STDIOFS_fgetc,
    STDIOFS_fgets,
    STDIOFS_fopen,
    STDIOFS_fputs,
    STDIOFS_fread,
    STDIOFS_fseek,
    STDIOFS_ftell,
    STDIOFS_rewind,
    STDIOFS_ungetc,
    STDIOFS_vfprintf,
    STDIOFS_vfscanf,
};

struct vfs_state* STDIOFS_InitPath(const char* path) {
    struct stat statbuf;

    int result = stat(path, &statbuf);
    if (result != 0) {
        LOG_WARN("Cannot open %s (%s)", path, strerror(errno));
        return NULL;
    }


    struct vfs_state* newState = malloc(sizeof(struct vfs_state));
    newState->functions = &gSTDIOFS_Functions;
    struct stdiofs_handle* handle = malloc(sizeof(struct stdiofs_handle));
    strcpy(handle->path, path);
    newState->handle = handle;
    return newState;
}
