// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * charmake.c:  Manage main character creation tabbed dialog.
 */

#include "client.h"
#include "char.h"

CharAppearance *ap;     // Current appearance settings and choices
list_type spells;       // List of available spells
list_type skills;       // List of available skills

// Info for each tab of tabbed dialog
typedef struct {
   int     name;          // Resource id of name
   int     template_id;   // Resource id of dialog template
   DLGPROC dialog_proc;   // Dialog procedure for tab
} TabPage;

// Window handles of modeless dialogs, one per tab
TabPage tab_pages[] = {
{ IDS_CHAR_NAME,      IDD_CHARNAME,       CharNameDialogProc,   },
{ IDS_CHARAPPEARANCE, IDD_CHARAPPEARANCE, CharFaceDialogProc,   },
{ IDS_CHARSTATS,      IDD_CHARSTATS,      CharStatsDialogProc,  },
{ IDS_CHARSPELLS,     IDD_CHARSPELLS,     CharSpellsDialogProc, },
{ IDS_CHARSKILLS,     IDD_CHARSKILLS,     CharSkillsDialogProc, },
};

#define NUM_TAB_PAGES (sizeof(tab_pages) / sizeof(TabPage))

// Constants for indices into tab_pages
#define TAB_NAME       0
#define TAB_APPEARANCE 1
#define TAB_STATS      2
#define TAB_SPELLS     3
#define TAB_SKILLS     4

HWND hMakeCharDialog = NULL;

int spell_points;                       // Number of points left

static HWND hTab;      // Handle of tab control
static HWND hTabPage;  // Handle of currently active child modeless dialog, depending on active tab

static Bool enter_game;// True when user fills in dialog and hits OK

extern ID    char_to_use;     /* ID # of character player wants to use in game */
extern char  name_to_use[];   /* name of character player wants to use in game */

static int  CALLBACK MakeCharSheetInit(HWND hDlg, UINT uMsg, LPARAM lParam);
/********************************************************************/
/*
 * MakeChar:  Bring up character creation dialog.
 */
void MakeChar(CharAppearance *ap_init, list_type spells_init, list_type skills_init)
{
   int i;
   PROPSHEETHEADER psh;
   PROPSHEETPAGE psp[NUM_TAB_PAGES];

   ap = ap_init;
   spells = spells_init;
   skills = skills_init;

   // Prepare property sheet header
   psh.dwSize = sizeof(PROPSHEETHEADER);
   psh.dwFlags = PSH_PROPSHEETPAGE | PSH_USECALLBACK | PSH_NOAPPLYNOW;
   psh.hwndParent = cinfo->hMain;
   psh.hInstance = hInst;
   psh.pszCaption = GetString(hInst, IDS_DIALOGTITLE);
   psh.nPages = 0;
   psh.nStartPage = 0;
   psh.ppsp = (LPCPROPSHEETPAGE) &psp;
   psh.pfnCallback = MakeCharSheetInit;

   // Prepare property sheets
   for (i=0; i < NUM_TAB_PAGES; i++)
   {
      psh.nPages++;
      psp[i].dwSize = sizeof(PROPSHEETPAGE);
      psp[i].dwFlags = PSP_USETITLE;
      psp[i].hInstance = hInst;
      psp[i].pszTemplate = MAKEINTRESOURCE(tab_pages[i].template_id);
      psp[i].pfnDlgProc = (DLGPROC) tab_pages[i].dialog_proc;
      psp[i].pszTitle = MAKEINTRESOURCE(tab_pages[i].name);
      psp[i].lParam = i;
      psp[i].pfnCallback = NULL;
   }
   
   enter_game = False;

   PropertySheet(&psh);   

   if (!enter_game)
      /* Go back to main menu */
      RequestQuit();

   CharFaceExit();
   ap = CharAppearanceDestroy(ap);
   list_destroy(spells);
   list_destroy(skills);

   if (exiting)
      PostMessage(cinfo->hMain, BK_MODULEUNLOAD, 0, MODULE_ID);
}
/*******************************************************************/
/*
 * VerifySettings:  User has pressed Done button; check values of all settings.
 *   If settings are OK, send them to server.
 */
Bool VerifySettings(void)
{
   char name[MAX_CHARNAME], *new_name, desc[MAX_DESCRIPTION];
   int stats[NUM_CHAR_STATS], parts[NUM_FACE_OVERLAYS + 1];
   list_type l, spells_send, skills_send;
   BYTE gender;

   // Fill in name and description
   CharNameGetChoices(name, desc);

   /* Verify that username is ok */
   new_name = VerifyCharName(name);
   if (new_name == NULL)
   {
      ClientError(hInst, hMakeCharDialog, IDS_BADCHARNAME, MIN_CHARNAME);
      PropSheet_SetCurSel(hTab, 0, TAB_NAME);
      return False;
   }

   // If stats or spell/skill points remain, ask user to continue
   if (CharStatsGetPoints() > 0 && !AreYouSure(hInst, hMakeCharDialog, NO_BUTTON, IDS_STATPOINTSLEFT))
      return False;

   // Spell points check
   if (spell_points > 0 && !AreYouSure(hInst, hMakeCharDialog, NO_BUTTON, IDS_SPELLPOINTSLEFT))
      return False;

   // Fill in face bitmaps
   CharFaceGetChoices(ap, parts, &gender);

   // Fill in stat values
   CharStatsGetChoices(stats);

   // Build up list of chosen spells and skills
   spells_send = NULL;
   for (l = spells; l != NULL; l = l->next)
   {
      Spell *s = (Spell *)(l->data);
      if (s->chosen)
         spells_send = list_add_item(spells_send, (void *)s->id);
   }

   skills_send = NULL;
   for (l = skills; l != NULL; l = l->next)
   {
      Skill *s = (Skill *)(l->data);
      if (s->chosen)
         skills_send = list_add_item(skills_send, (void *)s->id);
   }

   // Send char info to server
   SendNewCharInfo(char_to_use, new_name, desc, gender, NUM_FACE_OVERLAYS + 1, parts,
      ap->hair_translations[ap->hairt_choice],
      ap->face_translations[ap->facet_choice],
      NUM_CHAR_STATS, stats, spells_send, skills_send);

   list_delete(spells_send);
   list_delete(skills_send);

   EnableWindow(GetDlgItem(hMakeCharDialog, IDOK), FALSE);
   return True;
}

/********************************************************************/
int CALLBACK MakeCharSheetInit(HWND hDlg, UINT uMsg, LPARAM lParam)
{
   int style;
  
   if (uMsg != PSCB_INITIALIZED)
      return 0;

   // Remove context-sensitive help button
   style = GetWindowLong(hDlg, GWL_EXSTYLE);
   SetWindowLong(hDlg, GWL_EXSTYLE, style & (~WS_EX_CONTEXTHELP));

   hMakeCharDialog = hDlg;
   hTab = PropSheet_GetTabControl(hDlg);

   spell_points = SPELL_POINTS_INITIAL;

   CharFaceInit();
   return 0;
}
/********************************************************************/
void CharTabPageCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
   UserDidSomething();

   switch (id)
   {
   case IDC_PREVIOUS_PAGE:
      PropSheet_SetCurSel(hMakeCharDialog, 0, TabCtrl_GetCurSel(hTab) - 1);
      return;

   case IDC_NEXT_PAGE:
      PropSheet_SetCurSel(hMakeCharDialog, 0, TabCtrl_GetCurSel(hTab) + 1);
      return;
   }
}
/********************************************************************/
void CharInfoValid(void)
{
   enter_game = True;
   if (hMakeCharDialog != NULL)
      EndDialog(hMakeCharDialog, IDOK);

   /* Player has made new character; start game with this char */
   GoToGame(char_to_use);
}
/********************************************************************/
void CharInfoInvalid(BYTE err_num)
{
   int resource_num;
   if (hMakeCharDialog != NULL)
   {
      switch (err_num)
      {
      case CC_NOT_FIRST_TIME: resource_num = IDS_NOTFIRSTTIME; break;
      case CC_NAME_TOO_LONG: resource_num = IDS_NAMETOOLONG; break;
      case CC_NAME_BAD_CHARACTERS: resource_num = IDS_NAMEBADCHARS; break;
      case CC_NAME_IN_USE: resource_num = IDS_CHARNAMEUSED; break;
      case CC_NO_MOB_NAME: resource_num = IDS_NOMOBNAME; break;
      case CC_NO_NPC_NAME: resource_num = IDS_NONPCNAME; break;
      case CC_NO_GUILD_NAME: resource_num = IDS_NOGUILDNAME; break;
      case CC_NO_BAD_WORDS: resource_num = IDS_NOBADWORDS; break;
      case CC_NO_CONFUSING_NAME: resource_num = IDS_NOCONFUSINGNAME; break;
      case CC_RETIRED_NAME: resource_num = IDS_RETIREDNAME; break;
      case CC_DESC_TOO_LONG: resource_num = IDS_DESCTOOLONG; break;
      case CC_INVALID_GENDER: resource_num = IDS_INVALIDGENDER; break;
      default: resource_num = IDS_GENERICERROR; break;
      }

      ClientError(hInst, hMakeCharDialog, resource_num);
      EnableWindow(GetDlgItem(hMakeCharDialog, IDOK), TRUE);
   }
}

