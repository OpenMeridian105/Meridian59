// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * statstat.c:  Handle statistics tab page.
 */

#include "client.h"
#include "stats.h"

typedef struct {
   int min;        // Minimum value of stat
   int max;        // Maximum value of stat
   int val;        // Current value of stat
   int cost;       // Cost of increasing stat by one point
   HWND hGraph;    // Handle of graph control for stat
} Stat;

static Stat stats[] = {
{ 1,   50,   25,   1,   NULL },       // Might
{ 1,   50,   25,   1,   NULL },       // Intellect
{ 1,   50,   25,   1,   NULL },       // Stamina
{ 1,   50,   25,   1,   NULL },       // Agility
{ 1,   50,   25,   1,   NULL },       // Mysticism
{ 1,   50,   25,   1,   NULL },       // Aim
};

#define NUM_STATS (sizeof stats / sizeof stats[0])

typedef struct {
   int min;        // Minimum school level
   int max;        // Maximum school level
   int val;        // Current school level
   int start;      // Original school level
   int cost;       // Cost of increasing school by one level
   HWND hGraph;    // Handle of graph control for stat
} School;

static School schools[] = {
{ 0,   6,   0,   0,   1,   NULL },       // Shal'ille
{ 0,   6,   0,   0,   1,   NULL },       // Qor
{ 0,   6,   0,   0,   1,   NULL },       // Kraanan
{ 0,   6,   0,   0,   1,   NULL },       // Faren
{ 0,   6,   0,   0,   1,   NULL },       // Riija
{ 0,   6,   0,   0,   1,   NULL },       // Jala
{ 0,   6,   0,   0,   1,   NULL },       // Weaponcraft
{ 0,   6,   0,   0,   1,   NULL },       // Roguecraft
};

static int  stat_points = STAT_POINTS_INITIAL;   // # of stat points remaining
static HWND hPoints;                             // Handle of "points left" graph control
static HWND hIntellect;

static Bool controls_created = False;     // True after graph controls have been created

static WNDPROC lpfnDefGraphProc;  /* Default graph control window procedure */

static Stat *CharFindControl(HWND hwnd);
static School *CharFindSchoolControl(HWND hwnd);
static void CharStatsInit(HWND hDlg);
static long CALLBACK StatGraphProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void CharStatsGraphChanging(HWND hDlg, WPARAM wParam, LPARAM lParam);
static BOOL StatsCanReduceIntellect();

/********************************************************************/
BOOL CALLBACK CharStatsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
   switch (message)
   {
   case WM_INITDIALOG:
      CharStatsInit(hDlg);
      break;
      
   case WM_NOTIFY:
      switch (((LPNMHDR) lParam)->code)
      {
      case PSN_SETACTIVE:
         SendMessage(hPoints, GRPH_POSSET, 0, stat_points);
         break;

      case PSN_APPLY:
         if (stat_points > 0)
         {
            if (!AreYouSure(hInst,hDlg,NO_BUTTON,IDS_STATPOINTSLEFT))
            {
               // No clicked, don't close
               SetWindowLong(hDlg, DWL_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
               return TRUE;
            }
            else
            {
               if (!VerifySettings())
               {
                  // No clicked (in VerifySettings()), don't close
                  SetWindowLong(hDlg, DWL_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                  return TRUE;
               }
            }
         }
         else if (stat_points < 0)
         {
            ClientError(hInst, hDlg, IDS_NEGATIVESTATSMSG);
            SetWindowLong(hDlg, DWL_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
         }
         else
         {
            if (!VerifySettings())
            {
               // No clicked (in VerifySettings()), don't close
               SetWindowLong(hDlg, DWL_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
               return TRUE;
            }
         }
         break;

      }
      return TRUE;

   case GRPHN_POSCHANGING:
      CharStatsGraphChanging(hDlg, wParam, lParam);
      break;
   }
   return FALSE;
}

// Stat reset intellect functions
// Level calculations are done using a points system server-side, with a base
// level of 16 points, and a max (at 50 int) of 36 points. Each multiple of 5
// int adds 2 points. Point list used server-side is in GetLevelLearnPoints.

/********************************************************************/
/*
 * GetLevelLearnPoints: Returns the points required to learn a specific level.
 */
int GetLevelLearnPoints(int level)
{
   // Mirrors the server's points list [1, 2, 4, 6, 8, 10]
   int points_needed = (level <= 2) ? level : level * 2 - 2;

   return MAX(0, points_needed);
}
/********************************************************************/
/*
 * GetMaxLearnPoints: Returns the max possible learn pts for the given int.
 */
int GetMaxLearnPoints(int intellect)
{
   return 16 + intellect * 2 / 5;
}
/********************************************************************/
/*
 * GetTotalLearnPoints: Returns the total learn pts for the character's schools.
 */
int GetTotalLearnPoints()
{
   int total_points = 0;
   for (int i = 0; i < NUM_CHAR_SCHOOLS; ++i)
      total_points += GetLevelLearnPoints(schools[i].val);

   return total_points;
}
/********************************************************************/
/*
 * GetIntellectNeeded: Returns the int needed for the character's schools.
 *   Note that this uses the school levels set in the UI, not the original
 *   levels.
 */
int GetIntellectNeeded()
{
   int points_needed = (GetTotalLearnPoints() - 16) * 5 / 2;

   return MAX(1, points_needed);
}
/********************************************************************/
void initStatsFromServer(int *stats_in, int *levels_in)
{
   int i;
   int sum = 0;
   
   for (i=0; i < NUM_CHAR_STATS; i++)
   {
      stats[i].val = stats_in[i];
      sum = sum + stats_in[i];
   }
   stat_points = 200 - sum;
   
   for (i = 0; i < NUM_CHAR_SCHOOLS; i++)
   {
      schools[i].val = levels_in[i];
      schools[i].start = levels_in[i];
   }
}
/********************************************************************/
void CharStatsInit(HWND hDlg)
{
   int i;
   HWND hGraph;

   controls_created = False;
   
   // Initialize stat graph controls
   for (i=0; i < NUM_CHAR_STATS; i++)
   {
      hGraph = GetDlgItem(hDlg, IDC_CHAR_GRAPH1 + i);
      
      lpfnDefGraphProc = (WNDPROC) GetWindowLong(hGraph, GWL_WNDPROC);
      
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BAR, GetColor(COLOR_BAR1));
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BKGND, GetColor(COLOR_BAR3));
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_SLIDERBKGND, 
                  GetSysColor(COLOR_BTNFACE));  // CTL3D color

      SendMessage(hGraph, GRPH_RANGESET, stats[i].min, stats[i].max);
      SendMessage(hGraph, GRPH_POSSET, 0, stats[i].val);

      stats[i].hGraph = hGraph;

      SubclassWindow(hGraph, StatGraphProc); 
   }
   
   // initialize points left graph control
   hPoints = GetDlgItem(hDlg, IDC_POINTSLEFT);
   
   // save handle of intellect control
   hIntellect = GetDlgItem(hDlg, IDC_CHAR_GRAPH2);
   
   SendMessage(hPoints, GRPH_COLORSET, GRAPHCOLOR_BAR, GetColor(COLOR_BAR2));
   SendMessage(hPoints, GRPH_COLORSET, GRAPHCOLOR_BKGND, GetColor(COLOR_BAR3));
   SendMessage(hPoints, GRPH_RANGESET, 0, STAT_POINTS_INITIAL);
   
   // initialize school graph controls
   for (i=0; i < NUM_CHAR_SCHOOLS; ++i)
   {
      hGraph = GetDlgItem(hDlg, IDC_CHAR_GRAPH7 + i);

      lpfnDefGraphProc = (WNDPROC) GetWindowLong(hGraph, GWL_WNDPROC);
      
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BAR, GetColor(COLOR_BAR1));
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_BKGND, GetColor(COLOR_BAR3));
      SendMessage(hGraph, GRPH_COLORSET, GRAPHCOLOR_SLIDERBKGND, 
                  GetSysColor(COLOR_BTNFACE));  // CTL3D color

      SendMessage(hGraph, GRPH_RANGESET, schools[i].min, schools[i].max);
      SendMessage(hGraph, GRPH_POSSET, 0, schools[i].val);

      schools[i].hGraph = hGraph;

      SubclassWindow(hGraph, StatGraphProc);
   }

   controls_created = True;
}
/********************************************************************/
/*
 * CharStatGraphChanging:  Handle GRAPHN_POSCHANGING message from graph control.
 */
void CharStatsGraphChanging(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
   Stat *s;
   School *sc;
   int index, cost;

   /* Update points graph */
   if ((HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH7) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH8) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH9) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH10) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH11) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH12) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH13) ||
       (HWND) wParam == GetDlgItem(hDlg, IDC_CHAR_GRAPH14))
   {
      sc = CharFindSchoolControl((HWND) wParam);
      if (sc == NULL)
         return;
      sc->val = lParam;
      
      if (SendMessage((HWND)wParam, GRPH_POSGET, 0, 0) < lParam)
      {
         int intNeeded = GetIntellectNeeded();
         if (intNeeded > SendMessage(hIntellect, GRPH_POSGET, 0, 0))
            SendMessage(hIntellect, GRPH_POSSET, 0, intNeeded);
      }
   }
   else
   {
      s = CharFindControl((HWND) wParam);
      if (s == NULL || (HWND) wParam == hPoints)
         return;

      index = SendMessage((HWND) wParam, GRPH_POSGET, 0, 0);  // Get old value
      cost = s->cost * (lParam - index);

      // Check if user is allowed to lower intellect, set slider back if not.
      if ((HWND)wParam == hIntellect
         && GetIntellectNeeded() > lParam)
      {
         SendMessage(hIntellect, GRPH_POSSET, 0, GetIntellectNeeded());
      }
      else
      {
         s->val = lParam;

         // Don't change points when controls are being created
         if (controls_created)
            stat_points -= cost;
      }
      SendMessage(hPoints, GRPH_POSSET, 0, stat_points);
   }
}
/********************************************************************/
/*
 * CharStatsGetChoices:  Fill in stats with currently selected stat values.
 */
void CharStatsGetChoices(int *buf)
{
   int i;
   
   for (i=0; i < NUM_CHAR_STATS; i++)
      buf[i] = stats[i].val;
}
/********************************************************************/
/*
 * CharSchoolsGetChoices:  Fill in stats with currently selected school levels.
 */
void CharSchoolsGetChoices(int *buf)
{
   int i;
   
   for (i=0; i < NUM_CHAR_SCHOOLS; i++)
      buf[i] = schools[i].val;
}
/********************************************************************/
/*
 * CharStatsGetPoints:  Return # of stat points remaining
 */
int CharStatsGetPoints(void)
{
  return stat_points;
}
/********************************************************************/
/* 
 * CharFindControl:  Return Stat structure for given graph control, or NULL if none. 
 */
Stat *CharFindControl(HWND hwnd)
{
   int i;

   for (i=0; i < NUM_CHAR_STATS; i++)
      if (stats[i].hGraph == hwnd)
         return &stats[i];
   
   return NULL;
}
/********************************************************************/
/* 
 * CharFindSchoolControl:  Return School structure for given graph control, or NULL if none. 
 */
School *CharFindSchoolControl(HWND hwnd)
{
   int i;

   for (i=0; i < NUM_CHAR_SCHOOLS; i++)
      if (schools[i].hGraph == hwnd)
         return &schools[i];
   
   return NULL;
}
/********************************************************************/
/*
 * StatGraphProc:  Subclassed window procedure for stat graph controls.
 */
long CALLBACK StatGraphProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
   Stat *s;
   School *sc;
   int new_pos, cur_pos;
   int cost;

   switch(msg)
   {
   case GRPH_POSSETUSER:
      /* See if user has enough points to make this change.  First, find stat */
      s = CharFindControl(hwnd);
      if (s != NULL)
      {
         cur_pos = SendMessage(hwnd, GRPH_POSGET, 0, 0);

         new_pos = lParam;
         
         if (s->hGraph == hIntellect)
         {
            int intellect_needed = GetIntellectNeeded();
            if (new_pos < intellect_needed)
            {
               new_pos = intellect_needed;
               return 0;
            }
         }
         
         cost = s->cost * (new_pos - cur_pos);

         if (cost > stat_points)
         {
            // Set value to greatest we can afford
            if (cost == 0)
               return 0;

            lParam = cur_pos + stat_points / s->cost;

            // Don't allow this to set int lower than the user needs
            // for the schools they have.
            if (lParam == cur_pos
               || (s->hGraph == hIntellect
                  && lParam < GetIntellectNeeded()))
               return 0;
         }
      }
      else
      {
         // not a stat graph, see if it is a school graph
         sc = CharFindSchoolControl(hwnd);
         
         if (sc != NULL)
         {
            // we aren't in the business of giving away school levels
            // do not increase school level past the original level
            if (lParam > sc->start)
            {
               lParam = sc->start;
            }
            else
            {
               cur_pos = SendMessage(hwnd, GRPH_POSGET, 0, 0);
            }
         }
         else
         {
            // not a spell or school graph, we should not get here.
            debug(("Couldn't find graph control in list!\n"));
            return 0L;
         }
      }
   }
   
   return CallWindowProc(lpfnDefGraphProc, hwnd, msg, wParam, lParam);
}