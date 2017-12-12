// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * fpsbox.h:  Header for fpsbox.c
 */

#ifndef _FPSBOX_H
#define _FPSBOX_H

/***************************************************************************/

BOOL Fpsbox_Create();
void Fpsbox_Destroy();
void Fpsbox_Reposition();
void SetFPSDisplay(int fps, int frameTime);

M59EXPORT void Fpsbox_GetRect(LPRECT lpRect);
/***************************************************************************/

#endif /* #ifndef _FPSBOX_H */
