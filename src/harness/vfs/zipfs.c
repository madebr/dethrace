#include "zipfs.h"
#include "vfs_private.h"

#include "harness/trace.h"

#include <string.h>

#include <zip.h>

#define STATE_TO_HANDLE(state) ((struct zipfs_handle*)(state->handle))

struct zipfs_handle {
    char path[512];
    zip_t* archive;
};

#define STATE_ZIPFS_HANDLE(STATE) ((struct zipfs_handle*)((STATE)->handle))
#define VFILE_FILE(VFILE) ((zip_file_t*)((VFILE)->file))

static char* ZIPFS_GetNextFileInDirectory(struct vfs_state* state, struct vfs_diriter_handle* diriter) {
    NOT_IMPLEMENTED();
}

struct vfs_diriter_handle* ZIPFS_OpenDir(struct vfs_state* state, char* path) {
    NOT_IMPLEMENTED();
}

static int ZIPFS_access(struct vfs_state* state, const char* pathname, int mode) {
    zip_stat_t sb;
    int result;
    result = zip_stat(STATE_TO_HANDLE(state)->archive, pathname, ZIP_FL_NOCASE | ZIP_FL_ENC_GUESS, &sb);
    if (result != 0) {
        return -1;
    }
    return 0;
#if 0
    zip_file_t* fd = zip_fopen(STATE_TO_HANDLE(state)->zip, pathname, ZIP_FL_COMPRESSED);
    if (fd == NULL) {
        return -1;
    }
    zip_fclose(fd);
    return 0;
#endif
}

static int ZIPFS_chdir(struct vfs_state* state, const char* path) {
    NOT_IMPLEMENTED();
}

int ZIPFS_fclose(struct vfs_state*, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int ZIPFS_feof(struct vfs_state*, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int ZIPFS_fgetc(struct vfs_state*, VFILE* stream) {
    NOT_IMPLEMENTED();
}

char* ZIPFS_fgets(struct vfs_state*, char* s, int size, VFILE* stream) {
    NOT_IMPLEMENTED();
}

static VFILE* ZIPFS_fopen(struct vfs_state* state, const char* path, const char* mode) {
    zip_file_t* file = zip_fopen(STATE_ZIPFS_HANDLE(state)->archive, path, ZIP_FL_NOCASE | ZIP_FL_ENC_GUESS);
    if (file == NULL) {
        return NULL;
    }
    VFILE* vfile = malloc(sizeof(VFILE));
    vfile->state = state;
    vfile->file = file;
    return vfile;
}

static int ZIPFS_fputs(struct vfs_state* state, const char* s, VFILE* stream) {
    NOT_IMPLEMENTED();
}

static size_t ZIPFS_fread(struct vfs_state*, void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    NOT_IMPLEMENTED();
}

static int ZIPFS_fseek(struct vfs_state*, VFILE* stream, long offset, int whence) {
    NOT_IMPLEMENTED();
}

static long ZIPFS_ftell(struct vfs_state*, VFILE* stream) {
    NOT_IMPLEMENTED();
}

static void ZIPFS_rewind(struct vfs_state*, VFILE* stream) {
    NOT_IMPLEMENTED();
}

static int ZIPFS_ungetc(struct vfs_state*, int c, VFILE* stream) {
    NOT_IMPLEMENTED();
}

static int ZIPFS_vfprintf(struct vfs_state*, VFILE *stream, const char *format, va_list ap) {
    NOT_IMPLEMENTED();
}

static int ZIPFS_vfscanf(struct vfs_state*, VFILE *stream, const char *format, va_list ap) {
    NOT_IMPLEMENTED();
}

const struct vfs_functions gZIPFS_Functions = {
    ZIPFS_GetNextFileInDirectory,
    ZIPFS_OpenDir,
    ZIPFS_access,
    ZIPFS_chdir,
    ZIPFS_fclose,
    ZIPFS_feof,
    ZIPFS_fgetc,
    ZIPFS_fgets,
    ZIPFS_fopen,
    ZIPFS_fputs,
    ZIPFS_fread,
    ZIPFS_fseek,
    ZIPFS_ftell,
    ZIPFS_rewind,
    ZIPFS_ungetc,
    ZIPFS_vfprintf,
    ZIPFS_vfscanf,
};

struct vfs_state* ZIPFS_InitPath(const char* path) {
    // FIXME: splatxmas demo is in subfolder, so change the syntax to allow specifying a subpath in a zip

    int errorCode;
    zip_t* archive = zip_open(path, ZIP_RDONLY, &errorCode);
    if (archive == NULL) {
        zip_error_t error;
        zip_error_init_with_code(&error, errorCode);
        LOG_WARN("libzip failed to open %s (%s)", path, zip_error_strerror(&error));
        return NULL;
    }
    struct vfs_state* newState = malloc(sizeof(struct vfs_state));
    newState->functions = &gZIPFS_Functions;
    struct zipfs_handle* handle = malloc(sizeof(struct zipfs_handle));
    strcpy(handle->path, path);
    handle->archive = archive;
    newState->handle = handle;
    return newState;
}
