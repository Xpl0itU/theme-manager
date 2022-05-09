#include <atomic>
#include <cstring>

#include <vector>

#include <coreinit/ios.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include "hash.h"
#include "screen.h"

int entrycount = 0;
int cursorPosition = 0;
bool isInstalling = false;
bool isBackup = false;

const std::string themesPath = "/vol/external01/wiiu/themes/";
std::vector<std::string> themes;

VPADStatus status;
VPADReadError error;
bool vpad_fatal = false;

int res;
int fsaFd;

std::string menuPath;

auto dirExists(const std::string &dir) -> bool {
    DIR *d = opendir(dir.c_str());
    if (d != nullptr) {
        closedir(d);
        return true;
    }
    return false;
}

std::vector<std::string> listFolders(const std::string &path) {
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

void check() {
    displayMessage("Checking Men.pack");
    if (hashFiles(themesPath + themes[cursorPosition] + "/Men.pack", menuPath + "/content/Common/Package/Men.pack") != 0) {
        displayMessage("Men.pack hash error");
        sleep(2);
    }

    displayMessage("Checking Men2.pack");
    if (hashFiles(themesPath + themes[cursorPosition] + "/Men2.pack", menuPath + "/content/Common/Package/Men2.pack") != 0) {
        displayMessage("Men2.pack hash error");
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

auto mkdir_p(const std::string &fPath) -> int { // Adapted from mkdir_p made by JonathonReinhart
    std::string _path;
    char *p;
    int found = 0;

    _path = fPath;

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
    displayMessage("Backing up Men.pack");
    mkdir_p(themesPath + "backup");
    if (copyFile(menuPath + "/content/Common/Package/Men.pack", themesPath + "backup/Men.pack") != 0) {
        displayMessage("Men.pack error");
        sleep(2);
    }
    displayMessage("Backing up Men2.pack");
    if (copyFile(menuPath + "/content/Common/Package/Men2.pack", themesPath + "backup/Men2.pack") != 0) {
        displayMessage("Men2.pack error");
        sleep(2);
    }
    displayMessage("Checking Men.pack");
    if (hashFiles(menuPath + "/content/Common/Package/Men.pack", themesPath + "backup/Men.pack") != 0) {
        displayMessage("Men.pack hash error");
        sleep(2);
    }
    displayMessage("Checking Men2.pack");
    if (hashFiles(menuPath + "/content/Common/Package/Men2.pack", themesPath + "backup/Men2.pack") != 0) {
        displayMessage("Men2.pack hash error");
        sleep(2);
    }
    isBackup = false;
}

void install() {
    clearBuffersEx();
    if (checkBackup() != 0)
        backup();
    displayMessage("Copying Men.pack");
    if (copyFile(themesPath + themes[cursorPosition] + "/Men.pack", menuPath + "/content/Common/Package/Men.pack") != 0) {
        displayMessage("Men.pack error");
        sleep(2);
    }
    displayMessage("Copying Men2.pack");
    if (copyFile(themesPath + themes[cursorPosition] + "/Men2.pack", menuPath + "/content/Common/Package/Men2.pack") == 0) {
        displayMessage("Theme installed.");
        sleep(2);
    } else {
        displayMessage("Men2.pack error");
        sleep(2);
    }
}

static bool cfwValid() {
    int handle = IOS_Open("/dev/mcp", IOS_OPEN_READ);
    bool ret = handle >= 0;
    if (ret) {
        char dummy[0x100];
        int in = 0xF9; // IPC_CUSTOM_COPY_ENVIRONMENT_PATH
        ret = IOS_Ioctl(handle, 100, &in, sizeof(in), dummy, sizeof(dummy)) == IOS_ERROR_OK;
        if (ret) {
            res = IOSUHAX_Open(NULL);
            if (res < 0) {
                promptError("IOSUHAX_Open failed. Please use Tiramisu");
                return 0;
            }
            if (ret)
                ret = IOSUHAX_read_otp((uint8_t *) dummy, 1) >= 0;
        }

        IOS_Close(handle);
    }

    return ret;
}

auto main() -> int {
    WHBProcInit();

    if (screenInit() == false) {
        WHBProcShutdown();
        return 1;
    }

    if (!cfwValid()) {
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

    screendeInit();
    WHBProcShutdown();

    unmount_fs("mlc");
    IOSUHAX_FSA_Close(fsaFd);

    return 1;
}
