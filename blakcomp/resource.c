// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * resource.c:  Write out resource file from symbol table information.
 */
#include "blakcomp.h"
#include <regex>

 // See codegen.c for explanation.
extern char *codegen_buffer;
extern int codegen_buffer_position;
extern int codegen_buffer_size;
extern int codegen_buffer_warning_size;
extern int print_dup_eng_ger;     /* Whether we print duplicate Eng-Ger rscs */
extern int print_missing_ger;     /* Whether we print missing German rscs */

static const int RSC_VERSION = 5;
static char rsc_magic[] = {0x52, 0x53, 0x43, 0x01};

char *GetStringFromResource(resource_type r, int j);
void ResourceStringModVerify(resource_type r, char *fname);
/******************************************************************************/
/*
 * check_for_class_resource_string: Uses a resource's ID to check if the same
 * resource has been defined in another class. Returns 1 if it has, 0 otherwise.
 */
/******************************************************************************/
Bool check_for_class_resource_string(id_type id)
{
   list_type c, l;
   resource_type r;
   class_type cl;

   for (c = st.classes; c != NULL; c = c->next)
   {
      cl = (class_type)c->data;

      for (l = cl->resources; l != NULL; l = l->next)
      {
         r = (resource_type)(l->data);
         printf("Checking %i and %i\n", r->lhs->idnum, id->idnum);
         // Check for matching ID.
         if (r->lhs->idnum == id->idnum)
         {
            return 1;
         }
      }
   }

   return 0;
}
/******************************************************************************/
/*
* write_resources: Write out resources to a .rsc file.  fname should be the
*    name of the .rsc file to receive the resources.  Resources are found by
*    looping through all the classes in the symbol table.
*/
/******************************************************************************/
void write_resources(char *fname)
{
   list_type c, l;
   resource_type r;
   int num_resources, i, temp;
   class_type cl;
   FILE *f;
   char *str;

   /* Count resources and compute length */
   num_resources = 0;
   for (c = st.classes; c != NULL; c = c->next)
   {
      cl = (class_type) c->data;
      if (cl->is_new)
      {
         for (l = cl->resources; l != NULL; l = l->next)
         {
            r = (resource_type)(l->data);

            for (int j = 0; j < MAX_LANGUAGE_ID; j++)
            {
               if (r->resource[j])
               {
                  num_resources++;
               }
            }
         }
      }
   }
   /* If no resources, do nothing */
   if (num_resources == 0)
      return;
   
   f = fopen(fname, "wb");
   if (f == NULL)
   {
      simple_error("Unable to open resource file %s!", fname);
      return;
   }

   codegen_buffer_position = 0;

   /* Write out header information */
   for (i = 0; i < 4; i++)
   {
      memcpy(&(codegen_buffer[codegen_buffer_position]), &rsc_magic[i], 1);
      codegen_buffer_position++;
   }
   
   temp = RSC_VERSION;
   memcpy(&(codegen_buffer[codegen_buffer_position]), &temp, 4);
   codegen_buffer_position += 4;
   memcpy(&(codegen_buffer[codegen_buffer_position]), &num_resources, 4);
   codegen_buffer_position += 4;

   /* Loop through classes in this source file, and then their resources */
   for (c = st.classes; c != NULL; c = c->next)
   {
      cl = (class_type) c->data;
      if (cl->is_new)
         for (l = cl->resources; l != NULL; l = l->next)
         {
            r = (resource_type) (l->data);

            // Verify string modifiers in different language strings.
            ResourceStringModVerify(r, fname);

            // For each language string present,
            // write out language data and string.
            for (int j = 0; j < MAX_LANGUAGE_ID; j++)
            {
               if (r->resource[j])
               {
                  // Write out id #
                  memcpy(&(codegen_buffer[codegen_buffer_position]), &r->lhs->idnum, 4);
                  codegen_buffer_position += 4;

                  // Language ID
                  memcpy(&(codegen_buffer[codegen_buffer_position]), &j, 4);
                  codegen_buffer_position += 4;

                  // String
                  str = GetStringFromResource(r, j);
                  int len = strlen(str) + 1;
                  memcpy(&(codegen_buffer[codegen_buffer_position]), str, len);
                  codegen_buffer_position += len;
               }
            }
         }
   }

   // Write the file in one call.
   fwrite(codegen_buffer, codegen_buffer_position, 1, f);
   fclose(f);
}
/******************************************************************************/
char *GetStringFromResource(resource_type r, int j)
{
   switch (r->resource[j]->type)
   {
      case C_STRING:
         return r->resource[j]->value.stringval;
      case C_FNAME:
         return r->resource[j]->value.fnameval;
      default:
         simple_error("Unknown resource type (%d) encountered",
            r->resource[j]->type);
         return NULL;
   }
}
/******************************************************************************/
/*
 * ResourceStringModVerify: Check each language string for a resource and warn
 *   the user if there is a mismatch in the number or types of modifiers.
 */
/******************************************************************************/
// 5 types of modifiers to check: %i, %d (deprecated), %s, %q, %r.
// %i and %d count as one.
#define NUM_RSCMOD_TYPES 4
typedef struct {
   int rsc_mod[NUM_RSCMOD_TYPES];
   int rsc_mod_count;
} check_rsc_type;

void ResourceStringModVerify(resource_type r, char *fname)
{
   if (!r)
      return;

   // If we don't have an English string, warn.
   if (!r->resource[0])
   {
      simple_warning("%s: No English string for resource %s.",
         fname, r->lhs->name);
      return;
   }

   // Check types only on non-string resources.
   if (r->resource[0]->type != C_STRING)
   {
      for (int i = 1; i < MAX_LANGUAGE_ID; ++i)
      {
         if (r->resource[i] && r->resource[i]->type != r->resource[0]->type)
            simple_warning("%s: Got mismatched resource types for resource %s.",
               fname, r->lhs->name);
      }
      return;
   }

   check_rsc_type check_array[MAX_LANGUAGE_ID];
   char *str;

   for (int i = 0; i < MAX_LANGUAGE_ID; ++i)
   {
      check_array[i].rsc_mod_count = 0;
      for (int j = 0; j < NUM_RSCMOD_TYPES; ++j)
         check_array[i].rsc_mod[j] = 0;

      if (r->resource[i])
      {
         if (r->resource[i]->type != r->resource[0]->type)
            simple_warning("%s: Got mismatched resource types for resource %s.",
               fname, r->lhs->name);

         str = GetStringFromResource(r, i);
         if (!str)
         {
            if (i == 0)
            {
               simple_warning("%s: No English string for resource %s.",
                  fname, r->lhs->name);
               return;
            }
            // Don't report missing strings for other languages, can be
            // valid reasons for this (i.e. string would be a duplicate).
            continue;
         }

         // Duplicate resource checker for Eng-Ger duplicates.
         if (print_dup_eng_ger
            && i == 1
            && strcmp(str, GetStringFromResource(r, 0)) == 0)
         {
            simple_warning("%s: Duplicate German string %s.",
               fname, r->lhs->name);
         }

         while (*str)
         {
            if (*str == '%')
            {
               ++str;
               if (*str == 'i' || *str == 'd')
               {
                  check_array[i].rsc_mod[0]++;
                  check_array[i].rsc_mod_count++;
               }
               else if (*str == 's')
               {
                  check_array[i].rsc_mod[1]++;
                  check_array[i].rsc_mod_count++;
               }
               else if (*str == 'q')
               {
                  check_array[i].rsc_mod[2]++;
                  check_array[i].rsc_mod_count++;
               }
               else if (*str == 'r')
               {
                  check_array[i].rsc_mod[3]++;
                  check_array[i].rsc_mod_count++;
               }
            }
            ++str;
         }

         // English resource is reference.
         if (i == 0)
            continue;
         // Check total count first.
         if (check_array[i].rsc_mod_count != check_array[0].rsc_mod_count)
         {
            simple_warning("%s: Got mismatched number of string modifiers in resource %s.",
               fname, r->lhs->name);
            return;
         }
         // Check number of each type of string modifier.
         for (int j = 0; j < NUM_RSCMOD_TYPES; ++j)
         {
            if (check_array[i].rsc_mod[j] != check_array[0].rsc_mod[j])
            {
               simple_warning("%s: Got mismatched types of string modifiers in resource %s.",
                  fname, r->lhs->name);
               return;
            }
         }
      }
      else if (i == 1)
      {
         // German string, check if we should report.
         // Okay for only English string present if it's something like "%r%r" or "100".
         // Don't search for i, q, r, s as they are used in string formatters.
         if (print_missing_ger
            && std::regex_match(GetStringFromResource(r, 0), std::regex(".*[a-hj-pt-zA-Z].*")))
            simple_warning("%s: No German string for resource %s.", fname, r->lhs->name);

      }
   }
}
