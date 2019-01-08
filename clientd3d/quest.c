// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * quest.c:  Quest UI for NPCs - see which quests are available,
 *   start new quests and complete quest nodes via UI
 */

#include "client.h"

#define MAX_PURCHASE 500
typedef struct {
   char title[60];
   object_node *obj; /* NPC object */
   list_type quests;
   HWND hwndListBox;
   HFONT hFontTitle;
} QuestDialogStruct;

static QuestDialogStruct *info = NULL;

/* Non-NULL when Quest dialog is displayed */
static HWND hwndQuestDialog = NULL;
static HWND hwndQuestRestrictions = NULL;

static void QuestCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify);
static BOOL CALLBACK QuestDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);
static void HandleSelectionChange(void);
/****************************************************************************/
/*
 * AbortQuestDialog:  Close quest dialog
 */
void AbortQuestDialog(void)
{
   if (hwndQuestDialog == NULL)
      return;

   /* Simulate the user pressing cancel */
   SendMessage(hwndQuestDialog, WM_COMMAND, IDCANCEL, 0);
}
/****************************************************************************/
/*
 * SetFontToFitText:  Fits quest title to dialog box
 */
static int SetFontToFitText(QuestDialogStruct *info, HWND hwnd, int fontNum, const char *text)
{
   HFONT hOldFont;
   LOGFONT newFont;
   int height = 0;
   float ratio;
   RECT rcWindow, rcText;
   BOOL fit = FALSE;
   HDC hdc;
   bool multiLine = FALSE, firstPass = TRUE, allowMultiline = TRUE;

   hdc = GetDC(hwnd);
   if (info->hFontTitle && info->hFontTitle != GetFont(FONT_TITLES))
      DeleteObject(info->hFontTitle);
   info->hFontTitle = NULL;
   memcpy(&newFont, GetLogfont(fontNum), sizeof(LOGFONT));
   GetClientRect(hwnd, &rcWindow);
   ZeroMemory(&rcText, sizeof(rcText));
   while (!fit)
   {
      info->hFontTitle = CreateFontIndirect(&newFont);
      if (info->hFontTitle == NULL)
      {
         info->hFontTitle = CreateFontIndirect(GetLogfont(fontNum));
         break;
      }
      hOldFont = (HFONT)SelectObject(hdc, info->hFontTitle);
      height = DrawText(hdc, text, -1, &rcText, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
      SelectObject(hdc, hOldFont);

      // If the text doesn't fit inside the label but only a small amount would end up
      // on the 2nd line, just shrink it a little bit and don't spread over two lines.
      if (firstPass)
      {
         firstPass = FALSE;
         ratio = (float)rcWindow.right / (float)rcText.right;
         if (ratio > 0.75f)
            allowMultiline = FALSE;
      }

      // To fit, text must be single line and inside label width,
      // or multiline (2 lines max) and within 2*width and height/2.
      if (((!multiLine && rcText.right <= rcWindow.right)
         || (multiLine && rcText.right <= rcWindow.right * 2))
        && (!multiLine || (height <= rcWindow.bottom / 2)))
         fit = TRUE;
      else
      {
         if (allowMultiline)
            multiLine = TRUE;
         if (newFont.lfHeight < -4)
         {
            DeleteObject(info->hFontTitle);
            info->hFontTitle = NULL;
            newFont.lfHeight++;
         }
         else if (newFont.lfHeight > 4)
         {
            DeleteObject(info->hFontTitle);
            info->hFontTitle = NULL;
            newFont.lfHeight--;
         }
         else
            fit = TRUE; // smallest font allowable
      }
   }
   SetWindowFont(hwnd, info->hFontTitle, FALSE);
   ReleaseDC(hwnd, hdc);
   return height;
}
/*****************************************************************************/
/*
 * QuestDialogProc:  Dialog procedure for user selecting one or more objects to buy
 *   from a list of objects.
 *   lParam of the WM_INITDIALOG message should be a pointer to a BuyDialogStruct.
 */
BOOL CALLBACK QuestDialogProc(HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
   static HWND hwndBitmap;
   AREA area;
   RECT dlg_rect;
   HDC hdc;
   int index, yellow_pos = 0, white_pos = 0;
   object_node *quest_obj;
   list_type l = NULL;

   switch (message)
   {
   case WM_INITDIALOG:

      info = (QuestDialogStruct *)lParam;
      SetWindowText(hDlg, info->title);
      hwndBitmap = GetDlgItem(hDlg, IDC_DESCBITMAP);
      CenterWindow(hDlg, GetParent(hDlg));
      info->hwndListBox = GetDlgItem(hDlg, IDC_QUESTLIST);
      // Draw objects in owner-drawn list box
      SetWindowLong(info->hwndListBox, GWL_USERDATA, OD_DRAWOBJ);
      WindowBeginUpdate(info->hwndListBox);
      for (l = info->quests; l != NULL; l = l->next)
      {
         // works because the object is first in struct.
         quest_obj = (object_node *)l->data;
         if (quest_obj->objecttype == OT_QUESTACTIVE)
         {
            index = 0;
            ++yellow_pos;
            ++white_pos;
         }
         else if (quest_obj->objecttype == OT_QUESTVALID)
         {
            index = yellow_pos;
            ++white_pos;
         }
         else
            index = white_pos;

         index = ItemListAddItem(info->hwndListBox, quest_obj, index, False);
      }
      WindowEndUpdate(info->hwndListBox);
      hwndQuestDialog = hDlg;
      hwndQuestRestrictions = GetDlgItem(hDlg, IDC_QUESTNODEDESC);
      ListBox_SetCurSel(info->hwndListBox, 0);
      HandleSelectionChange();
      SendMessage(hwndQuestRestrictions, EM_SETBKGNDCOLOR, FALSE, RGB(240,240,240));
      return TRUE;

   HANDLE_MSG(hDlg, WM_COMMAND, QuestCommand);
   HANDLE_MSG(hDlg, WM_CTLCOLOREDIT, DialogCtlColor);
   HANDLE_MSG(hDlg, WM_CTLCOLORLISTBOX, DialogCtlColor);
   HANDLE_MSG(hDlg, WM_CTLCOLORSTATIC, DialogCtlColor);
   HANDLE_MSG(hDlg, WM_CTLCOLORDLG, DialogCtlColor);
   case WM_DRAWITEM:
      return ItemListDrawItem(hDlg, (const DRAWITEMSTRUCT *)lParam);
   case WM_PAINT:
      InvalidateRect(hwndBitmap, NULL, TRUE);
      UpdateWindow(hwndBitmap);
      /* fall through */

   case BK_ANIMATE:
      /* Draw object's bitmap */
      hdc = GetDC(hwndBitmap);
      GetClientRect(hwndBitmap, &dlg_rect);

      RectToArea(&dlg_rect, &area);
      DrawStretchedObjectGroup(hdc, info->obj, info->obj->animate->group, &area,
         GetSysColorBrush(COLOR_3DFACE));

      ReleaseDC(hwndBitmap, hdc);
      break;
   case WM_COMPAREITEM:
      return ItemListCompareItem(hDlg, (const COMPAREITEMSTRUCT *) lParam);

   case WM_MEASUREITEM:
      ItemListMeasureItem(hDlg, (MEASUREITEMSTRUCT *) lParam);
      return TRUE;

   case WM_DESTROY:
      hwndQuestDialog = NULL;
      hwndQuestRestrictions = NULL;
      return TRUE;
   }

   return FALSE;
}
/*****************************************************************************/
/*
 * HandleSelectionChange:  Changed quest selection.
 */
static void HandleSelectionChange(void)
{
   int index = (int)ListBox_GetCurSel(info->hwndListBox);
   object_node *obj = (object_node *)ListBox_GetItemData(info->hwndListBox, index);

   if (ListBox_GetSel(info->hwndListBox, index))
   {
      for (list_type l = info->quests; l != NULL; l = l->next)
      {
         quest_ui_node *q = (quest_ui_node *)l->data;
         if (q->obj.id == obj->id)
         {
            char *name = LookupNameRsc(q->obj.name_res);
            SetFontToFitText(info, GetDlgItem(hwndQuestDialog, IDC_QUESTNAME),
               (int)FONT_TITLES, name);
            SetDlgItemText(hwndQuestDialog, IDC_QUESTNAME, name);
            HDC hdc = GetDC(hwndQuestDialog);
            SetTextColor(hdc, GetPlayerNameColor(info->obj, name));
            ReleaseDC(hwndQuestDialog, hdc);

            SetDlgItemText(hwndQuestDialog, IDC_QUESTDESC, q->desc);
            SetDlgItemText(hwndQuestDialog, IDC_QUESTNODEDESC, "");
            DisplayMessageQuestRestrictions(q->secondary_desc, GetColor(COLOR_EDITFGD), 0);

            if (q->obj.objecttype == OT_QUESTACTIVE)
            {
               SetDlgItemText(hwndQuestDialog, IDC_QUESTREQLABEL, GetString(hInst, IDS_INSTRUCTIONS));
               SetDlgItemText(hwndQuestDialog, IDOK, GetString(hInst, IDS_CONTINUE));
               EnableWindow(GetDlgItem(hwndQuestDialog, IDOK), TRUE);
            }
            else
            {
               SetDlgItemText(hwndQuestDialog, IDOK, GetString(hInst, IDS_ACCEPT));
               SetDlgItemText(hwndQuestDialog, IDC_QUESTREQLABEL, GetString(hInst, IDS_REQUIREMENTS));
               EnableWindow(GetDlgItem(hwndQuestDialog, IDOK),
                  q->obj.objecttype == OT_QUESTVALID);
            }

            break;
         }
      }
   }
   else
   {
      SetDlgItemText(hwndQuestDialog, IDC_QUESTNAME, "");
      SetDlgItemText(hwndQuestDialog, IDC_QUESTDESC, "");
      SetDlgItemText(hwndQuestDialog, IDC_QUESTNODEDESC, "");
   }
}
/*****************************************************************************/
/*
 * QuestRestrictionsAddText:  Appends text to the quest restrictions editbox.
 */
void QuestRestrictionsAddText(char *message, int color, int style)
{
   int txtlen, msglen;

   txtlen = Edit_GetTextLength(hwndQuestRestrictions);
   msglen = strlen(message);

   /* Append new message */
   Edit_SetSel(hwndQuestRestrictions, txtlen, txtlen);

   if (config.colorcodes)
   {
      CHARFORMAT cformat;

      // Draw text in given color and style
      memset(&cformat, 0, sizeof(cformat));
      cformat.cbSize = sizeof(cformat);
      cformat.dwMask = CFM_COLOR;
      cformat.crTextColor = color;

      cformat.dwMask |= CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT | CFM_LINK;

      if (style & STYLE_BOLD)
         cformat.dwEffects |= CFE_BOLD;
      if (style & STYLE_ITALIC)
         cformat.dwEffects |= CFE_ITALIC;
      if (style & STYLE_UNDERLINE)
         cformat.dwEffects |= CFE_UNDERLINE;
      if (style & STYLE_STRIKEOUT)
         cformat.dwEffects |= CFE_STRIKEOUT;
      if (style & STYLE_LINK)
         cformat.dwEffects |= CFE_LINK;

      SendMessage(hwndQuestRestrictions, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cformat);
   }

   Edit_ReplaceSel(hwndQuestRestrictions, message);
}
/*****************************************************************************/
/*
 * QuestCommand:  Handle WM_COMMAND messages.
 */ 
void QuestCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify) 
{
   int index;
   object_node *obj;

   switch(id)
   {
   case IDC_QUESTLIST:
      if (codeNotify == LBN_SELCHANGE)
         HandleSelectionChange();
      break;
   case IDOK:
      // Send NPC ID and quest template object ID to server.
      index = (int)ListBox_GetCurSel(info->hwndListBox);
      obj = (object_node *)ListBox_GetItemData(info->hwndListBox, index);
      if (obj != NULL)
         RequestTriggerQuest(info->obj->id, obj->id);

      EndDialog(hDlg, IDOK);
      break;

   case ID_QUESTHELP:
      Info(hInst, hwndQuestDialog, IDS_QUESTHELP_TITLE, IDS_QUESTHELP);
      break;

   case IDCANCEL:
      EndDialog(hDlg, IDCANCEL);
      break;
   }
}
/*****************************************************************************/
/*
 * QuestList:  Server just sent us quest data for an NPC.
 *   Bring up quest UI dialog, populate with available quests.
 *   obj is NPC giving the quests
 *   quests is list of [quest template object, quest description, secondary text]
 *   secondary text is either quest node description or quest restriction list
 */
void QuestList(object_node *obj, list_type quests)
{
   QuestDialogStruct dlg_info;

   dlg_info.obj = obj;
   dlg_info.quests = quests;
   sprintf(dlg_info.title, "Quests - %s", LookupNameRsc(obj->name_res));
   if (hwndQuestDialog == NULL)
      DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_NPCQUESTS), hMain, QuestDialogProc, (LPARAM) &dlg_info);
}
