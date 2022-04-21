#include <dirent.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>

#include <iosuhax.h>
#include <iosuhax_devoptab.h>

extern int fsaFd;

int copyFile(std::string pPath, std::string oPath);
int32_t hashFiles(std::string file1, std::string file2);