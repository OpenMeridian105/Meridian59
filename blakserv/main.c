// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* main.c
*

  Blakserv is the server program for Blakston.  This is a windows application,
  so we have a WinMain.  There is a dialog box interface in interfac.c.
  
	This module starts all of our "subsystems" and calls the timer loop,
	which executes until we terminate (either by the window interface or by
	a system administrator logging in to administrator mode.
	
*/

#include "blakserv.h"

/* local function prototypes */
void MainServer(void);
void MainExitServer(void);

DWORD main_thread_id;
static bool in_main_loop = false;

#ifdef BLAK_PLATFORM_WINDOWS

int WINAPI WinMain(_In_ HINSTANCE hInstance,
                   _In_opt_ HINSTANCE hPrev_instance,
                   _In_ char *command_line,
                   _In_ int how_show)
{
   main_thread_id = GetCurrentThreadId();
   StoreInstanceData(hInstance, how_show);
   MainServer();
   return 0;
}

#else

int main(int argc, char **argv)
{
   MainServer();
   return 0;
}

#endif

#ifdef BLAK_PLATFORM_LINUX
Bool InMainLoop(void)
{
   return in_main_loop;
}

// On Windows, MainServer() is in main_windows.c
// On Linux, it's here with RunMainLoop() instead of ServiceTimers()
void MainServer(void)
{
   InitInterfaceLocks();
   InitInterface();

   InitMemory();
   InitConfig();
   LoadConfig();

   InitDebug();
   InitChannelBuffer();
   OpenDefaultChannels();

   if (ConfigBool(MYSQL_ENABLED))
   {
#ifdef BLAK_PLATFORM_WINDOWS
      lprintf("Starting MySQL writer\n");
#else
      lprintf("Starting PostgreSQL writer\n");
#endif
      MySQLInit(ConfigStr(MYSQL_HOST), ConfigInt(MYSQL_CPORT), ConfigStr(MYSQL_USERNAME),
         ConfigStr(MYSQL_PASSWORD), ConfigStr(MYSQL_DB));
   }

   lprintf("Starting %s\n", BlakServLongVersionString());

   InitClass();
   InitMessage();
   InitObject();
   InitList();
   InitTimer();
   InitSession();
   InitResource();
   AStarInit();
   InitRooms();
   InitString();
   InitUser();
   InitAccount();
   InitNameID();
   InitDLlist();
   InitSysTimer();
   InitMotd();
   InitLoadBof();
   InitTime();
   InitGameLock();
   InitBkodInterpret();
   InitBufferPool();
   InitTables();
   AddBuiltInDLlist();
   LoadMotd();
   LoadBof();
   LoadRsc();
   LoadKodbase();
   LoadAdminConstants();
   PauseTimers();

   if (LoadAll() == True)
   {
      SendTopLevelBlakodMessage(GetSystemObjectID(), LOADED_GAME_MSG, 0, NULL);
      DoneLoadAccounts();
   }

   InitCommCli();
   InitParseClient();
   InitProfiling();
   InitAsyncConnections();
   UpdateSecurityRedbook();
   UnpauseTimers();

   StartupComplete();
   InterfaceUpdate();

   lprintf("Status: %i accounts\n", GetNextAccountID());
   lprintf("-----------------------------------------------------------------------------------------------\n");
   dprintf("-----------------------------------------------------------------------------------------------\n");
   eprintf("-----------------------------------------------------------------------------------------------\n");
   gprintf("-----------------------------------------------------------------------------------------------\n");

   in_main_loop = true;

   AsyncSocketStart();

#ifdef BLAK_PLATFORM_WINDOWS
   SetWindowText(hwndMain, ConfigStr(CONSOLE_CAPTION));
   ServiceTimers();
#else
   RunMainLoop();
#endif

   MainExitServer();
}

void MainExitServer(void)
{
   lprintf("ExitServer terminating server\n");

   ExitAsyncConnections();
   CloseAllSessions();
   CloseDefaultChannels();

   ResetLoadMotd();
   ResetLoadBof();
   ResetTables();
   ResetBufferPool();
   ResetSysTimer();
   ResetDLlist();
   ResetNameID();
   ResetAccount();
   ResetUser();
   ResetString();
   ExitRooms();
   AStarShutdown();
   ResetResource();
   ResetTimer();
   ResetList();
   ResetObject();
   ResetMessage();
   ResetClass();
   ResetConfig();
   DeleteAllBlocks();
}
#endif // BLAK_PLATFORM_LINUX

void MainReloadGameData(void)
{
   ResetAdminConstants();
   ResetUser();
   ResetString();
   ResetRooms();
   ResetLoadMotd();
   ResetLoadBof();
   ResetDLlist();
   ResetNameID();
   ResetResource();
   ResetTimer();
   ResetList();
   ResetTables();
   ResetObject();
   ResetMessage();
   ResetClass();

   InitNameID();
   LoadMotd();
   LoadBof();
   LoadRsc();
   LoadKodbase();

   UpdateSecurityRedbook();
   LoadAdminConstants();
}

#ifdef BLAK_PLATFORM_WINDOWS
char * GetLastErrorStr()
{
   char *error_str;

   error_str = "No error string";

   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL,GetLastError(),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US),
      (LPTSTR) &error_str,0,NULL);
   return error_str;
}
#endif
