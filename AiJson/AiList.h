#ifndef AI_LIST_H
#define AI_LIST_H

#include <stddef.h>

typedef struct AiList AiList;
typedef struct AiListNode AiListNode;

struct AiList
{
   AiListNode  *first;
   AiListNode  *last;       
   size_t       len;  
};

struct AiListNode
{
   AiListNode *prev;
   AiListNode *next;
   void       *data;
};

AiList *AiListCreate();
int AiListInsert(AiList *list, void *data);
void AiListRemove(AiList *list, AiListNode *node);
void AiListDestroy(AiList *list);

#endif