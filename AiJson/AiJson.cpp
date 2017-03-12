#ifdef _MSC_VER
   #define _CRT_SECURE_NO_WARNINGS
#endif

#include "..\Common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "AiJson.h"
#include "AiStrAppender.h"

#define STR_TRUE  "true"
#define STR_FALSE "false"
#define STR_NULL  "null"

#define CHAR_FIRST_TRUE  't'
#define CHAR_FIRST_FALSE 'f'
#define CHAR_FIRST_NULL  'n'

#define CHAR_OBJECT_OPEN     '{'
#define CHAR_OBJECT_CLOSE    '}'
#define CHAR_ARRAY_OPEN      '['
#define CHAR_ARRAY_CLOSE     ']'
#define CHAR_NAME_SEPARATOR  ':'
#define CHAR_VALUE_SEPARATOR ','
#define CHAR_STR_OPEN_CLOSE  '"'
#define CHAR_EXPONENT        'e'
#define CHAR_DECIMAL         '.'
#define CHAR_PLUS            '+'
#define CHAR_MINUS           '-'
#define CHAR_ESCAPE_SIGNAL   '\\'
#define CHAR_UNICODE_CTRL    'u'

#define NUM_UNICODE_CTRL_DIGITS 4

#define NUM_ESCAPE_SYMBOLS 7

const char escapeSymbols[NUM_ESCAPE_SYMBOLS][2] =
{
   { '"', '"' },
   { '\\', '\\' },
   { 'b', '\b' },
   { 'f', '\f' },
   { 'n', '\n' },
   { 'r', '\r' },
   { 't', '\t' },
};

//if I used C99 I could have used [AI_JSON_E_...] = .. so you could reorder the enum
const char *errorStr[] =
{
#ifdef _DEBUG
   "Parsing successful",
   "Unexpected symbol",
   "Unexpected EOF",
   "Invalid hexadecimal digit",
   "Invalid escape sequence",
   "Character must be escaped",
   "Invalid number",
   "Expected array close",
   "Expected a name",
   "Expected name separator",
   "Expected object close",
   "Unknown error",
   "Failed to allocate",
   "Invalid value"
#else
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
   "",
#endif
};

typedef enum  ParserSymbol ParserSymbol;

enum ParserSymbol
{
   S_WHITESPACE,
   S_OBJECT_OPEN,
   S_OBJECT_CLOSE,
   S_ARRAY_OPEN,
   S_ARRAY_CLOSE,
   S_NAME_SEPARATOR,
   S_VALUE_SEPARATOR,
   S_STR_OPEN_CLOSE,
   S_OTHER
};

AiJsonError ParseArray(AiJson *json, char **str, AiList *array);
AiJsonError ParseObject(AiJson *json, char **str, AiList *object);

AiList *JsonListCreate(AiJson *json)
{
   AiList *list = AiListCreate();
   if(!list)
      return 0;
   if(!AiListInsert(json->_private.listList, list))
      return 0;
   return list;
}

void *JsonMalloc(AiJson *json, size_t size)
{
   void *mem = Alloc(size);
   if(!mem)
      return 0;
   if(!AiListInsert(json->_private.mallocList, mem))
   {
      Funcs::pFree(mem);
      return 0;
   }
   return mem;
}

ParserSymbol ParserLexer(char c)
{
   switch(c)
   {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
         return S_WHITESPACE;
      case CHAR_OBJECT_OPEN:
         return S_OBJECT_OPEN;
      case CHAR_OBJECT_CLOSE:
         return S_OBJECT_CLOSE;
      case CHAR_ARRAY_OPEN:
         return S_ARRAY_OPEN;
      case CHAR_ARRAY_CLOSE:
         return S_ARRAY_CLOSE;
      case CHAR_STR_OPEN_CLOSE:
         return S_STR_OPEN_CLOSE;
      case CHAR_NAME_SEPARATOR:
         return S_NAME_SEPARATOR;
      case CHAR_VALUE_SEPARATOR:
         return S_VALUE_SEPARATOR;
   }
   return S_OTHER;   
}

//Used to keep track of line and column
char IncreaseStr(AiJson *json, char **str, int increase)
{
   int i;
   for(i = 0; i < increase; ++i)
   {
      switch(**str)
      {
         case '\n':
         {
            ++json->line;
            json->column = 1;
            break;
         }
         case '\r':
            break;
         default:
         {
            ++json->column;
            break;
         }
      }
      (*str)++;
   }
   return *(*str - 1);  
}

void SkipWspaces(AiJson *json, char **str)
{
   while(ParserLexer(**str) == S_WHITESPACE)
   {
      IncreaseStr(json, str, 1);
   }
}

AiJsonError ParseString(AiJson *json, char **str, char **out)
{
   size_t i;
   size_t x;
   IncreaseStr(json, str, 1);
   for(x = 0;;++x)
   {
      if((*str)[x] == CHAR_STR_OPEN_CLOSE)
         break;
      if((*str)[x] == CHAR_ESCAPE_SIGNAL)
         ++x;
      if(!(*str)[x])
         return AI_JSON_E_UNEXPECTED_EOF;  
   }
   *out = (char *) JsonMalloc(json, x + 1);
   (*out)[x] = 0;
   if(!*out)
      return AI_JSON_E_ALLOC;
   for(i = 0; i < x; ++i)
   {
      if(**str == CHAR_ESCAPE_SIGNAL)
      {
         IncreaseStr(json, str, 1);
         if(**str)
         {
            int j;
            for(j = 0; j < NUM_ESCAPE_SYMBOLS; ++j)
            {
               if(**str == escapeSymbols[j][0])
               {
                  (*out)[i] = escapeSymbols[j][1];
                  goto exit;
               }
            }
            if(**str == CHAR_UNICODE_CTRL)
            {
               char  hex[NUM_UNICODE_CTRL_DIGITS + 1];
               int   j = 0;
               int   codePoint;
               char *end;
               for(j = 0; j < NUM_UNICODE_CTRL_DIGITS; ++j)
               {
                  hex[j] = IncreaseStr(json, str, 1);
                  if(!hex[j] || !Funcs::pIsxdigit(hex[j]))
                     return AI_JSON_E_INVALID_HEX_DIGIT;
               }
               hex[j] = 0;

               (*Funcs::pErrno()) = 0;
               codePoint = Funcs::pStrtol(hex, &end, 16);
               if((*Funcs::pErrno()) != 0 || hex == end)
                  return AI_JSON_E_UNKNOWN;

               if(codePoint <= 0x7F)
                  (*out)[i] = codePoint;   
               else if(codePoint <= 0x7FF)
               {
                  (*out)[i]   = 0xC0 | (codePoint >> 6);
                  (*out)[++i] = 0x80 | (codePoint & 0x3F); 
               }
               else if(codePoint <= 0xFFFF)
               {
                  (*out)[i]   = 0xE0 | (codePoint >> 12);
                  (*out)[++i] = 0x80 | (codePoint >> 6) & 0x3F;
                  (*out)[++i] = 0x80 | (codePoint & 0x3F);
               }  
            }
            else
               return AI_JSON_E_INVALID_ESCAPE_SEQUENCE;  
         }
         else
            return AI_JSON_E_UNEXPECTED_EOF;
      }
      else if(**str == '"')
      {
         (*out)[i] = 0;
         break;
      }
      else
      {
         int j;
         for(j = 0; j < NUM_ESCAPE_SYMBOLS; ++j)
         {
            if(**str == escapeSymbols[j][1])
               return AI_JSON_E_CHAR_MUST_BE_ESCAPED;
         }
         (*out)[i] = **str;
      }
exit:
      IncreaseStr(json, str, 1);
   }
   IncreaseStr(json, str, 1);
   return AI_JSON_E_OK;
}

AiJsonError ParseNumberNextCharCheck(int i, char curr, char next, int exponent, int decimal)
{
   if(!curr && (Funcs::pIsdigit(next) || next == CHAR_MINUS)) 
      return AI_JSON_E_OK;    
   else if(curr == CHAR_DECIMAL && Funcs::pIsdigit(next))
      return AI_JSON_E_OK;  
   else if(curr == CHAR_EXPONENT && (Funcs::pIsdigit(next) || next == CHAR_PLUS || next == CHAR_MINUS))
      return AI_JSON_E_OK;
   else if((curr == CHAR_MINUS || curr == CHAR_PLUS) && (Funcs::pIsdigit(next)))
      return AI_JSON_E_OK;
   //JSON doesn't allow an integer to start with a zero because in JavaScript an integer starting with a zero is in base eight
   else if(!(!exponent && curr == '0' && i == 2 && next != CHAR_DECIMAL))
   {
      if(Funcs::pIsdigit(curr) && (Funcs::pIsdigit(next) || (!exponent && next == CHAR_EXPONENT || (next == CHAR_DECIMAL && !decimal))))
         return AI_JSON_E_OK;
   }
   return AI_JSON_E_INVALID_NUM; 
}

AiJsonError ParseNumber(AiJson *json, char **str, AiJsonValue *value)
{
   char  *numStart = *str;
   char  *num;
   char   curr     = 0;
   char   next     = *(*str)++;
   int    exponent = 0;
   int    decimal  = 0;
   size_t i;
   AiJsonError  err;

   for(i = 1;; ++i)
   {
      err = ParseNumberNextCharCheck(i, curr, next, exponent, decimal);
      if(err != AI_JSON_E_OK)
         return err;
      curr = next;
      next = Funcs::pTolower(IncreaseStr(json, str, 1));
      if(curr == CHAR_EXPONENT)
         exponent = 1;
      else if(curr == CHAR_DECIMAL)
         decimal = 1;
      if(ParserLexer(next) != S_OTHER)
      {
         (*str)--;
         break;
      }  
   }

   num = (char *) JsonMalloc(json, i + 1);
   if(!num)
      return AI_JSON_E_ALLOC;
   num[i] = 0;
   Funcs::pMemcpy(num, numStart, i);

   (*Funcs::pErrno()) = 0;
   if(decimal)
   {
      value->type     = AI_JSON_DOUBLE;
      value->data.dbl = Funcs::pStrtod(num, &numStart);
   }
   else
   {
      value->type         = AI_JSON_SLONG;
      value->data.sLong   = Funcs::pStrtol(num, &numStart, 10);
      if((*Funcs::pErrno()) != 0 || num == numStart)
      {  
         (*Funcs::pErrno()) = 0;
         value->type       = AI_JSON_ULONG;
         value->data.uLong = Funcs::pStrtoul(num, &numStart, 10); 
      }
   }
   if((*Funcs::pErrno()) != 0 || num == numStart)
   {
      value->type = AI_JSON_STRING;
      value->data.string = num;        
   }
   return AI_JSON_E_OK;
}

AiJsonError ParseValue(AiJson *json, char **str, AiJsonValue *value)
{
   AiJsonError err;
   switch(**str)
   {
      case CHAR_STR_OPEN_CLOSE:
      {
         char *data;
         value->type = AI_JSON_STRING;
         err = ParseString(json, str, &data);
         if(err != AI_JSON_E_OK)
            return err;
         value->data.string = data;
         break;
      }
      case CHAR_OBJECT_OPEN:
      {
         value->type = AI_JSON_OBJECT;
         value->data.object = JsonListCreate(json);
         if(!value->data.object)
            return AI_JSON_E_ALLOC;
         err = ParseObject(json, str, value->data.array);
         if(err != AI_JSON_E_OK)
            return err;
         break;
      }
      case CHAR_ARRAY_OPEN:
      {
         value->type = AI_JSON_ARRAY;
         value->data.array = JsonListCreate(json);
         if(!value->data.array)
            return AI_JSON_E_ALLOC;
         err = ParseArray(json, str, value->data.array);
         if(err != AI_JSON_E_OK)
            return err;
         break;
      }
      case CHAR_FIRST_TRUE:
      case CHAR_FIRST_FALSE:
      {
         value->type = AI_JSON_BOOL;
         if(!Funcs::pStrncmp(*str, STR_TRUE, sizeof(STR_TRUE) - 1))
         {
            value->data.boolean = 1;
            IncreaseStr(json, str, sizeof(STR_TRUE) - 1);
         }
         else if(!Funcs::pStrncmp(*str, STR_FALSE, sizeof(STR_FALSE) - 1))
         {
            value->data.boolean = 0;
            IncreaseStr(json, str, sizeof(STR_FALSE) - 1);
         }
         else
            return AI_JSON_E_INVALID_VALUE;
         break; 
      }
      case CHAR_FIRST_NULL:
      {
         value->type = AI_JSON_NULL;
         if(Funcs::pStrncmp(*str, STR_NULL, sizeof(STR_NULL) - 1))
            return AI_JSON_E_INVALID_VALUE;
         IncreaseStr(json, str, sizeof(STR_NULL) - 1);
         break;
      }
      default:
      {
         err = ParseNumber(json, str, value);
         if(err != AI_JSON_E_OK)
            return err;
         break;
      }
   }
   return AI_JSON_E_OK;
}

AiJsonError ParseArray(AiJson *json, char **str, AiList *array)
{
   IncreaseStr(json, str, 1);
   SkipWspaces(json, str);
   if(ParserLexer(**str) == S_ARRAY_CLOSE)
   {
      IncreaseStr(json, str, 1);
      return AI_JSON_E_OK;
   }

   for(;;)
   {
      AiJsonValue *value;
      AiJsonError  err;
      if(!(value = (AiJsonValue *) JsonMalloc(json, sizeof(*value))))
         return AI_JSON_E_ALLOC;
      
      AiListInsert(array, value);
      
      SkipWspaces(json, str);
      err = ParseValue(json, str, value);
      if(err != AI_JSON_E_OK)
         return err;

      SkipWspaces(json, str);
      if(ParserLexer(**str) != S_VALUE_SEPARATOR)
         break;
      IncreaseStr(json, str, 1);
   }
   SkipWspaces(json, str);
   if(ParserLexer(**str) != S_ARRAY_CLOSE)
      return AI_JSON_E_EXPECTED_ARRAY_CLOSE;
   IncreaseStr(json, str, 1);
   return AI_JSON_E_OK; 
}

AiJsonError ParseObject(AiJson *json, char **str, AiList *object)
{
   IncreaseStr(json, str, 1);
   SkipWspaces(json, str);
   if(ParserLexer(**str) == S_OBJECT_CLOSE)
   {
      IncreaseStr(json, str, 1);
      return AI_JSON_E_OK;
   }

   for(;;)
   {
      AiJsonObjectField *field;
      AiJsonError        err;

      if(!(field = (AiJsonObjectField *) JsonMalloc(json, sizeof(*field))))
         return AI_JSON_E_ALLOC;
      Funcs::pMemset(field, 0, sizeof(*field));

      AiListInsert(object, field);

      SkipWspaces(json, str);
      if(**str != CHAR_STR_OPEN_CLOSE)
         return AI_JSON_E_EXPECTED_NAME;
      err = ParseString(json, str, &field->name);
      if(err != AI_JSON_E_OK)
         return err;

      SkipWspaces(json, str);
      if(ParserLexer(**str) != S_NAME_SEPARATOR)
         return AI_JSON_E_EXPECTED_NAME_SEPARATOR;
      IncreaseStr(json, str, 1);

      SkipWspaces(json, str);
      err = ParseValue(json, str, &field->value);
      if(err != AI_JSON_E_OK)
         return err;

      SkipWspaces(json, str);
      if(ParserLexer(**str) != S_VALUE_SEPARATOR)
         break;
      IncreaseStr(json, str, 1); 
   }
   SkipWspaces(json, str);
   if(ParserLexer(**str) != S_OBJECT_CLOSE)
      return AI_JSON_E_EXPECTED_OBJECT_CLOSE;
   IncreaseStr(json, str, 1);
   return AI_JSON_E_OK; 
}

AiJson *JsonCreate()
{  
   AiJson *json       = 0;
   AiList *listList   = 0;
   AiList *mallocList = 0;
   AiList *root       = 0;

   json = (AiJson *) Alloc(sizeof(*json));
   if(!json)
      goto err;

   //People use calloc but I have the habit of using malloc
   Funcs::pMemset(json, 0, sizeof(*json)); 
   json->line   = 1;
   json->column = 1;

   listList = AiListCreate();
   json->_private.listList = listList;
   if(!json->_private.listList)
      goto err;

   mallocList = AiListCreate();
   json->_private.mallocList = mallocList;
   if(!json->_private.mallocList)
      goto err;
   
   root                   = JsonListCreate(json);
   json->root.data.object = root; //since this is a union will work for arrays too of course
   if(!json->root.data.object)
      goto err;

   return json;
err:
   Funcs::pFree(json);
   AiListDestroy(root);
   AiListDestroy(listList);
   AiListDestroy(mallocList);
   return 0;
}

AiJson *AiJsonParse(char *str)
{
   AiJson      *json;
   const char   utf8bom[] = "\xEF\xBB\xBF";

   if(!str)
      return 0;

   json = JsonCreate();
   if(!json)
      return 0;

   if(!Funcs::pStrncmp(str, utf8bom, sizeof(utf8bom) - 1))
      str += sizeof(utf8bom) - 1;

   SkipWspaces(json, &str);
   if(ParserLexer(*str) == S_OBJECT_OPEN)
   {
      json->root.type = AI_JSON_OBJECT;
      json->error     = ParseObject(json, &str, json->root.data.object);
   }      
   else if(ParserLexer(*str) == S_ARRAY_OPEN)
   {
      json->root.type = AI_JSON_ARRAY;
      json->error     = ParseArray(json, &str, json->root.data.array);
   }

   if(json->error == AI_JSON_E_OK)
   {
      SkipWspaces(json, &str);
      if(*str)
         json->error = AI_JSON_E_UNEXPECTED_SYMBOL;
   }
   json->errorMsg = (char *) errorStr[json->error];
   return json;
}

AiJsonValue *ReturnOrRemoveValue(AiList *list, AiListNode *node, AiJsonValue *value, int remove)
{
   if(remove)
   {
      AiListRemove(list, node);
      return 0;
   }
   return value;
} 

AiJsonValue *ReturnOrRemoveValueObject(AiList *object, char *name, int remove)
{
   AiListNode *curr;

   if(!object || !name)
      return 0; 

   curr = object->first;
   while(curr)
   {
      AiJsonObjectField *field = (AiJsonObjectField *) curr->data;
      if(!Funcs::pLstrcmpA(field->name, name))
         return ReturnOrRemoveValue(object, curr, &field->value, remove);
      curr = curr->next;
   }
   return 0;  
}

void AiJsonRemoveValueObject(AiList *object, char *name)
{
   ReturnOrRemoveValueObject(object, name, 1);
}

AiJsonValue *AiJsonGetValueObject(AiList *object, char *name)
{
   return ReturnOrRemoveValueObject(object, name, 0);
}

int AiJsonInsertValueObject(AiJson *json, AiList *object, char *name, AiJsonValue *value)
{
   AiJsonObjectField *field;
   if(!json || !object || !name || !value)
      return 0;

   if(!(field = (AiJsonObjectField *) JsonMalloc(json, sizeof(*field))))
      return 0;
   field->name  = name;
   field->value.type = value->type;
   field->value.data = value->data;
   AiListInsert(object, field);
   return 1;
}

AiJsonValue *ReturnOrRemoveValueArray(AiList *array, size_t index, int remove)
{
   size_t i;
   AiListNode *curr;

   if(!array)
      return 0;

   if((index + 1) < array->len)
      return 0; 

   curr = array->first;
   for(i = 0; curr; ++i)
   {
      if(index == i)
         return ReturnOrRemoveValue(array, curr, (AiJsonValue *) curr->data, remove);
      curr = curr->next;
   }
   return 0;
}

void AiJsonRemoveValueArray(AiList *array, size_t index)
{
   ReturnOrRemoveValueArray(array, index, 1);
}

AiJsonValue *AiJsonGetValueArray(AiList *array, size_t index)
{
   return ReturnOrRemoveValueArray(array, index, 0);
}

int AiJsonInsertValueArray(AiJson *json, AiList *array, AiJsonValue *value)
{
   AiJsonValue *valueCopy = (AiJsonValue *) JsonMalloc(json, sizeof(*valueCopy));
   if(!value || !valueCopy)
      return 0;
   valueCopy->type = value->type;
   valueCopy->data = value->data;
   return AiListInsert(array, valueCopy);
}

void AiJsonDestroy(AiJson *json)
{
   AiListNode *curr = json->_private.mallocList->first;
   while(curr)
   {
      Funcs::pFree(curr->data);
      curr = curr->next;
   }
   curr = json->_private.listList->first;
   while(curr)
   {
      AiListDestroy((AiList *) curr->data);
      curr = curr->next;
   }
   AiListDestroy(json->_private.listList);
   AiListDestroy(json->_private.mallocList);
   Funcs::pFree(json);
   json = 0;
}

void AppendEscape(AiStrAppender *appender, char *str)
{
   char *start = str;
   char *end   = start;
   for(;;)
   {
      int j;
      for(j = 0; j < NUM_ESCAPE_SYMBOLS; ++j)
      {
         if(*end == escapeSymbols[j][1])
         {
            char temp[3];
            *end = 0;
            AiStrAppenderWork(appender, start);
            temp[0] = CHAR_ESCAPE_SIGNAL;
            temp[1] = escapeSymbols[j][0];
            temp[2] = 0;
            AiStrAppenderWork(appender, temp);
            ++end;
            if(!*end)
               goto exit;
            start = end;
            end   = start;
            goto lContinue;
         }
      }
      if(!*end++)
         break;
lContinue:;
   }
exit:
   AiStrAppenderWork(appender, start);
}

void InsertSpaces(AiStrAppender *appender, int spaces)
{
   char *str;
   if(!spaces)
      return;
   str = (char *) Alloc(spaces + 1);
   Funcs::pMemset(str, ' ', spaces);
   str[spaces] = 0;
   AiStrAppenderWork(appender, str);
   Funcs::pFree(str);
}

void AiJsonDumpObjectArray(AiList        *object,
                           AiStrAppender *appender, 
                           int            level, 
                           int            spaces,
                           int            isObject)
{   
   const int maxSpaces = 4;
   int       padding   = level * spaces;                             
   AiListNode *curr      = object->first;
   char buff[128];

   if(spaces > maxSpaces)
      spaces = maxSpaces;

   AiStrAppenderWorkChar(appender, isObject ? CHAR_OBJECT_OPEN : CHAR_ARRAY_OPEN);
   if(spaces)
      AiStrAppenderWorkChar(appender, '\n');
 
   while(curr)
   {
      AiJsonValue       *value;
      AiJsonObjectField *field;
      if(isObject)
      {
         field = (AiJsonObjectField *) curr->data;
         InsertSpaces(appender, padding + spaces);
         AiStrAppenderWorkChar(appender, CHAR_STR_OPEN_CLOSE);
         AppendEscape(appender, field->name);
         AiStrAppenderWorkChar(appender, CHAR_STR_OPEN_CLOSE);
         AiStrAppenderWorkChar(appender, CHAR_NAME_SEPARATOR);
         if(spaces)
            AiStrAppenderWorkChar(appender, ' ');
      }
      else
         InsertSpaces(appender, padding + spaces);

      value = isObject ? &field->value : (AiJsonValue *) curr->data;
      switch(value->type)
      {
         case AI_JSON_STRING:
         {
            AiStrAppenderWorkChar(appender, CHAR_STR_OPEN_CLOSE);
            AppendEscape(appender, value->data.string);
            AiStrAppenderWorkChar(appender, CHAR_STR_OPEN_CLOSE);
            break;
         }
         case AI_JSON_OBJECT:
         {
            AiJsonDumpObjectArray(value->data.object, appender, level + 1, spaces, 1);
            break;   
         }
         case AI_JSON_ARRAY:
         {
            AiJsonDumpObjectArray(value->data.array, appender, level + 1, spaces, 0);
            break;
         }
         case AI_JSON_NULL:
         {
            AiStrAppenderWork(appender, STR_NULL);
            break;
         }
         case AI_JSON_DOUBLE:
         {
            Funcs::pWsprintfA(buff, "%f", value->data.dbl);
            AiStrAppenderWork(appender, buff);
            break;
         }
         case AI_JSON_ULONG:
         {
            Funcs::pWsprintfA(buff, "%lu", value->data.uLong);
            AiStrAppenderWork(appender, buff);
            break;
         }
         case AI_JSON_SLONG:
         {
            Funcs::pWsprintfA(buff, "%ld", value->data.sLong);
            AiStrAppenderWork(appender, buff);
            break;
         }
         case AI_JSON_BOOL:
         {
            AiStrAppenderWork(appender, value->data.boolean ? STR_TRUE : STR_FALSE);
            break;
         }
      }
     
      if(curr != object->last)
         AiStrAppenderWorkChar(appender, CHAR_VALUE_SEPARATOR);
      if(spaces)
         AiStrAppenderWorkChar(appender, '\n');
      curr = curr->next;
   }
   InsertSpaces(appender, padding);
   AiStrAppenderWorkChar(appender, isObject ? CHAR_OBJECT_CLOSE : CHAR_ARRAY_CLOSE);
}

char *AiJsonDump(AiJson *json, int spaces)
{                        
   AiStrAppender appender;
   if(!AiStrAppenderInit(&appender))
      return 0;
   AiJsonDumpObjectArray(json->root.data.object, &appender, 0, spaces, json->root.type == AI_JSON_OBJECT);
   return appender.str;
}