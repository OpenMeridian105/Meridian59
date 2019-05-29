// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * skills.h:  Header file for skills.c
 */

#ifndef _SKILLS_H
#define _SKILLS_H

 // Return values for FindSkillByName
#define SKILL_NOMATCH    ((skill *) 0)
#define SKILL_AMBIGUOUS  ((skill *) -1)

typedef struct {
   object_node obj;
   BYTE        num_targets;
   BYTE        school;
   BYTE        is_active;
} skill;

void SkillsInit(void);
void SkillsExit(void);

void SkillsGotSchools(BYTE num, ID *schools);
void SetSkills(list_type new_skills);
void FreeSkills(void);
void AddSkill(skill *sp);
void RemoveSkill(ID id);
skill *FindSkillByName(char *name);
skill *FindSkillByID(ID id);
char *GetSkillName(char *str, char **next);

void UserPerformSkill(void);
void SkillPerform(skill *sp);
void MenuSkillChosen(int index);

object_node* GetSkillObject( ID idFind );

#endif