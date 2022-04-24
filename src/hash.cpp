#include "hash.h"

#define IO_MAX_FILE_BUFFER (1024 * 1024) // 1 MB

auto replace(std::string &str, const std::string &from, const std::string &to) -> bool {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

static auto newlibToFSA(std::string path) -> std::string {
    if (path[3] == ':') {
        switch (path[0]) {
            case 'm':
                replace(path, "mlc:", "/vol/storage_mlc01");
                break;
        }
    }
    return path;
}

auto copyFile(const std::string &pPath, const std::string &oPath) -> int {
    int srcFd = -1, destFd = -1;
    int buf_size = IO_MAX_FILE_BUFFER;
    
    int source = IOSUHAX_FSA_OpenFile(fsaFd, pPath.c_str(), "rb", &srcFd);
   

    int dest = IOSUHAX_FSA_OpenFile(fsaFd, oPath.c_str(), "wb", &destFd);
   

    char *buffer;
 
        buffer = static_cast<char *>(aligned_alloc(0x40, IO_MAX_FILE_BUFFER));
        if (buffer == nullptr) {
            IOSUHAX_FSA_CloseFile(fsaFd, destFd);
	    IOSUHAX_FSA_CloseFile(fsaFd, srcFd);
            free(buffer);

            return -1;
        }
    

 
    int size = 0;

    while ((size = IOSUHAX_FSA_ReadFile(fsaFd, buffer, 0x01, buf_size, srcFd, 0)) > 0)
        IOSUHAX_FSA_WriteFile(fsaFd, buffer, 0x01, size, destFd, 0);
    
    IOSUHAX_FSA_CloseFile(fsaFd, destFd);
    IOSUHAX_FSA_CloseFile(fsaFd, srcFd);
    free(buffer);

    IOSUHAX_FSA_ChangeMode(fsaFd, newlibToFSA(oPath).c_str(), 0x644);

    return 0;
}

auto loadFile(const char *fPath, uint8_t **buf) -> int32_t {
    int ret = 0;
    FILE *file = fopen(fPath, "rb");
    if (file != nullptr) {
        struct stat st {};
        stat(fPath, &st);
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
    auto *file1Buf = new uint8_t;
    auto *file2Buf = new uint8_t;
    loadFile(file1.c_str(), &file1Buf);
    loadFile(file2.c_str(), &file2Buf);
    std::hash<uint8_t> ptr_hash;
    if (ptr_hash(*file1Buf) == ptr_hash(*file2Buf)) {
        free(file1Buf);
        free(file2Buf);
        return 0;
    }

    delete file1Buf;
    delete file2Buf;
    return 1;
}
