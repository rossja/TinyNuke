#pragma once
#include "Common.h"

struct HttpRequestData
{
   BOOL        post;
   int         port;
   char       *host;
   char       *path;
   BYTE       *inputBody;
   int         inputBodySize;
   BYTE       *outputBody;
   int         outputBodySize;
};

BOOL HttpSubmitRequest(HttpRequestData &httpRequestData);