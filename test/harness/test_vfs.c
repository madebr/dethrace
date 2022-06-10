#include "tests.h"

#include <string.h>

#include "common/globvars.h"
#include "pd/sys.h"

#include "harness/vfs.h"

static void test_vfs_getc_eof(void) {
    char buffer[PATH_MAX + 1];
    FILE* f;
    int nb;
    int c;

    create_temp_file(buffer, "test_read");

    f = VFS_fopen(buffer, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(VFS_feof(f));

    nb = VFS_fprintf(f, "abcdef");
    TEST_ASSERT_EQUAL_INT(6, nb);
    TEST_ASSERT_FALSE(VFS_feof(f));

    VFS_fclose(f);

    f = VFS_fopen(buffer, "rb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT('a', c);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT('b', c);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT('c', c);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT('d', c);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT('e', c);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT('f', c);
    TEST_ASSERT_FALSE(VFS_feof(f));

    c = VFS_fgetc(f);
    TEST_ASSERT_EQUAL_INT(EOF, c);
    TEST_ASSERT_TRUE(VFS_feof(f));

    VFS_fclose(f);
}

static void test_vfs_gets_eof(void) {
    char buffer[PATH_MAX + 1];
    char sbuf[32];
    FILE* f;
    int nb;
    char* s;

    create_temp_file(buffer, "test_read");

    f = VFS_fopen(buffer, "wb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(VFS_feof(f));

    nb = VFS_fprintf(f, "abc\ndef\nghi\njkl");
    TEST_ASSERT_EQUAL_INT(15, nb);
    TEST_ASSERT_FALSE(VFS_feof(f));

    VFS_fclose(f);

    f = VFS_fopen(buffer, "rb");
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_FALSE(VFS_feof(f));

    s = VFS_fgets(sbuf, sizeof(sbuf), f);
    TEST_ASSERT_EQUAL_STRING("abc\n", s);
    TEST_ASSERT_FALSE(VFS_feof(f));

    s = VFS_fgets(sbuf, sizeof(sbuf), f);
    TEST_ASSERT_EQUAL_STRING("def\n", s);
    TEST_ASSERT_FALSE(VFS_feof(f));

    s = VFS_fgets(sbuf, sizeof(sbuf), f);
    TEST_ASSERT_EQUAL_STRING("ghi\n", s);
    TEST_ASSERT_FALSE(VFS_feof(f));

    s = VFS_fgets(sbuf, sizeof(sbuf), f);
    TEST_ASSERT_EQUAL_STRING("jkl", s);
    TEST_ASSERT_FALSE(VFS_feof(f));

    s = VFS_fgets(sbuf, sizeof(sbuf), f);
    TEST_ASSERT_NULL(s);
    TEST_ASSERT_TRUE(VFS_feof(f));

    VFS_fclose(f);
}

void test_vfs_suite() {
    UnitySetTestFile(__FILE__);

    RUN_TEST(test_vfs_getc_eof);
    RUN_TEST(test_vfs_gets_eof);
}
