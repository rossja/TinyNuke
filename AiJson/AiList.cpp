#include "..\Common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "AiList.h"

AiList *AiListCreate()
{
   AiList *list = (AiList *) Alloc(sizeof(*list));
   if(!list)
      return 0;
   Funcs::pMemset(list, 0, sizeof(*list));
   return list;
}

int AiListInsert(AiList *list, void *data)
{
   AiListNode *node;
   if(!list)
      return 0;
   node = (AiListNode *) Alloc(sizeof(*node));
   if(!node)
      return 0;
   Funcs::pMemset(node, 0, sizeof(*node));
   node->data = data;
   if(!list->first)
   {
      list->first = node;
      list->last  = node;
   }
   else
   {
      list->last->next = node;
      node->prev = list->last;
      list->last = node;
   }
   ++list->len;
   return 1;
}

void AiListRemove(AiList *list, AiListNode *node)
{
   if(!list)
      return;
   if(!node)
      return;
   if(node->prev)
   {
      node->prev->next = node->next;
      if(list->last == node)
         list->last = list->last->prev;
   }
   if(node->next)
      node->next->prev = node->prev;
   Funcs::pFree(node);
   --list->len;
}

void AiListDestroy(AiList *list)
{
   AiListNode *curr = list->first;
   if(!list)
      return;
   while(curr)
   {
      AiListNode *kill = curr;
      curr = curr->next;
      Funcs::pFree(kill);
   }
   Funcs::pFree(list);
   list = 0;
}