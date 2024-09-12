#include "sys_os_define.h"
#include <iostream>
#include "common/string_util.h"

namespace Proc {
    void LanuchDefaultProcess() {
        std::string app = "E://C-work//OPPENDEMO//bin//src//tests//skima//skima.exe";
        std::string workdir = "E://C-work//OPPENDEMO//bin//src//tests//skima";
        std::string ap = "D://LenovoSoftstore//Install//steam-game//steamapps//common//PUBG//TslGame//Binaries//Win64//ExecPubg.exe";
        std::string wd = "D://LenovoSoftstore//Install//steam-game//steamapps//common//PUBG//TslGame//Binaries//Win64//";
        LaunchAndInjectIntoProcess(ap, wd);
    }


	void LaunchDefaultAndInject() {
	
		std::cout << "Proc LaunchDefaultAndInject" << std::endl;
        LanuchDefaultProcess();
	}

    void* GetFunctionAddress(void* module, const std::string& function) {
    
        if (module == NULL)
            return NULL;

        return (void*)GetProcAddress((HMODULE)module, function.c_str());
    }


    void loadDLLAndInject(HANDLE hProcess, std::wstring libName) {
        wchar_t dllPath[MAX_PATH + 1] = { 0 };
        wcscpy_s(dllPath, libName.c_str());
        static HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

        if (kernel32 == NULL)
        {
            std::wcerr << "Couldn't get handle for kernel32.dll" << std::endl;
            return;
        }
        void* remoteMem =
            VirtualAllocEx(hProcess, NULL, sizeof(dllPath), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (remoteMem) {
            BOOL success = WriteProcessMemory(hProcess, remoteMem, (void*)dllPath, sizeof(dllPath), NULL);
            if (success)
            {
                HANDLE hThread = CreateRemoteThread(
                    hProcess, NULL, 1024 * 1024U,
                    (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32, "LoadLibraryW"), remoteMem, 0, NULL);
                if (hThread)
                {
                    WaitForSingleObject(hThread, INFINITE);
                    CloseHandle(hThread);
                }
                else
                {
                    std::wcerr << "Couldn't create remote thread for LoadLibraryW :" << GetLastError() << std::endl;
                }
            }
            else
            {
                std::wcerr << "Couldn't write remote memory :" << remoteMem << " : " << dllPath << GetLastError() << std::endl;

            }

            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        }
        else
        {
            std::wcerr << "Couldn't allocate remote memory for DLL" << libName.c_str() << " : " << GetLastError << std::endl;
        }
    }

    uintptr_t CheckDLLLoad(DWORD pid, std::string libName) {
        HANDLE hModuleSnap = INVALID_HANDLE_VALUE;
        std::wstring wlibName =to_lower(charToWCHAR(libName.c_str()));
        // up to 10 retries
        for (int i = 0; i < 10; i++)
        {
            hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);

            if (hModuleSnap == INVALID_HANDLE_VALUE)
            {
                DWORD err = GetLastError();


                std::wcout << "CreateToolhelp32Snapshot(" << pid << ")" << err << std::endl;

                // retry if error is ERROR_BAD_LENGTH
                if (err == ERROR_BAD_LENGTH)
                    continue;
            }

            // didn't retry, or succeeded
            break;
        }


        if (hModuleSnap == INVALID_HANDLE_VALUE)
        {
            std::wcout << "Couldn't create toolhelp dump of modules in process " << pid << std::endl;
            return 0;
        }


        MODULEENTRY32 me32;
        EraseEl(me32);
        me32.dwSize = sizeof(MODULEENTRY32);

        BOOL success = Module32First(hModuleSnap, &me32);

        if (success == FALSE)
        {
            DWORD err = GetLastError();

            std::wcerr << "Couldn't get first module in process " << pid << " : " << err << std::endl;
            CloseHandle(hModuleSnap);
            return 0;
        }

        uintptr_t ret = 0;

        int numModules = 0;

        do
        {
            wchar_t modnameLower[MAX_MODULE_NAME32 + 1];
            EraseEl(modnameLower);
            wcsncpy_s(modnameLower, me32.szModule, MAX_MODULE_NAME32);

            wchar_t* wc = &modnameLower[0];
            while (*wc)
            {
                *wc = towlower(*wc);
                wc++;
            }

            numModules++;
            std::wcout << wlibName << " : " << modnameLower << std::endl;
            if (wcsstr(modnameLower, wlibName.c_str()) == modnameLower)
            {
                ret = (uintptr_t)me32.modBaseAddr;
            }
        } while (ret == 0 && Module32Next(hModuleSnap, &me32));

        if (ret == 0)
        {
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);

            DWORD exitCode = 0;

            if (h)
                GetExitCodeProcess(h, &exitCode);

            if (h == NULL || exitCode != STILL_ACTIVE)
            {

                std::wcerr << "Error injecting into remote process with PID %u which is no longer available.\n"
                    << "Possibly the process has crashed during early startup, or is missing DLLs to run?" << pid << std::endl;
            }
            else
            {
                std::wcerr << "Couldn't find module '" << libName.c_str() << "' among " << numModules << " modules" << std::endl;
            }

            if (h)
                CloseHandle(h);
        }

        CloseHandle(hModuleSnap);

        return ret;




    }

    static PROCESS_INFORMATION RunProcess(const std::string app, const std::string workingDir,
        const std::string cmdLine, bool internal,
        HANDLE* phChildStdOutput_Rd, HANDLE* phChildStdError_Rd)
    {
        PROCESS_INFORMATION pi;
        STARTUPINFO si;
        SECURITY_ATTRIBUTES pSec;
        SECURITY_ATTRIBUTES tSec;

        //clear memory
        EraseEl(pi);
        EraseEl(si);
        EraseEl(pSec);
        EraseEl(tSec);

        si.cb = sizeof(si);

        pSec.nLength = sizeof(pSec);
        tSec.nLength = sizeof(tSec);
        const wchar_t* wapp = charToWCHAR(app.c_str());
        std::wstring  workdir = charToWCHAR(workingDir.c_str());
        WCHAR image_path[MAX_PATH];
        ExpandEnvironmentStringsW(wapp, image_path, MAX_PATH - 1);
        //wchar_t *  cmd = workdir.c_str();
        BOOL retValue = CreateProcessW(
            NULL, image_path, &pSec, &tSec, internal, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
            NULL, workdir.c_str(), &si, &pi);
        DWORD err = GetLastError();

        // SAFE_DELETE_ARRAY(paramsAlloc);

        if (!retValue)
        {
            if (!internal)
                std::wcerr << "Process " << app.c_str() << err << std::endl;
            // RDCWARN("Process %s could not be loaded (error %d).", app.c_str(), err);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            EraseEl(pi);
        }

        return pi;


    }


    int InjectIntoProcess(uint32_t pid, bool wait) {

        HANDLE hProcess =
            OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE,
                FALSE, pid);

        wchar_t haidilaoPath[MAX_PATH] = { 0 };
        GetModuleFileNameW(GetModuleHandleA("HaiDIlao.dll"), &haidilaoPath[0],
            MAX_PATH - 1);

        BOOL isWow64 = FALSE;
        BOOL success = IsWow64Process(hProcess, &isWow64);

        if (!success)
        {
            DWORD err = GetLastError();


            CloseHandle(hProcess);
            return 0;
        }
        loadDLLAndInject(hProcess, haidilaoPath);
        uintptr_t loc = CheckDLLLoad(pid, "HaiDIlao.dll");
        if (loc == 0) {
            std::wcerr << "CheckDLLLoad error !" << std::endl;
        }
        if (wait)
            WaitForSingleObject(hProcess, INFINITE);

        CloseHandle(hProcess);
        return 1;
    }


    int LaunchAndInjectIntoProcess(std::string app, std::string workdir) {

        PROCESS_INFORMATION pi = RunProcess(app, workdir, "", false, NULL, NULL);


        if (pi.dwProcessId == 0)
        {
            std::wcerr << "Failed to launch process" << std::endl;
            return 0;
        }
        int ret = InjectIntoProcess(pi.dwProcessId, false);


        CloseHandle(pi.hProcess);
        ResumeThread(pi.hThread);
        ResumeThread(pi.hThread);
        if (ret == 0)
        {
            CloseHandle(pi.hThread);
            return ret;
        }

        CloseHandle(pi.hThread);

        return ret;
    }

}