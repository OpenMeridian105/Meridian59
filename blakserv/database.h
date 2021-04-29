// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* database.h
*

 BASIC USAGE:
 --------------------------
  * You must not call any method marked with '_' prefix from outside database.c
  * You should call method MySQLRecorGeneric to record data to DB

 STEPS TO ADD A RECORD:
 --------------------------
  1) Design a SQL table layout and add the SQL query text to section SQL in database.c.
  2) Design a stored procedure and add the SQL query text to section SQL in database.c.
  3) Add the queries from (1) and (2) to _MySQLVerifySchema() in database.c.
  4) --- TEST IF THEY GET CREATED ---
  5) Fill out entries in the sql_recordtype enum and Statistics_Table array
     below. The constant in sql_recordtype must also be added to blakston.khd.

  NOTES: Only use the kod types TAG_NIL, TAG_INT, TAG_STRING, TAG_RESOURCE
  and TAG_SESSION (which converts to account ID). If recording stuff from the
  server code directly (i.e. not through RecordStat/kod) you can use int/string/null
  but no other types are supported (but can be added as required).
*/

#ifndef _DATABASE_H
#define _DATABASE_H

#include <mysql.h>

#pragma region Structs/Enums
typedef struct sql_queue_node sql_queue_node;
typedef struct sql_queue sql_queue;
typedef struct sql_data_node sql_data_node;
typedef enum sql_recordtype sql_recordtype;
typedef enum sql_worker_state sql_worker_state;

struct sql_data_node
{
   int type; // kod type
   union {
      int num; // also stores 0 for nil, num for session etc
      char *str; // string, resource, debugstr
   } value;
};

struct sql_queue_node
{
	sql_recordtype type;
   int num_fields;
	void* data;
	sql_queue_node* next;
};

struct sql_queue
{
	HANDLE mutex;
	int count;
	sql_queue_node* first;
	sql_queue_node* last;
};

typedef struct {
   sql_recordtype  stat_type;
   char   *procedure_name;
   int num_fields;
} sql_statistic_type;

enum sql_recordtype
{
   STAT_NONE = 0,
   STAT_TOTALMONEY = 1,
   STAT_MONEYCREATED = 2,
   STAT_PLAYERLOGIN = 3,
   STAT_ASSESS_DAM = 4,
   STAT_PLAYERDEATH = 5,
   STAT_PLAYER = 6,
   STAT_PLAYERSUICIDE = 7,
   STAT_GUILD = 8,
   STAT_GUILDDISBAND = 9,
   STAT_MAX = 9 // Equal to highest defined stat.
};

// e.g. STAT_TOTALMONEY is the constant required to determine which stat
// is being recorded when calling RecordStat() from kod. WriteTotalMoney
// is the SQL procedure called when writing a record to the database. The
// 3rd field here (1 for the STAT_TOTALMONEY example) is the number of
// columns in the record.
static sql_statistic_type Statistics_Table[] = {
   {STAT_NONE, NULL, 0},
   {STAT_TOTALMONEY, "WriteTotalMoney", 1},
   {STAT_MONEYCREATED, "WriteMoneyCreated", 1},
   {STAT_PLAYERLOGIN, "WritePlayerLogin", 3},
   {STAT_ASSESS_DAM, "WritePlayerAssessDamage", 7},
   {STAT_PLAYERDEATH, "WritePlayerDeath", 5},
   {STAT_PLAYER, "WritePlayer", 13},
   {STAT_PLAYERSUICIDE, "WritePlayerSuicide", 2},
   {STAT_GUILD, "WriteGuild", 4},
   {STAT_GUILDDISBAND, "WriteGuildDisband", 1}
};

enum sql_worker_state
{
	STOPPED			= 0,
	STOPPING		= 1,
	STARTING		= 2,
	INITIALIZED		= 3,
	CONNECTED		= 4,
	SCHEMAVERIFIED	= 5
};
#pragma endregion

char *MySQLDuplicateString(char *str);
void FreeDataNodeMemory(int total_fields, int fields_entered, sql_data_node data[]);
void MySQLInit(char* Host, int Port, char* User, char* Password, char* DB);
void MySQLEnd();
BOOL MySQLRecordGeneric(int type, int num_fields, sql_data_node data[]);
void __cdecl _MySQLWorker(void* Parameters);
void _MySQLVerifySchema();
BOOL _MySQLEnqueue(sql_queue_node* Node);
BOOL _MySQLDequeue(BOOL processNode);
void _MySQLCallProc(char* Name, MYSQL_BIND Parameters[]);
void _MySQLWriteNode(sql_queue_node* Node, BOOL ProcessNode);
#endif
