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
            case 'u':
                replace(path, "usb:", "/vol/storage_usb01");
                break;
            case 'm':
                replace(path, "mlc:", "/vol/storage_mlc01");
                break;
            case 's':
                replace(path, "slc:", "/vol/storage_slccmpt01");
                break;
        }
    }
    return path;
}

int copyFile(std::string pPath, std::string oPath) {
    FILE *source = fopen(pPath.c_str(), "rb");
    if (source == nullptr)
        return -1;

    FILE *dest = fopen(oPath.c_str(), "wb");
    if (dest == nullptr) {
        fclose(source);
        return -1;
    }

    char *buffer[3];
    for (int i = 0; i < 3; i++) {
        buffer[i] = (char *) aligned_alloc(0x40, IO_MAX_FILE_BUFFER);
        if (buffer[i] == nullptr) {
            fclose(source);
            fclose(dest);
            for (i--; i >= 0; i--)
                free(buffer[i]);

            return -1;
        }
    }

    setvbuf(source, buffer[0], _IOFBF, IO_MAX_FILE_BUFFER);
    setvbuf(dest, buffer[1], _IOFBF, IO_MAX_FILE_BUFFER);
    int size = 0;

    while ((size = fread(buffer[2], 1, IO_MAX_FILE_BUFFER, source)) > 0) {
         fwrite(buffer[2], 1, size, dest);
    }
    fclose(source);
    fclose(dest);
    for (int i = 0; i < 3; i++)
        free(buffer[i]);

    IOSUHAX_FSA_ChangeMode(fsaFd, newlibToFSA(oPath).c_str(), 0x644);

    return 0;
}

int32_t loadFile(const char *fPath, uint8_t **buf) {
    int ret = 0;
    FILE *file = fopen(fPath, "rb");
    if (file != nullptr) {
        struct stat st;
        stat(fPath, &st);
        int size = st.st_size;

        *buf = (uint8_t *) malloc(size);
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

int32_t hashFiles(std::string file1, std::string file2) {
    uint8_t *file1Buf;
    uint8_t *file2Buf;
    loadFile(file1.c_str(), &file1Buf);
    loadFile(file2.c_str(), &file2Buf);
    std::hash<uint8_t> ptr_hash;
    if(ptr_hash(*file1Buf) == ptr_hash(*file2Buf)) {
        free(file1Buf);
        free(file2Buf);
        return 0;
    }

    free(file1Buf);
    free(file2Buf);
    return 1;
}
