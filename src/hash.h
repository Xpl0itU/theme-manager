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

int copyFile(const std::string &pPath, const std::string &oPath);
int32_t hashFiles(const std::string &file1, const std::string &file2);
#endif
