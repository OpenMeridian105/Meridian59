// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * quest.h:  Header file for quest.c
 */

#ifndef _QUEST_H
#define _QUEST_H

void AbortQuestDialog(void);
void QuestList(object_node *obj, list_type quests);
void QuestRestrictionsAddText(char *message, int color, int style);

#endif /* #ifndef _QUEST_H */
