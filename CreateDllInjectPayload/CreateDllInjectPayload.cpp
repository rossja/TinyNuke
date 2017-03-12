#include <Windows.h>
#include <stdio.h>

typedef struct _LSA_UNICODE_STRING
{
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

typedef struct _STRING {
  USHORT Length;
  USHORT MaximumLength;
  PCHAR  Buffer;
} ANSI_STRING, *PANSI_STRING;

typedef NTSTATUS (NTAPI *T_LdrLoadDll) (PWCHAR PathToFile,
   ULONG Flags,
   PUNICODE_STRING ModuleFileName,
   PHANDLE ModuleHandle);

typedef NTSTATUS (NTAPI *T_LdrGetProcedureAddress) (HMODULE ModuleHandle,
   PANSI_STRING FunctionName,
   WORD Oridinal,
   PVOID *FunctionAddress);

typedef VOID (NTAPI *T_RtlInitAnsiString) (PANSI_STRING DestinationString, PCHAR SourceString);

typedef NTSTATUS (NTAPI *T_RtlAnsiStringToUnicodeString) (PUNICODE_STRING DestinationString,
   PANSI_STRING SourceString,
   BOOLEAN AllocateDestinationString);

typedef VOID (NTAPI *T_RtlFreeUnicodeString) (PUNICODE_STRING UnicodeString);

struct InjectData
{
   BYTE *base;
   IMAGE_BASE_RELOCATION *baseRelocation;
   IMAGE_IMPORT_DESCRIPTOR *importDesc;
   T_RtlInitAnsiString pRtlInitAnsiString;
   T_RtlAnsiStringToUnicodeString pRtlAnsiStringToUnicodeString;
   T_LdrLoadDll pLdrLoadDll;
   T_LdrGetProcedureAddress pLdrGetProcedureAddress;
   T_RtlFreeUnicodeString pRtlFreeUnicodeString;
};

typedef BOOL (WINAPI *T_DllMain) (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

DWORD WINAPI Payload(InjectData *injectData)
{
   IMAGE_DOS_HEADER *dosHeader = (IMAGE_DOS_HEADER *) injectData->base;
   IMAGE_NT_HEADERS *ntHeaders = (IMAGE_NT_HEADERS *) (injectData->base + dosHeader->e_lfanew);
   IMAGE_BASE_RELOCATION *baseRelocation = (IMAGE_BASE_RELOCATION *) injectData->baseRelocation;

   size_t delta = (size_t) injectData->base - ntHeaders->OptionalHeader.ImageBase; 
  
   while(baseRelocation->VirtualAddress)
   {
      if(baseRelocation->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION))
      {
         DWORD count = (baseRelocation->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
         WORD *pWord = (WORD *) (baseRelocation + 1);
         
         for(DWORD i = 0; i < count; ++i)
         {
            DWORD type = pWord[i] >> 12;
            DWORD offset = pWord[i] & 0xfff;
            
            switch(type)
            {
               case IMAGE_REL_BASED_HIGHLOW:
               {
                  DWORD *patchAddress;

                  patchAddress = (DWORD *) (((DWORD) injectData->base) + baseRelocation->VirtualAddress + offset);
                  *patchAddress += (DWORD) delta;
                  break;
               }
               case IMAGE_REL_BASED_DIR64:
               {
                  DWORD64 *patchAddress;

                  patchAddress = (DWORD64 *) (((DWORD64) injectData->base) + baseRelocation->VirtualAddress + offset);
                  *patchAddress += (DWORD64) delta;
                  break;
               }
            }
         }
      }
      baseRelocation = (IMAGE_BASE_RELOCATION *) ((BYTE *) baseRelocation + baseRelocation->SizeOfBlock);
   }

   IMAGE_IMPORT_DESCRIPTOR *importDesc = (IMAGE_IMPORT_DESCRIPTOR *) injectData->importDesc;
   
   while(importDesc->Name)
   {
      ANSI_STRING aDllStr;
      injectData->pRtlInitAnsiString(&aDllStr, (char *) ((BYTE *) injectData->base + importDesc->Name));
      
      UNICODE_STRING uDllStr;
      injectData->pRtlAnsiStringToUnicodeString(&uDllStr, &aDllStr, TRUE);

      HANDLE hModule;
      injectData->pLdrLoadDll(NULL, NULL, &uDllStr, &hModule);
      injectData->pRtlFreeUnicodeString(&uDllStr);

      IMAGE_THUNK_DATA *origThunkData = (IMAGE_THUNK_DATA *) ((BYTE *) injectData->base + importDesc->OriginalFirstThunk);
      IMAGE_THUNK_DATA *firstThunkData = (IMAGE_THUNK_DATA *) ((BYTE *) injectData->base + importDesc->FirstThunk);

      while(origThunkData->u1.AddressOfData)
      {
         ANSI_STRING aFuncStr;

         if(origThunkData->u1.Ordinal & IMAGE_ORDINAL_FLAG)
            injectData->pRtlInitAnsiString(&aFuncStr, (char *) (origThunkData->u1.Ordinal & 0xFFFF));
         else
         {
            IMAGE_IMPORT_BY_NAME *importByName = (IMAGE_IMPORT_BY_NAME *) ((BYTE *) injectData->base + origThunkData->u1.AddressOfData);
            injectData->pRtlInitAnsiString(&aFuncStr, importByName->Name);
         }

         PVOID func;
         injectData->pLdrGetProcedureAddress((HMODULE) hModule, &aFuncStr, NULL, &func);
         firstThunkData->u1.Function = (size_t) func;

         ++origThunkData;
         ++firstThunkData;
      }
      ++importDesc;
   }
   
   T_DllMain pDllMain = (T_DllMain) ((BYTE *) injectData->base + ntHeaders->OptionalHeader.AddressOfEntryPoint);
   pDllMain((HMODULE) injectData->base, DLL_PROCESS_ATTACH, NULL);
   return 0;
   
}

void AfterPayload() { }

int main(int argc, char **argv)
{
   BYTE *func = (BYTE *) Payload;
   DWORD funcSize = (DWORD) AfterPayload - (DWORD) Payload;

   FILE *file = fopen("out.txt", "w");   
   fprintf(file, "DWORD payloadSize = %d;\n", funcSize);

   fputs("BYTE payload[] = { ", file);
   for(DWORD i = 0; i < funcSize; ++i)
      fprintf(file, "0x%02x, ", func[i]);
   fputs("};", file);
}