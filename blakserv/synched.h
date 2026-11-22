// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * synched.h
 *
 */

#ifndef _SYNCHED_H
#define _SYNCHED_H

typedef struct
{
   int failed_tries;
   int download_count; /* 0 if not downloading, otherwise count */
} synched_data;

/* Godot client: version major 91 (Classic 50, Ogre 90). Its own version number (0.9.0 = 9090, 1.0.0 = 9100,
   9000 + MAJOR*100 + MINOR*10 + PATCH, a developer build 99999) comes in the 4-byte bandwidth field of AP_LOGIN
   (the minor byte is too small) and is compared with MinGodotVersion. os_type 2 = Windows, anything else = Linux. */
#define GODOT_VERSION_MAJOR 91
#define GODOT_VERSION_DEV 99999
#define GODOT_OS_WINDOWS 2

void GodotVersionText(int number, char *text, int size);

#endif
