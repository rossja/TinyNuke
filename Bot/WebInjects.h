#pragma once
#include "..\Common.h"
#include "..\Panel.h"
#include "..\AiJson\AiJson.h"

void LoadWebInjects();
AiList *GetWebInject(char *host, char *path);
void ReplaceWebInjects(char **buffer, AiList *injects);
BOOL UrlIsBlacklisted(char *url);