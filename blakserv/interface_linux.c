// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.

#include "blakserv.h"

static int sessions_logged_on = 0;
static int console_session_id = INVALID_ID;
static pthread_t interface_thread;

#define ADMIN_RESPONSE_SIZE (256 * 1024)

static char admin_response_buf[ADMIN_RESPONSE_SIZE+1];
static int len_admin_response_buf;

static void InterfaceAddAdminBuffer(char *buf, int len_buf);

// InitInterface: called late (after StartupComplete) only in interactive mode.
// Creates the console admin session and starts the console input thread.
void InitInterface(void)
{
   connection_node conn;
   session_node *s;

   len_admin_response_buf = 0;

   conn.type = CONN_CONSOLE;
   s = CreateSession(conn);
   if (s == NULL)
      FatalError("Interface can't make session for console");
   s->account = GetConsoleAccount();
   InitSessionState(s, STATE_ADMIN);
   console_session_id = s->session_id;

   int err = pthread_create(&interface_thread, NULL, &InterfaceMainLoop, NULL);
   if (err != 0)
      eprintf("Unable to start interface thread: %s\n", strerror(err));
   else
      dprintf("Interface console thread started\n");
}

void* InterfaceMainLoop(void* arg)
{
   char *line = (char*) malloc(200);
   size_t size;

   for (;;)
   {
      printf("blakadm> ");
      fflush(stdout);
      if (getline(&line, &size, stdin) == -1)
         break;

      // Strip trailing newline from getline
      size_t len = strlen(line);
      if (len > 0 && line[len - 1] == '\n')
         line[len - 1] = '\0';

      if (strcmp(line, "quit") == 0)
         break;

      if (strlen(line) == 0)
         continue;

      EnterServerLock();
      TryAdminCommand(console_session_id, line);
      LeaveServerLock();
   }

   free(line);
   SetQuit();

   return NULL;
}

void StartupPrintf(const char *fmt, ...)
{
   char s[200];
   va_list marker;

   va_start(marker, fmt);
   vsnprintf(s, sizeof(s), fmt, marker);
   va_end(marker);

   if (strlen(s) > 0)
   {
      if (s[strlen(s)-1] == '\n')
         s[strlen(s)-1] = 0;
   }

   printf("Startup: %s\n", s);
}

int GetUsedSessions(void)
{
   return sessions_logged_on;
}

void InterfaceUpdate(void)
{
}

void InterfaceLogon(session_node *s)
{
   sessions_logged_on++;
}

void InterfaceLogoff(session_node *s)
{
   sessions_logged_on--;
}

void InterfaceUpdateSession(session_node *s)
{
}

void InterfaceUpdateChannel(void)
{
}

void InterfaceSendBufferList(buffer_node *blist)
{
   buffer_node *bn;

   bn = blist;
   while (bn != NULL)
   {
      InterfaceAddAdminBuffer(bn->buf, bn->len_buf);
      bn = bn->next;
   }

   DeleteBufferList(blist);
}

void InterfaceSendBytes(char *buf, int len_buf)
{
   InterfaceAddAdminBuffer(buf, len_buf);
}

static void InterfaceAddAdminBuffer(char *buf, int len_buf)
{
   if (len_buf > ADMIN_RESPONSE_SIZE)
      len_buf = ADMIN_RESPONSE_SIZE;

   if (len_admin_response_buf + len_buf > ADMIN_RESPONSE_SIZE)
      len_admin_response_buf = 0;

   memcpy(admin_response_buf + len_admin_response_buf, buf, len_buf);
   len_admin_response_buf += len_buf;
   admin_response_buf[len_admin_response_buf] = 0;

   printf("%s", admin_response_buf);
   len_admin_response_buf = 0;
}

void FatalErrorShow(const char *filename, int line, const char *str)
{
   fprintf(stderr, "FATAL ERROR: File %s line %i\n%s\n", filename, line, str);
   exit(1);
}
