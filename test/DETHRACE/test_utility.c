#include "tests.h"

#include "common/loading.h"
#include "common/utility.h"
#include <string.h>

void test_utility_EncodeLinex(void) {
    char buf[50];
    gEncryption_method = 1;
    // first line of GENERAL.TXT, "@" prefix and line ending stripped
    char input[] = "\x29\x2a\x9c\x22\x61\x4d\x5e\x5f\x60\x34\x64\x57\x8d\x2b\x82\x7b\x33\x4c";
    strcpy(buf, input);
    EncodeLine(buf);
    //TEST_ASSERT_EQUAL_INT(2, gEncryption_method);
    char expected[] = "0.01\t\t\t\t\t// Hither";
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_utility_DecodeLine2(void) {
    char buf[50];
    gEncryption_method = 1;
    // first line of GENERAL.TXT, "@" prefix and line ending stripped
    char input[] = "\x29\x2a\x9c\x22\x61\x4d\x5e\x5f\x60\x34\x64\x57\x8d\x2b\x82\x7b\x33\x4c";
    strcpy(buf, input);
    DecodeLine2(buf);
    char expected[] = "0.01\t\t\t\t\t// Hither";
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_utility_EncodeLine2(void) {
    char buf[50];
    strcpy(buf, "0.01\t\t\t\t\t// Hither");
    EncodeLine2(buf);
    char expected[] = "\x29\x2a\x9c\x22\x61\x4d\x5e\x5f\x60\x34\x64\x57\x8d\x2b\x82\x7b\x33\x4c";
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_utility_StripCR(void) {
    char buf[50];
    strcpy(buf, "new\nline");
    StripCR(buf);
    TEST_ASSERT_EQUAL_STRING("new", buf);
    strcpy(buf, "line");
    StripCR(buf);
    TEST_ASSERT_EQUAL_STRING("line", buf);
}

static void get_system_temp_folder(char *buffer, size_t bufferSize) {
#ifdef _WIN32
    GetTempPathA(bufferSize, buffer);
#else
    strcpy(buffer, "/tmp/");
#endif
}

void test_utility_GetALineWithNoPossibleService(void) {
    char tmpPath[256];
    char s[256];
    VFILE* file;

    create_temp_file(tmpPath, "testfile");
    file = VFS_fopen(tmpPath, "wt");
    TEST_ASSERT_NOT_NULL(file);
    VFS_fprintf(file, "hello world\r\n  space_prefixed\r\n\r\n\ttab_prefixed\r\n$ignored_prefix\r\nlast_line");
    VFS_fclose(file);

    file = VFS_fopen(tmpPath, "rt");
    TEST_ASSERT_NOT_NULL(file);

    char* result = GetALineWithNoPossibleService(file, s);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("hello world", s);

    result = GetALineWithNoPossibleService(file, s);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("space_prefixed", s);

    result = GetALineWithNoPossibleService(file, s);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("tab_prefixed", s);

    result = GetALineWithNoPossibleService(file, s);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_STRING("last_line", s);

    result = GetALineWithNoPossibleService(file, s);
    TEST_ASSERT_NULL(result);

    VFS_fclose(file);
}

void test_utility_PathCat(void) {
    char buf[256];
    PathCat(buf, "a", "b");
    TEST_ASSERT_EQUAL_STRING("a/b", buf);

    PathCat(buf, "a", "");
    TEST_ASSERT_EQUAL_STRING("a", buf);
}

void test_utility_IRandomBetween(void) {
    tU32 source_y_delta;

    source_y_delta = ((66 << 16) / 67) - 0x10000;
    printf("delta %x, %x\n", source_y_delta, source_y_delta >> 16);
}

void test_utility_suite(void) {
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_utility_EncodeLinex);
    RUN_TEST(test_utility_DecodeLine2);
    RUN_TEST(test_utility_EncodeLine2);
    RUN_TEST(test_utility_StripCR);
    RUN_TEST(test_utility_GetALineWithNoPossibleService);
    RUN_TEST(test_utility_PathCat);
    RUN_TEST(test_utility_IRandomBetween);
}
