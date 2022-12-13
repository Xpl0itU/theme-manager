#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <cstring>
#include <mbedtls/md5.h>
#include "hash.h"
#include "fsUtils.h"
#include "LockingQueue.h"

#define IO_MAX_FILE_BUFFER (1024 * 1024) // 1 MB

static char *hash_file_md5(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    unsigned char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        mbedtls_md5_update(&ctx, buf, n);
    }
    unsigned char output[16];
    mbedtls_md5_finish(&ctx, output);
    mbedtls_md5_free(&ctx);
    fclose(fp);
    char *result = (char *)malloc(33);
    for (int i = 0; i < 16; i++) {
        sprintf(result + i * 2, "%02x", output[i]);
    }
    result[32] = '\0';
    return result;
}

bool copyFile(const std::string &pPath, const std::string &oPath) {
    FILE *source = fopen(pPath.c_str(), "rb");
    if (source == nullptr)
        return false;

    FILE *dest = fopen(oPath.c_str(), "wb");
    if (dest == nullptr) {
        fclose(source);
        return false;
    }

    char *buffer[3];
    for (int i = 0; i < 3; ++i) {
        buffer[i] = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
        if (buffer[i] == nullptr) {
            fclose(source);
            fclose(dest);
            for (--i; i >= 0; --i)
                free(buffer[i]);

            return false;
        }
    }

    setvbuf(source, buffer[0], _IOFBF, IO_MAX_FILE_BUFFER);
    setvbuf(dest, buffer[1], _IOFBF, IO_MAX_FILE_BUFFER);

    int size = 0;

    while ((size = fread(buffer[2], 1, IO_MAX_FILE_BUFFER, source)) > 0)
        fwrite(buffer[2], 1, size, dest);

    fclose(source);
    fclose(dest);
    for (auto &i : buffer)
        free(i);

    FSChangeMode(__wut_devoptab_fs_client, &cmdBlk, (char *) oPath.c_str(), (FSMode) 0x644, (FSMode) 0x777, FS_ERROR_FLAG_ALL);

    return true;
}

static auto loadFile(const std::string &fPath, uint8_t **buf) -> int32_t {
    int ret = 0;
    FILE *file = fopen(fPath.c_str(), "rb");
    if (file != nullptr) {
        struct stat st {};
        stat(fPath.c_str(), &st);
        int size = st.st_size;

        *buf = static_cast<uint8_t *>(malloc(size));
        if (*buf != nullptr) {
            if (fread(*buf, size, 1, file) == 1)
                ret = size;
            else {
                free(*buf);
                ret = -1;
            }
        } else
            ret = 1;
        fclose(file);
        OSMemoryBarrier();
    }
    return ret;
}

bool hashFiles(const std::string &file1, const std::string &file2) {
    char *file1Hash = hash_file_md5(file1.c_str());
    char *file2Hash = hash_file_md5(file2.c_str());
    if (strcmp(file1Hash, file2Hash) == 0) {
        free(file1Hash);
        free(file2Hash);
        return true;
    }
    free(file1Hash);
    free(file2Hash);
    return false;
}
