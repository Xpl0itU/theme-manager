#include "fsUtils.h"

int entrycount = 0;

auto dirExists(const std::string &dir) -> bool {
    DIR *d = opendir(dir.c_str());
    if (d != nullptr) {
        closedir(d);
        return true;
    }
    return false;
}

std::vector<std::string> listFolders(const std::string &path) {
    std::vector<std::string> folders;
    DIR *dir_ = opendir(path.c_str());
    struct dirent *ent = nullptr;
    while ((ent = readdir(dir_)) != nullptr) {
        if (ent->d_type == DT_DIR) { // folder
            folders.push_back(ent->d_name);
            ++entrycount;
        }
    }

    return folders;
}

auto checkEntry(std::string fPath) -> int {
    struct stat st {};
    if (stat(fPath.c_str(), &st) == -1)
        return 0;

    if (S_ISDIR(st.st_mode))
        return 2;

    return 1;
}

auto mkdir_p(const std::string &fPath) -> int { // Adapted from mkdir_p made by JonathonReinhart
    std::string _path;
    char *p;
    int found = 0;

    _path = fPath;

    for (p = (char *) _path.c_str() + 1; *p != 0; ++p) {
        if (*p == '/') {
            ++found;
            if (found > 2) {
                *p = '\0';
                if (checkEntry(_path) == 0)
                    if (mkdir(_path.c_str(), DEFFILEMODE) == -1)
                        return -1;
                *p = '/';
            }
        }
    }

    if (checkEntry(_path) == 0)
        if (mkdir(_path.c_str(), DEFFILEMODE) == -1)
            return -1;

    return 0;
}