/**
 * @file main.cpp
 * @author Kassant
 * @brief Main SKSE library configuration.
 *
 * Cleaned up a little bit by Kasplat.
 */

#include <ShlObj.h>
#include "BranchTrampoline.h"
#include "Hook_Skill.h"
#include "Settings.h"
#include "Utilities.h"
#include "PluginAPI.h"
#include "skse_version.h"

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

    gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\SkyrimUncapper.log");
    void* imgBase = GetModuleHandle(NULL);
    _MESSAGE("Compiled skse:0x%08X runtime:0x%08X. Now skse:0x%08X runtime:0x%08X", (unsigned)PACKED_SKSE_VERSION, (unsigned)CURRENT_RELEASE_RUNTIME, (unsigned)skse->skseVersion, (unsigned)skse->runtimeVersion);
    _MESSAGE("imagebase = %016I64X", imgBase);
    _MESSAGE("gDllHandle = %016I64X", gDllHandle);

    if (!g_branchTrampoline.Create(1024 * 64, imgBase)) {
        _ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
        return false;
    }
    if (!g_localTrampoline.Create(1024 * 64, gDllHandle)) {
        _ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
        return false;
    }

    std::string path;
    if (!GetDllDirWithSlash(path)) {
        return false;
    }

    path += "SkyrimUncapper.ini";
    if (!settings.ReadConfig(path)) {
        return false;
    }

    Hook_Skill_Commit();
    _MESSAGE("Init complete");
    return true;
}

extern "C" {
    DLL_EXPORT SKSEPluginVersionData SKSEPlugin_Version = {
        SKSEPluginVersionData::kVersion,
        1, // version number of your plugin
        "SkyrimUncapperAE",
        "", // name
        "", // support@example.com
        0, // kVersionIndependentEx_NoStructUse
        SKSEPluginVersionData::kVersionIndependent_Signatures | SKSEPluginVersionData::kVersionIndependent_StructsPost629,
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
