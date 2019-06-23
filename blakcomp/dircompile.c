// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/* util.c
* Utility procedures for Blakod compiler
*/

#include "blakcomp.h"
#include "Windows.h"
#include "psapi.h"

#if defined(WIN32) || defined(WIN64)
// Copied from linux libc sys/stat.h:
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

extern list_type directory_list;
extern list_type file_list;
extern int codegen_ok;
extern char bof_output_dir[_MAX_PATH];
extern char rsc_output_dir[_MAX_PATH];
extern int print_unref_rscs;     /* Whether we print out rscs with no references */

typedef struct {
   char *dir_name; // Name of this directory.
   char *parent_file_name; // Parent file, if present.
   list_type dir_file_list;
} *dir_data, dir_struct;

list_type failed_compile_files;

// Local function prototypes.
void fill_lists_from_makefile(char *full_path, int recompiled_parent);
int recompile_check(char *name, dir_data d, int recompiled_parent);

int rsctime = 0;
int timecounter = 0;

void compile_directory_mode()
{
   //list_type current_file_ptr;
   char full_path[_MAX_PATH];

   if (directory_list == NULL)
   {
      printf("No directory specified!\n");
      exit(1);
   }

   // How this should work:
   // The directory given is the starting directory.
   // Step 1: Get current path.
   // Step 2: Recurse on makefiles, building a list of directories each with a list of files.
   // Step 3: Compile each file list in order like normal, except all in the same bc.exe process.
   // Step 4: Send each bof/rsc to proper output location as they are encountered.
   // Step 5: Output a single kodbase.txt at the end.

   // Get the current, full path.
   if (_fullpath(full_path, (char *)directory_list->data, _MAX_PATH) == NULL)
   {
      printf("Invalid path specified!\n");
      exit(1);
   }

   time_t timeStart, timeEnd;
   timeStart = time(NULL);

   failed_compile_files = NULL;

   // Remove trailing \ if we have one.
   int len = strlen(full_path) - 1;
   if (len > 1 && full_path[len] == '\\')
      full_path[len] = 0;

   // Remove directory (should now be null).
   directory_list = list_delete_first(directory_list);
   directory_list = NULL;

   // Read makefiles, fill dir/file lists for all directories.
   fill_lists_from_makefile(full_path, 0);

   // Set up parser, read in database file
   initialize_parser();
   if (!load_kodbase())
      simple_error("Error loading database; continuing with compilation");

   // Initial setup before codegen runs. Only need to do it once.
   codegen_init();

   // Compile by directory.
   for (list_type dirs = directory_list; dirs != NULL; dirs = dirs->next)
   {
      // For each directory, compile files.
      dir_data d = (dir_data)dirs->data;

      int compile = True;
      if (d->dir_file_list)
      {
         for (list_type f = failed_compile_files; f != NULL; f = f->next)
         {
            if (stricmp(d->parent_file_name, (char *)f->data) == 0)
            {
               // Compile of the parent failed, so we don't compile any
               // of the children.
               compile = False;
               failed_compile_files = list_append(failed_compile_files, d->dir_file_list);
               d->dir_file_list = NULL;

               break;
            }
         }
         if (compile)
         {
            if (d->dir_name)
               printf("Building %s\n", strrchr(d->dir_name, '\\') + 1);

            compile_file_list(d->dir_name, d->dir_file_list);

            d->dir_file_list = list_destroy(d->dir_file_list);

         }
      }

      SafeFree(d->dir_name);
      d->dir_name = NULL;
      if (d->parent_file_name)
      {
         SafeFree(d->parent_file_name);
         d->parent_file_name = NULL;
      }
      SafeFree(d);
   }

   // Some files may have failed to compile, we should probably fail the whole
   // process with a better error message in that case instead of being permissive.
   // Partially failed build is still a broken build.
   if (codegen_ok)
      save_kodbase();

   if (print_unref_rscs)
   {
      list_type rsc_list = table_get_all(st.globalvars);
      for (list_type l = rsc_list; l != NULL; l = l->next)
      {
         id_type ID = (id_type)l->data;
         if (ID->type == I_RESOURCE
            && ID->is_string_rsc
            && ID->reference_num == 0)
         {
            printf("Found rsc %s with no references\n", ID->name);
         }
      }
   }

   // Note - this gives a rough overview of unused properties
   // but isn't totally accurate depending on class build order
   // and where props are used. Conversely, a global table ignores
   // cases where a class has an unused prop with the same name
   // as a used prop in an unrelated class hierarchy.
   /*for (list_type l = st.classes; l != NULL; l = l->next)
   {
      class_type cl = (class_type)l->data;
      for (list_type p = cl->properties; p != NULL; p = p->next)
      {
         property_type prop = (property_type)p->data;
         if (stricmp(prop->id->name,"piTimeLimit") == 0)
         {
            printf("Found prop %i:%s with %i references in class %s\n",
               prop->id->idnum, prop->id->name, prop->id->reference_num, cl->class_id->name);
         }
         if (prop->id->reference_num == 0)
         {
            printf("Found prop %s with no references in class %s\n",
               prop->id->name, cl->class_id->name);
         }
      }
   }*/

   // Codegen cleanup.
   codegen_exit();

   // Print some stats.
   timeEnd = time(NULL);
   printf("Elapsed time: %lld seconds\n", timeEnd - timeStart);

   PROCESS_MEMORY_COUNTERS pmc;
   GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
   printf("Peak mem: %i bytes\n", (int)pmc.PeakWorkingSetSize);
   printf("Current mem: %i bytes\n",(int) pmc.WorkingSetSize);

   return;
}

#define KOD_EXISTS_NO_RECOMPILE 1
#define KOD_EXISTS_RECOMPILE 2

// Get bofs from makefile, compare modified times and add to file_list.
// Also add any subdirectory to directory_list.
void fill_lists_from_makefile(char *full_path, int recompiled_parent)
{
   FILE *makefile;
   char makefile_path[_MAX_PATH], dir_name[_MAX_PATH];
   char temp[256], file_name[_MAX_PATH];
   struct stat dir_check;
   char *tmpptr;
   int done = False;

   sprintf(makefile_path, "%s\\makefile", full_path);
   makefile = fopen(makefile_path, "r");
   if (makefile == NULL)
   {
      printf("Cannot open makefile for directory %s!\n", full_path);
      exit(1);
   }

   dir_data d = (dir_data) SafeMalloc(sizeof(dir_struct));
   d->dir_name = strdup(full_path);

   // Have to allocate based on size here, no strdup.
   // Length of parent dir string, plus 4 for .kod, plus 1 for \0
   d->parent_file_name = (char *)SafeMalloc(strlen(full_path) + 4 + 1);
   strcpy(d->parent_file_name, full_path);
   strcat(d->parent_file_name, ".kod");

   d->dir_file_list = NULL;
   // Add our directory to the list to compile.
   directory_list = list_add_item(directory_list, d);

   int filelen = 0;
   int at_bofs = False;
   while (fgets(temp, 256, makefile) != NULL)
   {
      // Our line must be greater than 6 characters
      if (strlen(temp) <= 6 || temp[0] == '#')
         continue;

      // Our line must start with BOFS
      if (at_bofs || temp[0] == 'B' && temp[1] == 'O' && temp[2] == 'F' && temp[3] == 'S')
      {
         tmpptr = temp;

         if (!at_bofs)
         {
            at_bofs = True;
            // Advance to files.
            tmpptr += 4;
         }

         do
         {
            if (*tmpptr == ' ' || *tmpptr == '=' || *tmpptr == '\t')
               continue;
            if (*tmpptr == '.')
            {
               // Handle file.
               file_name[filelen] = 0;

               sprintf(dir_name, "%s\\%s", full_path, file_name);
               int retval = recompile_check(dir_name, d, recompiled_parent);
               // Returns > 0 if we should check for a directory.
               if (retval)
               {
                  if (stat(dir_name, &dir_check) == 0
                     && S_ISDIR(dir_check.st_mode))
                  {
                     // Call on this directory.
                     fill_lists_from_makefile(dir_name, retval);
                  }
               }
               filelen = 0;
               // Skip bof.
               tmpptr += 3;
            }
            else if (*tmpptr == '\\')
            {
               // Next line.
               break;
            }
            else if (*tmpptr == '\n')
            {
               done = True;
               break;
               // Finished.
            }
            else
            {
               file_name[filelen] = *tmpptr;
               ++filelen;
            }
         } while (++tmpptr);
         if (done)
         {
            break;
         }
      }
   }
   fclose(makefile);
}

// Compilation failed on a file. Prevents all descendents from being compiled.
void compile_failed(char *filename)
{
   if (filename)
      failed_compile_files = list_add_item(failed_compile_files, filename);
}

// Checks whether we should recompile, adds to file list.
// Returns true if there was a kod file, and caller can check for directory.
int recompile_check(char *name, dir_data d, int recompiled_parent)
{
   char bofname[_MAX_PATH], kodname[_MAX_PATH], lkodname[_MAX_PATH];
   struct stat time_bof, time_kod, time_lkod;

   sprintf(bofname, "%s.bof", name);
   sprintf(kodname, "%s.kod", name);
   sprintf(lkodname, "%s.lkod", name);

   // Kod must exist, others don't have to.
   if (stat(kodname, &time_kod) != 0)
   {
      simple_error("Found makefile entry with missing kod file %s", strrchr(kodname,'\\') + 1);
      return False;
   }

   // Automatic recompile.
   if (recompiled_parent == KOD_EXISTS_RECOMPILE
      || stat(bofname, &time_bof) != 0)
   {
      d->dir_file_list = list_add_item(d->dir_file_list, strdup(kodname)); // malloc
      return KOD_EXISTS_RECOMPILE;
   }

   if (stat(lkodname, &time_lkod) != 0)
   {
      // No lkod present.
      // If the time on kod is greater than on the bof, recompile.
      if (time_bof.st_mtime < time_kod.st_mtime)
      {
         d->dir_file_list = list_add_item(d->dir_file_list, strdup(kodname)); // malloc
         return KOD_EXISTS_RECOMPILE;
      }
   }
   else
   {
      // If the time on either kod or lkod is greater than on the bof, recompile.
      if (time_bof.st_mtime < time_kod.st_mtime
         || time_bof.st_mtime < time_lkod.st_mtime)
      {
         d->dir_file_list = list_add_item(d->dir_file_list, strdup(kodname)); // malloc
         return KOD_EXISTS_RECOMPILE;
      }
   }

   return KOD_EXISTS_NO_RECOMPILE;
}

void dircompile_copy_files(char *bof_source, char *rsc_source, char *bofname, char *rscname)
{
   char combine[_MAX_PATH];

   sprintf(combine, "%s\\%s", bof_output_dir, bofname);
   CopyFile(bof_source, combine, FALSE);

   sprintf(combine, "%s\\%s", rsc_output_dir, rscname);
   CopyFile(rsc_source, combine, FALSE);
}
