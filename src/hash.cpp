#include "hash.h"

#define IO_MAX_FILE_BUFFER (1024 * 1024) // 1 MB

bool replace(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

static std::string newlibToFSA(std::string path) {
    if (path[3] == ':') {
        switch (path[0]) {
            case 'm':
                replace(path, "mlc:", "/vol/storage_mlc01");
                break;
        }
    }
    return path;
}

static uint16_t getCRC(uint8_t *bytes, int length) {
    uint16_t crc = 0x0000;
    for (int byteIndex = 0; byteIndex < length; ++byteIndex)
        for (int bitIndex = 7; bitIndex >= 0; --bitIndex)
            crc = (((crc << 1) | ((bytes[byteIndex] >> bitIndex) & 0x1)) ^ (((crc & 0x8000) != 0) ? 0x1021 : 0));
    for (int counter = 16; counter > 0; --counter)
        crc = ((crc << 1) ^ (((crc & 0x8000) != 0) ? 0x1021 : 0));

    return (crc & 0xFFFF);
}

auto copyFile(const std::string &pPath, const std::string &oPath) -> int {
    FILE *source = fopen(pPath.c_str(), "rb");
    if (source == nullptr)
        return -1;

    FILE *dest = fopen(oPath.c_str(), "wb");
    if (dest == nullptr) {
        fclose(source);
        return -1;
    }

    char *buffer[3];
    for (int i = 0; i < 3; ++i) {
        buffer[i] = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
        if (buffer[i] == nullptr) {
            fclose(source);
            fclose(dest);
            for (--i; i >= 0; --i)
                free(buffer[i]);

            return -1;
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

    IOSUHAX_FSA_ChangeMode(fsaFd, newlibToFSA(oPath).c_str(), 0x644);

    return 0;
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
            else
                free(*buf);
        }
        fclose(file);
    }
    return ret;
}

auto hashFiles(const std::string &file1, const std::string &file2) -> int {
    uint8_t *file1Buf = nullptr;
    uint8_t *file2Buf = nullptr;
    int32_t file1Size = loadFile(file1, &file1Buf);
    int32_t file2Size = loadFile(file2, &file2Buf);
    if (getCRC(file1Buf, file1Size) == getCRC(file2Buf, file2Size)) {
        free(file1Buf);
        free(file2Buf);
        return 0;
    }

    free(file1Buf);
    free(file2Buf);
    return 1;
}
