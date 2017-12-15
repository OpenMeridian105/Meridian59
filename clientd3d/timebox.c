// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * timebox.c:  Game time info for toolbar.
 */

#include "client.h"

/***************************************************************************/
// ASSUME: Only one timebox can be created.
static WNDPROC pfnDefTimeboxProc = NULL;
static HWND hwndTimebox = NULL;

static RECT timeboxRect; // Dimensions needed to display longest time string.
static char timeFmtString[80]; // Format string, e.g. "Time: %02d:%02d"

static int timer_id;            // id of timebox timer, 0 if none

/* local function prototypes */
static LRESULT CALLBACK Timebox_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void CALLBACK TimeboxTimerProc(HWND hwnd, UINT msg, UINT timer, DWORD dwTime);

/****************************************************************************/
/*
 * GetMeridianTime:  Returns an integer of hours*100 + minutes.
 *   e.g. 1745 for 5:45pm, or 201 for 2:01am.
 */ 
int GetMeridianTime()
{
   int game_time = 0;
   time_t t = time(NULL);
   struct tm tm_time = *gmtime(&t);

   // sum up real seconds elapsed since last m59 midnight
   // which happens every 2 hours
   int secs = (tm_time.tm_hour % 2) * 60 * 60;
   secs += tm_time.tm_min * 60;
   secs += tm_time.tm_sec;

   // scale to M59 secnods
   secs *= M59SECONDSPERSECOND;

   // Don't worry about translating seconds, just hours + minutes.
   game_time = secs / 3600 * 100;
   game_time += (secs % 3600) / 60;

   return game_time;
}
/****************************************************************************/
/*
 * DrawTimeboxBackground:  Make up for lack of features in static control.
 */
void DrawTimeboxBackground()
{
   RECT rect;

   if (hwndTimebox == NULL)
      return;

   HDC hdcTimebox = GetDC(hwndTimebox);
   if (hdcTimebox == NULL)
      return;

   BOOL res = GetClientRect(hwndTimebox, &rect);
   if (!res)
      return;
   
   // Optional colored border.
   /*RECT borderRect;
   borderRect.bottom = rect.bottom + 3;
   borderRect.top = rect.top - 3;
   borderRect.left = rect.left - 3;
   borderRect.right = rect.right + 3;
   FillRect(hdcTimebox, &borderRect, GetBrush(COLOR_TIME_BORDER));*/

   DrawWindowBackground(hdcTimebox, &rect, rect.left, rect.top);
   ReleaseDC(hwndTimebox, hdcTimebox);
}
/****************************************************************************/
/*
 * SetMeridianTime:  Set the static control's text to current game time.
 */
void SetMeridianTime()
{
   if (hwndTimebox == NULL)
      return;

   char time_combined[80];
   int game_time = GetMeridianTime();

   sprintf(time_combined, timeFmtString, game_time / 100, game_time % 100);

   // Draw a frame around the time, plus redraw background.
   DrawTimeboxBackground();

   SetWindowText(hwndTimebox, time_combined);
}
/****************************************************************************/
/*
 * Timebox_Create:  Create the timebox control.
 */
BOOL Timebox_Create()
{
   if (hwndTimebox)
   {
      debug(("Timebox_Create: already exists!\n"));
      return FALSE;
   }

   // Get ideal font, or use fallback. Don't set yet, but need it for draw area calc.
   HFONT font = GetFont(FONT_TOOLBAR_INFO);
   if (!font)
      font = GetFont(FONT_EDIT);

   // Set up format string.
   LoadString(hInst, IDS_TIMETOOLBAR, timeFmtString, sizeof(timeFmtString));

   // Get dimensions of our draw area.
   char setupString[80];
   sprintf(setupString, timeFmtString, 88, 88);
   HDC hDC = GetDC(NULL);

   // Use our font to get dimensions.
   SelectFont(hDC, font);
   timeboxRect = { 0, 0, 0, 0 };
   DrawText(hDC, setupString, strlen(setupString), &timeboxRect, DT_CALCRECT);
   // Little extra padding.
   timeboxRect.right += 6;
   timeboxRect.bottom += 4;
   // Create control.
   hwndTimebox = CreateWindow("static", "", WS_CHILD | SS_CENTER,
      0, 0, timeboxRect.right, timeboxRect.bottom, hMain, (HMENU)IDS_TIME0, hInst, NULL);
   if (!hwndTimebox)
   {
      debug(("Timebox_Create: failed CreateWindow()\n"));
      return FALSE;
   }

   pfnDefTimeboxProc = SubclassWindow(hwndTimebox, Timebox_WndProc);

   // Set font.
   SetWindowFont(hwndTimebox, font, FALSE);

   // Set the time.
   SetMeridianTime();

   Timebox_Reposition();

   return TRUE;
}
/****************************************************************************/
/*
 * Timebox_Destroy:  Destroy the timebox control.
 */
void Timebox_Destroy()
{
	DestroyWindow(hwndTimebox);
   hwndTimebox = NULL;
}
/****************************************************************************/
/*
 * Timebox_Reposition:   Move timebox to appropriate location around toolbar.
 */
void Timebox_Reposition()
{
   RECT rcToolbar, rcLagBox;
   AREA view;

   if (hwndTimebox == NULL)
      return;

   ToolbarGetUnionRect(&rcToolbar);
   Lagbox_GetRect(&rcLagBox);

   // Get proper right position.
   LONG rightPos;
   if (rcToolbar.right > rcLagBox.right)
      rightPos = rcToolbar.right;
   else
      rightPos = rcLagBox.right;

   // Check if client is sized wide enough to draw time.
   CopyCurrentView(&view);
   if (rightPos + timeboxRect.right + TOOLBAR_SEPARATOR_WIDTH * 2 > view.x + view.cx * 3 / 4)
   {
      ShowWindow(hwndTimebox, SW_HIDE);
      return;
   }

   // Set draw position based on whether toolbar icons and lagbox are present.
   if (IsRectEmpty(&rcToolbar))
   {
      if (IsRectEmpty(&rcLagBox))
         MoveWindow(hwndTimebox, TOOLBAR_X, TOOLBAR_Y, 20, 20, TRUE);
      else
         MoveWindow(hwndTimebox, rcLagBox.right + TOOLBAR_SEPARATOR_WIDTH * 2, rcLagBox.top, 20, 20, TRUE);
      ShowWindow(hwndTimebox, SW_NORMAL);
      InvalidateRect(hwndTimebox, NULL, FALSE);
   }
   else
   {
      MoveWindow(hwndTimebox, rightPos + TOOLBAR_SEPARATOR_WIDTH * 2, rcToolbar.top,// + 2,
         timeboxRect.right, timeboxRect.bottom, TRUE);
      ShowWindow(hwndTimebox, SW_NORMAL);
      InvalidateRect(hwndTimebox, NULL, FALSE);
   }
}
/****************************************************************************/
/*
 * Timebox_GetRect:  Get area of timebox.
 */
void Timebox_GetRect(LPRECT lpRect)
{
	if (!lpRect)
		return;

	memset(lpRect, 0, sizeof(*lpRect));
	if (hwndTimebox && IsWindowVisible(hwndTimebox))
	{
		GetWindowRect(hwndTimebox, lpRect);
		ScreenToClient(hMain, (LPPOINT)lpRect);
		ScreenToClient(hMain, (LPPOINT)lpRect+1);
	}
}
/****************************************************************************/
/*
 * Timebox_WndProc:  Window proc
 */
LRESULT CALLBACK Timebox_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
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
      DrawTimeboxBackground();
      // Paint the rest.
      return CallWindowProc(pfnDefTimeboxProc, hWnd, uMsg, wParam, lParam);
   case WM_RBUTTONDOWN:
   case WM_LBUTTONDOWN:
   case WM_LBUTTONUP:
      return TRUE;

   default:
      return CallWindowProc(pfnDefTimeboxProc, hWnd, uMsg, wParam, lParam);
   }

   return 0;
}
/****************************************************************************/
/*
 * TimeboxTimerProc:  Sets the game time in the toolbar.
 */
void CALLBACK TimeboxTimerProc(HWND hwnd, UINT msg, UINT timer, DWORD dwTime)
{
   // Set new time string.
   SetMeridianTime();
}
/****************************************************************************/
/*
 * TimeboxTimerAbort:  Kill the game time timer.
 */
void TimeboxTimerAbort(void)
{
   if (timer_id != 0)
   {
      KillTimer(NULL, timer_id);
      timer_id = 0;
   }
}
/****************************************************************************/
/*
 * TimeboxTimerStart:  Start timer to set the game time in toolbar every 5 sec.
 */
void TimeboxTimerStart(void)
{
   timer_id = SetTimer(NULL, 0, (UINT)5000, TimeboxTimerProc);
}
