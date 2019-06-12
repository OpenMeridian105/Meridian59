// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * statcach.c:  Maintain a cache of statistics, so that we don't have to ask the server
 *   for stats every time the user changes stat groups.
 *
 *   Cache entries are referenced by group numbers, which start at 1, although internally
 *   they are kept in an array which starts at 0.
 */

#include "client.h"
#include "merintr.h"

typedef struct {
   Bool      valid;                      // True when entry is valid
   list_type stats;                      // A group of stats
} StatCacheEntry;

typedef struct {
   int             size;                 // Number of cache entries
   StatCacheEntry *entries;
} StatCache;

static StatCache stat_cache;        // Cache stat groups we've retrieved from server already

/* local function prototypes */
static Bool CompareStats(void *s1, void *s2);
Bool CompareListStatsByObjID(void *s1, void *s2);
Statistic *StatCacheInsertListStat(BYTE group, Statistic *s);
/************************************************************************/
void StatCacheCreate(void)
{
   stat_cache.size = 0;   
}
/************************************************************************/
void StatCacheDestroy(void)
{
   int i;

   for (i=0; i < stat_cache.size; i++)
      if (stat_cache.entries[i].valid)
	 list_destroy(stat_cache.entries[i].stats);

   if (stat_cache.entries != NULL)
      SafeFree(stat_cache.entries);

   stat_cache.size    = 0;
   stat_cache.entries = NULL;
}
/************************************************************************/
/*
 * CompareStats: Compare two statistics based on num field.
 */
Bool CompareStats(void *s1, void *s2)
{
   return ((Statistic *) s1)->num == ((Statistic *) s2)->num;
}
/************************************************************************/
/*
 * CompareStatsByObjID: Compare two list statistics based on object ID.
 */
Bool CompareListStatsByObjID(void *s1, void *s2)
{
   return ((Statistic *)s1)->list.id == ((Statistic *)s2)->list.id;
}
/************************************************************************/
/*
 * StatCacheGetEntry:  If cache contains a valid entry for given group,
 *   set stats to it and return True.
 *   Otherwise, return False.
 */
Bool StatCacheGetEntry(int group, list_type *stats)
{
   if (group > stat_cache.size)
      return False;

   group--;    // First group is array index 0
   if (!stat_cache.entries[group].valid)
      return False;

   *stats = stat_cache.entries[group].stats;
   return True;
}
/************************************************************************/
/*
 * StatCacheSetEntry:  Set the given group in the cache to the given list of stats.
 */
void StatCacheSetEntry(int group, list_type stats)
{
   if (group > stat_cache.size)
   {
      debug(("StatCacheSetEntry got entry too large %d; size is %d\n", group, stat_cache.size));
      return;
   }

   group--;    // First group is array index 0

   // Destroy previous stats, if any
   if (stat_cache.entries[group].valid)
      list_destroy(stat_cache.entries[group].stats);
   
   stat_cache.entries[group].stats = stats;
   stat_cache.entries[group].valid = True;
}
/************************************************************************/
/*
 * StatCacheSetSize:  Clear out stat cache and set its size
 */
void StatCacheSetSize(int size)
{
   int i;

   StatCacheDestroy();
   
   stat_cache.size = size;
   stat_cache.entries = (StatCacheEntry *) SafeMalloc(size * sizeof(StatCacheEntry));
   for (i=0; i < size; i++)
   {
      stat_cache.entries[i].valid = False;
      stat_cache.entries[i].stats = NULL;
   }
}
/************************************************************************/
/*
 * StatCacheUpdate:  Given stat in group has changed; update cache and return
 *   resultant Statistic structure or NULL if stat could not found or added.
 */
Statistic *StatCacheUpdate(BYTE group, Statistic *s)
{
   Statistic *old_stat;
   list_type stats;

   if (group > stat_cache.size || !stat_cache.entries[group-1].valid)
      return NULL;

   stats = stat_cache.entries[group-1].stats;

   /* Find old stat in list */
   if (group == STATS_SPELLS || group == STATS_SKILLS)
   {
      old_stat = (Statistic *)list_find_item(stats, s, CompareListStatsByObjID);
      if (old_stat == NULL)
      {
         return StatCacheInsertListStat(group, s);
      }
   }
   else
   {
      old_stat = (Statistic *)list_find_item(stats, s, CompareStats);
   }

   if (old_stat == NULL)
   {
      debug(("Tried to change nonexistent stat #%d in group #%d", s->num, (int) group));
      return NULL;
   }

   old_stat->name_res = s->name_res;
   old_stat->type     = s->type;
   switch (s->type)
   {
   case STATS_NUMERIC:
      memcpy(&old_stat->numeric, &s->numeric, sizeof(old_stat->numeric));
      break;

   case STATS_LIST:
      memcpy(&old_stat->list, &s->list, sizeof(old_stat->list));
      break;

   default:
      debug(("StatsCacheUpdate got illegal stat type %d\n", (int) s->type));
      break;
   }
   return old_stat;
}
/************************************************************************/
/*
 * StatCacheInsertListStat:  Received a new stat for group; update cache
 *   and return resultant Statistic structure.
 */
Statistic *StatCacheInsertListStat(BYTE group, Statistic *s)
{
   Statistic *new_stat = (Statistic *)SafeMalloc(sizeof(Statistic));
   new_stat->name_res = s->name_res;
   new_stat->type = s->type;
   new_stat->num = s->num;

   memcpy(&new_stat->list, &s->list, sizeof(new_stat->list));

   list_type stats = stat_cache.entries[group - 1].stats;

   // Inserting, so renumber any entries with a greater num value.
   for (list_type l = stats; l != NULL; l = l->next)
   {
      Statistic *ls = (Statistic *)l->data;
      if (ls->num >= s->num)
         ls->num++;
   }
 
   stat_cache.entries[group - 1].stats = list_add_item(stats, new_stat);

   // Gets displayed later, but we need to fully update it here to
   // include the inserted stat. Display stat system likely needs
   // some refactoring to better handle inserts/deletes.
   if (group != StatsGetCurrentGroup())
   {
      //	ajw Changes to make Inventory act somewhat like one of the stats groups.
      if (StatsGetCurrentGroup() == STATS_INVENTORY)
      {
         //	Inventory must be going away.
         ShowInventory(False);
      }
   }
   DisplayStatGroup(group, stat_cache.entries[group - 1].stats);

   return new_stat;
}
/************************************************************************/
/*
 * StatCacheRemoveListStat:  Called when server tells us to remove a skill/spell stat.
 */
void StatCacheRemoveListStat(BYTE group, int object_id)
{
   int num = -1, count = 0;

   list_type stats = stat_cache.entries[group - 1].stats;

   for (list_type l = stats; l != NULL; l = l->next, count++)
   {
      Statistic *s = (Statistic*)l->data;
      if (s->list.id == object_id)
      {
         num = s->num;
         stat_cache.entries[group - 1].stats = list_delete_item(stats, s, CompareStats);
         stats = stat_cache.entries[group - 1].stats;
         SafeFree(s);
         break;
      }
   }

   if (num == -1)
   {
      debug(("Could not find list stat object ID %i to remove in stat group %i\n",
         object_id, group));
      return;
   }

   // Renumber existing num values greater than the removed one.
   for (list_type l = stats; l != NULL; l = l->next)
   {
      Statistic *s = (Statistic*)l->data;
      if (s->num > num)
      {
         s->num--;
      }
   }

   if (group != StatsGetCurrentGroup())
   {
      //	ajw Changes to make Inventory act somewhat like one of the stats groups.
      if (StatsGetCurrentGroup() == STATS_INVENTORY)
      {
         //	Inventory must be going away.
         ShowInventory(False);
      }
   }
   DisplayStatGroup(group, stats);
}
