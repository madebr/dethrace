#include "harness/vfs.h"
#include "vfs_private.h"

#include "harness/trace.h"

#include "stdiofs.h"
#include "zipfs.h"

#include <stddef.h>
#include <string.h>

struct vfs_state *gVfsStates;

#define FOREACH_VFS_STATE(STATE, STATES) for (struct vfs_state* STATE = (STATES); state != NULL; state = state->next)

int VFS_Init(const char* paths) {
    if (paths == NULL) {
        LOG_INFO("DETHRACE_ROOT_DIR is not set, assuming '.'");
        paths = ".";
    }
    const char* currentPath = paths;
    char pathBuffer[260];
    gVfsStates = NULL;
    struct vfs_state** destVfs = &gVfsStates;
    while (currentPath != NULL) {
        char* endPos = strchr(currentPath, ':');
        size_t pathLen;
        if (endPos == NULL) {
            pathLen = strlen(currentPath);
            strncpy(pathBuffer, currentPath, pathLen);
            currentPath = NULL;
        } else {
            pathLen = endPos - currentPath;
            strncpy(pathBuffer, currentPath, pathLen);
            currentPath += pathLen + 1;
            if (*currentPath == '\0') {
                currentPath = NULL;
            }
        }
        pathBuffer[pathLen] = '\0';
        struct vfs_state* newVfs = NULL;
        if (strstr(pathBuffer, ".zip") != NULL || strstr(pathBuffer, ".ZIP") != NULL) {
#if defined(DETHRACE_VFS_ZIP)
            newVfs = ZIPFS_InitPath(pathBuffer);
#else
            LOG_INFO("DethRace was built without zip support, ignoring %s", pathBuffer);
#endif
        } else {
            newVfs = STDIOFS_InitPath(pathBuffer);
        }
        if (newVfs == NULL) {
            LOG_INFO("Failed to open %s", pathBuffer);
            continue;
        }
        LOG_INFO("VFS search path: %s", pathBuffer);
        newVfs->next = NULL;
        *destVfs = newVfs;
        destVfs = &newVfs->next;
    }
    if (gVfsStates == NULL) {
        LOG_PANIC("Search path empty. Did you set DETHRACE_ROOT_DIR correctly?");
    }
    return 0;
}

vfs_diriter* VFS_OpenDir(char* path) {
    vfs_diriter* diriter;

    diriter = malloc(sizeof(vfs_diriter));
    FOREACH_VFS_STATE(state, gVfsStates) {
        if (state->next != NULL) {
            LOG_PANIC("VFS_OpenDir does not support multiple paths yet");
        }
        diriter->current_state = state;
        diriter->handle = state->functions->OpenDir(state, path);
        if (diriter->handle != NULL) {
            return diriter;
        }
    }
    free(diriter);
    return NULL;
}

char* VFS_GetNextFileInDirectory(vfs_diriter* diriter) {
    char* result;

    if (diriter == NULL) {
        return NULL;
    }
    result = diriter->current_state->functions->GetNextFileInDirectory(diriter->current_state, diriter->handle);
    if (result == NULL) {
        // FIXME: use next state
    }
    return result;
}

int VFS_access(const char* path, int mode) {
    FOREACH_VFS_STATE(state, gVfsStates) {
         int r = state->functions->access(state, path, mode);
         if (r == 0) {
             return 0;
         }
    }
    return -1;
}

int VFS_chdir(const char* path) {
    NOT_IMPLEMENTED();
}

VFILE* VFS_fopen(const char* path, const char* mode) {
    FOREACH_VFS_STATE(state, gVfsStates) {
        VFILE* f = state->functions->fopen(state, path, mode);
        if (f != NULL) {
            return f;
        }
    }
    return NULL;
}

int VFS_fclose(VFILE* stream) {
    int result;
    result = stream->state->functions->fclose(stream->state, stream);
    return result;
}

int VFS_fseek(VFILE* stream, long offset, int whence) {
    int result;
    result = stream->state->functions->fseek(stream->state, stream, offset, whence);
    return result;
}

int VFS_fprintf(VFILE* stream, const char* format, ...) {
    va_list ap;
    int result;
    va_start(ap, format);
    result = stream->state->functions->vfprintf(stream->state, stream, format, ap);
    va_end(ap);
    return result;
}

int VFS_vfprintf(VFILE *stream, const char *format, va_list ap) {
    int result;
    result = stream->state->functions->vfprintf(stream->state, stream, format, ap);
    return result;
}

int VFS_fscanf(VFILE* stream, const char* format, ...) {
    va_list ap;
    int result;
    va_start(ap, format);
    result = stream->state->functions->vfscanf(stream->state, stream, format, ap);
    va_end(ap);
    return result;
}

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    size_t result;
    result = stream->state->functions->fread(stream->state, ptr, size, nmemb, stream);
    return result;
}

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_feof(VFILE* stream) {
    int result;
    result = stream->state->functions->feof(stream->state, stream);
    return result;
}

long VFS_ftell(VFILE* stream) {
    int result;
    result = stream->state->functions->ftell(stream->state, stream);
    return result;
}

void VFS_rewind(VFILE* stream) {
    stream->state->functions->rewind(stream->state, stream);
}

int VFS_fgetc(VFILE* stream) {
    int result;
    result = stream->state->functions->fgetc(stream->state, stream);
    return result;
}

int VFS_ungetc(int c, VFILE* stream) {
    int result;
    result = stream->state->functions->ungetc(stream->state, c, stream);
    return result;
}

char* VFS_fgets(char* s, int size, VFILE* stream) {
    char* result;
    result = stream->state->functions->fgets(stream->state, s, size, stream);
    return result;
}

int VFS_fputc(int c, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_fputs(const char* s, VFILE* stream) {
    return stream->state->functions->fputs(stream->state, s, stream);
}

int VFS_remove(const char* pathname) {
    NOT_IMPLEMENTED();
}

int VFS_rename(const char* oldpath, const char* newpath) {
    NOT_IMPLEMENTED();
}
