// Meridian 59 - PostgreSQL database implementation for Linux
// Replaces MySQL database.h/database.c with libpq-based PostgreSQL support.
// Same public API as the MySQL version for seamless integration.

#ifndef _DATABASE_PG_H
#define _DATABASE_PG_H

#include <libpq-fe.h>

#pragma region Structs/Enums

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
   STAT_SPELLS = 10,
   STAT_SPELL_REAGENT = 11,
   STAT_TREASURE_GEN = 12,
   STAT_MONSTER = 13,
   STAT_MONSTER_ZONE = 14,
   STAT_NPCS = 15,
   STAT_WEAPONS = 16,
   STAT_ROOMS = 17,
   STAT_NPC_ZONE = 18,
   STAT_NPC_SELLITEM = 19,
   STAT_NPC_SELLSKILL = 20,
   STAT_NPC_SELLSPELL = 21,
   STAT_REAGENTS = 22,
   STAT_FOOD = 23,
   STAT_AMMO = 24,
   STAT_ARMOR = 25,
   STAT_MISCITEMS = 26,
   STAT_RINGS = 27,
   STAT_RODS = 28,
   STAT_POTIONS = 29,
   STAT_SCROLLS = 30,
   STAT_WANDS = 31,
   STAT_QUESTITEMS = 32,
   STAT_SKILLS = 33,
   STAT_NECKLACE = 34,
   STAT_INSTRUMENTS = 35,
   STAT_GEMS = 36,
   STAT_OFFERINGS = 37,
   STAT_QUESTS = 38,
   STAT_TREASURE_EXTRA = 39,
   STAT_TREASURE_MAGIC = 40,
   STAT_NPC_SELLCOND = 41,
   STAT_LOGPEN = 42,
   STAT_LOGPEN_ITEM = 43,
   STAT_QUESTGIVER = 44,
   STAT_ACCOUNTS = 45,
   STAT_ACCOUNT_CHARS = 46,
   STAT_COMPL_QUESTS = 47,
   STAT_COMPL_QUEST_ITEMS = 48,
   STAT_MAXSTAT = 48
};

typedef enum sql_recordtype sql_recordtype;

struct sql_data_node
{
   int type;
   union {
      int num;
      char *str;
   } value;
};

struct sql_queue_node
{
   sql_recordtype type;
   int num_fields;
   void *data;
   sql_queue_node *next;
};

struct sql_queue
{
   pthread_mutex_t mutex;
   int count;
   sql_queue_node *first;
   sql_queue_node *last;
};

// timestamp_pos: 0 = no auto-timestamp, 1 = NOW() as first value, -1 = NOW() as last value
// has_serial: true if table has SERIAL PRIMARY KEY as first column
typedef struct {
   sql_recordtype stat_type;
   int num_fields;
   bool enabled;
   const char *table_name;
   int timestamp_pos;
   bool has_serial;
} sql_statistic_type;

// Public API - same names as MySQL version for compatibility
void MySQLInit(char *Host, int Port, char *User, char *Password, char *DB);
void MySQLEnd();
BOOL MySQLRecordGeneric(int type, int num_fields, sql_data_node data[]);
BOOL MySQLEmptyTable(int type);
UINT64 MySQLGetRecordCount();
char *MySQLDuplicateString(char *str);
bool MySQLIsTypeEnabled(int type);
bool MySQLSetTypeEnabled(int type, bool enabled);
void FreeDataNodeMemory(int total_fields, int fields_entered, sql_data_node data[]);

#endif
