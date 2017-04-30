/*
 * rscload.c:  Load a rsc file, and call a function for each loaded resource.
 */

#include <stdio.h>

#ifdef BLAK_PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include "rscload.h"

static const int RSC_VERSION = 5;
static const int MAX_RSC_LEN = 20000;
static char rsc_magic[] = {0x52, 0x53, 0x43, 0x01};

static bool RscFileRead(char *fname, FILE *f, RscCallbackProc callback);
static bool RscFileReadMapped(char *fname, char *mem, int len, RscCallbackProc callback);
/***************************************************************************/
/*
 * RscFileLoad:  Open the given rsc file, and call the callback procedure
 *   for each resource in the file.
 *   Return true iff every resource in the file is passed to the callback, false otherwise.
 */
bool RscFileLoad(char *fname, RscCallbackProc callback)
{
#ifdef BLAK_PLATFORM_WINDOWS
   HANDLE fh = CreateFile(fname, GENERIC_READ, FILE_SHARE_READ, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
   if (fh == INVALID_HANDLE_VALUE)
   {
      return false;
   }

   int length = GetFileSize(fh, NULL);
   if (length < 12)
   {
      CloseHandle(fh);
      return false;
   }

   HANDLE mapfh = CreateFileMapping(fh, NULL, PAGE_READONLY, 0, length, NULL);
   if (mapfh == NULL)
   {
      CloseHandle(fh);
      return false;
   }

   char *mem = (char *)MapViewOfFile(mapfh, FILE_MAP_READ, 0, 0, 0);
   if (mem == NULL)
   {
      CloseHandle(mapfh);
      CloseHandle(fh);
      return false;
   }
   bool retval = RscFileReadMapped(fname, mem, length, callback);
   UnmapViewOfFile(mem);
   CloseHandle(mapfh);
   CloseHandle(fh);
#else
   FILE *f;

   if (callback == NULL)
      return false;

   f = fopen(fname, "rb");
   if (f == NULL)
      return false;

   bool retval = RscFileRead(fname, f, callback);
   fclose(f); 
#endif
   return retval; 
}
/***************************************************************************/
/*
 * RscFileRead:  Read an rsc FILE structure.
 */
bool RscFileRead(char *fname, FILE *f, RscCallbackProc callback)
{
   int i, num_resources, version, rsc_num, lang_id;
   unsigned char byte;

   // Check magic number and version
   for (i = 0; i < 4; i++)
      if (fread(&byte, 1, 1, f) != 1 || byte != rsc_magic[i])
         return false;

   if (fread(&version, 1, 4, f) != 4 || version != RSC_VERSION)
      return false;
   
   if (fread(&num_resources, 1, 4, f) != 4 || num_resources < 0)
      return false;

   // Read each resource
   for (i=0; i < num_resources; i++)
   {
      if (fread(&rsc_num, 1, 4, f) != 4)
         return false;

      // each resource has a language number and a string
      if (fread(&lang_id, 1, 4, f) != 4)
         return false;

      char str[MAX_RSC_LEN];
      int pos = 0;
      while (!feof(f) && fread(&str[pos], 1, 1, f) == 1)
      {
         // Too big for buffer?
         if (pos == MAX_RSC_LEN - 1)
            return false;
         
         // Reached end of string?
         if (str[pos] == '\0')
            break;
         pos++;
      }

      if (!(*callback)(fname, rsc_num, lang_id, str))
         return false;
   }

   return true;
}
/***************************************************************************/
/*
* RscFileReadMapped:  Read a mem-mapped rsc file (faster than FILE).
*/
bool RscFileReadMapped(char *fname, char *mem, int len, RscCallbackProc callback)
{
   int i, j, num_resources, rsc_num, lang_id;

   // Check magic number and version
   for (i = 0; i < 4; i++)
      if (mem[i] != rsc_magic[i])
         return false;

   if (*((int *)(mem + i)) != RSC_VERSION)
      return false;
   i += 4;

   num_resources = *((int *)(mem + i));
   i += 4;
   if (num_resources < 0)
      return false;

   // Read each resource
   for (j = 0; j < num_resources; j++)
   {
      rsc_num = *((int *)(mem + i));
      i += 4;

      lang_id = *((int *)(mem + i));
      i += 4;

      char str[MAX_RSC_LEN];
      int pos = 0;
      while (i < len)
      {
         str[pos] = mem[i];
         ++i;

         // Too big for buffer?
         if (pos == MAX_RSC_LEN - 1)
            return false;

         // Reached end of string?
         if (str[pos] == '\0')
            break;
         pos++;
      }

      if (!(*callback)(fname, rsc_num, lang_id, str))
         return false;
   }

   return true;
}
