/**
 * @file main.cpp
 * @author Kassent
 * @author Vadfromnu
 * @author Andrew Spaulding (Kasplat)
 * @brief Main SKSE library configuration.
 *
 * Cleaned up a little bit by Kasplat.
 */

#include <ShlObj.h>
#include "BranchTrampoline.h"
#include "Utilities.h"
#include "PluginAPI.h"
#include "skse_version.h"

#include "RelocPatch.h"
#include "Settings.h"

#define DLL_EXPORT __declspec(dllexport)

IDebugLog gLog;
HINSTANCE gDllHandle;
UInt32    g_pluginHandle = kPluginHandle_Invalid;

static bool GetDllDirWithSlash(std::string& path)
{
    char dllPath[4096]; // with \\?\ prefix path can be longer than MAX_PATH, so just using some magic number

    const auto ret = GetModuleFileNameA(gDllHandle, dllPath, sizeof(dllPath));
    if (ret == 0) {
        _ERROR("GetModuleFileNameA fail:%u", (unsigned)GetLastError());
        return false;
    }
    path.assign(dllPath, ret);

    const auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        _ERROR("Strange dll path:%s", path.c_str());
        return false;
    }
    path.resize(pos + 1);

    return true;
}

static bool SkyrimUncapper_Initialize(const SKSEInterface* skse)
{
    static bool isInit = false;
    if (isInit) {
        _WARNING("Already initialized.");
        return true;
    }
    isInit = true;

    gLog.OpenRelative(
        CSIDL_MYDOCUMENTS,
        "\\My Games\\Skyrim Special Edition\\SKSE\\SkyrimUncapper.log"
    );
    void *img_base = GetModuleHandle(NULL);
    _MESSAGE(
        "Compiled skse:0x%08X runtime:0x%08X. "
        "Running skse:0x%08X runtime:0x%08X",
        (unsigned int) PACKED_SKSE_VERSION,
        (unsigned int) CURRENT_RELEASE_RUNTIME,
        (unsigned int) skse->skseVersion, 
        (unsigned int) skse->runtimeVersion
    );
    _MESSAGE("imagebase = %016I64X", img_base);

    std::string path;
    if (!GetDllDirWithSlash(path)) {
        return false;
    }

    path += "SkyrimUncapper.ini";
    if (!settings.ReadConfig(path)) {
        return false;
    }

    if (ApplyGamePatches(img_base, skse->runtimeVersion) < 0) {
        _ERROR("Failed to apply game patches. See log for details.");
        return false;
    }

    _MESSAGE("Init complete");
    return true;
}

extern "C" {
    DLL_EXPORT SKSEPluginVersionData SKSEPlugin_Version = {
        SKSEPluginVersionData::kVersion,
        1, // version number of your plugin
        "SkyrimUncapperAE",
        "Andrew Spaulding (Kasplat)", // name
        "andyespaulding@gmail.com", // support@example.com
        SKSEPluginVersionData::kVersionIndependentEx_NoStructUse, // We manually check our offsets against the game version
        SKSEPluginVersionData::kVersionIndependent_AddressLibraryPostAE,
        {0}, // works with any version of the script extender. you probably do not need to put anything here
        0 // minimum version of the script extender required, compared against PACKED_SKSE_VERSION
    };

    BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpreserved) {
        (void)lpreserved;

        switch (dwReason) {
            case DLL_PROCESS_ATTACH:
                gDllHandle = hinstDLL;
                break;
            case DLL_PROCESS_DETACH:
                break;
        };

        return TRUE;
    }

    DLL_EXPORT bool SKSEPlugin_Load(const SKSEInterface* skse) {
        if (skse->isEditor) { // ianpatt: yup no more editor. obscript gone (mostly)
            return false;
        }

        g_pluginHandle = skse->GetPluginHandle();
        return SkyrimUncapper_Initialize(skse);
    }
};
