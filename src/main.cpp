#include <atomic>
#include <cstring>
#include <dirent.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <coreinit/cache.h>
#include <coreinit/screen.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include <iosuhax.h>
#include <iosuhax_devoptab.h>

#define IO_MAX_FILE_BUFFER (1024 * 1024) // 1 MB

int entrycount = 0;
int cursorPosition = 0;
bool isInstalling = false;
bool isBackup = false;
std::string themesPath = "/vol/external01/wiiu/themes/";

VPADStatus status;
VPADReadError error;
bool vpad_fatal = false;

size_t tvBufferSize;
size_t drcBufferSize;
void *tvBuffer;
void *drcBuffer;
int fsaFd;

std::string menuPath;

// check if a directory exists
bool dirExists(const char *dir) {
    DIR *d = opendir(dir);
    if (d) {
        closedir(d);
        return true;
    }
    return false;
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

bool replace(std::string &str, const std::string &from, const std::string &to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

static std::string newlibToFSA(std::string path) {
    if (path[3] == ':') {
        switch (path[0]) {
            case 'u':
                replace(path, "usb:", "/vol/storage_usb01");
                break;
            case 'm':
                replace(path, "mlc:", "/vol/storage_mlc01");
                break;
            case 's':
                replace(path, "slc:", "/vol/storage_slccmpt01");
                break;
        }
    }
    return path;
}

std::vector<std::string> files(std::string path) {
    std::vector<std::string> files;
    int dir_ = opendir(path.c_str());
    struct dirent *ent;
    while ((ent = readdir(dir_)) != NULL) {
        if (ent->d_type == DT_DIR) { // regular file
            files.push_back(ent->d_name);
            ++entrycount;
        }
    }

    return files;
}

int copyFile(std::string pPath, std::string oPath) {
    FILE *source = fopen(pPath.c_str(), "rb");
    if (source == nullptr)
        return -1;

    FILE *dest = fopen(oPath.c_str(), "wb");
    if (dest == nullptr) {
        fclose(source);
        return -1;
    }

    char *buffer[3];
    for (int i = 0; i < 3; i++) {
        buffer[i] = (char *) aligned_alloc(0x40, IO_MAX_FILE_BUFFER);
        if (buffer[i] == nullptr) {
            fclose(source);
            fclose(dest);
            for (i--; i >= 0; i--)
                free(buffer[i]);

            return -1;
        }
    }

    setvbuf(source, buffer[0], _IOFBF, IO_MAX_FILE_BUFFER);
    setvbuf(dest, buffer[1], _IOFBF, IO_MAX_FILE_BUFFER);
    int size = 0;
    int bytesWritten = 0;

    while ((size = fread(buffer[2], 1, IO_MAX_FILE_BUFFER, source)) > 0) {
        bytesWritten = fwrite(buffer[2], 1, size, dest);
    }
    fclose(source);
    fclose(dest);
    for (int i = 0; i < 3; i++)
        free(buffer[i]);

    IOSUHAX_FSA_ChangeMode(fsaFd, newlibToFSA(oPath).c_str(), 0x644);

    return 0;
}

void promptError(const char *message, ...) {
    OSScreenClearBufferEx(SCREEN_TV, 0x00000000);
    OSScreenClearBufferEx(SCREEN_DRC, 0x00000000);

    DCFlushRange(tvBuffer, tvBufferSize);
    DCFlushRange(drcBuffer, drcBufferSize);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    va_list va;
    va_start(va, message);
    char *tmp = nullptr;
    if ((vasprintf(&tmp, message, va) >= 0) && (tmp != nullptr)) {
        OSScreenPutFontEx(SCREEN_TV, 0, 0, tmp);
        OSScreenPutFontEx(SCREEN_DRC, 0, 0, tmp);
    }
    if (tmp != nullptr)
        free(tmp);
    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
    va_end(va);
    sleep(2);
}

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

void header() {
    if(isBackup) {
        console_print_pos(0, 0, "Backup current theme...");
        console_print_pos(0, 1, "----------------------------------------------------------------------------");
        console_print_pos(0, 2, "Please wait...");
        console_print_pos(0, 3, "----------------------------------------------------------------------------");
    } else if(isInstalling) {
        console_print_pos(0, 0, "Installing theme...");
        console_print_pos(0, 1, "----------------------------------------------------------------------------");
        console_print_pos(0, 2, "Please wait...");
        console_print_pos(0, 3, "----------------------------------------------------------------------------");
    } else {
        console_print_pos(0, 0, "Select a theme:");
        console_print_pos(40, 0, "R: backup current theme");
        console_print_pos(0, 1, "----------------------------------------------------------------------------");
    }
}

int checkEntry(const char *fPath) {
    struct stat st;
    if (stat(fPath, &st) == -1)
        return 0;

    if (S_ISDIR(st.st_mode))
        return 2;
    
    return 1;
}

int mkdir_p(const char *fPath) { //Adapted from mkdir_p made by JonathonReinhart
    std::string _path;
    char *p;
    int found = 0;

    _path.assign(fPath);

    for (p = (char *) _path.c_str() + 1; *p != 0; p++) {
        if (*p == '/') {
            found++;
            if (found > 2) {
                *p = '\0';
                if (checkEntry(_path.c_str()) == 0)
                    if (mkdir(_path.c_str(), DEFFILEMODE) == -1)
                        return -1;
                *p = '/';
            }
        }
    }

    if (checkEntry(_path.c_str()) == 0)
        if (mkdir(_path.c_str(), DEFFILEMODE) == -1)
            return -1;

    return 0;
}

int main() {
    WHBProcInit();

    OSScreenInit();

    tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    tvBuffer = memalign(0x100, tvBufferSize);
    drcBuffer = memalign(0x100, drcBufferSize);

    if (!tvBuffer || !drcBuffer) {
        if (tvBuffer) free(tvBuffer);
        if (drcBuffer) free(drcBuffer);

        OSScreenShutdown();
        WHBProcShutdown();

        return 1;
    }

    OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);

    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);

    int res = IOSUHAX_Open(NULL);
    if (res < 0) {
        promptError("IOSUHAX_Open failed.");
        return 0;
    }

    int fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        promptError("IOSUHAX_FSA_Open failed.");
        return 0;
    }

    WHBMountSdCard();
    mount_fs("mlc", fsaFd, NULL, "/vol/storage_mlc01");
    std::vector<std::string> themes = files(themesPath);

    if (dirExists("mlc:/sys/title/00050010/10040200"))
        menuPath = "mlc:/sys/title/00050010/10040200";
    else if (dirExists("mlc:/sys/title/00050010/10040100"))
        menuPath = "mlc:/sys/title/00050010/10040100";
    else if (dirExists("mlc:/sys/title/00050010/10040000"))
        menuPath = "mlc:/sys/title/00050010/10040000";

    while (WHBProcIsRunning()) {
        VPADRead(VPAD_CHAN_0, &status, 1, &error);

        clearBuffersEx();
        header();

        for (int a = 0; a < entrycount; ++a)
            console_print_pos(1, a + 3, themes[a].c_str());

        console_print_pos(0, cursorPosition + 3, ">");

        if (status.trigger & VPAD_BUTTON_DOWN)
            ++cursorPosition;

        if (status.trigger & VPAD_BUTTON_UP)
            --cursorPosition;

        if (status.trigger & VPAD_BUTTON_A)
            isInstalling = true;
        
        if (status.trigger & VPAD_BUTTON_R)
            isBackup = true;
        
        if(isBackup) {
            if(!isInstalling) {
                clearBuffersEx();
                header();
                console_print_pos(0, 5, "Backing up Men.pack");
                flipBuffers();
                mkdir_p((themesPath + "backup").c_str());
                if(copyFile(menuPath + "/content/Common/Package/Men.pack", themesPath + "backup/Men.pack") == 0) {
                    clearBuffersEx();
                    header();
                } else {
                    clearBuffersEx();
                    header();
                    console_print_pos(0, 5, "Men.pack error");
                    flipBuffers();
                    sleep(2);
                }
                clearBuffersEx();
                header();
                console_print_pos(0, 5, "Backing up Men2.pack");
                flipBuffers();
                if(copyFile(menuPath + "/content/Common/Package/Men2.pack", themesPath + "backup/Men2.pack") == 0) {
                    clearBuffersEx();
                    header();
                } else {
                    clearBuffersEx();
                    header();
                    console_print_pos(0, 5, "Men2.pack error");
                    flipBuffers();
                    sleep(2);
                }
                isBackup = false;
            }
        }

        if (isInstalling) {
            clearBuffersEx();
            header();
            console_print_pos(0, 5, "Copying Men.pack");
            flipBuffers();
            if (copyFile(themesPath + themes[cursorPosition] + "/Men.pack", menuPath + "/content/Common/Package/Men.pack") == 0) {
                clearBuffersEx();
                header();
                flipBuffers();
            } else {
                clearBuffersEx();
                header();
                console_print_pos(0, 5, "Men.pack error");
                flipBuffers();
                sleep(2);
            }
            clearBuffersEx();
            header();
            console_print_pos(0, 5, "Copying Men2.pack");
            flipBuffers();
            if (copyFile(themesPath + themes[cursorPosition] + "/Men2.pack", menuPath + "/content/Common/Package/Men2.pack") == 0) {
                clearBuffersEx();
                header();
                IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_mlc01");
                console_print_pos(0, 5, "Theme installed.");
                flipBuffers();
                sleep(2);
                isInstalling = false;
            } else {
                clearBuffersEx();
                header();
                console_print_pos(0, 5, "Men2.pack error");
                flipBuffers();
                sleep(2);
                isInstalling = false;
            }
        }

        if (cursorPosition < 0)
            cursorPosition = entrycount - 1;
        else if (cursorPosition > entrycount - 1)
            cursorPosition = 0;

        flipBuffers();
    }

    if (tvBuffer) free(tvBuffer);
    if (drcBuffer) free(drcBuffer);

    OSScreenShutdown();
    WHBProcShutdown();

    unmount_fs("mlc");
    IOSUHAX_FSA_Close(fsaFd);
    IOSUHAX_Close();

    return 1;
}