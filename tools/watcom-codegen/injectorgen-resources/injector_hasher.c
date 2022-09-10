#include <windows.h>
#include <bcrypt.h>

#include <stdio.h>

#define NT_SUCCESS(Status)          (((NTSTATUS)(Status)) >= 0)

#define STATUS_UNSUCCESSFUL         ((NTSTATUS)0xC0000001L)


char* hook_calculate_sha256(const char *path) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    DWORD cbData = 0;
    DWORD cbHash = 0;
    DWORD cbHashObject = 0;
    PBYTE pbHashObject = NULL;
    PBYTE pbHash = NULL;
    int success = 0;
    char *readable_hash = NULL;
    BYTE buffer[512];
    FILE *f;
    size_t count;
    int i;

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Could not open \"%s\".\n", path);
    }

    // open an algorithm handle
    if (!NT_SUCCESS(status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0))) {
        fprintf(stderr, "**** Error 0x%lx returned by BCryptOpenAlgorithmProvider\n", status);
        goto cleanup;
    }

    // calculate the size of the buffer to hold the hash object
    if (!NT_SUCCESS(status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(cbHashObject), &cbData, 0))) {
        fprintf(stderr, "**** Error 0x%lx returned by BCryptGetProperty\n", status);
        goto cleanup;
    }

    // allocate the hash object on the heap
    pbHashObject = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHashObject);
    if (pbHashObject == NULL) {
        fprintf(stderr, "**** memory allocation failed\n");
        goto cleanup;
    }

    // calculate the length of the hash
    if (!NT_SUCCESS(status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(cbHash), &cbData, 0))) {
        fprintf(stderr, "**** Error 0x%lx returned by BCryptGetProperty\n", status);
        goto cleanup;
    }

    // allocate the hash buffer on the heap
    pbHash = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbHash);
    if (pbHash == NULL) {
        fprintf(stderr, "**** memory allocation failed\n");
        goto cleanup;
    }

    // create a hash
    if (!NT_SUCCESS(status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0))) {
        fprintf(stderr, "**** Error 0x%lx returned by BCryptCreateHash\n", status);
        goto cleanup;
    }

    // hash some data
    while (!feof(f)) {
        count = fread(buffer, 1, sizeof(buffer), f);
        if (!NT_SUCCESS(status = BCryptHashData(hHash, (PBYTE)buffer, count, 0))) {
            fprintf(stderr, "**** Error 0x%lx returned by BCryptHashData\n", status);
            goto cleanup;
        }

    }

    // allocate readable hash
    readable_hash = malloc((2 * cbHash + 1) * sizeof(char));
    if (readable_hash == NULL) {
        fprintf(stderr, "**** memory allocation for readable hash failed\n");
        goto cleanup;
    }

    // close the hash
    if (!NT_SUCCESS(status = BCryptFinishHash(hHash, pbHash, cbHash, 0))) {
        fprintf(stderr, "**** Error 0x%lx returned by BCryptFinishHash\n", status);
        goto cleanup;
    }
    success = 1;

    for (i = 0; i < cbHash; i++) {
        sprintf(&readable_hash[2*i], "%02x", pbHash[i]);
    }

cleanup:
    if (hAlg != NULL) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    if (hHash!= NULL) {
        BCryptDestroyHash(hHash);
    }

    if (pbHashObject != NULL) {
        HeapFree(GetProcessHeap(), 0, pbHashObject);
    }

    if (pbHash != NULL) {
        HeapFree(GetProcessHeap(), 0, pbHash);
    }

    if (f != NULL) {
        fclose(f);
    }

    if (!success) {
        free(readable_hash);
        readable_hash = NULL;
    }
    return readable_hash;
}
