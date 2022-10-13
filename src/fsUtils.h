#pragma once

#include <coreinit/filesystem.h>
#include <vector>

#include "hash.h"

extern int entrycount;
extern FSClient *__wut_devoptab_fs_client;
extern FSCmdBlock cmdBlk;

auto dirExists(const std::string &dir) -> bool;
std::vector<std::string> listFolders(const std::string &path);
auto checkEntry(std::string fPath) -> int;
auto mkdir_p(const std::string &fPath) -> int;
