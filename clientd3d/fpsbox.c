// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * fpsbox.c:  FPS info for toolbar.
 */

#include "client.h"

/***************************************************************************/
// ASSUME: Only one fpsbox can be created.
static WNDPROC pfnDefFpsboxProc = NULL;
static HWND hwndFpsbox = NULL;

static RECT fpsboxRect; // Dimensions needed to display longest fps string.
static char fpsFmtString[80]; // Format string, e.g. "FPS: %d (%dms)"
static int lastFps; // Last FPS redraw.

/* local function prototypes */
static LRESULT CALLBACK Fpsbox_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/****************************************************************************/
/*
 * DrawFPSbosBackground:  Make up for lack of features in static control.
 */
void DrawFPSboxBackground()
{
   if (hwndFpsbox == NULL)
      return;

   RECT rect;
   HDC hdcFpsbox = GetDC(hwndFpsbox);

   if (hdcFpsbox == NULL)
      return;

   BOOL res = GetClientRect(hwndFpsbox, &rect);
   if (!res)
      return;

   // Optional colored border.
   /*RECT borderRect;
   borderRect.bottom = rect.bottom + 3;
   borderRect.top = rect.top - 3;
   borderRect.left = rect.left - 3;
   borderRect.right = rect.right + 3;
   FillRect(hdcFpsbox, &borderRect, GetBrush(COLOR_TIME_BORDER));*/

   DrawWindowBackground(hdcFpsbox, &rect, rect.left, rect.top);
   ReleaseDC(hwndFpsbox, hdcFpsbox);
}
/****************************************************************************/
/*
 * SetFPSDisplay:  Set the static control's text with FPS data.
 */
void SetFPSDisplay(int fps, int frameTime)
{
   char fps_combined[80];

   if (!config.showFPS || hwndFpsbox == NULL)
      return;

   // Don't redraw if the number is the same.
   if (lastFps != 0 && lastFps == fps)
      return;

   sprintf(fps_combined, fpsFmtString, fps, frameTime);

   // Draw a frame around the fps, plus redraw background.
   DrawFPSboxBackground();
   SetWindowText(hwndFpsbox, fps_combined);
   lastFps = fps;
}

/****************************************************************************/
/*
 * Fpsbox_Create:  Create the fpsbox control.
 */
BOOL Fpsbox_Create()
{
   if (hwndFpsbox)
   {
      debug(("Fpsbox_Create: already exists!\n"));
      return FALSE;
   }

   // Get ideal font, or use fallback. Don't set yet, but need it for draw area calc.
   HFONT font = GetFont(FONT_TOOLBAR_INFO);
   if (!font)
      font = GetFont(FONT_EDIT);

   // Set up format string.
   LoadString(hInst, IDS_FPSTOOLBAR, fpsFmtString, sizeof(fpsFmtString));

   // Get dimensions of our draw area.
   char setupString[80];
   sprintf(setupString, fpsFmtString, 8888, 888);
   HDC hDC = GetDC(NULL);

   // Use our font to get dimensions.
   SelectFont(hDC, font);
   fpsboxRect = { 0, 0, 0, 0 };
   DrawText(hDC, setupString, strlen(setupString), &fpsboxRect, DT_CALCRECT);
   // Little extra padding.
   fpsboxRect.right += 2;
   fpsboxRect.bottom += 4;

   // Create control.
   hwndFpsbox = CreateWindow("static", "", WS_CHILD | SS_CENTER,
      0, 0, fpsboxRect.right, fpsboxRect.bottom, hMain, (HMENU)IDS_FPS0, hInst, NULL);
   if (!hwndFpsbox)
   {
      debug(("Fpsbox_Create: failed CreateWindow()\n"));
      return FALSE;
   }

   pfnDefFpsboxProc = SubclassWindow(hwndFpsbox, Fpsbox_WndProc);

   // Set font.
   SetWindowFont(hwndFpsbox, font, FALSE);

   // Set default fps data.
   SetFPSDisplay(0, 0);

   Fpsbox_Reposition();

   return TRUE;
}
/****************************************************************************/
/*
 * Fpsbox_Destroy:  Destroy the fpsbox control.
 */
void Fpsbox_Destroy()
{
	DestroyWindow(hwndFpsbox);
   hwndFpsbox = NULL;
}
/****************************************************************************/
/*
 * Fpsbox_Reposition:   Move fpsbox to appropriate location around toolbar.
 */
void Fpsbox_Reposition()
{
   RECT rcToolbar, rcLagbox, rcTimebox;
   AREA view;

   if (hwndFpsbox == NULL)
      return;

   ToolbarGetUnionRect(&rcToolbar);
   Lagbox_GetRect(&rcLagbox);
   Timebox_GetRect(&rcTimebox);

   // Get proper position.
   LONG rightPos, topPos;
   if (rcToolbar.right > rcLagbox.right)
   {
      rightPos = rcToolbar.right;
      topPos = rcToolbar.top;
   }
   else
   {
      rightPos = rcLagbox.right;
      topPos = rcLagbox.top;
   }
   if (rcTimebox.right > rightPos)
   {
      rightPos = rcTimebox.right;
      topPos = rcTimebox.top - 2;
   }

   // Check if client is sized wide enough to draw fps.
   CopyCurrentView(&view);
   if (rightPos + fpsboxRect.right + TOOLBAR_SEPARATOR_WIDTH * 2 > view.x + view.cx * 5 / 6)
   {
      ShowWindow(hwndFpsbox, SW_HIDE);
      return;
   }

   rightPos += TOOLBAR_SEPARATOR_WIDTH * 2;
   topPos += 2;

   // Set draw position based on whether toolbar icons, timebox and lagbox are present.
   if (IsRectEmpty(&rcToolbar) && IsRectEmpty(&rcLagbox) && IsRectEmpty(&rcTimebox))
      MoveWindow(hwndFpsbox, TOOLBAR_X, TOOLBAR_Y, 20, 20, TRUE);
   else
      MoveWindow(hwndFpsbox, rightPos, topPos, fpsboxRect.right, fpsboxRect.bottom, TRUE);

   ShowWindow(hwndFpsbox, config.showFPS ? SW_NORMAL : SW_HIDE);
   InvalidateRect(hwndFpsbox, NULL, FALSE);
}
/****************************************************************************/
/*
 * Fpsbox_GetRect:  Get area of fpsbox.
 */
void Fpsbox_GetRect(LPRECT lpRect)
{
	if (!lpRect)
		return;

	memset(lpRect, 0, sizeof(*lpRect));
	if (hwndFpsbox && IsWindowVisible(hwndFpsbox))
	{
		GetWindowRect(hwndFpsbox, lpRect);
		ScreenToClient(hMain, (LPPOINT)lpRect);
		ScreenToClient(hMain, (LPPOINT)lpRect+1);
	}
}
/****************************************************************************/
/*
 * Fpsbox_WndProc:  Window proc
 */
LRESULT CALLBACK Fpsbox_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   MSG msg;
   msg.hwnd = hWnd;
   msg.message = uMsg;
   msg.wParam = wParam;
   msg.lParam = lParam;
   TooltipForwardMessage(&msg);
   switch (uMsg)
   {
   case WM_ERASEBKGND:
      // don't erase the background
      return TRUE; // yes, we erased the background, honest
   case WM_PAINT:
      // Paint frame + background.
      DrawFPSboxBackground();
      // Paint the rest.
      return CallWindowProc(pfnDefFpsboxProc, hWnd, uMsg, wParam, lParam);
   case WM_RBUTTONDOWN:
   case WM_LBUTTONDOWN:
   case WM_LBUTTONUP:
      return TRUE;

   default:
      return CallWindowProc(pfnDefFpsboxProc, hWnd, uMsg, wParam, lParam);
   }

   return 0;
}
