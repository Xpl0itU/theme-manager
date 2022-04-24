#include <atomic>
#include <cstdarg>
#include <cstring>
#include <malloc.h>

#include <cstdint>
#include <unistd.h>
#include <vector>

#include <coreinit/cache.h>
#include <coreinit/ios.h>
#include <coreinit/screen.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include "hash.h"

#define IO_MAX_FILE_BUFFER (1024 * 1024) // 1 MB

int entrycount = 0;
int cursorPosition = 0;
bool isInstalling = false;
bool isBackup = false;

const std::string themesPath = "/vol/external01/wiiu/themes/";
std::vector<std::string> themes;

VPADStatus status;
VPADReadError error;
bool vpad_fatal = false;

size_t tvBufferSize;
size_t drcBufferSize;
void *tvBuffer;
void *drcBuffer;

int res;
int fsaFd;

std::string menuPath;

static void flipBuffers() {
    DCFlushRange(tvBuffer, tvBufferSize);
    DCFlushRange(drcBuffer, drcBufferSize);

    OSScreenFlipBuffersEx(SCREEN_TV);
    OSScreenFlipBuffersEx(SCREEN_DRC);
}

static void clearBuffersEx() {
    OSScreenClearBufferEx(SCREEN_TV, 0x00000000);
    OSScreenClearBufferEx(SCREEN_DRC, 0x00000000);
}

static void clearBuffers() {
    clearBuffersEx();
    flipBuffers();
}

auto dirExists(const char *dir) -> bool {
    DIR *d = opendir(dir);
    if (d != nullptr) {
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

std::vector<std::string> listFolders(std::string path) {
    std::vector<std::string> folders;
    DIR *dir_ = opendir(path.c_str());
    struct dirent *ent = nullptr;
    while ((ent = readdir(dir_)) != nullptr) {
        if (ent->d_type == DT_DIR) { // folder
            folders.push_back(ent->d_name);
            ++entrycount;
        }
    }

    return folders;
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

static void warning() {
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

static void header() {
    if (isBackup) {
        console_print_pos(0, 0, "Backup current theme...");
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

void check() {
    header();
    console_print_pos(0, 5, "Checking Men.pack");
    if (hashFiles(themesPath + themes[cursorPosition] + "/Men.pack", menuPath + "/content/Common/Package/Men.pack") != 0) {
        clearBuffersEx();
        header();
        console_print_pos(0, 5, "Men.pack hash error");
        flipBuffers();
        sleep(2);
    }

    clearBuffersEx();
    header();
    console_print_pos(0, 5, "Checking Men2.pack");
    if (hashFiles(themesPath + themes[cursorPosition] + "/Men2.pack", menuPath + "/content/Common/Package/Men2.pack") != 0) {
        clearBuffersEx();
        header();
        console_print_pos(0, 5, "Men2.pack hash error");
        flipBuffers();
        sleep(2);
    }
}

auto checkEntry(std::string fPath) -> int {
    struct stat st {};
    if (stat(fPath.c_str(), &st) == -1)
        return 0;

    if (S_ISDIR(st.st_mode))
        return 2;

    return 1;
}

auto mkdir_p(const char *fPath) -> int { //Adapted from mkdir_p made by JonathonReinhart
    std::string _path;
    char *p;
    int found = 0;

    _path.assign(fPath);

    for (p = (char *) _path.c_str() + 1; *p != 0; ++p) {
        if (*p == '/') {
            ++found;
            if (found > 2) {
                *p = '\0';
                if (checkEntry(_path) == 0)
                    if (mkdir(_path.c_str(), DEFFILEMODE) == -1)
                        return -1;
                *p = '/';
            }
        }
    }

    if (checkEntry(_path) == 0)
        if (mkdir(_path.c_str(), DEFFILEMODE) == -1)
            return -1;

    return 0;
}

auto checkBackup() -> int {
    if ((checkEntry(themesPath + "backup/") == 2) && (checkEntry(themesPath + "backup/Men.pack") == 1) && (checkEntry(themesPath + "backup/Men2.pack") == 1))
        return 0;
    return -1;
}

void backup() {
    clearBuffersEx();
    header();
    console_print_pos(0, 5, "Backing up Men.pack");
    flipBuffers();
    mkdir_p((themesPath + "backup").c_str());
    if (copyFile(menuPath + "/content/Common/Package/Men.pack", themesPath + "backup/Men.pack") == 0) {
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
    if (copyFile(menuPath + "/content/Common/Package/Men2.pack", themesPath + "backup/Men2.pack") == 0) {
        clearBuffersEx();
        header();
    } else {
        clearBuffersEx();
        header();
        console_print_pos(0, 5, "Men2.pack error");
        flipBuffers();
        sleep(2);
    }

    clearBuffersEx();
    header();
    console_print_pos(0, 5, "Checking Men.pack");
    if (hashFiles(menuPath + "/content/Common/Package/Men.pack", themesPath + "backup/Men.pack") != 0) {
        clearBuffersEx();
        header();
        console_print_pos(0, 5, "Men.pack hash error");
        flipBuffers();
        sleep(2);
    }

    clearBuffersEx();
    header();
    console_print_pos(0, 5, "Checking Men2.pack");
    if (hashFiles(menuPath + "/content/Common/Package/Men2.pack", themesPath + "backup/Men2.pack") != 0) {
        clearBuffersEx();
        header();
        console_print_pos(0, 5, "Men2.pack hash error");
        flipBuffers();
        sleep(2);
    }
    isBackup = false;
}

void install() {
    clearBuffersEx();
    if (checkBackup() != 0)
        backup();
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
        console_print_pos(0, 5, "Theme installed.");
        flipBuffers();
        sleep(2);
    } else {
        clearBuffersEx();
        header();
        console_print_pos(0, 5, "Men2.pack error");
        flipBuffers();
        sleep(2);
        isInstalling = false;
    }
}

static bool cfwValid()
{
	int handle = IOS_Open("/dev/mcp", IOS_OPEN_READ);
	bool ret = handle >= 0;
	if(ret)
	{
		char dummy[0x100];
		int in = 0xF9; // IPC_CUSTOM_COPY_ENVIRONMENT_PATH
		ret = IOS_Ioctl(handle, 100, &in, sizeof(in), dummy, sizeof(dummy)) == IOS_ERROR_OK;
		if(ret)
		{
			res = IOSUHAX_Open(NULL);
            if (res < 0) {
                promptError("IOSUHAX_Open failed. Please use Tiramisu");
                return 0;
            }
			if(ret)
				ret = IOSUHAX_read_otp((uint8_t *)dummy, 1) >= 0;
		}

		IOS_Close(handle);
	}

	return ret;
}

auto main() -> int {
    WHBProcInit();

    OSScreenInit();

    tvBufferSize = OSScreenGetBufferSizeEx(SCREEN_TV);
    drcBufferSize = OSScreenGetBufferSizeEx(SCREEN_DRC);

    tvBuffer = memalign(0x100, tvBufferSize);
    drcBuffer = memalign(0x100, drcBufferSize);

    if ((tvBuffer == nullptr) || (drcBuffer == nullptr)) {
        if (tvBuffer != nullptr)
            free(tvBuffer);
        if (drcBuffer != nullptr)
            free(drcBuffer);

        OSScreenShutdown();
        WHBProcShutdown();

        return 1;
    }

    OSScreenSetBufferEx(SCREEN_TV, tvBuffer);
    OSScreenSetBufferEx(SCREEN_DRC, drcBuffer);

    OSScreenEnableEx(SCREEN_TV, true);
    OSScreenEnableEx(SCREEN_DRC, true);

    if(!cfwValid()) {
        promptError("This CFW version is not supported, please use Tiramisu.");
        return 0;
    }

    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        promptError("IOSUHAX_FSA_Open failed.");
        return 0;
    }

    WHBMountSdCard();
    mount_fs("mlc", fsaFd, NULL, "/vol/storage_mlc01");
    themes = listFolders(themesPath);

    if (dirExists("mlc:/sys/title/00050010/10040200"))
        menuPath = "mlc:/sys/title/00050010/10040200";
    else if (dirExists("mlc:/sys/title/00050010/10040100"))
        menuPath = "mlc:/sys/title/00050010/10040100";
    else if (dirExists("mlc:/sys/title/00050010/10040000"))
        menuPath = "mlc:/sys/title/00050010/10040000";

    warning();

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

        if (isBackup && !isInstalling)
            backup();

        if (isInstalling) {
            install();
            IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_mlc01");
            check();
            isInstalling = false;
        }

        if (cursorPosition < 0)
            cursorPosition = entrycount - 1;
        else if (cursorPosition > entrycount - 1)
            cursorPosition = 0;

        flipBuffers();
    }

    if (tvBuffer != nullptr)
        free(tvBuffer);
    if (drcBuffer != nullptr)
        free(drcBuffer);

    OSScreenShutdown();
    WHBProcShutdown();

    unmount_fs("mlc");
    IOSUHAX_FSA_Close(fsaFd);

    return 1;
}
