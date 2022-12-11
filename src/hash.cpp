#include <future>
#include <coreinit/cache.h>
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

static uint16_t getCRC(uint8_t *bytes, int length) {
    uint16_t crc = 0x0000;
    for (int byteIndex = 0; byteIndex < length; ++byteIndex)
        for (int bitIndex = 7; bitIndex >= 0; --bitIndex)
            crc = (((crc << 1) | ((bytes[byteIndex] >> bitIndex) & 0x1)) ^ (((crc & 0x8000) != 0) ? 0x1021 : 0));
    for (int counter = 16; counter > 0; --counter)
        crc = ((crc << 1) ^ (((crc & 0x8000) != 0) ? 0x1021 : 0));

    return (crc & 0xFFFF);
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

auto hashFiles(const std::string &file1, const std::string &file2) -> int {
    uint8_t *file1Buf = nullptr;
    uint8_t *file2Buf = nullptr;
    int32_t file1Size;
    int32_t file2Size;
    if((file1Size = loadFile(file1, &file1Buf)) != -1)
        if((file2Size = loadFile(file2, &file2Buf)) != -1)
            if (getCRC(file1Buf, file1Size) == getCRC(file2Buf, file2Size)) {
                free(file1Buf);
                free(file2Buf);
                return 0;
            }

    free(file1Buf);
    free(file2Buf);
    return 1;
}
