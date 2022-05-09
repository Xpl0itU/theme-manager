#include "screen.h"

static size_t tvBufferSize;
static size_t drcBufferSize;
static void *tvBuffer;
static void *drcBuffer;

void flipBuffers() {
    DCFlushRange(tvBuffer, tvBufferSize);
    DCFlushRange(drcBuffer, drcBufferSize);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

void clearBuffersEx() {
    OSScreenClearBufferEx(SCREEN_TV, 0x00000000);
    OSScreenClearBufferEx(SCREEN_DRC, 0x00000000);
}

void clearBuffers() {
    clearBuffersEx();
    flipBuffers();
}

void console_print_pos(int x, int y, const char *format, ...) { // Source: ftpiiu
    char *tmp = nullptr;

    va_list va;
    va_start(va, format);
    if ((vasprintf(&tmp, format, va) >= 0) && (tmp != nullptr)) {
        OSScreenPutFontEx(SCREEN_TV, x, y, tmp);
        OSScreenPutFontEx(SCREEN_DRC, x, y, tmp);
    }
    va_end(va);
    if (tmp != nullptr)
        free(tmp);
}

void promptError(const char *message, ...) {
    clearBuffers();
    va_list va;
    va_start(va, message);
    char *tmp = nullptr;
    if ((vasprintf(&tmp, message, va) >= 0) && (tmp != nullptr)) {
        OSScreenPutFontEx(SCREEN_TV, 0, 0, tmp);
        OSScreenPutFontEx(SCREEN_DRC, 0, 0, tmp);
    }
    if (tmp != nullptr)
        free(tmp);
    flipBuffers();
    va_end(va);
    sleep(2);
}

void warning() {
    clearBuffersEx();
    console_print_pos(0, 0, "WARNING");
    console_print_pos(0, 1, "----------------------------------------------------------------------");
    console_print_pos(0, 2, "INSTALLING THEMES IS DANGEROUS, CONTINUE AT YOUR OWN RISK");
    console_print_pos(0, 3, "A BAD THEME MAY LEAD TO A (RECOVERABLE) BRICK");
    console_print_pos(0, 4, "PLEASE ENSURE THAT YOU HAVE A COLDBOOT SOLUTION INSTALLED");
    console_print_pos(0, 5, "----------------------------------------------------------------------");
    flipBuffers();
    sleep(5);
}

void header() {
    clearBuffersEx();
    if (isBackup) {
        console_print_pos(0, 0, "Backing up current theme...");
        console_print_pos(0, 1, "----------------------------------------------------------------------");
        console_print_pos(0, 2, "Please wait...");
        console_print_pos(0, 3, "----------------------------------------------------------------------");
    } else if (isInstalling) {
        console_print_pos(0, 0, "Installing theme...");
        console_print_pos(0, 1, "----------------------------------------------------------------------");
        console_print_pos(0, 2, "Please wait...");
        console_print_pos(0, 3, "----------------------------------------------------------------------");
    } else {
        console_print_pos(0, 0, "Select a theme:");
        console_print_pos(40, 0, "R: backup current theme");
        console_print_pos(0, 1, "----------------------------------------------------------------------");
    }
}

void displayMessage(std::string message) {
    header();
    console_print_pos(0, 5, message.c_str());
    flipBuffers();
}

void screendeInit() {
    if (tvBuffer != nullptr)
        free(tvBuffer);
    if (drcBuffer != nullptr)
        free(drcBuffer);

    OSScreenShutdown();

    return;
}

bool screenInit() {
    OSScreenInit();

    tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    tvBuffer = memalign(0x100, tvBufferSize);
    drcBuffer = memalign(0x100, drcBufferSize);

    if ((tvBuffer == nullptr) || (drcBuffer == nullptr)) {
        screendeInit();
        return false;
    }

    OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);

    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);

    return true;
}