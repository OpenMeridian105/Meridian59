// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * timebox.h:  Header for timebox.c
 */

#ifndef _TIMEBOX_H
#define _TIMEBOX_H

/***************************************************************************/

#define M59SECONDSPERSECOND 86400 / 7200;

BOOL Timebox_Create();
void Timebox_Destroy();
void Timebox_Reposition();

M59EXPORT void Timebox_GetRect(LPRECT lpRect);

void TimeboxTimerStart(void);
void TimeboxTimerAbort(void);
/***************************************************************************/

#endif /* #ifndef _TIMEBOX_H */
