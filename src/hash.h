#pragma once

#include <dirent.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>

extern int fsaFd;

auto copyFile(const std::string &pPath, const std::string &oPath) -> int;
auto hashFiles(const std::string &file1, const std::string &file2) -> int;