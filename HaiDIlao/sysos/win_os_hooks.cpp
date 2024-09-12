#include <windows.h>

#include <tlhelp32.h>
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include "common/common.h"
#include "hook/hooks.h"
#include "common/thread.h"
#include "common/string_util.h"
std::map<void**, void*> s_InstalledHooks;
Threading::CriticalSection installedLock;


bool ApplyHook(FuncitonEntry& hook, void** IATentry, bool& already)
{
    DWORD oldProtection = PAGE_EXECUTE;

    if (*IATentry == hook.hook)
    {
        already = true;
        return true;
    }

#if ENABLED_LOG_WIN
   
    std::wcout << "Patching IAT for " << hook.function.c_str() << IATentry << hook.hook << std::endl;
#endif

    {
        SCOPED_LOCK(installedLock);
        if (s_InstalledHooks.find(IATentry) == s_InstalledHooks.end())
            s_InstalledHooks[IATentry] = *IATentry;
    }

    BOOL success = VirtualProtect(IATentry, sizeof(void*), PAGE_READWRITE, &oldProtection);
    if (!success)
    {
      
        std::wcerr << "Failed to make IAT entry writeable" << IATentry << std::endl;
        return false;
    }

    *IATentry = hook.hook;

    success = VirtualProtect(IATentry, sizeof(void*), oldProtection, &oldProtection);
    if (!success)
    {
        std::wcerr << "Failed to restore IAT entry protection" << IATentry << std::endl;
        return false;
    }

    return true;
}

struct Hookset
{
	HMODULE module = NULL;
	bool hooksfetched = false;
	std::vector<HMODULE> altmodules;
	std::vector<FuncitonEntry> FunctionHooks;
	DWORD OrdinalBase = 0;
	std::vector<std::string> OrdinalNames;
	std::vector<FunctionCallback> Callbacks;
	Threading::CriticalSection ordinallock;

	void FetchOrdinalNames()
	{
        SCOPED_LOCK(ordinallock);

        // return if we already fetched the ordinals
        if (!OrdinalNames.empty())
            return;

        byte* baseAddress = (byte*)module;

#if ENABLED_LOG_WIN
        std::wcout << "FetchOrdinalNames" << std::endl;
       
#endif

        PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)baseAddress;

        if (dosheader->e_magic != 0x5a4d)
            return;

        char* PE00 = (char*)(baseAddress + dosheader->e_lfanew);
        PIMAGE_FILE_HEADER fileHeader = (PIMAGE_FILE_HEADER)(PE00 + 4);
        PIMAGE_OPTIONAL_HEADER optHeader =
            (PIMAGE_OPTIONAL_HEADER)((BYTE*)fileHeader + sizeof(IMAGE_FILE_HEADER));

        DWORD eatOffset = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

        IMAGE_EXPORT_DIRECTORY* exportDesc = (IMAGE_EXPORT_DIRECTORY*)(baseAddress + eatOffset);

        WORD* ordinals = (WORD*)(baseAddress + exportDesc->AddressOfNameOrdinals);
        DWORD* names = (DWORD*)(baseAddress + exportDesc->AddressOfNames);

        DWORD count = MIN(exportDesc->NumberOfFunctions, exportDesc->NumberOfNames);

        WORD maxOrdinal = 0;
        for (DWORD i = 0; i < count; i++)
            maxOrdinal = MAX(maxOrdinal, ordinals[i]);

        OrdinalBase = exportDesc->Base;
        OrdinalNames.resize(maxOrdinal + 1);

        for (DWORD i = 0; i < count; i++)
        {
            OrdinalNames[ordinals[i]] = (char*)(baseAddress + names[i]);

#if ENABLED_LOG_WIN
            std::wcout << "ordinal found: " << OrdinalNames[ordinals[i]].c_str() << (uint32_t)ordinals[i] << std::endl;
#endif
        }
	}
};


struct CachedHookData
{
	bool hookAll = true;

	std::map<std::string, Hookset> DllHooks;
	HMODULE ownmodule = NULL;
	Threading::CriticalSection lock;
	char lowername[512] = {};

	std::set<std::string> ignores;

	bool missedOrdinals = false;
	std::function<HMODULE(LPCWSTR&, HANDLE, DWORD)> libraryIntercept;

	int32_t posthooking = 0;
	void ApplyHooks(const char* modName, HMODULE module) {
        {
            size_t i = 0;
            while (modName[i])
            {
                lowername[i] = (char)tolower(modName[i]);
                i++;
            }
            lowername[i] = 0;
        }
        if (strstr(lowername, "fraps") || strstr(lowername, "gameoverlayrenderer") ||
            strstr(lowername, STRINGIZE(RDOC_BASE_NAME) ".dll") == lowername)
            return;
        for (auto it = DllHooks.begin(); it != DllHooks.end(); ++it)
        {
            if (!_stricmp(it->first.c_str(), modName))
            {
                if (it->second.module == NULL)
                {
                    it->second.module = module;

                    it->second.hooksfetched = true;

                    // fetch all function hooks here, since we want to fill out the original function pointer
                    // even in case nothing imports from that function (which means it would not get filled
                    // out through FunctionHook::ApplyHook)
                    for (FuncitonEntry& hook : it->second.FunctionHooks)
                    {
                        if (hook.orig && *hook.orig == NULL)
                            *hook.orig = GetProcAddress(module, hook.function.c_str());
                    }

                    it->second.FetchOrdinalNames();
                }
                else if (it->second.module != module)
                {
                    // if it's already in altmodules, bail
                    bool already = false;

                    for (size_t i = 0; i < it->second.altmodules.size(); i++)
                    {
                        if (it->second.altmodules[i] == module)
                        {
                            already = true;
                            break;
                        }
                    }

                    if (already)
                        break;

                    // check if the previous module is still valid
                    SetLastError(0);
                    char filename[MAX_PATH] = {};
                    GetModuleFileNameA(it->second.module, filename, MAX_PATH - 1);
                    DWORD err = GetLastError();
                    char* slash = strrchr(filename, L'\\');

                    std::string basename = slash ? str_to_lower(std::string(slash + 1)) : "";

                    if (err == 0 && basename == it->first)
                    {
                        // previous module is still loaded, add this to the alt modules list
                        it->second.altmodules.push_back(module);
                    }
                    else
                    {
                       
                        for (FuncitonEntry& hook : it->second.FunctionHooks)
                        {
                            if (hook.orig)
                                *hook.orig = GetProcAddress(module, hook.function.c_str());
                        }

                        it->second.module = module;
                    }
                }
            }
        }
        // for safety (and because we don't need to), ignore these modules
        if (!_stricmp(modName, "kernel32.dll") || !_stricmp(modName, "powrprof.dll") ||
            !_stricmp(modName, "CoreMessaging.dll") || !_stricmp(modName, "opengl32.dll") ||
            !_stricmp(modName, "gdi32.dll") || !_stricmp(modName, "gdi32full.dll") ||
            !_stricmp(modName, "windows.storage.dll") || !_stricmp(modName, "nvoglv32.dll") ||
            !_stricmp(modName, "nvoglv64.dll") || !_stricmp(modName, "vulkan-1.dll") ||
            !_stricmp(modName, "atio6axx.dll") || !_stricmp(modName, "atioglxx.dll") ||
            !_stricmp(modName, "nvcuda.dll") || strstr(lowername, "cudart") == lowername ||
            strstr(lowername, "msvcr") == lowername || strstr(lowername, "msvcp") == lowername ||
            strstr(lowername, "nv-vk") == lowername || strstr(lowername, "amdvlk") == lowername ||
            strstr(lowername, "igvk") == lowername || strstr(lowername, "nvopencl") == lowername ||
            strstr(lowername, "nvapi") == lowername)
            return;
        if (ignores.find(lowername) != ignores.end())
            return;
        wchar_t modpath[1024] = { 0 };
        GetModuleFileNameW(module, modpath, 1023);
        if (modpath[0] == 0)
            return;
        HMODULE refcountModHandle = LoadLibraryW(modpath);
        byte* baseAddress = (byte*)refcountModHandle;

        PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)baseAddress;

        if (dosheader->e_magic != 0x5a4d)
        {
#ifdef  ENABLED_LOG_WIN
            std::wcout << "Ignoring module "<< modName << std::endl;
#endif //  ENABLED_LOG_WIN

            FreeLibrary(refcountModHandle);
            return;
        }
        char* PE00 = (char*)(baseAddress + dosheader->e_lfanew);
        PIMAGE_FILE_HEADER fileHeader = (PIMAGE_FILE_HEADER)(PE00 + 4);
        PIMAGE_OPTIONAL_HEADER optHeader =
            (PIMAGE_OPTIONAL_HEADER)((BYTE*)fileHeader + sizeof(IMAGE_FILE_HEADER));

        DWORD iatOffset = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

        IMAGE_IMPORT_DESCRIPTOR* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(baseAddress + iatOffset);
        while (iatOffset && importDesc->FirstThunk)
        {
            const char* dllName = (const char*)(baseAddress + importDesc->Name);

#if ENABLED_LOG_WIN
            std::wcout << "found IAT for  " << dllName << std::endl;
#endif

            Hookset* hookset = NULL;

            for (auto it = DllHooks.begin(); it != DllHooks.end(); ++it)
                if (!_stricmp(it->first.c_str(), dllName))
                    hookset = &it->second;

            if (hookset && importDesc->OriginalFirstThunk > 0)
            {
                IMAGE_THUNK_DATA* origFirst =
                    (IMAGE_THUNK_DATA*)(baseAddress + importDesc->OriginalFirstThunk);
                IMAGE_THUNK_DATA* first = (IMAGE_THUNK_DATA*)(baseAddress + importDesc->FirstThunk);

#if ENABLED_LOG_WIN
                std::wcout << "Hooking imports for " << dllName << std::endl;
#endif

                while (origFirst->u1.AddressOfData)
                {
                    void** IATentry = (void**)&first->u1.AddressOfData;

                    struct hook_find
                    {
                        bool operator()(const FuncitonEntry& a, const char* b)
                        {
                            return strcmp(a.function.c_str(), b) < 0;
                        }
                    };


                    if (IMAGE_SNAP_BY_ORDINAL64(origFirst->u1.AddressOfData))

                    {
                        // low bits of origFirst->u1.AddressOfData contain an ordinal
                        WORD ordinal = IMAGE_ORDINAL64(origFirst->u1.AddressOfData);

#if ENABLED_LOG_WIN
                        std::wcout << "Found ordinal import " << (uint32_t)ordinal << std::endl;
#endif

                        if (!hookset->OrdinalNames.empty())
                        {
                            if (ordinal >= hookset->OrdinalBase)
                            {
                                // rebase into OrdinalNames index
                                DWORD nameIndex = ordinal - hookset->OrdinalBase;

                                // it's perfectly valid to have more functions than names, we only
                                // list those with names - so ignore any others
                                if (nameIndex < hookset->OrdinalNames.size())
                                {
                                    const char* importName = (const char*)hookset->OrdinalNames[nameIndex].c_str();

#if ENABLED_LOG_WIN
                                    std::wcout << "Located ordinal " << (uint32_t)ordinal << " as " << importName << std::endl;
#endif

                                    auto found =
                                        std::lower_bound(hookset->FunctionHooks.begin(), hookset->FunctionHooks.end(),
                                            importName, hook_find());

                                    if (found != hookset->FunctionHooks.end() &&
                                        !strcmp(found->function.c_str(), importName) && ownmodule != module)
                                    {
                                        bool already = false;
                                        bool applied;
                                        {
                                            SCOPED_LOCK(lock);
                                            applied = ApplyHook(*found, IATentry, already);
                                        }

                                        // if we failed, or if it's already set and we're not doing a missedOrdinals
                                        // second pass, then just bail out immediately as we've already hooked this
                                        // module and there's no point wasting time re-hooking nothing
                                        if (!applied || (already && !missedOrdinals))
                                        {
#if ENABLED_LOG_WIN
                                            std::wcout<<"Stopping hooking module,"<< (int)applied<<(int)already<<
                                                (int)missedOrdinals << std::endl;
#endif
                                            FreeLibrary(refcountModHandle);
                                            return;
                                        }
                                    }
                                }
                            }
                            else
                            {
                 
                                std::wcerr << "Import ordinal is below ordinal base in %s importing module" << modName << dllName << std::endl;
                            }
                        }
                        else
                        {
#if ENABLED_LOG_WIN
                      
                            std::wcout << "missed ordinals, will try again" << std::endl;
#endif
                            missedOrdinals = true;
                        }

                        // continue
                        origFirst++;
                        first++;
                        continue;
                    }

                    IMAGE_IMPORT_BY_NAME* import =
                        (IMAGE_IMPORT_BY_NAME*)(baseAddress + origFirst->u1.AddressOfData);

                    const char* importName = (const char*)import->Name;

#if ENABLED_LOG_WIN
               
                    std::wcout << "Found normal import" << importName << std::endl;
#endif

                    auto found = std::lower_bound(hookset->FunctionHooks.begin(),
                        hookset->FunctionHooks.end(), importName, hook_find());

                    if (found != hookset->FunctionHooks.end() &&
                        !strcmp(found->function.c_str(), importName) && ownmodule != module)
                    {
                        bool already = false;
                        bool applied;
                        {
                            SCOPED_LOCK(lock);
                            applied = ApplyHook(*found, IATentry, already);
                        }

                        // if we failed, or if it's already set and we're not doing a missedOrdinals
                        // second pass, then just bail out immediately as we've already hooked this
                        // module and there's no point wasting time re-hooking nothing
                        if (!applied || (already && !missedOrdinals))
                        {
#if ENABLED_LOG_WIN
                            std::wcout << "Stopping hooking module,"<< (int)applied<<(int)already<<
                                (int)missedOrdinals << std::endl;
#endif
                            FreeLibrary(refcountModHandle);
                            return;
                        }
                    }

                    origFirst++;
                    first++;
                }
            }
            else
            {
                if (hookset)
                {
#if ENABLED_LOG_WIN
                   std::wcout << "!! Invalid IAT found for " << dllName << importDesc->OriginalFirstThunk << importDesc->FirstThunk << std::endl;
#endif
                }
            }

            importDesc++;
        }
        FreeLibrary(refcountModHandle);
	}

};


static CachedHookData* s_HookData = NULL;

#ifdef UNICODE
#undef MODULEENTRY32
#undef Module32First
#undef Module32Next
#endif

static void ForAllModules(std::function<void(const MODULEENTRY32& me32)> callback)
{
    HANDLE hModuleSnap = INVALID_HANDLE_VALUE;

    // up to 10 retries
    for (int i = 0; i < 10; i++)
    {
        hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());

        if (hModuleSnap == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            std::cout << "CreateToolhelp32Snapshot() ->" << err << std::endl;
            // retry if error is ERROR_BAD_LENGTH
            if (err == ERROR_BAD_LENGTH)
                continue;
        }

        // didn't retry, or succeeded
        break;
    }

    if (hModuleSnap == INVALID_HANDLE_VALUE)
    {
       
        std::wcerr << "Couldn't create toolhelp dump of modules in process "  << std::endl;
        return;
    }

    MODULEENTRY32 me32;
    EraseEl(me32);
    me32.dwSize = sizeof(MODULEENTRY32);

    BOOL success = Module32First(hModuleSnap, &me32);

    if (success == FALSE)
    {
        DWORD err = GetLastError();
        std::wcerr << "Couldn't get first module in process: " << err << std::endl;
        CloseHandle(hModuleSnap);
        return;
    }

    do
    {
        callback(me32);
    } while (Module32Next(hModuleSnap, &me32));

    CloseHandle(hModuleSnap);
}


static void HookAllModules()
{
    if (!s_HookData->hookAll)
        return;

    ForAllModules(
        [](const MODULEENTRY32& me32) { s_HookData->ApplyHooks(me32.szModule, me32.hModule); });


    int32_t prev = Atomic::CmpExch32(&s_HookData->posthooking, 0, 1);

    if (prev != 0)
        return;

    for (auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    {
        if (it->second.module == NULL)
            continue;

        if (!it->second.hooksfetched)
        {
            it->second.hooksfetched = true;


            for (FuncitonEntry& hook : it->second.FunctionHooks)
            {
                if (hook.orig && *hook.orig == NULL)
                    *hook.orig = GetProcAddress(it->second.module, hook.function.c_str());
            }
        }

        std::vector<FunctionCallback> callbacks;
        // don't call callbacks next time
        callbacks.swap(it->second.Callbacks);

        for (FunctionCallback cb : callbacks)
            if (cb)
                cb(it->second.module);
    }

    Atomic::CmpExch32(&s_HookData->posthooking, 1, 0);
}

static bool IsAPISet(const wchar_t* filename)
{

	return true;
}
static bool IsAPISet(const char* filename)
{
	size_t len = strlen(filename);
	wchar_t * wfn =new wchar_t[len];

	// assume ASCII not UTF, just upcast plainly to wchar_t
	for (size_t i = 0; i < len; i++)
		wfn[i] = wchar_t(filename[i]);

	return IsAPISet(wfn);
}


HMODULE WINAPI Hooked_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE fileHandle, DWORD flags)
{
    bool dohook = true;

    if (s_HookData->libraryIntercept)
    {
        LPCWSTR ent = (LPCWSTR)lpLibFileName;
        HMODULE ret = s_HookData->libraryIntercept(ent, fileHandle, flags);
        if (ret)
            return ret;
        dohook = false;
    }

    if (flags == 0 && GetModuleHandleA(lpLibFileName))
        dohook = false;

    SetLastError(S_OK);

    // we can use the function naked, as when setting up the hook for LoadLibraryExA, our own module
    // was excluded from IAT patching
  
    HMODULE mod = LoadLibraryExA(lpLibFileName, fileHandle, flags);

#if ENABLEDLOG
    std::wcout << "LoadLibraryA(" << lpLibFileName << ")" << std::endl;
#endif

    DWORD err = GetLastError();

    if (dohook && mod && !IsAPISet(lpLibFileName))
        HookAllModules();

    SetLastError(err);

    return mod;
}

HMODULE WINAPI Hooked_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE fileHandle, DWORD flags)
{
    bool dohook = true;

    if (s_HookData->libraryIntercept)
    {
        HMODULE ret =
            s_HookData->libraryIntercept(lpLibFileName, fileHandle, flags);
        if (ret)
            return ret;
        dohook = false;
    }

    if (flags == 0 && GetModuleHandleW(lpLibFileName))
        dohook = false;

    if (flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE))
        dohook = false;

    SetLastError(S_OK);

#if ENABLED_LOG_WIN
    std::wcout << "LoadLibraryA(" << lpLibFileName << ")" << std::endl;
#endif

    // we can use the function naked, as when setting up the hook for LoadLibraryExA, our own module
    // was excluded from IAT patching
    HMODULE mod = LoadLibraryExW(lpLibFileName, fileHandle, flags);

    DWORD err = GetLastError();

    if (dohook && mod && !IsAPISet(lpLibFileName))
        HookAllModules();

    SetLastError(err);

    return mod;
}

HMODULE WINAPI Hooked_LoadLibraryA(LPCSTR lpLibFileName)
{
    return Hooked_LoadLibraryExA(lpLibFileName, NULL, 0);
}

HMODULE WINAPI Hooked_LoadLibraryW(LPCWSTR lpLibFileName)
{
    return Hooked_LoadLibraryExW(lpLibFileName, NULL, 0);
}

static bool OrdinalAsString(void* func)
{
    return uint64_t(func) <= 0xffff;
}

FARPROC WINAPI Hooked_GetProcAddress(HMODULE mod, LPCSTR func)
{
    if (mod == NULL || func == NULL || mod == s_HookData->ownmodule)
        return GetProcAddress(mod, func);

#if ENABLED_LOG_WIN
    std::wcout << "Hooked_GetProcAddress(" << mod<<" ," << func << ")" << std::endl;
#endif

    for (auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
    {
        if (it->second.module == NULL)
        {
            it->second.module = GetModuleHandleA(it->first.c_str());
            if (it->second.module)
            {
                // fetch all function hooks here, since we want to fill out the original function pointer
                // even in case nothing imports from that function (which means it would not get filled
                // out through FunctionHook::ApplyHook)
                for (FuncitonEntry& hook : it->second.FunctionHooks)
                {
                    if (hook.orig && *hook.orig == NULL)
                        *hook.orig = GetProcAddress(it->second.module, hook.function.c_str());
                }

                it->second.FetchOrdinalNames();
            }
        }

        bool match = (mod == it->second.module);

        if (!match && !it->second.altmodules.empty())
        {
            for (size_t i = 0; !match && i < it->second.altmodules.size(); i++)
                match = (mod == it->second.altmodules[i]);
        }

        if (match)
        {
#if ENABLED_LOG_WIN
          
            std::wcout << "Located module " << it->first.c_str() << std::endl;
#endif

            if (OrdinalAsString((void*)func))
            {
#if ENABLED_LOG_WIN
                std::wcout << "Ordinal hook " << std::endl;
#endif

                uint32_t ordinal = (uint16_t)(uintptr_t(func) & 0xffff);

                if (ordinal < it->second.OrdinalBase)
                {
                    std::wcerr << "Unexpected ordinal - lower than ordinalbase  " << (uint32_t)it->second.OrdinalBase 
                        << it->first.c_str() << std::endl;
                  
                    SetLastError(S_OK);
                    return GetProcAddress(mod, func);
                }

                ordinal -= it->second.OrdinalBase;

                if (ordinal >= it->second.OrdinalNames.size())
                {
                    std::wcerr << "Unexpected ordinal - higher than fetched ordinal names   " <<
                        (uint32_t)it->second.OrdinalNames.size()
                        << it->first.c_str() << std::endl;
    

                    SetLastError(S_OK);
                    return GetProcAddress(mod, func);
                }

                func = it->second.OrdinalNames[ordinal].c_str();

#if ENABLED_LOG_WIN
               
                std::wcout << "found ordinal "<<func << std::endl;
#endif
            }

            FuncitonEntry search(func, NULL, NULL);

            auto found =
                std::lower_bound(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end(), search);
            if (found != it->second.FunctionHooks.end() && !(search < *found))
            {
                FARPROC realfunc = GetProcAddress(mod, func);

#if ENABLED_LOG_WIN
           
                std::wcout << "Found hooked function, returning hook pointer  " << found->hook << std::endl;
#endif

                SetLastError(S_OK);

                if (realfunc == NULL)
                    return NULL;

                return (FARPROC)found->hook;
            }
        }
    }

#if ENABLED_LOG_WIN
    std::wcout << "No matching hook found, returning original  " << std::endl;
#endif

    SetLastError(S_OK);

    return GetProcAddress(mod, func);
}
static void InitHookData()
{
    if (!s_HookData)
    {
        s_HookData = new CachedHookData;

       
        s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
            FuncitonEntry("LoadLibraryA", NULL, &Hooked_LoadLibraryA));
        s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
            FuncitonEntry("LoadLibraryW", NULL, &Hooked_LoadLibraryW));
        s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
            FuncitonEntry("LoadLibraryExA", NULL, &Hooked_LoadLibraryExA));
        s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
            FuncitonEntry("LoadLibraryExW", NULL, &Hooked_LoadLibraryExW));
        s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(
            FuncitonEntry("GetProcAddress", NULL, &Hooked_GetProcAddress));

        for (const char* apiset :
            { "api-ms-win-core-libraryloader-l1-1-0.dll", "api-ms-win-core-libraryloader-l1-1-1.dll",
             "api-ms-win-core-libraryloader-l1-1-2.dll", "api-ms-win-core-libraryloader-l1-2-0.dll",
             "api-ms-win-core-libraryloader-l1-2-1.dll" })
        {
            s_HookData->DllHooks[apiset].FunctionHooks.push_back(
                FuncitonEntry("LoadLibraryA", NULL, &Hooked_LoadLibraryA));
            s_HookData->DllHooks[apiset].FunctionHooks.push_back(
                FuncitonEntry("LoadLibraryW", NULL, &Hooked_LoadLibraryW));
            s_HookData->DllHooks[apiset].FunctionHooks.push_back(
                FuncitonEntry("LoadLibraryExA", NULL, &Hooked_LoadLibraryExA));
            s_HookData->DllHooks[apiset].FunctionHooks.push_back(
                FuncitonEntry("LoadLibraryExW", NULL, &Hooked_LoadLibraryExW));
            s_HookData->DllHooks[apiset].FunctionHooks.push_back(
                FuncitonEntry("GetProcAddress", NULL, &Hooked_GetProcAddress));
        }

        GetModuleHandleEx(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCTSTR)&s_HookData, &s_HookData->ownmodule);
    }
}
void LibraryHooks::RegisterHookedLibraryFunction(const char* libraryName, const FuncitonEntry& hook)
{
    if (!_stricmp(libraryName, "kernel32.dll"))
    {
        if (hook.function == "LoadLibraryA" || hook.function == "LoadLibraryW" ||
            hook.function == "LoadLibraryExA" || hook.function == "LoadLibraryExW" ||
            hook.function == "GetProcAddress")
        {
            std::wcerr << "Cannot hook LoadLibrary* or GetProcAddress, as these are hooked internally" << std::endl;
            return;
        }
    }
    s_HookData->DllHooks[str_to_lower(libraryName)].FunctionHooks.push_back(hook);
}

void LibraryHooks::RegisterHookedLibrary(const char* libraryName, FunctionCallback loadedCallback)
{
    s_HookData->DllHooks[str_to_lower(libraryName)].Callbacks.push_back(loadedCallback);
}


void LibraryHooks::BeginHookRegistration()
{
    InitHookData();
}

void LibraryHooks::EndHookRegistration()
{
    for (auto it = s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
        std::sort(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end());
    HookAllModules();
    if (s_HookData->missedOrdinals)
    {
#if ENABLED_LOG_WIN
        std::wcout << "Missed ordinals - applying hooks again" << std::endl;
#endif

        // we need to do a second pass now that we know ordinal names to finally hook
        // some imports by ordinal only.
        HookAllModules();

        s_HookData->missedOrdinals = false;
    }
}
bool LibraryHooks::Detect(const char* identifier)
{
    bool ret = false;
    ForAllModules([&ret, identifier](const MODULEENTRY32& me32) {
        if (GetProcAddress(me32.hModule, identifier) != NULL)
            ret = true;
        });
    return ret;
}