#pragma once

#include <vector>

#include "hash.h"

extern int entrycount;

auto dirExists(const std::string &dir) -> bool;
std::vector<std::string> listFolders(const std::string &path);
auto checkEntry(std::string fPath) -> int;
auto mkdir_p(const std::string &fPath) -> int;
