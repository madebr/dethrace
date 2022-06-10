#include "harness/vfs.h"

#include "harness/os.h"
#include "harness/trace.h"

#include <physfs.h>
#include <physfs/extras/ignorecase.h>

#if defined(dethrace_stdio_vfs_aliased)
#error "stdio functions aliased to vfs functions"
#endif

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

#define VFILE_MAGIC 0xbabd3da705220cb
#define VFS_MIN(X, Y) ((X) <= (Y) ? (X) : (Y))

typedef enum {
    VFILE_READ,
    VFILE_WRITE,
    VFILE_APPEND,
} VFILE_type;

typedef struct VFILE {
    uint64_t magic;
    PHYSFS_File *file;
    uint64_t size;
    uint64_t position;
    uint64_t capacity;
    char* buffer;
    VFILE_type type;
} VFILE;

#define VFS_DIRITER_MAGIC 0xca5c2851dad132f8

typedef struct vfs_diriter {
    uint64_t magic;
    char** list;
    size_t index;
} vfs_diriter;

static VFILE* cast_vfile(FILE* file) {
    if (((VFILE*)file)->magic != VFILE_MAGIC) {
        abort();
    }
    return (VFILE*)file;
}

static vfs_diriter * cast_diriter(os_diriter* diriter) {
    if (diriter == NULL) {
        abort();
    }
    if (((VFILE*)diriter)->magic != VFS_DIRITER_MAGIC) {
        abort();
    }
    return (vfs_diriter*)diriter;
}

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
        if (PHYSFS_getWriteDir() == NULL) {
            if (OS_IsDirectory(pathBuffer)) {
                VFS_SetWriteDir(pathBuffer);
                if (PHYSFS_getWriteDir() != NULL) {
                    LOG_INFO("VFS write path: %s", pathBuffer);
                }
            }
        }
    }
#if defined(_WIN32)
    _set_printf_count_output(1);
#endif
    return 0;
}

void VFS_SetWriteDir(const char* path) {
    PHYSFS_mount(path, "/", 1);
    PHYSFS_setWriteDir(path);
}

os_diriter* VFS_OpenDir(char* path) {
    char** list;
    vfs_diriter* diriter;

    list = PHYSFS_enumerateFiles(path);
    if (list == NULL) {
        return NULL;
    }
    diriter = malloc(sizeof(vfs_diriter));
    diriter->magic = VFS_DIRITER_MAGIC;
    diriter->list = list;
    diriter->index = 0;
    return (os_diriter*)diriter;
}

char* VFS_GetNextFileInDirectory(os_diriter* diriter) {
    char* result;
    vfs_diriter* vfs_diriter;

    vfs_diriter = cast_diriter(diriter);
    result = vfs_diriter->list[vfs_diriter->index];
    vfs_diriter->index++;
    if (result == NULL) {
        PHYSFS_freeList(vfs_diriter->list);
        free(vfs_diriter);
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
    vfile->magic = VFILE_MAGIC;
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
    vfile->magic = VFILE_MAGIC;
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

FILE* VFS_fopen(const char* path, const char* mode) {
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
    return (FILE*)vfile;
}

int VFS_fclose(FILE* stream) {
    int result;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    result = 0;
    free(vfile->buffer);
    if (vfile->file != NULL) {
        result = PHYSFS_close(vfile->file) ? 0 : EOF;
    }
    free(stream);
    return result;
}

int VFS_fseek(FILE* stream, long offset, int whence) {
    int result;
    int64_t new_position;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return -1;
    }
    switch (whence) {
    case SEEK_SET:
        new_position = offset;
        break;
    case SEEK_CUR:
        new_position = vfile->position + offset;
        break;
    case SEEK_END:
        new_position = vfile->size;
        break;
    default:
        return -1;
    }
    result = 0;
    if (new_position < 0) {
        new_position = 0;
    } else if ((uint64_t)new_position > vfile->size) {
        new_position = vfile->size + 1;
        result = -1;
    }
    vfile->position = new_position;
    return result;
}

size_t vfs_fprintf_marker_internal;

int VFS_fprintf_internal(FILE* stream, const char* format, ...) {
    va_list ap;
    char hugebuffer[4096];
    int nb;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    va_start(ap, format);
    nb = vsnprintf(hugebuffer, sizeof(hugebuffer), format, ap);
    va_end(ap);

    PHYSFS_writeBytes(vfile->file, hugebuffer, vfs_fprintf_marker_internal);

    return nb;
}

int VFS_vfprintf(FILE *stream, const char *format, va_list ap) {
    char hugebuffer[4096];
    int nb;
    VFILE* vfile;

    vfile = cast_vfile(stream);

    nb = vsnprintf(hugebuffer, sizeof(hugebuffer), format, ap);
    hugebuffer[sizeof(hugebuffer)-1] = '\0';

    PHYSFS_writeBytes(vfile->file, hugebuffer, strlen(hugebuffer));

    return nb;
}

size_t vfs_scanf_marker_internal;

int VFS_fscanf_internal(FILE* stream, const char* format, ...) {
    va_list ap;
    int nb;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return 0;
    }
    if (vfile->position >= vfile->size) {
        return 0;
    }

    va_start(ap, format);
    nb = vsscanf(&vfile->buffer[vfile->position], format, ap);
    va_end(ap);

    vfile->position += vfs_scanf_marker_internal;

    return nb;
}

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t block_i;
    int switcheroo;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return 0;
    }

    switcheroo = 0;
    if (size == 1) {
        size = nmemb;
        nmemb = 1;
        switcheroo = 1;
    }

    for (block_i = 0; block_i < nmemb; block_i++) {
        if (vfile->size - vfile->position < size) {
            vfile->position = vfile->size + 1;
            return block_i;
        }
        memcpy(ptr, &vfile->buffer[vfile->position], size);
        vfile->position += size;
        ptr = ((char*)ptr) + size;
    }
    if (switcheroo) {
        return size;
    }
    return block_i;
}

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t block_i;
    int switcheroo;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type == VFILE_APPEND) {
        NOT_IMPLEMENTED();
    }
    if (vfile->type != VFILE_WRITE) {
        return 0;
    }

    switcheroo = 0;
    if (size == 1) {
        size = nmemb;
        nmemb = 1;
        switcheroo = 1;
    }

    for (block_i = 0; block_i < nmemb; block_i++) {
        PHYSFS_writeBytes(vfile->file, ptr, size);
        ptr = ((char*)ptr) + size;
    }
    if (switcheroo) {
        return size;
    }
    return block_i;
}

int VFS_feof(FILE* stream) {
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return 0;
    }
    return vfile->position > vfile->size;
}

long VFS_ftell(FILE* stream) {
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return -1;
    }
    if (vfile->position >= vfile->size) {
        return (long)vfile->size;
    }
    return (long)vfile->position;
}

void VFS_rewind(FILE* stream) {
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return;
    }
    vfile->position = 0;
}

int VFS_fgetc(FILE* stream) {
    int c;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return EOF;
    }
    if (vfile->position >= vfile->size) {
        vfile->position = vfile->size + 1;
        return EOF;
    }

    c = vfile->buffer[vfile->position];
    vfile->position++;
    return c;
}

uint64_t VFS_filesize(FILE* stream) {
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        NOT_IMPLEMENTED();
    }
    return vfile->size;
}

char* VFS_internal_buffer(FILE* stream) {
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        NOT_IMPLEMENTED();
    }
    return vfile->buffer;
}

int VFS_ungetc(int c, FILE* stream) {
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return EOF;
    }
    if (vfile->position <= 0) {
        return EOF;
    }
    if (vfile->size == 0) {
        return EOF;
    }

    vfile->position--;
    vfile->buffer[vfile->position] = c;
    return c;
}

char* VFS_fgets(char* s, int size, FILE* stream) {
    char* end;
    uint64_t copy_size;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type != VFILE_READ) {
        return NULL;
    }
    if (vfile->position >= vfile->size) {
        vfile->position = vfile->size + 1;
        return NULL;
    }
    if (size <= 0) {
        return NULL;
    }
    end = strpbrk(&vfile->buffer[vfile->position], "\n");
    if (end == NULL) {
        copy_size = vfile->size - vfile->position;
    } else {
        copy_size = end - &vfile->buffer[vfile->position] + 1;
    }
    copy_size = VFS_MIN((size_t)size - 1, copy_size);

    memcpy(s, &vfile->buffer[vfile->position], copy_size);
    s[copy_size] = '\0';
    vfile->position += copy_size;
    return s;
}

int VFS_fputc(int c, FILE* stream) {
    char character;
    PHYSFS_sint64 count;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type == VFILE_APPEND) {
        NOT_IMPLEMENTED();
    }
    if (vfile->type != VFILE_WRITE) {
        return 0;
    }

    character = c;
    count = PHYSFS_writeBytes(vfile->file, &character, 1);

    return (int)count;
}

int VFS_fputs(const char* s, FILE* stream) {
    PHYSFS_sint64 res;
    VFILE* vfile;

    vfile = cast_vfile(stream);
    if (vfile->type == VFILE_APPEND) {
        NOT_IMPLEMENTED();
    }
    if (vfile->type != VFILE_WRITE) {
        return EOF;
    }

    res = PHYSFS_writeBytes(vfile->file, s, strlen(s));
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
