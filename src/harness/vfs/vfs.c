#include "harness/vfs.h"

#include "harness/trace.h"

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
#endif

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

#define VFS_MIN(X, Y) ((X) <= (Y) ? (X) : (Y))

int VFS_Init(int argc, const char* argv[], const char* paths) {
    int result;

    result = PHYSFS_init(argv[0]);
    if (result == 0) {
        PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
        LOG_WARN("PHYSFS_init failed with %d (%s)", ec, PHYSFS_getErrorByCode(ec));
        return 1;
    }
    if (paths == NULL) {
        LOG_INFO("DETHRACE_ROOT_DIR is not set, assuming '/'");
        paths = "/";
    }
    const char* currentPath = paths;
    char pathBuffer[260];
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
        result = PHYSFS_mount(pathBuffer, NULL, 1);
        if (result == 0) {
            PHYSFS_ErrorCode ec = PHYSFS_getLastErrorCode();
            LOG_WARN("PHYSFS_mount(\"%s\", NULL, 1) failed with %d (%s)", pathBuffer, ec, PHYSFS_getErrorByCode(ec));
            continue;
        }
        LOG_INFO("VFS search path: %s", pathBuffer);
    }
    return 0;
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

    result = diriter->list[diriter->index];
    diriter->index++;
    if (result == NULL) {
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

typedef PHYSFS_EnumerateCallbackResult (*PHYSFS_EnumerateCallback)(void *data,
    const char *origdir, const char *fname);
PHYSFS_DECL int PHYSFS_enumerate(const char *dir, PHYSFS_EnumerateCallback c,
    void *d);

typedef struct {
    const char* ref_filename;
    int found;
    char path[512];
} vfs_physfs_case_insensitive_data;

static int case_insensitive_search_callback(void* data, const char* origdir, const char* fname) {
    vfs_physfs_case_insensitive_data* ci_data = data;
    if (strcasecmp(fname, ci_data->ref_filename) == 0) {
        strcpy(ci_data->path, origdir);
        strcat(ci_data->path, PHYSFS_getDirSeparator());
        strcat(ci_data->path, fname);
        ci_data->found = 1;
        return PHYSFS_ENUM_STOP;
    }
    return PHYSFS_ENUM_OK;
}

static VFILE* case_insensitive_open(const char* path, VFILE* (*callback)(const char*)) {
    VFILE* vfile;
    vfs_physfs_case_insensitive_data data;
    char dir[256];
    const char* filename;
    const char* filename2;

    // First, let's try the 'easy' way
    vfile = callback(path);
    if (vfile != NULL) {
        return vfile;
    }

    // Try the hard way by iterating the directory
//    strcpy(dir, "/");
    filename = path;
    while (1) {
        filename2 = strpbrk(filename, "/\\");
        if (filename2 == NULL) {
            break;
        }
        filename = filename2 + 1;
    }
    strncpy(dir, path, filename - path);
    dir[filename - path] = '\0';
    data.ref_filename = filename;
    data.found = 0;
    PHYSFS_enumerate(dir, case_insensitive_search_callback, &data);
    if (data.found) {
        return callback(data.path);
    }
    return NULL;
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
    vfile->capacity = 512;
    vfile->position = 0;
    vfile->type = VFILE_WRITE;
    vfile->buffer = malloc(vfile->capacity);
    return vfile;
}

static VFILE* vfile_openAppend(const char* path) {
    NOT_IMPLEMENTED();
}

VFILE* VFS_fopen(const char* path, const char* mode) {
    VFILE* vfile;

    if (strchr(mode, 'r') != NULL) {
        vfile = case_insensitive_open(path, vfile_openRead);
    }
    else if (strchr(mode, 'w') != NULL) {
        vfile = case_insensitive_open(path, vfile_openWrite);
    }
    else if (strchr(mode, 'a') != NULL) {
        vfile = case_insensitive_open(path, vfile_openAppend);
    }
    if (vfile == NULL) {
//        LOG_WARN("Failed to open %s", path);
    }
    return vfile;
}

int VFS_fclose(VFILE* stream) {
    int result;

    free(stream->buffer);
    if (stream->file != NULL) {
        result = PHYSFS_close(stream->file);
    }
    free(stream);
    return result != 0 ? 0 : EOF;
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
        new_position = stream->position;
        result = -1;
    }
    stream->position = new_position;
    return result;
}

int VFS_fprintf(VFILE* stream, const char* format, ...) {
    NOT_IMPLEMENTED();
}

int VFS_vfprintf(VFILE *stream, const char *format, va_list ap) {
    NOT_IMPLEMENTED();
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
        size = VFS_MIN(size, stream->size - stream->position);
        switcheroo = 1;
    }

    for (block_i = 0; block_i < nmemb; block_i++) {
        if (stream->size - stream->position < size) {
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
    NOT_IMPLEMENTED();
}

int VFS_feof(VFILE* stream) {

    if (stream->type != VFILE_READ) {
        return 0;
    }
    return stream->position >= stream->size;
}

long VFS_ftell(VFILE* stream) {

    if (stream->type != VFILE_READ) {
        return -1;
    }
    return stream->position;
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
    NOT_IMPLEMENTED();
}

int VFS_fputs(const char* s, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_remove(const char* pathname) {
    NOT_IMPLEMENTED();
}

int VFS_rename(const char* oldpath, const char* newpath) {
    NOT_IMPLEMENTED();
}
