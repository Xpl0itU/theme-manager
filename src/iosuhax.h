#include <iosuhax.h>
#include <iosuhax_disc_interface.h>
#include <coreinit/ios.h>
#include <stdlib.h>
#include <stdint.h>

enum CFWVersion {
    FAILED,
    NONE,
    MOCHA_NORMAL,
    TIRAMISU_RPX,
    MOCHA_LITE,
    HAXCHI,
    HAXCHI_MCP,
    DUMPLING,
    CEMU,
};

struct MCPVersion {
    int major;
    int minor;
    int patch;
    char region[4];
};

#define IOSUHAX_MAGIC_WORD 0x4E696365

CFWVersion testIosuhax();
CFWVersion getCFWVersion();