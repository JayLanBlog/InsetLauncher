// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "common/common.h"
#include "hook/hooks.h"

static void RigsterHook() {
    if (LibraryHooks::Detect("launch__replay__marker")) {
        std::wcout << "launch__replay__marker" << std::endl;
        return;
    }
    std::cout << "DllMain be RegisterHooks" << std::endl;
    LibraryHooks::RegisterHooks();
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
      
        RigsterHook();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

