#include "brstdfile.h"

#include "CORE/FW/diag.h"

#include "harness/hooks.h"
#include "harness/trace.h"
#include "harness/vfs.h"

#include <stdio.h>
#include <string.h>

// Global variables
br_filesystem BrStdioFilesystem = {
    "Standard IO",
    BrStdioAttributes,
    BrStdioOpenRead,
    BrStdioOpenWrite,
    BrStdioClose,
    BrStdioEof,
    BrStdioGetChar,
    BrStdioPutChar,
    BrStdioRead,
    BrStdioWrite,
    BrStdioGetLine,
    BrStdioPutLine,
    BrStdioAdvance
};
br_filesystem* _BrDefaultFilesystem = &BrStdioFilesystem;

br_uint_32 BrStdioAttributes() {
    return BR_FS_ATTR_READABLE | BR_FS_ATTR_WRITEABLE | BR_FS_ATTR_HAS_TEXT | BR_FS_ATTR_HAS_BINARY | BR_FS_ATTR_HAS_ADVANCE;
}

void* BrStdioOpenRead(char* name, br_size_t n_magics, br_mode_test_cbfn* identify, int* mode_result) {
    VFILE* fh;
    char* br_path;
    char config_path[512];
    char try_name[512];
    char* cp;
    br_uint_8 magics[16];
    int open_mode;
    LOG_TRACE("(\"%s\", %d, %p, %p)", name, n_magics, identify, mode_result);

    open_mode = BR_FS_MODE_BINARY;
    fh = VFS_fopen(name, "rb");
    if (fh == NULL) {
        // skip logic around getting BRENDER_PATH from ini files etc
        return NULL;
    }

    if (n_magics != 0) {
        if (VFS_fread(magics, 1u, n_magics, fh) != n_magics) {
            VFS_fclose(fh);
            return 0;
        }
        if (identify != NULL) {
            open_mode = identify(magics, n_magics);
        }
        if (mode_result != NULL) {
            *mode_result = open_mode;
        }
    }

    VFS_fclose(fh);
    if (open_mode == BR_FS_MODE_BINARY) {
        fh = VFS_fopen(name, "rb");
    } else if (open_mode == BR_FS_MODE_TEXT) {
        fh = VFS_fopen(name, "rt");
    } else {
        BrFailure("BrStdFileOpenRead: invalid open_mode %d", open_mode);
        return NULL;
    }
    return fh;
}

void* BrStdioOpenWrite(char* name, int mode) {
    VFILE* fh;

    if (mode == BR_FS_MODE_TEXT) {
        fh = VFS_fopen(name, "w");
    } else {
        fh = VFS_fopen(name, "wb");
    }

    return fh;
}

void BrStdioClose(void* f) {
    LOG_TRACE("(%p)", f);
    
    VFS_fclose(f);
}

int BrStdioEof(void* f) {
    return VFS_feof(f);
}

int BrStdioGetChar(void* f) {
    int c;
    c = VFS_fgetc(f);
    //LOG_DEBUG("file pos: %d, char: %d", VFS_ftell(f) - 1, c);
    return c;
}

void BrStdioPutChar(int c, void* f) {
    VFS_fputc(c, f);
}

br_size_t BrStdioRead(void* buf, br_size_t size, unsigned int n, void* f) {
    int i;

    LOG_TRACE9("(%p, %d, %d, %p)", buf, size, n, f);
    i = VFS_fread(buf, size, n, f);
    return i;
}

br_size_t BrStdioWrite(void* buf, br_size_t size, unsigned int n, void* f) {
    return VFS_fwrite(buf, size, n, f);
}

br_size_t BrStdioGetLine(char* buf, br_size_t buf_len, void* f) {
    br_size_t l;

    LOG_TRACE9("(%p, %d, %p)", buf, buf_len, f);
    if (VFS_fgets(buf, buf_len, f) == NULL) {
        return 0;
    }

    l = strlen(buf);

    if (l != 0 && buf[l - 1] == '\n') {
        buf[--l] = '\0';
    }

    return l;
}

void BrStdioPutLine(char* buf, void* f) {
    VFS_fputs(buf, f);
    VFS_fputc('\n', f);
}

void BrStdioAdvance(br_size_t count, void* f) {
    VFS_fseek(f, count, SEEK_CUR);
}
