#include "Server.h"

int main(int argc, char **argv)
{
   if(argc < 1)
      wprintf(L"Port not provided\n");
   else if(!ReverseSocksServer::Start(atoi(argv[1])))
      wprintf(L"Could not start the server (Error: %d)\n", WSAGetLastError()); 
   getchar();
   return 0;
}