#include "harness/vfs.h"

#include "harness/trace.h"

#include <physfs/extras/ignorecase.h>
#include <physfs.h>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define strcasecmp _stricmp
#define strdup _strdup
#endif

#ifndef W_OK
#define W_OK 2
#endif

#define VFS_MIN(X, Y) ((X) <= (Y) ? (X) : (Y))

typedef enum {
    VFILE_READ,
    VFILE_WRITE,
    VFILE_APPEND,
} VFILE_type;

typedef struct VFILE {
    PHYSFS_File *file;
    uint64_t size;
    uint64_t position;
    uint64_t capacity;
    char* buffer;
    VFILE_type type;
} VFILE;

typedef struct vfs_diriter {
    char** list;
    size_t index;
} vfs_diriter;

int VFS_Init(int argc, const char* argv[], const char* paths) {
    int result;

    result = PHYSFS_init(argv[0]);
    if (result == 0) {
        PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
        LOG_WARN("PHYSFS_init failed with %d (%s)", ec, PHYSFS_getErrorByCode(ec));
        return 1;
    }
    if (paths == NULL) {
        paths = ".";
        LOG_INFO("DETHRACE_ROOT_DIR is not set, assuming '%s'", paths);
    }
    const char* currentPath = paths;
    char pathBuffer[260];
    while (currentPath != NULL) {
        char* endPos = strchr(currentPath, ';');
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
        result = PHYSFS_mount(pathBuffer, NULL, 1);
        if (result == 0) {
            PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
            LOG_WARN("PHYSFS_mount(\"%s\", NULL, 1) failed with %d (%s)", pathBuffer, ec, PHYSFS_getErrorByCode(ec));
            continue;
        }
        LOG_INFO("VFS search path: %s", pathBuffer);
    }
#if defined(_WIN32)
    _set_printf_count_output(1);
#endif
    return 0;
}

static const char* vfs_write_dir;

void VFS_SetWriteDir(const char* path) {
    if (vfs_write_dir != NULL) {
        PHYSFS_unmount(vfs_write_dir);
    }
    PHYSFS_mount(path, "/", 1);
    PHYSFS_setWriteDir(path);
}

vfs_diriter* VFS_OpenDir(char* path) {
    char** list;
    vfs_diriter* diriter;

    list = PHYSFS_enumerateFiles(path);
    if (list == NULL) {
        return NULL;
    }
    diriter = malloc(sizeof(vfs_diriter));
    diriter->list = list;
    diriter->index = 0;
    return diriter;
}

char* VFS_GetNextFileInDirectory(vfs_diriter* diriter) {
    char* result;

    if (diriter == NULL) {
        return NULL;
    }
    result = diriter->list[diriter->index];
    diriter->index++;
    if (result == NULL) {
        PHYSFS_freeList(diriter->list);
        free(diriter);
    }
    return result;
}

int VFS_access(const char* path, int mode) {
    PHYSFS_Stat stat;
    int result;

    result = PHYSFS_stat(path, &stat);
    if (result == 0) {
        return -1;
    }
    if ((mode & W_OK) != 0) {
        if (stat.readonly) {
            return -1;
        }
    }
    return 0;
}

int VFS_chdir(const char* path) {
    NOT_IMPLEMENTED();
}

static VFILE* vfile_openRead(const char* path) {
    PHYSFS_File* f;
    VFILE* vfile;

    f = PHYSFS_openRead(path);
    if (f == NULL) {
        return NULL;
    }
    vfile = malloc(sizeof(VFILE));
    if (vfile == NULL) {
        PHYSFS_close(f);
        return NULL;
    }
    vfile->file = NULL;
    vfile->size = PHYSFS_fileLength(f);
    vfile->capacity = vfile->size;
    vfile->position = 0;
    vfile->type = VFILE_READ;
    // FIXME: or use `PHYSFS_setBuffer`?
    vfile->buffer = malloc(vfile->size);
    PHYSFS_readBytes(f, vfile->buffer, vfile->size);
    PHYSFS_close(f);
    return vfile;
}

static VFILE* vfile_openWrite(const char* path) {
    PHYSFS_File* f;
    VFILE* vfile;

    f = PHYSFS_openWrite(path);
    if (f == NULL) {
        return NULL;
    }
    vfile = malloc(sizeof(VFILE));
    if (vfile == NULL) {
        PHYSFS_close(f);
        return NULL;
    }
    vfile->file = f;
    vfile->size = 0;
    vfile->capacity = 0;
    vfile->position = 0;
    vfile->type = VFILE_WRITE;
    vfile->buffer = NULL;
    return vfile;
}

static VFILE* vfile_openAppend(const char* path) {
    NOT_IMPLEMENTED();
}

VFILE* VFS_fopen(const char* path, const char* mode) {
    VFILE* vfile;
    char* path_modified;
    int found;

    path_modified = strdup(path);
    found = PHYSFSEXT_locateCorrectCase(path_modified) == 0;
    vfile = NULL;
    if (strchr(mode, 'r') != NULL) {
        if (found) {
            vfile = vfile_openRead(path_modified);
        }
    }
    else if (strchr(mode, 'w') != NULL) {
        vfile = vfile_openWrite(path_modified);
    }
    else if (strchr(mode, 'a') != NULL) {
        vfile = vfile_openAppend(path_modified);
    }
    if (vfile == NULL) {
//        LOG_WARN("Failed to open %s", path);
    }
    free(path_modified);
    return vfile;
}

int VFS_fclose(VFILE* stream) {
    int result;

    result = 0;
    free(stream->buffer);
    if (stream->file != NULL) {
        result = PHYSFS_close(stream->file) ? 0 : EOF;
    }
    free(stream);
    return result;
}

int VFS_fseek(VFILE* stream, long offset, int whence) {
    int result;
    int64_t new_position;

    if (stream->type != VFILE_READ) {
        return -1;
    }
    switch (whence) {
    case SEEK_SET:
        new_position = offset;
        break;
    case SEEK_CUR:
        new_position = stream->position + offset;
        break;
    case SEEK_END:
        new_position = stream->size;
        break;
    default:
        return -1;
    }
    result = 0;
    if (new_position < 0) {
        new_position = 0;
    } else if ((uint64_t)new_position > stream->size) {
        new_position = stream->size + 1;
        result = -1;
    }
    stream->position = new_position;
    return result;
}

size_t vfs_fprintf_marker_internal;

int VFS_fprintf_internal(VFILE* stream, const char* format, ...) {
    va_list ap;
    char hugebuffer[4096];
    int nb;

    va_start(ap, format);
    nb = vsnprintf(hugebuffer, sizeof(hugebuffer), format, ap);
    va_end(ap);

    PHYSFS_writeBytes(stream->file, hugebuffer, vfs_fprintf_marker_internal);

    return nb;
}

int VFS_vfprintf(VFILE *stream, const char *format, va_list ap) {
    char hugebuffer[4096];
    int nb;

    nb = vsnprintf(hugebuffer, sizeof(hugebuffer), format, ap);
    hugebuffer[sizeof(hugebuffer)-1] = '\0';

    PHYSFS_writeBytes(stream->file, hugebuffer, strlen(hugebuffer));

    return nb;
}

size_t vfs_scanf_marker_internal;

int VFS_fscanf_internal(VFILE* stream, const char* format, ...) {
    va_list ap;
    int nb;

    if (stream->type != VFILE_READ) {
        return 0;
    }
    if (stream->position >= stream->size) {
        return 0;
    }

    va_start(ap, format);
    nb = vsscanf(&stream->buffer[stream->position], format, ap);
    va_end(ap);

    stream->position += vfs_scanf_marker_internal;

    return nb;
}

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    size_t block_i;
    int switcheroo;

    if (stream->type != VFILE_READ) {
        return 0;
    }

    switcheroo = 0;
    if (size == 1) {
        size = nmemb;
        nmemb = 1;
        switcheroo = 1;
    }

    for (block_i = 0; block_i < nmemb; block_i++) {
        if (stream->size - stream->position < size) {
            stream->position = stream->size + 1;
            return block_i;
        }
        memcpy(ptr, &stream->buffer[stream->position], size);
        stream->position += size;
        ptr = ((char*)ptr) + size;
    }
    if (switcheroo) {
        return size;
    }
    return block_i;
}

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    size_t block_i;
    int switcheroo;

    if (stream->type == VFILE_APPEND) {
        NOT_IMPLEMENTED();
    }
    if (stream->type != VFILE_WRITE) {
        return 0;
    }

    switcheroo = 0;
    if (size == 1) {
        size = nmemb;
        nmemb = 1;
        switcheroo = 1;
    }

    for (block_i = 0; block_i < nmemb; block_i++) {
        PHYSFS_writeBytes(stream->file, ptr, size);
        ptr = ((char*)ptr) + size;
    }
    if (switcheroo) {
        return size;
    }
    return block_i;
}

int VFS_feof(VFILE* stream) {

    if (stream->type != VFILE_READ) {
        return 0;
    }
    return stream->position > stream->size;
}

long VFS_ftell(VFILE* stream) {

    if (stream->type != VFILE_READ) {
        return -1;
    }
    if (stream->position >= stream->size) {
        return (long)stream->size;
    }
    return (long)stream->position;
}

void VFS_rewind(VFILE* stream) {

    if (stream->type != VFILE_READ) {
        return;
    }
    stream->position = 0;
}

int VFS_fgetc(VFILE* stream) {
    int c;

    if (stream->type != VFILE_READ) {
        return EOF;
    }
    if (stream->position >= stream->size) {
        stream->position = stream->size + 1;
        return EOF;
    }

    c = stream->buffer[stream->position];
    stream->position++;
    return c;
}

int VFS_ungetc(int c, VFILE* stream) {

    if (stream->type != VFILE_READ) {
        return EOF;
    }
    if (stream->position <= 0) {
        return EOF;
    }
    if (stream->size == 0) {
        return EOF;
    }

    stream->position--;
    stream->buffer[stream->position] = c;
    return c;
}

char* VFS_fgets(char* s, int size, VFILE* stream) {
    char* end;
    uint64_t copy_size;

    if (stream->type != VFILE_READ) {
        return NULL;
    }
    if (stream->position >= stream->size) {
        stream->position = stream->size + 1;
        return NULL;
    }
    if (size <= 0) {
        return NULL;
    }
    end = strpbrk(&stream->buffer[stream->position], "\n");
    if (end == NULL) {
        copy_size = stream->size - stream->position;
    } else {
        copy_size = end - &stream->buffer[stream->position] + 1;
    }
    copy_size = VFS_MIN((size_t)size - 1, copy_size);

    memcpy(s, &stream->buffer[stream->position], copy_size);
    s[copy_size] = '\0';
    stream->position += copy_size;
    return s;
}

int VFS_fputc(int c, VFILE* stream) {
    char character;
    PHYSFS_sint64 count;

    if (stream->type == VFILE_APPEND) {
        NOT_IMPLEMENTED();
    }
    if (stream->type != VFILE_WRITE) {
        return 0;
    }

    character = c;
    count = PHYSFS_writeBytes(stream->file, &character, 1);

    return (int)count;
}

int VFS_fputs(const char* s, VFILE* stream) {
    PHYSFS_sint64 res;

    if (stream->type == VFILE_APPEND) {
        NOT_IMPLEMENTED();
    }
    if (stream->type != VFILE_WRITE) {
        return EOF;
    }

    res = PHYSFS_writeBytes(stream->file, s, strlen(s));
    if (res < 0) {
        return EOF;
    }
    return (int)res;

}

int VFS_remove(const char* pathname) {
    NOT_IMPLEMENTED();
}

int VFS_rename(const char* oldpath, const char* newpath) {
    NOT_IMPLEMENTED();
}
