/*
 * rscmerge.c:  Combine multiple rsc files into one.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "rscload.h"
#include <vector>
#include <string>

typedef std::vector<std::string> StringVector;

typedef struct _Resource {
   int   number;
   char *string[MAX_LANGUAGE_ID];
   struct _Resource *next;
} Resource;

#define RSC_TABLE_SIZE 1023

static Resource **resources;
static int num_resources;

static const int RSC_VERSION = 5;
static char rsc_magic[] = {0x52, 0x53, 0x43, 0x01};

static void Error(char *fmt, ...);

/************************************************************************/
/*
 * SafeMalloc:  Calls _fmalloc to get small chunks of memory ( < 64K).
 *   Need to use this procedure name since util library calls it.
 */
void *SafeMalloc(unsigned int bytes)
{
   void *temp = (void *) malloc(bytes);
   if (temp == NULL)
   {
      Error("Out of memory!\n");
      exit(1);
   }

   return temp;
}
/************************************************************************/
/*
 * SafeFree:  Free a pointer allocated by SafeMalloc.
 */
void SafeFree(void *ptr)
{
   if (ptr == NULL)
   {
      Error("Freeing a NULL pointer!\n");
      return;
   }
   free(ptr);
}
/************************************************************************/
/*
 * FindMatchingFiles:  Copy from blakserv/files.c
 */
bool FindMatchingFiles(char *path, std::vector<std::string> *files)
{
#ifdef BLAK_PLATFORM_WINDOWS

   HANDLE hFindFile;
   WIN32_FIND_DATA search_data;

   files->clear();
   hFindFile = FindFirstFile(path, &search_data);
   if (hFindFile == INVALID_HANDLE_VALUE)
      return false;

   do
   {
      files->push_back(search_data.cFileName);
   } while (FindNextFile(hFindFile, &search_data));
   FindClose(hFindFile);

   return true;

#elif BLAK_PLATFORM_LINUX
   // Warning, not tested in rscmerge.c.
   struct dirent *entry;
   std::string spath = path;
   std::size_t last_found = spath.find_last_of("/\\");
   std::string sext = spath.substr(last_found + 2);
   spath = spath.substr(0, last_found);

   DIR *dir = opendir(spath.c_str());
   if (dir == NULL)
      return false;

   while (entry = readdir(dir))
   {
      std::string filename = entry->d_name;
      if (filename != "." && filename != "..")
      {
         if (filename.length() > sext.length() &&
            filename.substr(filename.length() - sext.length()) == sext)
         {
            files->push_back(filename);
         }
      }
   }

   closedir(dir);

   return true;

#else
#error No platform implementation of FindMatchingFiles
#endif
}
/***************************************************************************/
void Usage(void)
{
   printf("Usage: rscmerge -o <output filename> <input filename> ...\n");
   exit(1);
}
/**************************************************************************/
void Error(char *fmt, ...)
{
   char s[200];
   va_list marker;
    
   va_start(marker,fmt);
   vsprintf(s,fmt,marker);
   va_end(marker);

   printf("%s\n", s);
   exit(1);
}
/***************************************************************************/
/*
 * SaveRscFile:  Save contents of global var resources to given rsc filename.
 *   Return true on success.
 */
bool SaveRscFile(char *filename)
{
   int i, temp;
   Resource *r;
   FILE *f;

   /* If no resources, do nothing */
   if (num_resources == 0)
      return true;

   f = fopen(filename, "wb");
   if (f == NULL)
      return false;
   
   /* Write out header information */
   for (i=0; i < 4; i++)
      fwrite(&rsc_magic[i], 1, 1, f);

   temp = RSC_VERSION;
   fwrite(&temp, 4, 1, f);
   fwrite(&num_resources, 4, 1, f);

   for (i = 0; i < RSC_TABLE_SIZE; ++i)
   {
      r = resources[i];
      while (r != NULL)
      {
         // For each language string present,
         // write out language data and string.
         for (int j = 0; j < MAX_LANGUAGE_ID; j++)
         {
            if (r->string[j])
            {
               // Write out id #
               fwrite(&r->number, 4, 1, f);

               // Write language id.
               fwrite(&j, 4, 1, f);

               // Write string.
               fwrite(r->string[j], strlen(r->string[j]) + 1, 1, f);
            }
         }
         r = r->next;
      }
   }

   fclose(f);
   return true;
}
/***************************************************************************/
/*
 * EachRscCallback:  Called for each resource that's loaded.
 */
bool EachRscCallback(char *filename, int rsc, int lang_id, char *string)
{
   Resource *r;

   // See if we've added this resource already.
   r = resources[rsc % RSC_TABLE_SIZE];
   while (r != NULL)
   {
      if (r->number == rsc)
      {
         ++num_resources;
         r->string[lang_id] = strdup(string);
         return true;
      }
      r = r->next;
   }

   r = (Resource *) SafeMalloc(sizeof(Resource));
   r->number = rsc;

   for (int i = 0; i < MAX_LANGUAGE_ID; i++)
   {
      if (i == lang_id)
      {
         r->string[i] = strdup(string);
         ++num_resources;
      }
      else
      {
         r->string[i] = NULL;
      }
   }

   // add to resources table
   int hash_num = r->number % RSC_TABLE_SIZE;
   r->next = resources[hash_num];
   resources[hash_num] = r;

   return true;
}
/***************************************************************************/
/*
 * LoadRscFiles:  Read resources from given rsc files into global resources variable.
 *   Return true on success.
 */
bool LoadRscFiles(int num_files, char **foldername)
{
   char file_load_path[MAX_PATH + FILENAME_MAX];
   sprintf(file_load_path, "%s\\*.rsc", *foldername);
   StringVector files;
   if (FindMatchingFiles(file_load_path, &files))
   {
      for (StringVector::iterator it = files.begin(); it != files.end(); ++it)
      {
         sprintf(file_load_path, "%s\\%s", *foldername, it->c_str());

         if (!RscFileLoad(file_load_path, EachRscCallback))
         {
            printf("Failure reading rsc file %s!\n", file_load_path);
            return false;
         }
      }
   }

   return true;
}
void InitResourceTable()
{
   // Init resource table
   resources = (Resource **)SafeMalloc(RSC_TABLE_SIZE * sizeof(Resource *));

   for (int i = 0; i < RSC_TABLE_SIZE; i++)
      resources[i] = NULL;

   // Resources found so far.
   num_resources = 0;
}
void FreeResourceTable()
{
   Resource *r, *temp;

   for (int i = 0; i < RSC_TABLE_SIZE; ++i)
   {
      r = resources[i];
      while (r != NULL)
      {
         temp = r->next;
         for (int j = 0; j < MAX_LANGUAGE_ID; j++)
         {
            if (r->string[j])
            {
               SafeFree(r->string[j]);
               r->string[j] = NULL;
            }
         }
         SafeFree(r);
         r = temp;
      }
      resources[i] = NULL;
   }
   SafeFree(resources);
   num_resources = 0;
}
/***************************************************************************/
int main(int argc, char **argv)
{
   int arg, len;
   char *output_filename = NULL;
   int output_file_found = 0;

   if (argc < 3)
      Usage();
   
   for (arg = 1; arg < argc; arg++)
   {
      len = strlen(argv[arg]);
      if (len == 0)
         break;
      
      if (argv[arg][0] != '-')
         break;
      
      if (len < 2)
      {
         printf("Ignoring unknown option -\n");
         continue;
      }
      
      switch(argv[arg][1])
      {
      case 'o':
         arg++;
         if (arg >= argc)
         {
            printf("Missing output filename\n");
            break;
         }
         output_filename = argv[arg];
         output_file_found = 1;
         break;
      }
   }
   
   if (!output_file_found)
      Error("No output file specified");
   
   if (arg >= argc)
      Error("No input files specified");

   InitResourceTable();

   if (!LoadRscFiles(argc - arg, argv + arg))
      Error("Unable to load rsc files.");

   if (!SaveRscFile(output_filename))
      Error("Unable to save rsc file.");

   FreeResourceTable();
}
