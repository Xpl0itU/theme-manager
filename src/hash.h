#ifndef _HASH_H
#define _HASH_H

#include <dirent.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include <iosuhax.h>
#include <iosuhax_devoptab.h>

extern int fsaFd;

auto copyFile(const std::string &pPath, const std::string &oPath) -> int;
auto hashFiles(const std::string &file1, const std::string &file2) -> int;
#endif
