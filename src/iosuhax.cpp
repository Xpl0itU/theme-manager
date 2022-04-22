#include "iosuhax.h"
#include "hash.h"

int32_t fsaHandle = -1;
int32_t iosuhaxHandle = -1;
CFWVersion currCFWVersion = CFWVersion::NONE;

CFWVersion testIosuhax() {
    IOSHandle mcpHandle = IOS_Open("/dev/mcp", (IOSOpenMode)0);
    if (mcpHandle < IOS_ERROR_OK) {
        return CFWVersion::FAILED;
    }

    IOSHandle iosuhaxHandle = IOS_Open("/dev/iosuhax", (IOSOpenMode)0);
    // Cemu triggers this
    if (iosuhaxHandle >= IOS_ERROR_OK) {
        // Close iosuhax
        IOS_Close(iosuhaxHandle);

        // Test behavior of Mocha version by sending a subcommand to IOCTL100 to see which implementation is underneath
        constexpr int32_t dummyBufferSize = 0x300;
        void* dummyBuffer = aligned_alloc(0x20, 0x300);
        *(uint32_t*)dummyBuffer = 0;
        int32_t returnValue = 0;
        IOS_Ioctl(mcpHandle, 100, dummyBuffer, dummyBufferSize, &returnValue, sizeof(returnValue));
        free(dummyBuffer);
        IOS_Close(mcpHandle);

        if (returnValue == 1) {
            currCFWVersion = CFWVersion::TIRAMISU_RPX;
        }
        else if (returnValue == 2) {
            currCFWVersion = CFWVersion::MOCHA_LITE;
        }
        else if (returnValue == 3) {
            currCFWVersion = CFWVersion::DUMPLING;
        }
        else {
            currCFWVersion = CFWVersion::MOCHA_NORMAL;
        }
        return currCFWVersion;
    }

    // Test if haxchi FW is used with MCP hook
    uint32_t* returnValue = (uint32_t*)aligned_alloc(0x20, 0x100);
    IOS_Ioctl(mcpHandle, 0x5B, NULL, 0, returnValue, sizeof(*returnValue));
    if (*returnValue == IOSUHAX_MAGIC_WORD) {
        free(returnValue);
        IOS_Close(mcpHandle);
        currCFWVersion = CFWVersion::HAXCHI_MCP;
        return CFWVersion::HAXCHI_MCP;
    }
    free(returnValue);

    MCPVersion* mcpVersion = (MCPVersion*)aligned_alloc(0x20, 0x100);
    IOS_Ioctl(mcpHandle, 0x89, NULL, 0, mcpVersion, sizeof(*mcpVersion));
    if (mcpVersion->major == 99 && mcpVersion->minor == 99 && mcpVersion->patch == 99) {
        free(mcpVersion);
        IOS_Close(mcpHandle);
        currCFWVersion = CFWVersion::HAXCHI;
        return CFWVersion::HAXCHI;
    }
    free(mcpVersion);

    if (IOS_Open("/dev/nonexistent", (IOSOpenMode)0) == 0x1) {
        IOS_Close(mcpHandle);
        currCFWVersion = CFWVersion::CEMU;
        return CFWVersion::CEMU;
    }

    IOS_Close(mcpHandle);
    currCFWVersion = CFWVersion::NONE;
    return CFWVersion::NONE;
}

CFWVersion getCFWVersion() {
    return currCFWVersion;
}