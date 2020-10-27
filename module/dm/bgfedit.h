// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * bgfedit.h:  Header file for bgfedit.c
 */

 /***************************************************************************/

typedef struct {
   int x;
   int y;
   char index;
} *hotspot_data, hotspot_struct;

typedef struct {
   int    xoffset;
   int    yoffset;
   hotspot_data *hotspots;
   BYTE   num_hotspots;
} *frame_data, frame_struct;

typedef struct {
   ID     idnum;
   int    num_frames;
   frame_data *frames;
   BYTE   shrink;
} *bitmap_original_data, bitmap_original_struct;

HWND GetBGFEditorDlg();
void ShowBGFEditorDlg();

/***************************************************************************/
