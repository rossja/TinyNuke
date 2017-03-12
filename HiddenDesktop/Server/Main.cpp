#include "Common.h"
#include "ControlWindow.h"
#include "Server.h"

#define majVer 1
#define minVer 0

int CALLBACK WinMain(HINSTANCE hInstance,
   HINSTANCE hPrevInstance,
   LPSTR lpCmdLine,
   int nCmdShow)
{
   AllocConsole();

   freopen("CONIN$", "r", stdin); 
   freopen("CONOUT$", "w", stdout); 
   freopen("CONOUT$", "w", stderr); 

   SetConsoleTitle(TEXT("Hidden Desktop"));
  
   wprintf(TEXT("Version: %d.%d\n"), majVer, minVer);
   wprintf(TEXT("Compiled: %S @ %S\n"), __DATE__, __TIME__);

   if(!StartServer(atoi(lpCmdLine)))
   {
      wprintf(TEXT("Could not start the server (Error: %d)\n"), WSAGetLastError()); 
      getchar();
      return 0;
   }
   return 0;
}