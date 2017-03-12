#ifndef AI_JSON_H
#define AI_JSON_H

#include "AiList.h"

typedef struct AiJsonObjectField AiJsonObjectField;
typedef struct AiJsonValue       AiJsonValue;
typedef struct AiJsonArray       AiJsonArray;
typedef struct AiJson            AiJson;
typedef enum   AiJsonType        AiJsonType;
typedef enum   AiJsonError       AiJsonError;

enum AiJsonType
{
   AI_JSON_STRING,
   AI_JSON_DOUBLE,
   AI_JSON_ULONG,
   AI_JSON_SLONG,
   AI_JSON_OBJECT,
   AI_JSON_ARRAY,
   AI_JSON_BOOL,
   AI_JSON_NULL
};

//do not reorder
enum AiJsonError
{
   AI_JSON_E_OK,
   AI_JSON_E_UNEXPECTED_SYMBOL,
   AI_JSON_E_UNEXPECTED_EOF,
   AI_JSON_E_INVALID_HEX_DIGIT,
   AI_JSON_E_INVALID_ESCAPE_SEQUENCE,
   AI_JSON_E_CHAR_MUST_BE_ESCAPED,
   AI_JSON_E_INVALID_NUM,
   AI_JSON_E_EXPECTED_ARRAY_CLOSE,
   AI_JSON_E_EXPECTED_NAME,
   AI_JSON_E_EXPECTED_NAME_SEPARATOR,
   AI_JSON_E_EXPECTED_OBJECT_CLOSE,
   AI_JSON_E_UNKNOWN,
   AI_JSON_E_ALLOC,
   AI_JSON_E_INVALID_VALUE,
};

struct AiJsonValue
{
   AiJsonType type;
   union
   {
      char         *string;
      long          sLong;
      unsigned long uLong;
      double        dbl;
      AiList       *array;
      AiList       *object;
      int           boolean;    
   } data;    
};

struct AiJsonObjectField
{
   char       *name;
   AiJsonValue value;
};

struct AiJson
{
   AiJsonValue root;
   AiJsonError error;
   char       *errorMsg;
   size_t      line;
   size_t      column;
   struct
   {
      AiList *mallocList;
      AiList *listList;
   } _private;
};

AiJson *AiJsonParse(char *str);
void AiJsonDestroy(AiJson *json);
char *AiJsonDump(AiJson *json, int spaces);

AiJsonValue *AiJsonGetValueObject(AiList *object, char *name);
void AiJsonRemoveValueObject(AiList *object, char *name);
int  AiJsonInsertValueObject(AiJson *json, AiList *object, char *name, AiJsonValue *value);

AiJsonValue *AiJsonGetValueArray(AiList *array, size_t index);
void AiJsonRemoveValueArray(AiList *array, size_t index);
int AiJsonInsertValueArray(AiJson *json, AiList *array, AiJsonValue *value);

#endif