// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * async.h
 *
 */

#ifndef _ASYNC_H
#define _ASYNC_H

void InitAsyncConnections(void);
void ExitAsyncConnections(void);
void AsyncSocketStart(void);
void AsyncNameLookup(HANDLE hLookup,int error);
void ResetUDP(void);
int GetLastUDPReadTime(void);

#ifdef BLAK_PLATFORM_LINUX
int AsyncSocketAccept(SOCKET sock,int event,int error,int connection_type);
void AsyncSocketSelect(SOCKET sock,int event,int error);
#endif
void AsyncSocketClose(SOCKET sock);
void AsyncSocketWrite(SOCKET sock);
void AsyncSocketRead(SOCKET sock);
void AsyncSocketReadUDP(SOCKET sock);

#endif
