#include <coreinit/core.h>
#include <coreinit/ios.h>
#include <coreinit/foreground.h>
#include <padscore/kpad.h>
#include <proc_ui/procui.h>
#include <sndcore2/core.h>
#include <vpad/input.h>
#include <whb/proc.h>
#include <whb/sdcard.h>

#include <cstring>

#include <mocha/mocha.h>

#include "fsUtils.h"
#include "hash.h"
#include "screen.h"

static int cursorPosition = 0;
bool isInstalling = false;
bool isBackup = false;

static const std::string themesPath = "/vol/external01/wiiu/themes/";
static std::vector<std::string> themes;

static VPADStatus status;
static VPADReadError error;
KPADStatus kpad_status;

FSCmdBlock cmdBlk;

static std::string menuPath;

bool AppRunning() {
    bool app = true;
    if (OSIsMainCore()) {
        switch (ProcUIProcessMessages(true)) {
            case PROCUI_STATUS_EXITING:
                // Being closed, prepare to exit
                app = false;
                screendeInit();

                Mocha_DeInitLibrary();
                FSShutdown();
                ProcUIShutdown();
                break;
            case PROCUI_STATUS_RELEASE_FOREGROUND:
                // Free up MEM1 to next foreground app, deinit screen, etc.
                app = false;
                screendeInit();

                Mocha_DeInitLibrary();
                FSShutdown();
                ProcUIDrawDoneRelease();
                break;
            case PROCUI_STATUS_IN_FOREGROUND:
                // Executed while app is in foreground
                app = true;
                break;
            case PROCUI_STATUS_IN_BACKGROUND:
                break;
        }
    }

    return app;
}

static void check() {
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

static auto checkBackup() -> int {
    if ((checkEntry(themesPath + "backup/") == 2) && (checkEntry(themesPath + "backup/Men.pack") == 1) && (checkEntry(themesPath + "backup/Men2.pack") == 1))
        return 0;
    return -1;
}

static void backup() {
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

static void install() {
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
    FSInit();
    FSInitCmdBlock(&cmdBlk);
    FSSetCmdPriority(&cmdBlk, 0);
    bool mochaReady = Mocha_InitLibrary() == MOCHA_RESULT_SUCCESS;
    bool ret = mochaReady;
    if(ret) {
        ret = Mocha_UnlockFSClient(__wut_devoptab_fs_client) == MOCHA_RESULT_SUCCESS;
        if(ret) {
            WiiUConsoleOTP otp;
            ret = Mocha_ReadOTP(&otp) == MOCHA_RESULT_SUCCESS;
            if(ret) {
                MochaRPXLoadInfo info = {
                    .target = 0xDEADBEEF,
                    .filesize = 0,
                    .fileoffset = 0,
                    .path = "dummy"
                };

                MochaUtilsStatus s = Mocha_LaunchRPX(&info);
                ret = s != MOCHA_RESULT_UNSUPPORTED_API_VERSION && s != MOCHA_RESULT_UNSUPPORTED_COMMAND;
            }
        }
    }

    return ret;
}

auto main() -> int {
    AXInit();
    AXQuit();
    WHBProcInit();
    ProcUIInit(&OSSavesDone_ReadyToRelease);
    VPADInit();
    WPADInit();
    KPADInit();
    WPADEnableURCC(1);

    if (!screenInit()) {
        WHBProcShutdown();
        return 1;
    }

    if (!cfwValid()) {
        promptError("This CFW version is not supported, please use Tiramisu or Aroma.\nOr update your MochaPayload to the latest version.");
        return 0;
    }

    WHBMountSdCard();
    themes = listFolders(themesPath);

    if (dirExists("/vol/storage_mlc01/sys/title/00050010/10040200"))
        menuPath = "/vol/storage_mlc01/sys/title/00050010/10040200";
    else if (dirExists("/vol/storage_mlc01/sys/title/00050010/10040100"))
        menuPath = "/vol/storage_mlc01/sys/title/00050010/10040100";
    else if (dirExists("/vol/storage_mlc01/sys/title/00050010/10040000"))
        menuPath = "/vol/storage_mlc01/sys/title/00050010/10040000";

    warning();

    while (AppRunning()) {
        VPADRead(VPAD_CHAN_0, &status, 1, &error);
        memset(&kpad_status, 0, sizeof(KPADStatus));
        WPADExtensionType controllerType;
        for (int i = 0; i < 4; i++) {
            if (WPADProbe((WPADChan) i, &controllerType) == 0) {
                KPADRead((WPADChan) i, &kpad_status, 1);
                break;
            }
        }

        header();

        for (int i = 0; i < entrycount; ++i)
            console_print_pos(1, i + 3, themes[i].c_str());

        console_print_pos(0, cursorPosition + 3, ">");

        if ((status.trigger & (VPAD_BUTTON_DOWN | VPAD_STICK_L_EMULATION_DOWN)) |
            (kpad_status.trigger & (WPAD_BUTTON_DOWN)) |
            (kpad_status.classic.trigger & (WPAD_CLASSIC_BUTTON_DOWN | WPAD_CLASSIC_STICK_L_EMULATION_DOWN)) |
            (kpad_status.pro.trigger & (WPAD_PRO_BUTTON_DOWN | WPAD_PRO_STICK_L_EMULATION_DOWN)))
            ++cursorPosition;

        if ((status.trigger & (VPAD_BUTTON_UP | VPAD_STICK_L_EMULATION_UP)) |
            (kpad_status.trigger & (WPAD_BUTTON_UP)) |
            (kpad_status.classic.trigger & (WPAD_CLASSIC_BUTTON_UP | WPAD_CLASSIC_STICK_L_EMULATION_UP)) |
            (kpad_status.pro.trigger & (WPAD_PRO_BUTTON_UP | WPAD_PRO_STICK_L_EMULATION_UP)))
            --cursorPosition;

        if ((status.trigger & VPAD_BUTTON_A) |
            ((kpad_status.trigger & (WPAD_BUTTON_A)) | (kpad_status.classic.trigger & (WPAD_CLASSIC_BUTTON_A)) |
            (kpad_status.pro.trigger & (WPAD_PRO_BUTTON_A))))
            isInstalling = true;

        if ((status.trigger & VPAD_BUTTON_R) | (kpad_status.trigger & (WPAD_BUTTON_PLUS)) |
            (kpad_status.classic.trigger & (WPAD_CLASSIC_BUTTON_R)) |
            (kpad_status.pro.trigger & (WPAD_PRO_TRIGGER_R)))
            isBackup = true;

        if (isBackup && !isInstalling)
            backup();

        if (isInstalling) {
            install();
            check();
            isInstalling = false;
        }

        if (cursorPosition < 0)
            cursorPosition = entrycount - 1;
        else if (cursorPosition > entrycount - 1)
            cursorPosition = 0;

        flipBuffers();
    }
/*
    screendeInit();
    WHBProcShutdown();

    unmount_fs("mlc");
    IOSUHAX_FSA_Close(fsaFd);*/

    return 0;
}
