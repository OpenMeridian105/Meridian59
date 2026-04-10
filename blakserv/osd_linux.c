// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * osd_linux.c
 *
 * OS-dependent function implementations for Linux.
 * Based on vanilla Meridian59, extended for our codebase.
 */

#include "blakserv.h"

#include <vector>
#include <utility>

typedef std::pair<int, int> fd_conn_type;
static std::vector<fd_conn_type> accept_sockets;

// Track UDP socket separately
static SOCKET udp_socket = INVALID_SOCKET;

bool IsAcceptingSocket(int sock)
{
   for (auto it = accept_sockets.begin(); it != accept_sockets.end(); ++it)
   {
      if (it->first == sock)
         return true;
   }
   return false;
}

int GetAcceptingSocketConnectionType(int sock)
{
   for (auto it = accept_sockets.begin(); it != accept_sockets.end(); ++it)
   {
      if (it->first == sock)
         return it->second;
   }
   return 0;
}

void AddAcceptingSocket(int sock, int connection_type)
{
   accept_sockets.push_back(std::make_pair(sock, connection_type));
}

SOCKET GetUDPSocket(void)
{
   return udp_socket;
}

void SetUDPSocket(SOCKET sock)
{
   udp_socket = sock;
}

int GetLastError()
{
   return errno;
}

char *GetLastErrorStr()
{
   return strerror(errno);
}

HANDLE StartAsyncNameLookup(char *peer_addr, char *buf)
{
   // DNS reverse lookup not implemented on Linux
   return 0;
}

// Mutex implementation using pthread recursive mutex
void InitializeCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(cs, &attr);
   pthread_mutexattr_destroy(&attr);
}

void DeleteCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutex_destroy(cs);
}

void EnterCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutex_lock(cs);
}

void LeaveCriticalSection(CRITICAL_SECTION *cs)
{
   pthread_mutex_unlock(cs);
}

Mutex MutexCreate(void)
{
   pthread_mutex_t *m = new pthread_mutex_t;
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(m, &attr);
   pthread_mutexattr_destroy(&attr);
   return m;
}

bool MutexAcquire(Mutex mutex)
{
   return pthread_mutex_lock(mutex) == 0;
}

bool MutexAcquireWithTimeout(Mutex mutex, int timeoutMs)
{
   struct timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   ts.tv_sec += timeoutMs / 1000;
   ts.tv_nsec += (timeoutMs % 1000) * 1000000L;
   if (ts.tv_nsec >= 1000000000L)
   {
      ts.tv_nsec -= 1000000000L;
      ts.tv_sec += 1;
   }
   return pthread_mutex_timedlock(mutex, &ts) == 0;
}

bool MutexRelease(Mutex mutex)
{
   return pthread_mutex_unlock(mutex) == 0;
}

bool MutexClose(Mutex mutex)
{
   pthread_mutex_destroy(mutex);
   delete mutex;
   return true;
}
