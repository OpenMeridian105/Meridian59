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
   int num_fields;
   bool enabled;
   char *procedure_name;
   char *table_name;
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
   STAT_MAXSTAT = 46 // Equal to highest defined stat.
};

// Statistics_Table entries have the format:
//  - STAT_TYPE: the constant for this statistic used in RecordStat()
//  - int: the number of columns in the table
//  - bool: whether stat recording is enabled
//  - string: SQL write procedure name, called when writing a record to DB
//  - string: SQL table name
static sql_statistic_type Statistics_Table[] = {
   {STAT_NONE,           0, false,  NULL, NULL},
   {STAT_TOTALMONEY,     1, true, "WriteTotalMoney", "money_total"},
   {STAT_MONEYCREATED,   1, true, "WriteMoneyCreated", "money_created"},
   {STAT_PLAYERLOGIN,    3, true, "WritePlayerLogin", "player_logins"},
   {STAT_ASSESS_DAM,     7, true, "WritePlayerAssessDamage", "player_damaged"},
   {STAT_PLAYERDEATH,    5, true, "WritePlayerDeath", "player_death"},
   {STAT_PLAYER,        13, true, "WritePlayer", "player"},
   {STAT_PLAYERSUICIDE,  2, true, "WritePlayerSuicide", "player"},
   {STAT_GUILD,          4, true, "WriteGuild", "guild"},
   {STAT_GUILDDISBAND,   1, true, "WriteGuildDisband", "guild"},
   {STAT_SPELLS,        14, true, "WriteSpells", "wiki_spells"},
   {STAT_SPELL_REAGENT,  3, true, "WriteSpellReagent", "wiki_spell_reagent"},
   {STAT_TREASURE_GEN,   3, true, "WriteTreasureGen", "wiki_treasure_gen"},
   {STAT_MONSTER,       13, true, "WriteMonster", "wiki_monster"},
   {STAT_MONSTER_ZONE,   3, true, "WriteMonsterZone", "wiki_monster_zone"},
   {STAT_NPCS,           7, true, "WriteNpcs", "wiki_npcs"},
   {STAT_WEAPONS,       14, true, "WriteWeapons", "wiki_weapons"},
   {STAT_ROOMS,          6, true, "WriteRooms", "wiki_rooms"},
   {STAT_NPC_ZONE,       4, true, "WriteNpcZone", "wiki_npc_zone"},
   {STAT_NPC_SELLITEM,   3, true, "WriteNpcSellItem", "wiki_npc_sellitem"},
   {STAT_NPC_SELLSKILL,  2, true, "WriteNpcSellSkill", "wiki_npc_sellskill"},
   {STAT_NPC_SELLSPELL,  2, true, "WriteNpcSellSpell", "wiki_npc_sellspell"},
   {STAT_REAGENTS,      11, true, "WriteReagents", "wiki_reagents"},
   {STAT_FOOD,          11, true, "WriteFood", "wiki_food"},
   {STAT_AMMO,          11, true, "WriteAmmo", "wiki_ammo"},
   {STAT_ARMOR,         11, true, "WriteArmor", "wiki_armor"},
   {STAT_MISCITEMS,     11, true, "WriteMiscItems", "wiki_miscitems"},
   {STAT_RINGS,         11, true, "WriteRings", "wiki_rings"},
   {STAT_RODS,          11, true, "WriteRods", "wiki_rods"},
   {STAT_POTIONS,       11, true, "WritePotions", "wiki_potions"},
   {STAT_SCROLLS,       11, true, "WriteScrolls", "wiki_scrolls"},
   {STAT_WANDS,         11, true, "WriteWands", "wiki_wands"},
   {STAT_QUESTITEMS,    11, true, "WriteQuestItems", "wiki_questitems"},
   {STAT_SKILLS,        12, true, "WriteSkills", "wiki_skills"},
   {STAT_NECKLACE,      11, true, "WriteNecklace", "wiki_necklace"},
   {STAT_INSTRUMENTS,   11, true, "WriteInstruments", "wiki_instruments"},
   {STAT_GEMS,          11, true, "WriteGems", "wiki_gems"},
   {STAT_OFFERINGS,     11, true, "WriteOfferings", "wiki_offerings"},
   {STAT_QUESTS,        11, true, "WriteQuests", "wiki_quests"},
   {STAT_TREASURE_EXTRA, 4, true, "WriteTreasureExtra", "wiki_treasure_extra"},
   {STAT_TREASURE_MAGIC, 3, true, "WriteTreasureMagic", "wiki_treasure_magic"},
   {STAT_NPC_SELLCOND,   4, true, "WriteNpcSellCond", "wiki_npc_sellcond"},
   {STAT_LOGPEN,         7, true, "WriteLogpen", "player_logpen"},
   {STAT_LOGPEN_ITEM,    3, true, "WriteLogpenItem", "player_logpen_items"},
   {STAT_QUESTGIVER,     2, true, "WriteQuestGiver", "wiki_quest_giver"},
   {STAT_ACCOUNTS,       8, true, "WriteAccount", "server_accounts"},
   {STAT_ACCOUNT_CHARS,  2, true, "WriteAccountChar", "server_account_chars"}
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

UINT64 MySQLGetRecordCount();
char *MySQLDuplicateString(char *str);
bool MySQLIsTypeEnabled(int type);
bool MySQLSetTypeEnabled(int type, bool enabled);
void FreeDataNodeMemory(int total_fields, int fields_entered, sql_data_node data[]);
void MySQLInit(char* Host, int Port, char* User, char* Password, char* DB);
void MySQLEnd();
BOOL MySQLRecordGeneric(int type, int num_fields, sql_data_node data[]);
BOOL MySQLEmptyTable(int type);
void __cdecl _MySQLWorker(void* Parameters);
void _MySQLVerifySchema();
BOOL _MySQLEnqueue(sql_queue_node* Node);
BOOL _MySQLDequeue(BOOL processNode);
void _MySQLCallProc(char* Name, MYSQL_BIND Parameters[], BOOL nullParams);
void _MySQLWriteNode(sql_queue_node* Node, BOOL ProcessNode);
void _MySQLTruncate(sql_queue_node* Node, BOOL ProcessNode);
#endif
