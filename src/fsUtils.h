#pragma once

#include <coreinit/filesystem_fsa.h>
#include <vector>

#include "hash.h"

extern int entrycount;
extern FSAClientHandle fsClient;

auto dirExists(const std::string &dir) -> bool;
std::vector<std::string> listFolders(const std::string &path);
auto checkEntry(std::string fPath) -> int;
auto mkdir_p(const std::string &fPath) -> int;
