// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * osd_linux.h
 *
 * OS-dependent type definitions and function declarations for Linux.
 * Based on vanilla Meridian59's osd_linux.h, extended with IPv6 and UDP support.
 */

#ifndef _OSD_LINUX_H
#define _OSD_LINUX_H

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>

#define MAX_PATH PATH_MAX
#define O_BINARY 0
#define O_TEXT 0
#define stricmp strcasecmp
#define strnicmp strncasecmp

// MSVC-specific keywords
#define __forceinline inline __attribute__((always_inline))
#define _MM_ALIGN16 __attribute__((aligned(16)))
#define ZeroMemory(dest, size) memset((dest), 0, (size))

// Socket compatibility
typedef int SOCKET;
#define closesocket close
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAGetLastError() errno
#define SOCKADDR_IN6 sockaddr_in6
#define SOCKADDR sockaddr
#define IN6_ADDR in6_addr
#define FD_CLOSE   32
#define FD_ACCEPT  8
#define FD_READ    1
#define FD_WRITE   2

// Windows type compatibility
typedef unsigned char BYTE;
typedef unsigned short WORD;
#define MAKEWORD(low, high) ((WORD)((((WORD)(high)) << 8) | ((BYTE)(low))))
typedef unsigned long DWORD;
typedef long long INT64;
typedef void* PVOID;
typedef void* LPVOID;
typedef PVOID HANDLE;
typedef HANDLE HINSTANCE;
typedef HANDLE HWND;
typedef unsigned long long UINT64;
typedef unsigned int UINT;
typedef bool BOOL;
#define TRUE 1
#define FALSE 0
typedef unsigned long LPARAM;
typedef unsigned int WPARAM;
typedef const char* LPCSTR;
typedef LPCSTR LPCTSTR;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT 2
#define PROCESSOR_INTEL_386 386
#define PROCESSOR_INTEL_486 486
#define PROCESSOR_INTEL_PENTIUM 586

#define MAXGETHOSTSTRUCT 64

// Mutex type - uses pthread recursive mutex
typedef pthread_mutex_t* Mutex;
Mutex MutexCreate(void);
bool MutexAcquire(Mutex mutex);
bool MutexAcquireWithTimeout(Mutex mutex, int timeoutMs);
bool MutexRelease(Mutex mutex);
bool MutexClose(Mutex mutex);

// Critical section compatibility (maps to mutex)
typedef pthread_mutex_t CRITICAL_SECTION;
void InitializeCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);

// OSD function declarations (types that don't need forward decls)
int GetLastError();
char *GetLastErrorStr();

void RunMainLoop(void);
void EnableSendEvents(SOCKET sock);
void DisableSendEvents(SOCKET sock);
void WakeupMainLoop(void);

void StartupComplete(void);

void StartAsyncSocketAccept(SOCKET sock,int connection_type);
void StartAsyncSocketUDPRead(SOCKET sock);
HANDLE StartAsyncNameLookup(char *peer_addr,char *buf);

void FatalErrorShow(const char *filename,int line,const char *str);
#define FatalError(a) FatalErrorShow(__FILE__,__LINE__,a)

bool IsAcceptingSocket(int sock);
int GetAcceptingSocketConnectionType(int sock);
void AddAcceptingSocket(int sock, int connection_type);

SOCKET GetUDPSocket(void);
void SetUDPSocket(SOCKET sock);

#endif
