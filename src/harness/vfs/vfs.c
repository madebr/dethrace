#include "harness/vfs.h"

#include "harness/trace.h"

#include <physfs.h>
#include <unistd.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct VFILE {
    PHYSFS_File *file;
    char ungetc_char;
    char ungetc_valid;
} VFILE;

typedef struct vfs_diriter {
    char** list;
    size_t index;
} vfs_diriter;

static VFILE* create_VFILE(PHYSFS_File* file) {
    if (file == NULL) {
        return NULL;
    }
    VFILE* vfile = malloc(sizeof(VFILE));
    vfile->file = file;
    vfile->ungetc_valid = 0;
    return vfile;
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
            LOG_WARN("PHYSFS_mount(\"%s\", NULL, 1) failed with %d (%s)", ec, PHYSFS_getErrorByCode(ec));
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

static PHYSFS_File* case_insensitive_open(const char* path, PHYSFS_File* (*callback)(const char*)) {
    PHYSFS_File* file;
    vfs_physfs_case_insensitive_data data;
    char dir[256];
    const char* filename;
    const char* filename2;

    // First, let's try the 'easy' way
    file = callback(path);
    if (file != NULL) {
        return file;
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


VFILE* VFS_fopen(const char* path, const char* mode) {
    PHYSFS_File* file;

    file = NULL;
    if (strchr(mode, 'r') != NULL) {
        file = case_insensitive_open(path, PHYSFS_openRead);
    }
    if (strchr(mode, 'w') != NULL) {
        file = case_insensitive_open(path, PHYSFS_openWrite);
    }
    if (strchr(mode, 'a') != NULL) {
        file = case_insensitive_open(path, PHYSFS_openAppend);
    }
    if (file == NULL) {
//        LOG_WARN("Failed to open %s", path);
    }
    return create_VFILE(file);
}

int VFS_fclose(VFILE* stream) {
    int result;
    result = PHYSFS_close(stream->file);
    free(stream);
    return result != 0 ? 0 : EOF;
}

int VFS_fseek(VFILE* stream, long offset, int whence) {
    int result;
    switch (whence) {
    case SEEK_SET:
        result = PHYSFS_seek(stream->file, offset);
        break;
    case SEEK_CUR:
        result = PHYSFS_seek(stream->file, PHYSFS_tell(stream->file) + offset);
        break;
    case SEEK_END:
        result = PHYSFS_seek(stream->file, PHYSFS_fileLength(stream->file) + offset);
        break;
    default:
        return -1;
    }
    return result != 0 ? 0 : -1;
}

int VFS_fprintf(VFILE* stream, const char* format, ...) {
    NOT_IMPLEMENTED();
}

int VFS_vfprintf(VFILE *stream, const char *format, va_list ap) {
    NOT_IMPLEMENTED();
}

size_t vfs_scanf_marker_internal;

int VFS_fscanf_internal(VFILE* stream, const char* format, ...) {
    PHYSFS_sint64 location;
    char buf[256];
    va_list ap;
    int nb;

    location = PHYSFS_tell(stream->file);
    PHYSFS_readBytes(stream->file, buf, sizeof(buf));
    va_start(ap, format);
    nb = vsscanf(buf, format, ap);
    va_end(ap);
    PHYSFS_seek(stream->file, location + vfs_scanf_marker_internal);

    return nb;
}

size_t VFS_fread(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    size_t nb_items;
    PHYSFS_sint64 location;
    PHYSFS_sint64 actual;

    if (size == 0 || nmemb == 0) {
        return 0;
    }

    nb_items = 0;
    location = PHYSFS_tell(stream->file);

    if (stream->ungetc_valid) {
        ((char*)ptr)[0] = stream->ungetc_char;
        actual = PHYSFS_readBytes(stream->file, &((char*)ptr)[1], size - 1);
        if ((size_t)actual != size - 1) {
            PHYSFS_seek(stream->file, location);
            return nb_items;
        }
        ptr = &((char*)ptr)[size - 1];
        location += size - 1;
        nb_items += 1;
        stream->ungetc_valid = 0;
        nmemb--;
    }

    while (nmemb > 0) {
        actual = PHYSFS_readBytes(stream->file, (char*)ptr, size);
        if ((size_t)actual != size) {
            PHYSFS_seek(stream->file, location);
            return nb_items;
        }
        location += size;
        nb_items += 1;
        nmemb--;
        ptr = &((char*)ptr)[size];
    }
    return nb_items;
}

size_t VFS_fwrite(void* ptr, size_t size, size_t nmemb, VFILE* stream) {
    NOT_IMPLEMENTED();
}

int VFS_feof(VFILE* stream) {

    return PHYSFS_eof(stream->file);
}

long VFS_ftell(VFILE* stream) {

    return PHYSFS_tell(stream->file);
}

void VFS_rewind(VFILE* stream) {

    PHYSFS_seek(stream->file, 0);
}

int VFS_fgetc(VFILE* stream) {
    char c;
    PHYSFS_sint64 result;

    if (stream->ungetc_valid) {
        stream->ungetc_valid = 0;
        return stream->ungetc_char;
    }
    result = PHYSFS_readBytes(stream->file, &c, 1);
    if (result == 1) {
        return c;
    }
    return EOF;
}

int VFS_ungetc(int c, VFILE* stream) {

    if (stream->ungetc_valid) {
        return EOF;
    }
    stream->ungetc_char = c;
    stream->ungetc_valid = 1;
    return c;
}

#define MIN(X, Y)  (((X) <= (Y)) ? (X) : (Y))

char* VFS_fgets(char* s, int size, VFILE* stream) {
    PHYSFS_uint64 count;
    PHYSFS_uint64 left;
    PHYSFS_sint64 location;
    PHYSFS_sint64 actual;
    char c;

    if (size <= 0) {
        return NULL;
    }

    count = 0;
    left = size - 1;

    if (stream->ungetc_valid) {
        s[count] = stream->ungetc_char;
        stream->ungetc_valid = 0;
        count += 1;
        left -= 1;
    }
    location = PHYSFS_tell(stream->file);

    while (left > 0) {
        actual = PHYSFS_readBytes(stream->file, &c, 1);
        if (actual <= 0) {
            break;
        }
        if (c == '\0') {
            actual = 0;
            left = 0;
            break;
        } else if (c == '\n') {
            actual = 1;
            left = 0;
            break;
        }
        s[count] = c;
        count += actual;
        location += actual;
    }
    PHYSFS_seek(stream->file, location);
    if (count <= 0) {
        return NULL;
    }
    s[count] = '\0';
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
