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

#define LIST_COLUMN_ADD(a, b, c, d, e) \
   a.pszText = e; \
   a.cx = d; \
   ListView_InsertColumn(b, c, &a)

#define LIST_COLUMN_DATA_ADD(a, b, c, d, e) \
   a.iSubItem = d; \
   sprintf(c, "%i", e); \
   a.pszText = c; \
   ListView_SetItem(b, &a)

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
// Global to store selected subitem in hotspot list view.
static LVITEM lvHotspotItem;

// Globals for editing frame list view values.
static WNDPROC     wpOrigFrameEditProc;
static RECT        rcFrameSubItem;
// Globals for editing hotspot list view values.
static WNDPROC     wpOrigHotspotEditProc;
static RECT        rcHotspotSubItem;

// Global for subclassed edit box procedure.
static WNDPROC     shrinkEditProc;

// List of originals before they are modified.
static list_type original_cache;

static void OnBGFEditorCommand(HWND hDlg, int cmd_id, HWND hwndCtl, UINT codeNotify);
BOOL OnFrameListViewNotify(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL OnHotspotListViewNotify(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK BGFEditorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcFrameEditList(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcHotspotEditList(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProcEditShrink(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static void SetFrameOffset(int frame, int column, int value);
static void SetHotspotData(int index, int column, int value);
static void CacheOriginal(object_bitmap_type bmp);
static void CacheRestore(ID id);
static void ShowSelectedFrameData(HWND hDlg);
static void ShowSelectedHotspotData(HWND hDlg, int frame_index);
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
         break;
      }
      // fallthrough

   default:
      return CallWindowProc(shrinkEditProc, hDlg, uMsg, wParam, lParam);
   }
   return TRUE;
}
/****************************************************************************/
/*
 * WndProcFrameEditList:  Handler for editable frame list view subclass.
 */
LRESULT CALLBACK WndProcFrameEditList(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   char buffer[32];

   switch (uMsg)
   {
   case WM_WINDOWPOSCHANGING:
   {
      WINDOWPOS *pos = (WINDOWPOS*)lParam;

      pos->x = rcFrameSubItem.left;
      pos->cx = rcFrameSubItem.right;
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
         break;
      }
      return CallWindowProc(wpOrigFrameEditProc, hDlg, uMsg, wParam, lParam);

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
      // fallthrough

   default:
      return CallWindowProc(wpOrigFrameEditProc, hDlg, uMsg, wParam, lParam);
   }
   return TRUE;
}
/****************************************************************************/
/*
 * WndProcHotspotEditList:  Handler for editable hotspot list view subclass.
 */
LRESULT CALLBACK WndProcHotspotEditList(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   char buffer[32];

   switch (uMsg)
   {
   case WM_WINDOWPOSCHANGING:
   {
      WINDOWPOS *pos = (WINDOWPOS*)lParam;

      pos->x = rcHotspotSubItem.left;
      pos->cx = rcHotspotSubItem.right;
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
         SetHotspotData(lvHotspotItem.iItem, lvHotspotItem.iSubItem, value);
         break;
      }
      return CallWindowProc(wpOrigHotspotEditProc, hDlg, uMsg, wParam, lParam);

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
      // fallthrough

   default:
      return CallWindowProc(wpOrigHotspotEditProc, hDlg, uMsg, wParam, lParam);
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
      lvcol.fmt = LVCFMT_CENTER;
      LIST_COLUMN_ADD(lvcol, hList, 0, 40, "Index");
      LIST_COLUMN_ADD(lvcol, hList, 1, 46, "Width");
      LIST_COLUMN_ADD(lvcol, hList, 2, 46, "Height");
      LIST_COLUMN_ADD(lvcol, hList, 3, 54, "XOffset");
      LIST_COLUMN_ADD(lvcol, hList, 4, 54, "YOffset");

      // Set up Hotspot list
      hList = GetDlgItem(hDlg, IDC_HOTSPOTDATA);
      SetWindowFont(hList, GetFont(FONT_LIST), FALSE);

      // Columns
      ListView_SetExtendedListViewStyleEx(hList, LVS_EX_FULLROWSELECT,
         LVS_EX_FULLROWSELECT);
      LIST_COLUMN_ADD(lvcol, hList, 0, 40, "Index");
      LIST_COLUMN_ADD(lvcol, hList, 1, 32, "X");
      LIST_COLUMN_ADD(lvcol, hList, 2, 32, "Y");

      CenterWindow(hDlg, cinfo->hMain);
      return TRUE;

   case WM_ACTIVATE:
      *cinfo->hCurrentDlg = (wParam == 0) ? NULL : hDlg;
      return TRUE;

   case WM_NOTIFY:
      if (((LPNMHDR)lParam)->idFrom == IDC_FRAMEDATA)
         return OnFrameListViewNotify(hDlg, message, wParam, lParam);
      else if (((LPNMHDR)lParam)->idFrom == IDC_HOTSPOTDATA)
         return OnHotspotListViewNotify(hDlg, message, wParam, lParam);

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
 * OnFrameListViewNotify:  Handler for WM_NOTIFY messages from frame list view.
 */
BOOL OnFrameListViewNotify(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   static HWND hEdit;
   LPNMLISTVIEW pnmv;

   HWND hList = GetDlgItem(hDlg, IDC_FRAMEDATA);

   switch (((NMHDR*)lParam)->code)
   {
   case LVN_ITEMCHANGED:
      pnmv = (LPNMLISTVIEW)lParam;
      if ((pnmv->uChanged & LVIF_STATE)
         && (pnmv->uNewState & LVIS_SELECTED) != (pnmv->uOldState & LVIS_SELECTED))
      {
         if (pnmv->uNewState & LVIS_SELECTED)
         {
            lvFrameItem.iItem = pnmv->iItem;
            ShowSelectedHotspotData(hDlg, lvFrameItem.iItem);
         }
      }
      break;

   case NM_CLICK:
      lvFrameItem.iItem = ((NMITEMACTIVATE*)lParam)->iItem;
      lvFrameItem.iSubItem = ((NMITEMACTIVATE*)lParam)->iSubItem;
      break;

   case NM_DBLCLK:
      SendMessage(hList, LVM_EDITLABEL, lvFrameItem.iItem, 0);
      break;

   case LVN_BEGINLABELEDIT:
      if (lvFrameItem.iSubItem < 3
         || lvFrameItem.iSubItem > 4)
      {
         SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
         return TRUE;
      }
      char text[32];
      hEdit = (HWND)SendMessage(hList, LVM_GETEDITCONTROL, 0, 0);
      rcFrameSubItem.top = lvFrameItem.iSubItem;
      rcFrameSubItem.left = LVIR_LABEL;
      SendMessage(hList, LVM_GETSUBITEMRECT, lvFrameItem.iItem, (long)&rcFrameSubItem);
      rcFrameSubItem.right = SendMessage(hList, LVM_GETCOLUMNWIDTH, lvFrameItem.iSubItem, 0);
      wpOrigFrameEditProc = (WNDPROC)SetWindowLong(hEdit, GWL_WNDPROC, (long)WndProcFrameEditList);
      lvFrameItem.pszText = text;
      lvFrameItem.cchTextMax = 32;
      SendMessage(hList, LVM_GETITEMTEXT, lvFrameItem.iItem, (long)&lvFrameItem);
      SetWindowText(hEdit, lvFrameItem.pszText);
      break;

   case LVN_ENDLABELEDIT:
      SetWindowLong(hEdit, GWL_WNDPROC, (long)wpOrigFrameEditProc);
      if (lvFrameItem.iSubItem < 3
         || lvFrameItem.iSubItem > 4)
         return TRUE;
      lvFrameItem.pszText = ((NMLVDISPINFO*)lParam)->item.pszText;
      if (!lvFrameItem.pszText)
         return TRUE;
      SendMessage(hList, LVM_SETITEMTEXT, lvFrameItem.iItem, (long)&lvFrameItem);
      SetFrameOffset(lvFrameItem.iItem, lvFrameItem.iSubItem, atoi(lvFrameItem.pszText));
      break;
   }
   return TRUE;
}
/****************************************************************************/
/*
 * OnHotspotListViewNotify:  Handler for WM_NOTIFY messages from hotspot list view.
 */
BOOL OnHotspotListViewNotify(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   static HWND hEdit;
   LPNMLISTVIEW pnmv;

   HWND hList = GetDlgItem(hDlg, IDC_HOTSPOTDATA);

   switch (((NMHDR*)lParam)->code)
   {
   case LVN_ITEMCHANGED:
      pnmv = (LPNMLISTVIEW)lParam;
      if ((pnmv->uChanged & LVIF_STATE)
         && (pnmv->uNewState & LVIS_SELECTED) != (pnmv->uOldState & LVIS_SELECTED))
      {
         if (pnmv->uNewState & LVIS_SELECTED)
            lvHotspotItem.iItem = pnmv->iItem;
      }
      break;

   case NM_CLICK:
      lvHotspotItem.iItem = ((NMITEMACTIVATE*)lParam)->iItem;
      lvHotspotItem.iSubItem = ((NMITEMACTIVATE*)lParam)->iSubItem;
      break;

   case NM_DBLCLK:
      SendMessage(hList, LVM_EDITLABEL, lvHotspotItem.iItem, 0);
      break;

   case LVN_BEGINLABELEDIT:
      char text[32];
      hEdit = (HWND)SendMessage(hList, LVM_GETEDITCONTROL, 0, 0);
      rcHotspotSubItem.top = lvHotspotItem.iSubItem;
      rcHotspotSubItem.left = LVIR_LABEL;
      SendMessage(hList, LVM_GETSUBITEMRECT, lvHotspotItem.iItem, (long)&rcHotspotSubItem);
      rcHotspotSubItem.right = SendMessage(hList, LVM_GETCOLUMNWIDTH, lvHotspotItem.iSubItem, 0);
      wpOrigHotspotEditProc = (WNDPROC)SetWindowLong(hEdit, GWL_WNDPROC, (long)WndProcHotspotEditList);
      lvHotspotItem.pszText = text;
      lvHotspotItem.cchTextMax = 32;
      SendMessage(hList, LVM_GETITEMTEXT, lvHotspotItem.iItem, (long)&lvHotspotItem);
      SetWindowText(hEdit, lvHotspotItem.pszText);
      break;

   case LVN_ENDLABELEDIT:
      SetWindowLong(hEdit, GWL_WNDPROC, (long)wpOrigHotspotEditProc);
      lvHotspotItem.pszText = ((NMLVDISPINFO*)lParam)->item.pszText;
      if (!lvHotspotItem.pszText)
         return TRUE;
      SendMessage(hList, LVM_SETITEMTEXT, lvHotspotItem.iItem, (long)&lvHotspotItem);
      SetHotspotData(lvHotspotItem.iItem, lvHotspotItem.iSubItem, atoi(lvHotspotItem.pszText));
      break;
   }
   return TRUE;
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
         switch (column)
         {
         case 3:
            bmp->bmaps.pdibs[frame]->xoffset = value;
            break;
         case 4:
            bmp->bmaps.pdibs[frame]->yoffset = value;
            break;
         }
         break;
      }
   }
}
/****************************************************************************/
/*
 * SetHotspotData:  Sets a data value in the hotspot list view. Caches the
 *   original data if not already done.
 */
static void SetHotspotData(int index, int column, int value)
{
   if (column < 0 && column > 3)
      return;

   for (list_type l = bgf_list; l != NULL; l = l->next)
   {
      object_bitmap_type bmp = (object_bitmap_type)l->data;
      if (last_selected == bmp->idnum)
      {
         CacheOriginal(bmp);
         switch (column)
         {
         case 0:
            bmp->bmaps.pdibs[lvFrameItem.iItem]->numbers[index] = (char)value;
            break;
         case 1:
            bmp->bmaps.pdibs[lvFrameItem.iItem]->hotspots[index].x = value;
            break;
         case 2:
            bmp->bmaps.pdibs[lvFrameItem.iItem]->hotspots[index].y = value;
            break;
         }
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
      if (codeNotify == LBN_KILLFOCUS)
         break;

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
   HWND hList, hShrink;
   LV_ITEM lvitem;
   char buffer[8];

   isSelecting = TRUE;

   // Frame data
   hList = GetDlgItem(hDlg, IDC_FRAMEDATA);
   // Clear frame data.
   ListView_DeleteAllItems(hList);

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
            lvitem.iItem = ListView_GetItemCount(hList);
            lvitem.lParam = bmp->idnum;

            // Frame index
            lvitem.iSubItem = 0;
            sprintf(buffer, "%i", i + 1);
            lvitem.pszText = buffer;
            ListView_InsertItem(hList, &lvitem);

            // Add subitems
            lvitem.mask = LVIF_TEXT;

            // Width
            LIST_COLUMN_DATA_ADD(lvitem, hList, buffer, 1, bmp->bmaps.pdibs[i]->width);
            // Height
            LIST_COLUMN_DATA_ADD(lvitem, hList, buffer, 2, bmp->bmaps.pdibs[i]->height);
            // X offset
            LIST_COLUMN_DATA_ADD(lvitem, hList, buffer, 3, bmp->bmaps.pdibs[i]->xoffset);
            // Y offset
            LIST_COLUMN_DATA_ADD(lvitem, hList, buffer, 4, bmp->bmaps.pdibs[i]->yoffset);
         }
         if (bmp->bmaps.num_bitmaps > 0)
         {
            ListView_SetItemState(hList, 0, LVIS_SELECTED, LVIS_SELECTED);
            ListView_SetItemState(hList, 0, LVIS_FOCUSED, LVIS_FOCUSED);
         }
         break;
      }
   }
   isSelecting = FALSE;
}
/****************************************************************************/
/*
 * ShowSelectedHotspotData:  Displays the hotspot data for the selected frame in
 *   the hotspot list view.  Assumes last_selected is set prior to calling this.
 */
static void ShowSelectedHotspotData(HWND hDlg, int frame_index)
{
   HWND hList;
   LV_ITEM lvitem;
   char buffer[8];

   isSelecting = TRUE;

   hList = GetDlgItem(hDlg, IDC_HOTSPOTDATA);
   // Clear frame data.
   ListView_DeleteAllItems(hList);

   for (list_type l = bgf_list; l != NULL; l = l->next)
   {
      object_bitmap_type bmp = (object_bitmap_type)l->data;
      if (last_selected == bmp->idnum)
      {
         for (int i = 0; i < bmp->bmaps.pdibs[frame_index]->num_hotspots; ++i)
         {
            lvitem.mask = LVIF_TEXT | LVIF_PARAM;
            lvitem.iItem = ListView_GetItemCount(hList);
            lvitem.lParam = i;

            // Hotspot index
            bmp->bmaps.pdibs[frame_index]->numbers[i];
            lvitem.iSubItem = 0;

            sprintf(buffer, "%i", (int)bmp->bmaps.pdibs[frame_index]->numbers[i]);
            lvitem.pszText = buffer;
            ListView_InsertItem(hList, &lvitem);

            // Add subitems
            lvitem.mask = LVIF_TEXT;
            // X
            LIST_COLUMN_DATA_ADD(lvitem, hList, buffer, 1, bmp->bmaps.pdibs[frame_index]->hotspots[i].x);
            // Y
            LIST_COLUMN_DATA_ADD(lvitem, hList, buffer, 2, bmp->bmaps.pdibs[frame_index]->hotspots[i].y);
         }
         if (bmp->bmaps.pdibs[frame_index]->num_hotspots > 0)
         {
            ListView_SetItemState(hList, 0, LVIS_SELECTED, LVIS_SELECTED);
            ListView_SetItemState(hList, 0, LVIS_FOCUSED, LVIS_FOCUSED);
         }
         break;
      }
   }
   isSelecting = FALSE;
}
