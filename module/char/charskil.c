// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * charskil.c:  Handle skills tab page.
 */

#include "client.h"
#include "char.h"

static HWND hPoints;              // Handle of "points left" graph control
static HWND hList1, hList2;       // Handles of available and chosen skill list boxes

extern list_type skills;

static void MaybeEnableAddButton(HWND hDlg);
static void CharSkillsInit(HWND hDlg);
void CharSkillsCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNoify);
/********************************************************************/
BOOL CALLBACK CharSkillsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   switch (message)
   {
   case WM_INITDIALOG:
      CharSkillsInit(hDlg);
      break;

   case WM_NOTIFY:
      switch (((LPNMHDR) lParam)->code)
      {
      case PSN_SETACTIVE:
	 SendMessage(hPoints, GRPH_POSSET, 0, spell_points);
	 break;
      }
      return TRUE;      

      HANDLE_MSG(hDlg, WM_COMMAND, CharSkillsCommand);
   }
   return FALSE;
}
/********************************************************************/
void CharSkillsInit(HWND hDlg)
{
   list_type l;
   int index;

   hPoints = GetDlgItem(hDlg, IDC_POINTSLEFT);
   SendMessage(hPoints, GRPH_COLORSET, GRAPHCOLOR_BAR, GetColor(COLOR_BAR2));
   SendMessage(hPoints, GRPH_COLORSET, GRAPHCOLOR_BKGND, GetColor(COLOR_BAR3));
   SendMessage(hPoints, GRPH_RANGESET, 0, SPELL_POINTS_INITIAL);

   // Set skill list contents
   hList1 = GetDlgItem(hDlg, IDC_SKILLIST1);
   hList2 = GetDlgItem(hDlg, IDC_SKILLIST2);
   for (l = skills; l != NULL; l = l->next)
   {
      Skill *s = (Skill *) (l->data);

      index = ListBox_AddString(hList1, s->list_str);
      ListBox_SetItemData(hList1, index, s);
   }

   ListBox_SetCurSel(hList1, 0);
   FORWARD_WM_COMMAND(hDlg, IDC_SKILLIST1, hList1, LBN_SELCHANGE, CharSkillsDialogProc);
}
/********************************************************************/
/*
 * CharSkillsCommand:  Handle WM_COMMAND messages.
 */
void CharSkillsCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
   int index1, index2, i, iCount;
   Skill *s;
   char temp[MAXAMOUNT + 1];

   UserDidSomething();

   switch (id)
   {
   case IDC_ADDSKILL:
      index1 = ListBox_GetCurSel(hList1);
      if (index1 == LB_ERR)
	 break;
      
      s = (Skill *) ListBox_GetItemData(hList1, index1);

      // See if enough points to get skill
      if (s->cost > spell_points)
	 break;
      spell_points -= s->cost;
      SendMessage(hPoints, GRPH_POSSET, 0, spell_points);

      index2 = ListBox_AddString(hList2, s->list_str);
      ListBox_SetItemData(hList2, index2, s);
      s->chosen = True;
      ListBox_DeleteString(hList1, index1);

      ListBox_SetCurSel(hList1, min(index1, ListBox_GetCount(hList1) - 1));
      FORWARD_WM_COMMAND(hwnd, IDC_SKILLIST1, hList1, LBN_SELCHANGE, CharSkillsDialogProc);
      break;

   case IDC_REMOVESKILL:
      index2 = ListBox_GetCurSel(hList2);
      if (index2 == LB_ERR)
	 break;
      
      s = (Skill *) ListBox_GetItemData(hList2, index2);

      spell_points += s->cost;
      SendMessage(hPoints, GRPH_POSSET, 0, spell_points);

      index1 = ListBox_AddString(hList1, s->list_str);
      ListBox_SetItemData(hList1, index1, s);
      s->chosen = False;
      ListBox_DeleteString(hList2, index2);

      ListBox_SetCurSel(hList2, min(index2, ListBox_GetCount(hList2) - 1));
      FORWARD_WM_COMMAND(hwnd, IDC_SKILLIST2, hList2, LBN_SELCHANGE, CharSkillsDialogProc);

      // Removal of this skill may result in the removal of dependent skills.
      if (s->cost == 10)
      {
         // Let's see how many rank 1 spells of this school are left.
         iCount = 0;

         for (i = 0; i < ListBox_GetCount(hList2); ++i)
         {
            s = (Skill *) ListBox_GetItemData(hList2, i);

            if (s->cost == 10)
               ++iCount;
         }

         // Uh oh, we can't support those rank 2 skills anymore.
         if (iCount < 2)
         {
            for (i = 0; i < ListBox_GetCount(hList2); ++i)
            {
               s = (Skill *) ListBox_GetItemData(hList2, i);

               if (s->cost == 25)
               {
                  spell_points += s->cost;
                  SendMessage(hPoints, GRPH_POSSET, 0, spell_points);
                  index1 = ListBox_AddString(hList1, s->list_str);
                  ListBox_SetItemData(hList1, index1, s);
                  s->chosen = False;
                  ListBox_DeleteString(hList2, i);
                  ListBox_SetCurSel(hList2, min(i, ListBox_GetCount(hList2) - 1));
                  --i;
                  FORWARD_WM_COMMAND(hwnd, IDC_SKILLIST2, hList2, LBN_SELCHANGE, CharSkillsDialogProc);
               }
            }
         }
      }

      MaybeEnableAddButton(hwnd);
      break;

   case IDC_SKILLIST1:
      if (codeNotify != LBN_SELCHANGE)
	 break;

      index1 = ListBox_GetCurSel(hList1);
      if (index1 == LB_ERR)
	 break;
      
      s = (Skill *) ListBox_GetItemData(hList1, index1);
      SetDlgItemText(hwnd, IDC_SKILLINFO, LookupNameRsc(s->desc_res));
      sprintf(temp, "%d", s->cost);
      SetDlgItemText(hwnd, IDC_COST2, temp);

      MaybeEnableAddButton(hwnd);
      break;

   case IDC_SKILLIST2:
      if (codeNotify != LBN_SELCHANGE)
	 break;

      index2 = ListBox_GetCurSel(hList2);
      if (index2 == LB_ERR)
	 break;
      
      s = (Skill *) ListBox_GetItemData(hList2, index2);
      SetDlgItemText(hwnd, IDC_SKILLINFO, LookupNameRsc(s->desc_res));
      sprintf(temp, "%d", s->cost);
      SetDlgItemText(hwnd, IDC_COST2, temp);
      break;

   default:
      CharTabPageCommand(hwnd, id, hwndCtl, codeNotify);
      return;
   }
}

/********************************************************************/
/*
 * MaybeEnableAddButton: Enable/disable the "add spell" button
 *   depending on whether the currently selected spell in the available
 *   list box can be chosen.
 */
void MaybeEnableAddButton(HWND hDlg)
{
   int i;
   int iCount = 0;
   Skill *s;
   BOOL enable = TRUE;
   int index = ListBox_GetCurSel(hList1);

   if (index != LB_ERR)
   {
      // Count how many level 1 skills we have.
      for (i = 0; i < ListBox_GetCount(hList2); ++i)
      {
         s = (Skill *) ListBox_GetItemData(hList2, i);

         if (s->cost == 10)
            ++iCount;
      }

      s = (Skill *) ListBox_GetItemData(hList1, index);

      // If we don't have the required level 1 skills, disable button
      if (s->cost == 25 && iCount < 2)
         enable = FALSE;
   }

   EnableWindow(GetDlgItem(hDlg, IDC_ADDSKILL), enable);
}
