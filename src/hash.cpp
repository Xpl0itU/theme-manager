#include <future>
#include <coreinit/cache.h>
#include <cstring>
#include <mbedtls/md5.h>
#include "hash.h"
#include "fsUtils.h"
#include "LockingQueue.h"

#define IO_MAX_FILE_BUFFER (1024 * 1024) // 1 MB

typedef struct {
    void *buf;
    size_t len;
    size_t buf_size;
} file_buffer;

static file_buffer buffers[16];
static char *fileBuf[2];
static bool buffersInitialized = false;

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

static bool readThread(FILE *srcFile, LockingQueue<file_buffer> *ready, LockingQueue<file_buffer> *done) {
    file_buffer currentBuffer;
    ready->waitAndPop(currentBuffer);
    while ((currentBuffer.len = fread(currentBuffer.buf, 1, currentBuffer.buf_size, srcFile)) > 0) {
        done->push(currentBuffer);
        ready->waitAndPop(currentBuffer);
    }
    done->push(currentBuffer);
    return ferror(srcFile) == 0;
}

static bool writeThread(FILE *dstFile, LockingQueue<file_buffer> *ready, LockingQueue<file_buffer> *done, size_t totalSize) {
    uint bytes_written;
    size_t total_bytes_written = 0;
    file_buffer currentBuffer;
    ready->waitAndPop(currentBuffer);
    while (currentBuffer.len > 0 && (bytes_written = fwrite(currentBuffer.buf, 1, currentBuffer.len, dstFile)) == currentBuffer.len) {
        done->push(currentBuffer);
        ready->waitAndPop(currentBuffer);
        total_bytes_written += bytes_written;
    }
    done->push(currentBuffer);
    return ferror(dstFile) == 0;
}

static bool copyFileThreaded(FILE *srcFile, FILE *dstFile, size_t totalSize) {
    LockingQueue<file_buffer> read;
    LockingQueue<file_buffer> write;
    for (auto &buffer : buffers) {
        if (!buffersInitialized) {
            buffer.buf = aligned_alloc(0x40, IO_MAX_FILE_BUFFER);
            buffer.len = 0;
            buffer.buf_size = IO_MAX_FILE_BUFFER;
        }
        read.push(buffer);
    }
    if (!buffersInitialized) {
        fileBuf[0] = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
        fileBuf[1] = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
    }
    buffersInitialized = true;
    setvbuf(srcFile, fileBuf[0], _IOFBF, IO_MAX_FILE_BUFFER);
    setvbuf(dstFile, fileBuf[1], _IOFBF, IO_MAX_FILE_BUFFER);

    std::future<bool> readFut = std::async(std::launch::async, readThread, srcFile, &read, &write);
    std::future<bool> writeFut = std::async(std::launch::async, writeThread, dstFile, &write, &read, totalSize);
    bool success = readFut.get() && writeFut.get();
    OSMemoryBarrier();
    return success;
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

    struct stat st;
    if (stat(pPath.c_str(), &st) < 0)
        return false;
    int sizef = st.st_size;

    copyFileThreaded(source, dest, sizef);

    FSChangeMode(__wut_devoptab_fs_client, &cmdBlk, (char *) oPath.c_str(), (FSMode) 0x644, (FSMode) 0x777, FS_ERROR_FLAG_ALL);

    fclose(source);
    fclose(dest);

    return true;
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