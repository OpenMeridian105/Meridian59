// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * skills.c:  Deal with casting skills.
 * 
 * The "Skills" menu is added dynamically when the user enters the game.  When the server adds
 * or removes a skill from the player, we add or remove a menu item for it.  
 * The menu id of a skill is the index of the skill in the menu, plus a base constant.  
 * When the user chooses a skill from the menu, we take this index and find the skill.
 *
 * The skill list can become larger than the screen, so we subdivide it according to skill school.
 * Each of these corresponds to a submenu under the Skills menu.
 */

#include "client.h"
#include "merintr.h"
#include "skills.h"

static list_type skills = NULL;     /* All skills available to user */

static list_type temp_list = NULL;  /* Temporary one-node list to save allocation every time */
static object_node temp_obj;        /* Temporary object node in temp_list */

static ID use_skill;  /* Remember skill player wants to use while he selects target */

static void PerformCallback(ID id);

/********************************************************************/
/*
 * SkillsInit:  Initialize skills when game entered.
 */
void SkillsInit(void)
{
}
/********************************************************************/
/*
 * SkillsExit:  Free skills when game exited.
 */
void SkillsExit(void)
{
   FreeSkills();
}
/********************************************************************/
void SetSkills(list_type new_skills)
{
   list_type l;

   FreeSkills();

   for (l = new_skills; l != NULL; l = l->next)
      AddSkill( (skill *) (l->data));

   list_delete(new_skills);

   /* Make temporary list for sending a skill's target to server */
   list_delete(temp_list);
   temp_list = list_add_item(temp_list, &temp_obj);
}
/********************************************************************/
void FreeSkills(void)
{
   skills = ObjectListDestroy(skills);
   temp_list = list_delete(temp_list);
}
/********************************************************************/
void AddSkill(skill *sp)
{
   skills = list_add_item(skills, sp);
}
/********************************************************************/
void RemoveSkill(ID id)
{
   skill *old_skill = (skill *) list_find_item(skills, (void *) id, CompareIdObject);

   if (old_skill == NULL)
   {
      debug(("Tried to remove nonexistent skill #%ld\n", id));
      return;
   }

   skills = list_delete_item(skills, (void *) id, CompareIdObject);
   SafeFree(old_skill);
}
/********************************************************************/
/*
 * GetSkillsObject: Returns ptr to the skill object specified by id.
 */
object_node* GetSkillObject( ID idFind )
{
   skill *sp = FindSkillByID(idFind);
   if (sp)
      return &sp->obj;
   else
      return NULL;
}
/************************************************************************/
/*
 * GetSkillName:  Parse str, and return a pointer to the start of the first
 *   skill name in it, or NULL if there is none in str.
 *   If next is non-NULL and we find a skill name, fill next with
 *   a pointer to the position in str just after the end of the first skill name.
 *   str and *next can be identical.
 *   Modifies str by null terminating skill name.
 */
char *GetSkillName(char *str, char **next)
{
   char *ptr, *retval;
   Bool quoted = False;
   Bool ambiguous = False;

   if (str == NULL)
      return NULL;

   // Skip spaces
   while (*str == ' ')
      str++;

   if (*str == 0)
      return NULL;

   // See if name is quoted
   if (*str == '\"')
   {
      quoted = True;
      retval = str + 1;
   }
   else retval = str;

   // Skip to end of name and null terminate
   ptr = retval;

   do
   {
      if (quoted)
      {
         while (*ptr != '\"' && *ptr != 0)
            ptr++;
      }
      else
      {
         while (*ptr != ' ' && *ptr != 0)
            ptr++;
      }

      if (*ptr == 0)
      {
         if (next != NULL)
            *next = NULL;
         break;
      }
      else
      {
         if (next != NULL)
            *next = ptr + 1;

         // Null terminate spell name
         *ptr = 0;
      }

      if (!quoted)
         ambiguous = (SKILL_AMBIGUOUS == FindSkillByName(retval));
      if (ambiguous)
      {
         // restore spell name and try again by appending next word
         *ptr++ = ' ';
      }

   } while (ambiguous);

   return retval;
}

skill *FindSkillByID(ID idFind)
{
   skill* sp = NULL;
   list_type listptr;

   for( listptr = skills; listptr != NULL; listptr = listptr->next )
   {
      sp = (skill*)listptr->data;
      if( sp->obj.id == idFind )
	 return sp;
   }
   return NULL;
}
/********************************************************************/
/*
 * FindSkillByName:  Return the skill whose name best matches name.
 *   Return SKILL_NOMATCH if no spell matches, or SKILL_AMBIGUOUS if more than one
 *   skill matches equally well.
 */
skill *FindSkillByName(char *name)
{
   list_type l;
   skill *sk, *best_skill = NULL;
   int match, max_match;
   Bool tied;            // True if a different skill matches as well as best_skill
   char *ptr, *skill_name;

   max_match = 0;
   tied = False;
   for (l = skills; l != NULL; l = l->next)
   {
      sk = (skill *)(l->data);

      skill_name = LookupNameRsc(sk->obj.name_res);
      ptr = name;
      match = 0;
      while (*ptr != 0 && *skill_name != 0)
      {
         if (toupper(*ptr) != toupper(*skill_name))
         {
            match = 0;
            break;
         }
         match++;
         ptr++;
         skill_name++;
      }

      // Check for exact match, or extra characters in search string
      if (*skill_name == 0)
         if (*ptr == 0)
            return sk;
         else continue;

      if (match > max_match)
      {
         max_match = match;
         best_skill = sk;
         tied = False;
      }
      else if (match == max_match)
         tied = True;
   }


   if (max_match == 0)
      return SKILL_NOMATCH;

   if (tied)
      return SKILL_AMBIGUOUS;

   return best_skill;
}
/********************************************************************/
void UserPerformSkill(void)
{
   skill *sk = NULL, *temp;
   list_type sel_list, l;

   if (GetPlayer()->viewID && (GetPlayer()->viewID != GetPlayer()->id))
   {
      if (!(GetPlayer()->viewFlags & REMOTE_VIEW_CAST))
         return;
   }

   sel_list = DisplayLookList(cinfo->hMain, GetString(hInst, IDS_PERFORM), skills, LD_SORT);

   if (sel_list == NULL)
      return;

   /* Find spell in our private list */
   temp = (skill *)sel_list->data;
   for (l = skills; l != NULL; l = l->next)
   {
      sk = (skill *)l->data;
      if (sk->obj.id == temp->obj.id)
         break;
   }

   if (sk == NULL)
   {
      debug(("Couldn't find selected skill in skill list!\n"));
      return;
   }

   list_delete(sel_list);

   PerformAction(A_PERFORMSKILL, sk);
}
/********************************************************************/
/*
 * SkillPerform:  User wants to cast given skill.
 */
void SkillPerform(skill *sk)
{
   if (GetPlayer()->viewID && (GetPlayer()->viewID != GetPlayer()->id))
   {
      if (!(GetPlayer()->viewFlags & REMOTE_VIEW_CAST))
         return;
   }

   // Can only perform active skills.
   if (sk->is_active == 0)
   {
      GameMessagePrintf(GetString(hInst, IDS_NOTACTIVESKILL),
         LookupNameRsc(sk->obj.name_res));
      return;
   }

   /* If we don't need target, just perform skill */
   if (sk->num_targets == 0)
   {
      RequestPerform(sk->obj.id, NULL);
      return;
   }

   if (GetUserTargetID() != INVALID_ID)
   {
      ID id = GetUserTargetID();
      //	User has target already selected.
      if (id == GetPlayer()->id || FindVisibleObjectById(id))
      {
         /* Make temporary list for sending to server */
         temp_obj.id = GetUserTargetID();
         temp_obj.temp_amount = 1;
         RequestPerform(sk->obj.id, temp_list);
      }
      else	//	Target cannot be seen.
         GameMessage(GetString(hInst, IDS_TARGETNOTVISIBLEFORCAST));
      return;
   }

   /* Get target object from user */
   /* Save skill # */
   use_skill = sk->obj.id;

   /* Register callback & select item */
   GameSetState(GAME_SELECT);
   SetSelectCallback(PerformCallback);

   if (UserMouselookIsEnabled())
   {
      while (ShowCursor(TRUE) < 0)
         ShowCursor(TRUE);
   }
}
/********************************************************************/
/*
 * PerformCallback:  Called when user selects target of skill.
 */
void PerformCallback(ID id)
{
   /* Make temporary list for sending to server */
   temp_obj.id = id;
   temp_obj.temp_amount = 1;

   if (pinfo.resting)
      GameMessage(GetString(hInst, IDS_SKILLRESTING));
   else RequestPerform(use_skill, temp_list);

   if (UserMouselookIsEnabled())
   {
      while (ShowCursor(FALSE) >= 0)
         ShowCursor(FALSE);
   }
}
