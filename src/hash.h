#pragma once

#include <dirent.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>

extern int fsaFd;

bool copyFile(const std::string &pPath, const std::string &oPath);
auto hashFiles(const std::string &file1, const std::string &file2) -> int;