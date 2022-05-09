#pragma once

#include <coreinit/cache.h>
#include <coreinit/screen.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <malloc.h>
#include <string>
#include <unistd.h>

extern bool isBackup;
extern bool isInstalling;

void flipBuffers();
void clearBuffersEx();
void clearBuffers();
void console_print_pos(int x, int y, const char *format, ...);
void promptError(const char *message, ...);
void warning();
void header();
void displayMessage(std::string message);
bool screenInit();
void screendeInit();