// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * bgfedit.c:  Live edit BGF file data (but not save).
 */

#include "client.h"
#include "dm.h"

static HWND hBGFEditorDlg = NULL;
static BOOL bBGFEditorHidden = FALSE;

// Client's loaded BGF list.
static list_type bgf_list = NULL;
// Last BGF ID selected (i.e. current selection).
static ID last_selected = 0;
// Whether we are in the process of selecting a BGF (and filling list views).
static BOOL isSelecting = FALSE;
// Global to store selected subitem in frame list view.
static LVITEM lvFrameItem;

// Globals for editing list view values.
static WNDPROC     wpOrigEditProc;
static RECT        rcSubItem;
// Global for subclassed edit box procedure.
static WNDPROC     shrinkEditProc;

// List of originals before they are modified.
static list_type original_cache;

static void OnBGFEditorCommand(HWND hDlg, int cmd_id, HWND hwndCtl, UINT codeNotify);
BOOL CALLBACK BGFEditorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcEditList(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcEditShrink(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void SetFrameOffset(int frame, int column, int value);
static void CacheOriginal(object_bitmap_type bmp);
static void CacheRestore(ID id);
static void ShowSelectedFrameData(HWND hDlg);
static void LoadBGFList(HWND hDlg);

/****************************************************************************/
/*
 * IdCacheBitmapCompare:  Comparator function for IDs.
 */
Bool IdCacheBitmapCompare(void *idnum, void *b)
{
   return *((ID *)idnum) == ((bitmap_original_data)b)->idnum;
}
/****************************************************************************/
/*
 * GetBGFEditorDlg:  Return the dialog HWND or NULL.
 */
HWND GetBGFEditorDlg()
{
   return (bBGFEditorHidden ? NULL : hBGFEditorDlg);
}
/****************************************************************************/
/*
 * ShowBGFEditorDlg:  Display the BGF edit dialog, creating one if it doesn't
 *   exist.
 */
void ShowBGFEditorDlg()
{
   if (!hBGFEditorDlg)
   {
      hBGFEditorDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_BGFEDIT),
         cinfo->hMain, BGFEditorDialogProc);
      shrinkEditProc = SubclassWindow(GetDlgItem(hBGFEditorDlg, IDC_SHRINKFACTOR), WndProcEditShrink);
   }

   if (hBGFEditorDlg)
   {
      ShowWindow(hBGFEditorDlg, SW_SHOWNORMAL);
      bBGFEditorHidden = FALSE;
   }
}
/****************************************************************************/
/*
 * WndProcEditShrink:  Handler for shrink editbox subclass.
 */
LRESULT CALLBACK WndProcEditShrink(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   char buffer[32];

   switch (uMsg)
   {
   case WM_KEYDOWN:
      // If holding shift and right or left arrow, increment or decrement
      // value in edit box.
      if (((GetKeyState(VK_SHIFT) & 0x8000) != 0)
         && (wParam == VK_LEFT || wParam == VK_RIGHT))
      {
         GetWindowText(hDlg, buffer, 32);
         int value = atoi(buffer);
         if (value < 1 || (wParam == VK_LEFT && value == 1))
            break;
         sprintf(buffer, "%i", value + (1 * wParam == VK_LEFT ? -1 : 1));
         SetWindowText(hDlg, buffer);
      }
      break;

   default:
      return CallWindowProc(shrinkEditProc, hDlg, uMsg, wParam, lParam);
   }
   return TRUE;
}
/****************************************************************************/
/*
 * WndProcEditList:  Handler for editable list view subclass.
 */
LRESULT CALLBACK WndProcEditList(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   char buffer[32];

   switch (uMsg)
   {
   case WM_WINDOWPOSCHANGING:
   {
      WINDOWPOS *pos = (WINDOWPOS*)lParam;

      pos->x = rcSubItem.left;
      pos->cx = rcSubItem.right;
   }
   break;

   case WM_KEYDOWN:
      // If holding shift and right or left arrow, increment or decrement
      // value in edit box.
      if (((GetKeyState(VK_SHIFT) & 0x8000) != 0)
         && (wParam == VK_LEFT || wParam == VK_RIGHT))
      {
         GetWindowText(hDlg, buffer, 32);
         int value = atoi(buffer) + (1 * wParam == VK_LEFT ? -1 : 1);
         sprintf(buffer, "%i", value);
         SetWindowText(hDlg, buffer);
         SetFrameOffset(lvFrameItem.iItem, lvFrameItem.iSubItem, value);
      }
      break;

   case WM_CHAR:
      // Only allow numbers, movement (arrow keys etc), deleting and '-'.
      if (!(wParam == VK_BACK
         || wParam == VK_RETURN
         || wParam == VK_OEM_PLUS
         || wParam == VK_OEM_MINUS
         || wParam == VK_SUBTRACT
         || (wParam >= VK_END && wParam <= VK_DELETE)
         || (wParam >= '0' && wParam <= '9'))) {
         return FALSE;
      }

   default:
      return CallWindowProc(wpOrigEditProc, hDlg, uMsg, wParam, lParam);
   }
   return TRUE;
}
/****************************************************************************/
/*
 * BGFEditorDialogProc:  Handler for BGF editor dialog.
 */
BOOL CALLBACK BGFEditorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   HWND hList;
   LV_COLUMN lvcol;
   static HWND hEdit;
   static BOOL bEditing = FALSE;
   UINT code;

   hList = GetDlgItem(hDlg, IDC_FRAMEDATA);

   switch (message)
   {
   case WM_INITDIALOG:

      SetWindowFont(GetDlgItem(hDlg, IDC_BGFLIST), GetFont(FONT_LIST), FALSE);
      // Set up BGF list
      LoadBGFList(hDlg);
      
      // Set up Frame list
      hList = GetDlgItem(hDlg, IDC_FRAMEDATA);
      SetWindowFont(hList, GetFont(FONT_LIST), FALSE);
      // Columns
      ListView_SetExtendedListViewStyleEx(hList, LVS_EX_FULLROWSELECT,
         LVS_EX_FULLROWSELECT);
         
      // Add column headings
      lvcol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
      lvcol.pszText = "Index";
      lvcol.cx = 40;
      lvcol.fmt = LVCFMT_CENTER;
      ListView_InsertColumn(hList, 0, &lvcol);
      lvcol.pszText = "Width";
      lvcol.cx = 46;
      ListView_InsertColumn(hList, 1, &lvcol);
      lvcol.pszText = "Height";
      lvcol.cx = 46;
      ListView_InsertColumn(hList, 2, &lvcol);
      lvcol.pszText = "XOffset";
      lvcol.cx = 54;
      ListView_InsertColumn(hList, 3, &lvcol);
      lvcol.pszText = "YOffset";
      lvcol.cx = 54;
      ListView_InsertColumn(hList, 4, &lvcol);

      CenterWindow(hDlg, cinfo->hMain);
      return TRUE;

   case WM_ACTIVATE:
      *cinfo->hCurrentDlg = (wParam == 0) ? NULL : hDlg;
      return TRUE;

   case WM_NOTIFY:
      code = (UINT)-1 - ((NMHDR*)lParam)->code;
      switch (((NMHDR*)lParam)->code)
      {
      case NM_CLICK:
         lvFrameItem.iItem = ((NMITEMACTIVATE*)lParam)->iItem;
         lvFrameItem.iSubItem = ((NMITEMACTIVATE*)lParam)->iSubItem;

         break;

      case NM_DBLCLK:
         SendMessage(hList, LVM_EDITLABEL, lvFrameItem.iItem, 0);
         break;

      case LVN_BEGINLABELEDIT:
         if (lvFrameItem.iSubItem == 0)
         {
            SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
            return TRUE;
         }
         char text[32];
         bEditing = TRUE;
         hEdit = (HWND)SendMessage(hList, LVM_GETEDITCONTROL, 0, 0);
         rcSubItem.top = lvFrameItem.iSubItem;
         rcSubItem.left = LVIR_LABEL;
         SendMessage(hList, LVM_GETSUBITEMRECT, lvFrameItem.iItem, (long)&rcSubItem);
         rcSubItem.right = SendMessage(hList, LVM_GETCOLUMNWIDTH, lvFrameItem.iSubItem, 0);
         wpOrigEditProc = (WNDPROC)SetWindowLong(hEdit, GWL_WNDPROC, (long)WndProcEditList);
         lvFrameItem.pszText = text;
         lvFrameItem.cchTextMax = 32;
         SendMessage(hList, LVM_GETITEMTEXT, lvFrameItem.iItem, (long)&lvFrameItem);
         SetWindowText(hEdit, lvFrameItem.pszText);
         break;

      case LVN_ENDLABELEDIT:
         bEditing = 0;
         SetWindowLong(hEdit, GWL_WNDPROC, (long)wpOrigEditProc);
         if (!lvFrameItem.iSubItem)
            return TRUE;
         lvFrameItem.pszText = ((NMLVDISPINFO*)lParam)->item.pszText;
         if (!lvFrameItem.pszText)
            return TRUE;
         SendMessage(hList, LVM_SETITEMTEXT, lvFrameItem.iItem, (long)&lvFrameItem);
         SetFrameOffset(lvFrameItem.iItem, lvFrameItem.iSubItem, atoi(lvFrameItem.pszText));
         break;
      }
      return FALSE;

   HANDLE_MSG(hDlg, WM_COMMAND, OnBGFEditorCommand);

   case WM_DESTROY:
      hBGFEditorDlg = NULL;
      bBGFEditorHidden = FALSE;
      if (exiting)
         PostMessage(cinfo->hMain, BK_MODULEUNLOAD, 0, MODULE_ID);
      return TRUE;
   }

   return FALSE;
}
/****************************************************************************/
/*
 * SetFrameOffset:  Sets an offset value in the frame list view. Caches the
 *   original data if not already done.
 */
static void SetFrameOffset(int frame, int column, int value)
{
   if (column != 3 && column != 4)
      return;


   for (list_type l = bgf_list; l != NULL; l = l->next)
   {
      object_bitmap_type bmp = (object_bitmap_type)l->data;
      if (last_selected == bmp->idnum)
      {
         CacheOriginal(bmp);
         if (column == 3)
            bmp->bmaps.pdibs[frame]->xoffset = value;
         else
            bmp->bmaps.pdibs[frame]->yoffset = value;
         break;
      }
   }
}
/****************************************************************************/
/*
 * OnBGFEditorCommand:  Handler for WM_COMMAND messages received by the BGF
 *   edit dialog.
 */
static void OnBGFEditorCommand(HWND hDlg, int cmd_id, HWND hwndCtl, UINT codeNotify)
{
   HWND hListBGF, hListFrame, hShrink;
   char buffer[8];
   int index, shrink;
   ID id;

   switch (cmd_id)
   {
   case IDC_BGFLIST:
      hListFrame = GetDlgItem(hDlg, IDC_FRAMEDATA);
      hListBGF = GetDlgItem(hDlg, IDC_BGFLIST);
      index = ListBox_GetCurSel(hListBGF);
      if (index == LB_ERR)
      {
         // Clear frame data anyway, no selection.
         ListView_DeleteAllItems(hListFrame);
         break;
      }

      id = ListBox_GetItemData(hListBGF, index);

      // Save ID of this selection if it is new.
      if (codeNotify == LBN_SELCHANGE)
         last_selected = id;

      ShowSelectedFrameData(hDlg);
      break;

   case IDC_SHRINKFACTOR:
      if (isSelecting || codeNotify == EN_SETFOCUS || codeNotify == EN_KILLFOCUS)
         break;
      if (codeNotify == WM_KEYDOWN)
      {
         id = 0;
      }
      hShrink = GetDlgItem(hDlg, IDC_SHRINKFACTOR);
      GetWindowText(hShrink, buffer, 8);
      shrink = atoi(buffer);
      if (shrink > 0)
      {
         hListBGF = GetDlgItem(hDlg, IDC_BGFLIST);
         index = ListBox_GetCurSel(hListBGF);
         if (index == LB_ERR)
            break;
         id = ListBox_GetItemData(hListBGF, index);
         for (list_type l = bgf_list; l != NULL; l = l->next)
         {
            object_bitmap_type bmp = (object_bitmap_type)l->data;
            if (id == bmp->idnum)
            {
               CacheOriginal(bmp);
               for (int i = 0; i < bmp->bmaps.num_bitmaps; ++i)
               {
                  bmp->bmaps.pdibs[i]->shrink = shrink;
               }
               break;
            }
         }
      }
      break;

   case IDC_REFRESH:
      LoadBGFList(hBGFEditorDlg);
      break;

   case IDC_REOADBGF:
      if (last_selected > 0)
         CacheRestore(last_selected);
      break;

   case IDCANCEL: // "close"
      ShowWindow(hDlg, SW_HIDE);
      bBGFEditorHidden = TRUE;
      break;
   }
}
/****************************************************************************/
/*
 * CacheOriginal:  Caches the original data for a bgf file so that it can
 *   be restored if desired. Caches shrink factor, x & y offsets, and
 *   hotspot index and position.
 */
static void CacheOriginal(object_bitmap_type bmp)
{
   bitmap_original_data b;

   // Check if we already have it.
   b = (bitmap_original_data) list_find_item(original_cache, &bmp->idnum, IdCacheBitmapCompare);
   if (b)
      return;

   // Cache it.
   b = (bitmap_original_data)malloc(sizeof(bitmap_original_struct));
   b->idnum = bmp->idnum;
   b->num_frames = bmp->bmaps.num_bitmaps;
   b->shrink = bmp->bmaps.pdibs[0]->shrink;
   b->frames = (frame_data*)malloc(b->num_frames * sizeof(frame_data));

   for (int i = 0; i < b->num_frames; ++i)
   {
      b->frames[i] = (frame_data)malloc(sizeof(frame_struct));
      b->frames[i]->xoffset = bmp->bmaps.pdibs[i]->xoffset;
      b->frames[i]->yoffset = bmp->bmaps.pdibs[i]->yoffset;
      b->frames[i]->num_hotspots = bmp->bmaps.pdibs[i]->num_hotspots;
      if (bmp->bmaps.pdibs[i]->num_hotspots == 0)
         b->frames[i]->hotspots = 0;
      else
         b->frames[i]->hotspots = (hotspot_data*)malloc(b->frames[i]->num_hotspots * sizeof(hotspot_data));
      for (int j = 0; j < b->frames[i]->num_hotspots; ++j)
      {
         b->frames[i]->hotspots[j] = (hotspot_data)malloc(sizeof(hotspot_struct));
         b->frames[i]->hotspots[j]->index = bmp->bmaps.pdibs[i]->numbers[j];
         b->frames[i]->hotspots[j]->x = bmp->bmaps.pdibs[i]->hotspots[j].x;
         b->frames[i]->hotspots[j]->y = bmp->bmaps.pdibs[i]->hotspots[j].y;
      }
   }
   original_cache = list_add_first(original_cache, b);
}
/****************************************************************************/
/*
 * CacheRestore:  Restore the original shrink and offset values for a bgf
 *   from the cache kept of originals.
 */
static void CacheRestore(ID id)
{
   // Must be in cache, and must be in bgf list.
   bitmap_original_data b_orig;
   
   b_orig = (bitmap_original_data)list_find_item(original_cache, &id, IdCacheBitmapCompare);
   if (!b_orig)
   {
      // TODO: This shouldn't happen, and should probably display an error.
      return;
   }
   
   for (list_type l = bgf_list; l != NULL; l = l->next)
   {
      object_bitmap_type bmp = (object_bitmap_type)l->data;
      if (id == bmp->idnum)
      {
         // TODO: throw error if num_bitmaps or num_hotspots doesn't match up?
         for (int i = 0; i < b_orig->num_frames; ++i)
         {
            bmp->bmaps.pdibs[i]->shrink = b_orig->shrink;
            bmp->bmaps.pdibs[i]->xoffset = b_orig->frames[i]->xoffset;
            bmp->bmaps.pdibs[i]->yoffset = b_orig->frames[i]->yoffset;
            for (int j = 0; j < b_orig->frames[i]->num_hotspots; ++j)
            {
               bmp->bmaps.pdibs[i]->numbers[j] = b_orig->frames[i]->hotspots[j]->index;
               bmp->bmaps.pdibs[i]->hotspots[j].x = b_orig->frames[i]->hotspots[j]->x;
               bmp->bmaps.pdibs[i]->hotspots[j].y = b_orig->frames[i]->hotspots[j]->y;
            }
         }

         ShowSelectedFrameData(hBGFEditorDlg);
         break;
      }
   }
}
/****************************************************************************/
/*
 * LoadBGFList:  Obtains BGF cache list, displays it in listbox.
 */
static void LoadBGFList(HWND hDlg)
{
   // Keep track of selection if we have one, to replace it later.
   int select_index = -1;

   HWND hList = GetDlgItem(hDlg, IDC_BGFLIST);

   // Empty the list, in case it was in use.
   ListBox_ResetContent(hList);

   bgf_list = CacheGetObjectList();

   for (list_type l = bgf_list; l != NULL; l = l->next)
   {
      object_bitmap_type bmp = (object_bitmap_type)l->data;

      int index = ListBox_AddString(hList, LookupNameRsc(bmp->idnum));
      ListBox_SetItemData(hList, index, bmp->idnum);

      // Save new selected index, increment if something is placed before it.
      if (last_selected == bmp->idnum)
         select_index = index;
      else if (select_index >= 0 && index <= select_index)
         ++select_index;
   }

   // Replace selection.
   if (select_index >= 0)
      ListBox_SetCurSel(hList, select_index);
}
/****************************************************************************/
/*
 * ShowSelectedFrameData:  Displays the frame data for the selected bgf in
 *   the frame list view.  Assumes last_selected is set prior to calling this.
 */
static void ShowSelectedFrameData(HWND hDlg)
{
   HWND hListFrame, hShrink;
   LV_ITEM lvitem;
   char buffer[8];

   isSelecting = TRUE;

   hListFrame = GetDlgItem(hDlg, IDC_FRAMEDATA);
   // Clear frame data.
   ListView_DeleteAllItems(hListFrame);

   for (list_type l = bgf_list; l != NULL; l = l->next)
   {
      object_bitmap_type bmp = (object_bitmap_type)l->data;
      if (last_selected == bmp->idnum)
      {
         // Shrink factor.
         sprintf(buffer, "%i", (int)bmp->bmaps.pdibs[0]->shrink);
         hShrink = GetDlgItem(hDlg, IDC_SHRINKFACTOR);
         // Selects all text in the edit box, replaces it with buffer.
         Edit_SetSel(hShrink, 0, Edit_GetTextLength(hShrink));
         Edit_ReplaceSel(hShrink, buffer);

         for (int i = 0; i < bmp->bmaps.num_bitmaps; ++i)
         {
            lvitem.mask = LVIF_TEXT | LVIF_PARAM;
            lvitem.iItem = ListView_GetItemCount(hListFrame);
            lvitem.lParam = bmp->idnum;

            // Frame index
            lvitem.iSubItem = 0;
            sprintf(buffer, "%i", i + 1);
            lvitem.pszText = buffer;
            ListView_InsertItem(hListFrame, &lvitem);

            // Add subitems

            // Width
            lvitem.mask = LVIF_TEXT;
            lvitem.iSubItem = 1;
            sprintf(buffer, "%i", bmp->bmaps.pdibs[i]->width);
            lvitem.pszText = buffer;
            ListView_SetItem(hListFrame, &lvitem);
            // Height
            lvitem.iSubItem = 2;
            sprintf(buffer, "%i", bmp->bmaps.pdibs[i]->height);
            lvitem.pszText = buffer;
            ListView_SetItem(hListFrame, &lvitem);
            // X offset
            lvitem.iSubItem = 3;
            sprintf(buffer, "%i", bmp->bmaps.pdibs[i]->xoffset);
            lvitem.pszText = buffer;
            ListView_SetItem(hListFrame, &lvitem);
            // Y offset
            lvitem.iSubItem = 4;
            sprintf(buffer, "%i", bmp->bmaps.pdibs[i]->yoffset);
            lvitem.pszText = buffer;
            ListView_SetItem(hListFrame, &lvitem);
         }
         break;
      }
   }
   isSelecting = FALSE;
}
