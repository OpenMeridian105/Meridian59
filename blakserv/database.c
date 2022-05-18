// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* database.c
*
*/

#include "blakserv.h"

#define SLEEPTIME              10
#define SLEEPTIMENOCON         1000
#define MAX_RECORD_QUEUE       15000
#define RECORD_ENQUEUE_TIMEOUT 60
#define RECORD_DEQUEUE_TIMEOUT 60
#define MAX_ITEMS_UNTIL_LOOP   10
#define SLEEPINIT              100

// Max number of parameters to an SQL call. Based on max kod params.
#define MAX_SQL_PARAMS 45

sql_queue queue         = {0, 0, 0, 0};
HANDLE hMySQLWorker     = 0;
sql_worker_state state	= STOPPED;
MYSQL* mysql   = 0;
char* host     = 0;
char* user     = 0;
char* password = 0;
char* db       = 0;
int   port     = 3306;
UINT record_count = 0;

// Calls mysql_query with query 'b' and logs an error if query fails.
// Doesn't log 'table already exists' error.
#define MYSQL_QUERY_CHECKED(a, b, c) \
   c = mysql_query(a, b); \
   if (c != 0) { \
      const char *_err = mysql_error(a); \
      if (!(strstr(_err, "Table") && strstr(_err, "already exists"))) { \
         bprintf("MySQL error %i on line %i: %s", c, __LINE__, _err); } }

#pragma region SQL
#define SQLQUERY_CREATETABLE_MONEYTOTAL                 "\
   CREATE TABLE money_total (                            \
     money_total_time   DATETIME NOT NULL,               \
     money_total_amount VARCHAR(18) NOT NULL,            \
     PRIMARY KEY (money_total_time, money_total_amount)  \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_MONEYCREATED                   "\
   CREATE TABLE money_created (                              \
     money_created_time    DATETIME NOT NULL,                \
     money_created_amount  INT(11) NOT NULL,                 \
     PRIMARY KEY (money_created_time, money_created_amount)  \
   )                                                         \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYERLOGINS           "\
   CREATE TABLE player_logins (                      \
     plogin_account_name    VARCHAR(45) NOT NULL,    \
     plogin_character_name  VARCHAR(45) NOT NULL,    \
     plogin_IP              VARCHAR(45) NOT NULL,    \
     plogin_time            DATETIME NOT NULL,       \
     PRIMARY KEY (plogin_account_name, plogin_time)  \
   )                                                 \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYERDAMAGED              "\
   CREATE TABLE player_damaged (                         \
     idpdamaged        INT(11) NOT NULL AUTO_INCREMENT,  \
     pdamaged_who      VARCHAR(45) NOT NULL,             \
     pdamaged_attacker VARCHAR(45) NOT NULL,             \
     pdamaged_aspell   INT(11) NOT NULL,                 \
     pdamaged_atype    INT(11) NOT NULL,                 \
     pdamaged_applied  INT(11) NOT NULL,                 \
     pdamaged_original INT(11)   NOT NULL,               \
     pdamaged_weapon   VARCHAR(45) NOT NULL,             \
     pdamaged_time     DATETIME NOT NULL,                \
     PRIMARY KEY (idpdamaged)                            \
   )                                                     \
   ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYERDEATH      "\
   CREATE TABLE player_death (                 \
     pdeath_victim VARCHAR(45) NOT NULL,       \
     pdeath_killer VARCHAR(45) NOT NULL,       \
     pdeath_room   VARCHAR(45) NOT NULL,       \
     pdeath_attack VARCHAR(45) NOT NULL,       \
     pdeath_ispvp  INT(1) NOT NULL,            \
     pdeath_time   DATETIME NOT NULL,          \
     PRIMARY KEY (pdeath_victim, pdeath_time)  \
   )                                           \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"


#define SQLQUERY_CREATETABLE_LOGPEN                     "\
   CREATE TABLE player_logpen                            \
   (                                                     \
     player_name      VARCHAR(63) NOT NULL,              \
     logpen_time      DATETIME NOT NULL,                 \
     room_id          INT(6),                            \
     logpen_xp        INT(8) NOT NULL,                   \
     logpen_hp        INT(8) NOT NULL,                   \
     logpen_spellpct  INT(6) NOT NULL,                   \
     logpen_skillpct  INT(6) NOT NULL,                   \
     logpen_numitems  INT(4) NOT NULL,                   \
     PRIMARY KEY(player_name, logpen_time)               \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_LOGPEN_ITEMS               "\
   CREATE TABLE player_logpen_items                      \
   (                                                     \
     logpen_item_id   INT(12) NOT NULL AUTO_INCREMENT,   \
     player_name      VARCHAR(63) NOT NULL,              \
     logpen_time      DATETIME NOT NULL,                 \
     item_name        VARCHAR(63) NOT NULL,              \
     item_number      INT(6) NOT NULL,                   \
     PRIMARY KEY(logpen_item_id)                         \
   )                                                     \
   ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYER                         "\
   CREATE TABLE player (                                     \
     player_account_id     INT(11) NOT NULL,                 \
     player_name           VARCHAR(45) NOT NULL,             \
     player_home           VARCHAR(128) DEFAULT NULL,        \
     player_bind           VARCHAR(128) DEFAULT NULL,        \
     player_guild          VARCHAR(45) DEFAULT NULL,         \
     player_max_health     INT(6) DEFAULT NULL,              \
     player_max_mana       INT(6) DEFAULT NULL,              \
     player_might          INT(4) DEFAULT NULL,              \
     player_int            INT(4) DEFAULT NULL,              \
     player_myst           INT(4) DEFAULT NULL,              \
     player_stam           INT(4) DEFAULT NULL,              \
     player_agil           INT(4) DEFAULT NULL,              \
     player_aim            INT(4) DEFAULT NULL,              \
     player_suicide        INT(1) DEFAULT '0',               \
     player_suicide_time   DATETIME DEFAULT NULL,            \
     PRIMARY KEY(player_name)                                \
   )                                                         \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_GUILD                          "\
   CREATE TABLE guild (                                      \
      guild_name            VARCHAR(100) NOT NULL,           \
      guild_leader          VARCHAR(45) NOT NULL,            \
      guild_hall            VARCHAR(100) DEFAULT NULL,       \
      guild_rent_paid       INT(11) NOT NULL,                \
      guild_disbanded       INT(1) NOT NULL DEFAULT '0',     \
      guild_disbanded_time  DATETIME DEFAULT NULL,           \
      PRIMARY KEY(guild_name)                                \
   )                                                         \
   ENGINE = InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET = latin1;"

#define SQLQUERY_CREATETABLE_SPELLS                     "\
   CREATE TABLE wiki_spells                              \
   (                                                     \
     spell_id             INT(4) NOT NULL,               \
     spell_name           VARCHAR(63) NOT NULL,          \
     spell_name_ger       VARCHAR(63) NOT NULL,          \
     spell_icon           VARCHAR(63) NOT NULL,          \
     spell_desc           TEXT DEFAULT NULL,             \
     spell_desc_ger       TEXT DEFAULT NULL,             \
     spell_school         INT(4) DEFAULT NULL,           \
     spell_level          INT(4) DEFAULT NULL,           \
     spell_mana           INT(4) DEFAULT NULL,           \
     spell_chance         INT(4) DEFAULT NULL,           \
     spell_mediate_ratio  INT(4) DEFAULT NULL,           \
     spell_exertion       INT(4) DEFAULT NULL,           \
     spell_casttime       INT(4) DEFAULT NULL,           \
     spell_iflag          INT(8) NOT NULL,               \
     PRIMARY KEY(spell_id)                               \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_SKILLS                     "\
   CREATE TABLE wiki_skills                              \
   (                                                     \
     skill_id             INT(4) NOT NULL,               \
     skill_name           VARCHAR(63) NOT NULL,          \
     skill_name_ger       VARCHAR(63) NOT NULL,          \
     skill_icon           VARCHAR(63) NOT NULL,          \
     skill_desc           TEXT DEFAULT NULL,             \
     skill_desc_ger       TEXT DEFAULT NULL,             \
     skill_school         INT(4) DEFAULT NULL,           \
     skill_level          INT(4) DEFAULT NULL,           \
     skill_chance         INT(4) DEFAULT NULL,           \
     skill_mediate_ratio  INT(4) DEFAULT NULL,           \
     skill_exertion       INT(4) DEFAULT NULL,           \
     skill_iflag          INT(8) NOT NULL,               \
     PRIMARY KEY(skill_id)                               \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_SPELL_REAGENT              "\
   CREATE TABLE wiki_spell_reagent                       \
   (                                                     \
     spell_id              INT(4) NOT NULL,              \
     spell_reagent         VARCHAR(63) NOT NULL,         \
     spell_reagent_amount  INT(4) DEFAULT NULL,          \
     PRIMARY KEY(spell_id, spell_reagent)                \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_TREASURE_GEN               "\
   CREATE TABLE wiki_treasure_gen                        \
   (                                                     \
     treasure_id          INT(4) NOT NULL,               \
     item_name            VARCHAR(63) NOT NULL,          \
     item_chance          INT(4) DEFAULT NULL,           \
     PRIMARY KEY(treasure_id, item_name)                 \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_TREASURE_EXTRA             "\
   CREATE TABLE wiki_treasure_extra                      \
   (                                                     \
     treasure_id          INT(4) NOT NULL,               \
     item_name            VARCHAR(63) NOT NULL,          \
     item_min_amount      INT(8) DEFAULT NULL,           \
     item_max_amount      INT(8) DEFAULT NULL,           \
     PRIMARY KEY(treasure_id, item_name)                 \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_TREASURE_MAGIC                "\
   CREATE TABLE wiki_treasure_magic                         \
   (                                                        \
     treasure_id          INT(4) NOT NULL,                  \
     item_name            VARCHAR(63) NOT NULL,             \
     item_attribute_id    INT(4) NOT NULL,                  \
     PRIMARY KEY(treasure_id, item_name, item_attribute_id) \
   )                                                        \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_MONSTER                    "\
   CREATE TABLE wiki_monster                             \
   (                                                     \
     monster_name            VARCHAR(63) NOT NULL,       \
     monster_name_ger        VARCHAR(63) NOT NULL,       \
     monster_icon            VARCHAR(63) NOT NULL,       \
     monster_desc            TEXT DEFAULT NULL,          \
     monster_desc_ger        TEXT DEFAULT NULL,          \
     monster_level           INT(4) DEFAULT NULL,        \
     monster_karma           INT(4) DEFAULT NULL,        \
     monster_treasure        INT(4) DEFAULT NULL,        \
     monster_speed           INT(4) DEFAULT NULL,        \
     monster_behavior        INT(10) DEFAULT NULL,        \
     monster_difficulty      INT(4) DEFAULT NULL,        \
     monster_visiondistance  INT(4) DEFAULT NULL,        \
     monster_iflag           INT(8) DEFAULT NULL,        \
     PRIMARY KEY(monster_name)                           \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_MONSTER_ZONE               "\
   CREATE TABLE wiki_monster_zone                        \
   (                                                     \
     monster_rid          INT(6) NOT NULL,               \
     monster_name         VARCHAR(63) NOT NULL,          \
     monster_spawnchance  INT(4) DEFAULT NULL,           \
     PRIMARY KEY(monster_rid, monster_name)              \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NPCS                       "\
   CREATE TABLE wiki_npcs                                \
   (                                                     \
     npc_name            VARCHAR(63) NOT NULL,           \
     npc_name_ger        VARCHAR(63) NOT NULL,           \
     npc_icon            VARCHAR(63) NOT NULL,           \
     npc_desc            TEXT DEFAULT NULL,              \
     npc_desc_ger        TEXT DEFAULT NULL,              \
     npc_merchantmarkup  INT(4) DEFAULT NULL,            \
     npc_iflag           INT(8) DEFAULT NULL,            \
     PRIMARY KEY(npc_name)                               \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_WEAPONS                    "\
   CREATE TABLE wiki_weapons                             \
   (                                                     \
     weapon_name      VARCHAR(63) NOT NULL,              \
     weapon_name_ger  VARCHAR(63) NOT NULL,              \
     weapon_icon      VARCHAR(63) NOT NULL,              \
     weapon_group     INT(4) NOT NULL,                   \
     weapon_color     INT(4) NOT NULL,                   \
     weapon_desc      TEXT DEFAULT NULL,                 \
     weapon_desc_ger  TEXT DEFAULT NULL,                 \
     weapon_value     INT(8) DEFAULT NULL,               \
     weapon_weight    INT(4) DEFAULT NULL,               \
     weapon_bulk      INT(4) DEFAULT NULL,               \
     weapon_range     INT(4) DEFAULT NULL,               \
     weapon_skill     INT(4) DEFAULT NULL,               \
     weapon_prof      INT(4) DEFAULT NULL,               \
     weapon_iflag     INT(8) DEFAULT NULL,               \
     PRIMARY KEY(weapon_name)                            \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_ROOMS                      "\
   CREATE TABLE wiki_rooms                               \
   (                                                     \
     room_name      VARCHAR(63) NOT NULL,                \
     room_name_ger  VARCHAR(63) NOT NULL,                \
     room_roo       VARCHAR(63) NOT NULL,                \
     room_number    INT(6) NOT NULL,                     \
     room_region    INT(4) NOT NULL,                     \
     room_iflag     INT(8) NOT NULL,                     \
     PRIMARY KEY(room_number)                            \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NPC_ZONE                   "\
   CREATE TABLE wiki_npc_zone                            \
   (                                                     \
     npc_name      VARCHAR(63) NOT NULL,                 \
     npc_roomid    INT(6) NOT NULL,                      \
     npc_row       INT(4) NOT NULL,                      \
     npc_col       INT(4) NOT NULL,                      \
     PRIMARY KEY(npc_name, npc_roomid, npc_row, npc_col) \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NPC_SELLITEM             "\
   CREATE TABLE wiki_npc_sellitem                      \
   (                                                   \
     npc_name        VARCHAR(63) NOT NULL,             \
     npc_item_sold   VARCHAR(63) NOT NULL,             \
     item_color      INT(4) NOT NULL,                  \
     PRIMARY KEY(npc_name,npc_item_sold, item_color)   \
   )                                                   \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NPC_SELLSKILL              "\
   CREATE TABLE wiki_npc_sellskill                       \
   (                                                     \
     npc_name        VARCHAR(63) NOT NULL,               \
     npc_skill_id    INT(4) NOT NULL,                    \
     PRIMARY KEY(npc_name,npc_skill_id)                  \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NPC_SELLSPELL              "\
   CREATE TABLE wiki_npc_sellspell                       \
   (                                                     \
     npc_name          VARCHAR(63) NOT NULL,             \
     npc_spell_id      INT(4) NOT NULL,                  \
     PRIMARY KEY(npc_name,npc_spell_id)                  \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NPC_SELLCOND             "\
   CREATE TABLE wiki_npc_sellcond                      \
   (                                                   \
     npc_name        VARCHAR(63) NOT NULL,             \
     npc_item_sold   VARCHAR(63) NOT NULL,             \
     item_color      INT(4) NOT NULL,                  \
     item_price      INT(8) NOT NULL,                  \
     PRIMARY KEY(npc_name,npc_item_sold, item_color)   \
   )                                                   \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_REAGENTS                   "\
   CREATE TABLE wiki_reagents                            \
   (                                                     \
     reagent_name      VARCHAR(63) NOT NULL,             \
     reagent_name_ger  VARCHAR(63) NOT NULL,             \
     reagent_icon      VARCHAR(63) NOT NULL,             \
     reagent_group     INT(4) NOT NULL,                  \
     reagent_color     INT(4) NOT NULL,                  \
     reagent_desc      TEXT DEFAULT NULL,                \
     reagent_desc_ger  TEXT DEFAULT NULL,                \
     reagent_value     INT(8) DEFAULT NULL,              \
     reagent_weight    INT(4) DEFAULT NULL,              \
     reagent_bulk      INT(4) DEFAULT NULL,              \
     reagent_iflag     INT(8) DEFAULT NULL,              \
     PRIMARY KEY(reagent_name)                           \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_FOOD                       "\
   CREATE TABLE wiki_food                                \
   (                                                     \
     food_name      VARCHAR(63) NOT NULL,                \
     food_name_ger  VARCHAR(63) NOT NULL,                \
     food_icon      VARCHAR(63) NOT NULL,                \
     food_group     INT(4) NOT NULL,                     \
     food_color     INT(4) NOT NULL,                     \
     food_desc      TEXT DEFAULT NULL,                   \
     food_desc_ger  TEXT DEFAULT NULL,                   \
     food_value     INT(8) DEFAULT NULL,                 \
     food_weight    INT(4) DEFAULT NULL,                 \
     food_bulk      INT(4) DEFAULT NULL,                 \
     food_iflag     INT(8) DEFAULT NULL,                 \
     PRIMARY KEY(food_name)                              \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_AMMO                       "\
   CREATE TABLE wiki_ammo                                \
   (                                                     \
     ammo_name      VARCHAR(63) NOT NULL,                \
     ammo_name_ger  VARCHAR(63) NOT NULL,                \
     ammo_icon      VARCHAR(63) NOT NULL,                \
     ammo_group     INT(4) NOT NULL,                     \
     ammo_color     INT(4) NOT NULL,                     \
     ammo_desc      TEXT DEFAULT NULL,                   \
     ammo_desc_ger  TEXT DEFAULT NULL,                   \
     ammo_value     INT(8) DEFAULT NULL,                 \
     ammo_weight    INT(4) DEFAULT NULL,                 \
     ammo_bulk      INT(4) DEFAULT NULL,                 \
     ammo_iflag     INT(8) DEFAULT NULL,                 \
     PRIMARY KEY(ammo_name)                              \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_ARMOR                      "\
   CREATE TABLE wiki_armor                               \
   (                                                     \
     armor_name      VARCHAR(63) NOT NULL,               \
     armor_name_ger  VARCHAR(63) NOT NULL,               \
     armor_icon      VARCHAR(63) NOT NULL,               \
     armor_group     INT(4) NOT NULL,                    \
     armor_color     INT(4) NOT NULL,                    \
     armor_desc      TEXT DEFAULT NULL,                  \
     armor_desc_ger  TEXT DEFAULT NULL,                  \
     armor_value     INT(8) DEFAULT NULL,                \
     armor_weight    INT(4) DEFAULT NULL,                \
     armor_bulk      INT(4) DEFAULT NULL,                \
     armor_iflag     INT(8) DEFAULT NULL,                \
     PRIMARY KEY(armor_name)                             \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_MISCITEMS                  "\
   CREATE TABLE wiki_miscitems                           \
   (                                                     \
     misc_name      VARCHAR(63) NOT NULL,                \
     misc_name_ger  VARCHAR(63) NOT NULL,                \
     misc_icon      VARCHAR(63) NOT NULL,                \
     misc_group     INT(4) NOT NULL,                     \
     misc_color     INT(4) NOT NULL,                     \
     misc_desc      TEXT DEFAULT NULL,                   \
     misc_desc_ger  TEXT DEFAULT NULL,                   \
     misc_value     INT(8) DEFAULT NULL,                 \
     misc_weight    INT(4) DEFAULT NULL,                 \
     misc_bulk      INT(4) DEFAULT NULL,                 \
     misc_iflag     INT(8) DEFAULT NULL,                 \
     PRIMARY KEY(misc_name)                              \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_RINGS                      "\
   CREATE TABLE wiki_rings                               \
   (                                                     \
     rings_name      VARCHAR(63) NOT NULL,               \
     rings_name_ger  VARCHAR(63) NOT NULL,               \
     rings_icon      VARCHAR(63) NOT NULL,               \
     rings_group     INT(4) NOT NULL,                    \
     rings_color     INT(4) NOT NULL,                    \
     rings_desc      TEXT DEFAULT NULL,                  \
     rings_desc_ger  TEXT DEFAULT NULL,                  \
     rings_value     INT(8) DEFAULT NULL,                \
     rings_weight    INT(4) DEFAULT NULL,                \
     rings_bulk      INT(4) DEFAULT NULL,                \
     rings_iflag     INT(8) DEFAULT NULL,                \
     PRIMARY KEY(rings_name)                             \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_RODS                       "\
   CREATE TABLE wiki_rods                                \
   (                                                     \
     rods_name      VARCHAR(63) NOT NULL,                \
     rods_name_ger  VARCHAR(63) NOT NULL,                \
     rods_icon      VARCHAR(63) NOT NULL,                \
     rods_group     INT(4) NOT NULL,                     \
     rods_color     INT(4) NOT NULL,                     \
     rods_desc      TEXT DEFAULT NULL,                   \
     rods_desc_ger  TEXT DEFAULT NULL,                   \
     rods_value     INT(8) DEFAULT NULL,                 \
     rods_weight    INT(4) DEFAULT NULL,                 \
     rods_bulk      INT(4) DEFAULT NULL,                 \
     rods_iflag     INT(8) DEFAULT NULL,                 \
     PRIMARY KEY(rods_name)                              \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_POTIONS                    "\
   CREATE TABLE wiki_potions                             \
   (                                                     \
     potion_name      VARCHAR(63) NOT NULL,              \
     potion_name_ger  VARCHAR(63) NOT NULL,              \
     potion_icon      VARCHAR(63) NOT NULL,              \
     potion_group     INT(4) NOT NULL,                   \
     potion_color     INT(4) NOT NULL,                   \
     potion_desc      TEXT DEFAULT NULL,                 \
     potion_desc_ger  TEXT DEFAULT NULL,                 \
     potion_value     INT(8) DEFAULT NULL,               \
     potion_weight    INT(4) DEFAULT NULL,               \
     potion_bulk      INT(4) DEFAULT NULL,               \
     potion_iflag     INT(8) DEFAULT NULL,               \
     PRIMARY KEY(potion_name)                            \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_SCROLLS                    "\
   CREATE TABLE wiki_scrolls                             \
   (                                                     \
     scrolls_name      VARCHAR(63) NOT NULL,             \
     scrolls_name_ger  VARCHAR(63) NOT NULL,             \
     scrolls_icon      VARCHAR(63) NOT NULL,             \
     scrolls_group     INT(4) NOT NULL,                  \
     scrolls_color     INT(4) NOT NULL,                  \
     scrolls_desc      TEXT DEFAULT NULL,                \
     scrolls_desc_ger  TEXT DEFAULT NULL,                \
     scrolls_value     INT(8) DEFAULT NULL,              \
     scrolls_weight    INT(4) DEFAULT NULL,              \
     scrolls_bulk      INT(4) DEFAULT NULL,              \
     scrolls_iflag     INT(8) DEFAULT NULL,              \
     PRIMARY KEY(scrolls_name)                           \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_WANDS                      "\
   CREATE TABLE wiki_wands                               \
   (                                                     \
     wands_name      VARCHAR(63) NOT NULL,               \
     wands_name_ger  VARCHAR(63) NOT NULL,               \
     wands_icon      VARCHAR(63) NOT NULL,               \
     wands_group     INT(4) NOT NULL,                    \
     wands_color     INT(4) NOT NULL,                    \
     wands_desc      TEXT DEFAULT NULL,                  \
     wands_desc_ger  TEXT DEFAULT NULL,                  \
     wands_value     INT(8) DEFAULT NULL,                \
     wands_weight    INT(4) DEFAULT NULL,                \
     wands_bulk      INT(4) DEFAULT NULL,                \
     wands_iflag     INT(8) DEFAULT NULL,                \
     PRIMARY KEY(wands_name)                             \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_QUESTITEMS                 "\
   CREATE TABLE wiki_questitems                          \
   (                                                     \
     questitem_name      VARCHAR(63) NOT NULL,           \
     questitem_name_ger  VARCHAR(63) NOT NULL,           \
     questitem_icon      VARCHAR(63) NOT NULL,           \
     questitem_group     INT(4) NOT NULL,                \
     questitem_color     INT(4) NOT NULL,                \
     questitem_desc      TEXT DEFAULT NULL,              \
     questitem_desc_ger  TEXT DEFAULT NULL,              \
     questitem_value     INT(8) DEFAULT NULL,            \
     questitem_weight    INT(4) DEFAULT NULL,            \
     questitem_bulk      INT(4) DEFAULT NULL,            \
     questitem_iflag     INT(8) DEFAULT NULL,            \
     PRIMARY KEY(questitem_name)                         \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_NECKLACE                   "\
   CREATE TABLE wiki_necklace                            \
   (                                                     \
     necklace_name      VARCHAR(63) NOT NULL,            \
     necklace_name_ger  VARCHAR(63) NOT NULL,            \
     necklace_icon      VARCHAR(63) NOT NULL,            \
     necklace_group     INT(4) NOT NULL,                 \
     necklace_color     INT(4) NOT NULL,                 \
     necklace_desc      TEXT DEFAULT NULL,               \
     necklace_desc_ger  TEXT DEFAULT NULL,               \
     necklace_value     INT(8) DEFAULT NULL,             \
     necklace_weight    INT(4) DEFAULT NULL,             \
     necklace_bulk      INT(4) DEFAULT NULL,             \
     necklace_iflag     INT(8) DEFAULT NULL,             \
     PRIMARY KEY(necklace_name)                          \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_INSTRUMENTS                "\
   CREATE TABLE wiki_instruments                         \
   (                                                     \
     instrument_name      VARCHAR(63) NOT NULL,          \
     instrument_name_ger  VARCHAR(63) NOT NULL,          \
     instrument_icon      VARCHAR(63) NOT NULL,          \
     instrument_group     INT(4) NOT NULL,               \
     instrument_color     INT(4) NOT NULL,               \
     instrument_desc      TEXT DEFAULT NULL,             \
     instrument_desc_ger  TEXT DEFAULT NULL,             \
     instrument_value     INT(8) DEFAULT NULL,           \
     instrument_weight    INT(4) DEFAULT NULL,           \
     instrument_bulk      INT(4) DEFAULT NULL,           \
     instrument_iflag     INT(8) DEFAULT NULL,           \
     PRIMARY KEY(instrument_name)                        \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_GEMS                       "\
   CREATE TABLE wiki_gems                                \
   (                                                     \
     gem_name      VARCHAR(63) NOT NULL,                 \
     gem_name_ger  VARCHAR(63) NOT NULL,                 \
     gem_icon      VARCHAR(63) NOT NULL,                 \
     gem_group     INT(4) NOT NULL,                      \
     gem_color     INT(4) NOT NULL,                      \
     gem_desc      TEXT DEFAULT NULL,                    \
     gem_desc_ger  TEXT DEFAULT NULL,                    \
     gem_value     INT(8) DEFAULT NULL,                  \
     gem_weight    INT(4) DEFAULT NULL,                  \
     gem_bulk      INT(4) DEFAULT NULL,                  \
     gem_iflag     INT(8) DEFAULT NULL,                  \
     PRIMARY KEY(gem_name)                               \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_OFFERINGS                  "\
   CREATE TABLE wiki_offerings                           \
   (                                                     \
     offering_name      VARCHAR(63) NOT NULL,            \
     offering_name_ger  VARCHAR(63) NOT NULL,            \
     offering_icon      VARCHAR(63) NOT NULL,            \
     offering_group     INT(4) NOT NULL,                 \
     offering_color     INT(4) NOT NULL,                 \
     offering_desc      TEXT DEFAULT NULL,               \
     offering_desc_ger  TEXT DEFAULT NULL,               \
     offering_value     INT(8) DEFAULT NULL,             \
     offering_weight    INT(4) DEFAULT NULL,             \
     offering_bulk      INT(4) DEFAULT NULL,             \
     offering_iflag     INT(8) DEFAULT NULL,             \
     PRIMARY KEY(offering_name)                          \
   )                                                     \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_QUESTS                   "\
   CREATE TABLE wiki_quests                            \
   (                                                   \
     quest_id           INT(4) NOT NULL,               \
     quest_name         VARCHAR(63) NOT NULL,          \
     quest_name_ger     VARCHAR(63) NOT NULL,          \
     quest_icon         VARCHAR(63) NOT NULL,          \
     quest_icon_group   INT(2) NOT NULL,               \
     quest_desc         TEXT DEFAULT NULL,             \
     quest_desc_ger     TEXT DEFAULT NULL,             \
     quest_recent_time  INT(12) DEFAULT NULL,          \
     quest_schedule_pct INT(4) DEFAULT NULL,           \
     quest_est_time     INT(12) DEFAULT NULL,          \
     quest_est_diff     INT(4) DEFAULT NULL,           \
     PRIMARY KEY(quest_id)                             \
   )                                                   \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"


#define SQLQUERY_CREATETABLE_QUESTGIVERS              "\
   CREATE TABLE wiki_quest_giver                       \
   (                                                   \
     quest_id           INT(4) NOT NULL,               \
     quest_npc_name     VARCHAR(63) NOT NULL,          \
     PRIMARY KEY(quest_id, quest_npc_name)             \
   )                                                   \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"


#define SQLQUERY_CREATETABLE_ACCOUNTS "\
   CREATE TABLE server_accounts (acct_id INT(8) NOT NULL, acct_name VARCHAR(63) NOT NULL,\
     acct_password VARCHAR(63) NOT NULL, acct_email VARCHAR(255) NOT NULL, acct_type INT(2) NOT NULL,\
     acct_loggedin_time INT(12) NOT NULL, acct_last_login INT(12) NOT NULL, acct_suspend_time INT(12) NOT NULL,\
    PRIMARY KEY(acct_id)) \
   ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_ACCOUNT_CHARS "\
   CREATE TABLE server_account_chars (entry_id INT(8) NOT NULL AUTO_INCREMENT, acct_id INT(8) NOT NULL,\
      char_name VARCHAR(63) NOT NULL,\
    PRIMARY KEY(entry_id)) \
   ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATEPROC_MONEYTOTAL      "\
   CREATE PROCEDURE WriteTotalMoney(       \n\
      IN total_money VARCHAR(18))          \n\
   BEGIN                                   \n\
     INSERT INTO money_total               \n\
      SET                                  \n\
        money_total_amount = total_money,  \n\
        money_total_time = now();          \n\
   END"

#define SQLQUERY_CREATEPROC_MONEYCREATED        "\
   CREATE PROCEDURE WriteMoneyCreated(         \n\
      IN money_created INT(11))                \n\
   BEGIN                                       \n\
     INSERT INTO money_created                 \n\
      SET                                      \n\
        money_created_amount = money_created,  \n\
        money_created_time = now();            \n\
   END"

#define SQLQUERY_CREATEPROC_PLAYERLOGIN   "\
   CREATE PROCEDURE WritePlayerLogin(    \n\
     IN account   VARCHAR(45),           \n\
     IN charname  VARCHAR(45),           \n\
     IN ip        VARCHAR(45))           \n\
   BEGIN                                 \n\
     INSERT INTO player_logins           \n\
      SET                                \n\
      plogin_account_name   = account,   \n\
      plogin_character_name = charname,  \n\
      plogin_IP             = ip,        \n\
      plogin_time           = now();     \n\
   END"

#define SQLQUERY_CREATEPROC_PLAYERASSESSDAMAGE  "\
   CREATE PROCEDURE WritePlayerAssessDamage(   \n\
     IN who        VARCHAR(45),                \n\
     IN attacker   VARCHAR(45),                \n\
     IN aspell     INT(11),                    \n\
     IN atype      INT(11),                    \n\
     IN applied    INT(11),                    \n\
     IN original   INT(11),                    \n\
     IN weapon     VARCHAR(45))                \n\
   BEGIN                                       \n\
     INSERT INTO player_damaged                \n\
      SET                                      \n\
      pdamaged_who      = who,                 \n\
      pdamaged_attacker = attacker,            \n\
      pdamaged_aspell   = aspell,              \n\
      pdamaged_atype    = atype,               \n\
      pdamaged_applied  = applied,             \n\
      pdamaged_original = original,            \n\
      pdamaged_weapon   = weapon,              \n\
      pdamaged_time     = now();               \n\
   END"

#define SQLQUERY_CREATEPROC_PLAYERDEATH  "\
   CREATE PROCEDURE WritePlayerDeath(   \n\
     IN victim  VARCHAR(45),            \n\
     IN killer  VARCHAR(45),            \n\
     IN room    VARCHAR(45),            \n\
     IN attack  VARCHAR(45),            \n\
     IN ispvp   INT(1))                 \n\
   BEGIN                                \n\
     INSERT INTO player_death           \n\
      SET                               \n\
       pdeath_victim  = victim,         \n\
       pdeath_killer  = killer,         \n\
       pdeath_room    = room,           \n\
       pdeath_attack  = attack,         \n\
       pdeath_ispvp   = ispvp,          \n\
       pdeath_time    = now();          \n\
   END"

#define SQLQUERY_CREATEPROC_LOGPEN             "\
   CREATE PROCEDURE WriteLogpen(              \n\
     IN player_name      VARCHAR(63),         \n\
     IN room_id          INT(6),              \n\
     IN logpen_xp        INT(8),              \n\
     IN logpen_hp        INT(8),              \n\
     IN logpen_spellpct  INT(6),              \n\
     IN logpen_skillpct  INT(6),              \n\
     IN logpen_numitems  INT(4))              \n\
   BEGIN                                      \n\
     INSERT INTO player_logpen                \n\
      SET                                     \n\
       player_name  = player_name,            \n\
       logpen_time  = now(),                  \n\
       room_id      = room_id,                \n\
       logpen_xp    = logpen_xp,              \n\
       logpen_hp    = logpen_hp,              \n\
       logpen_spellpct = logpen_spellpct,     \n\
       logpen_skillpct  = logpen_skillpct,    \n\
       logpen_numitems = logpen_numitems;     \n\
   END"

#define SQLQUERY_CREATEPROC_LOGPEN_ITEM    "\
   CREATE PROCEDURE WriteLogpenItem(      \n\
     IN player_name     VARCHAR(63),      \n\
     IN item_name       VARCHAR(63),      \n\
     IN item_number     INT(6))           \n\
   BEGIN                                  \n\
     INSERT INTO player_logpen_items      \n\
      SET                                 \n\
       player_name     = player_name,     \n\
       logpen_time     = now(),           \n\
       item_name       = item_name,       \n\
       item_number     = item_number;     \n\
   END"

#define SQLQUERY_CREATEPROC_PLAYER    "\
   CREATE PROCEDURE WritePlayer(       \
     IN account_id   INT(11),          \
     IN p_name       VARCHAR(45),      \
     IN home         VARCHAR(128),     \
     IN bind         VARCHAR(128),     \
     IN guild        VARCHAR(45),      \
     IN max_health   INT(6),           \
     IN max_mana     INT(6),           \
     IN might        INT(4),           \
     IN p_int        INT(4),           \
     IN myst         INT(4),           \
     IN stam         INT(4),           \
     IN agil         INT(4),           \
     IN aim          INT(4))           \
   BEGIN                               \
     INSERT INTO player                \
      (  player_account_id,            \
         player_name,                  \
         player_home,                  \
         player_bind,                  \
         player_guild,                 \
         player_max_health,            \
         player_max_mana,              \
         player_might,                 \
         player_int,                   \
         player_myst,                  \
         player_stam,                  \
         player_agil,                  \
         player_aim)                   \
         VALUES (account_id,           \
            p_name,                    \
            home,                      \
            bind,                      \
            guild,                     \
            max_health,                \
            max_mana,                  \
            might,                     \
            p_int,                     \
            myst,                      \
            stam,                      \
            agil,                      \
            aim)                       \
      ON DUPLICATE KEY UPDATE          \
      player_account_id = account_id,  \
      player_home = home,              \
      player_bind = bind,              \
      player_guild = guild,            \
      player_max_health = max_health,  \
      player_max_mana = max_mana,      \
      player_might = might,            \
      player_int = p_int,              \
      player_myst = myst,              \
      player_stam = stam,              \
      player_agil = agil,              \
      player_aim = aim;                \
   END"

#define SQLQUERY_CREATEPROC_PLAYERSUICIDE    "\
   CREATE PROCEDURE WritePlayerSuicide(     \n\
     IN account_id   INT(11),               \n\
     IN name         VARCHAR(45))           \n\
   BEGIN                                    \n\
     UPDATE player                          \n\
      SET                                   \n\
      player_suicide = 1,                   \n\
      player_suicide_time = now()           \n\
      WHERE player_account_id = account_id  \n\
         AND player_name = name;            \n\
   END"

#define SQLQUERY_CREATEPROC_GUILD  "\
   CREATE PROCEDURE WriteGuild(     \
     IN name      VARCHAR(100),     \
     IN leader    VARCHAR(100),     \
     IN hall      VARCHAR(100),     \
     IN rent      INT(11))          \
   BEGIN                            \
     INSERT INTO guild              \
      (  guild_name,                \
         guild_leader,              \
         guild_hall,                \
         guild_rent_paid)           \
         VALUES (                   \
            name,                   \
            leader,                 \
            hall,                   \
            rent)                   \
      ON DUPLICATE KEY UPDATE       \
      guild_leader = leader,        \
      guild_hall = hall,            \
      guild_rent_paid = rent;       \
   END"

#define SQLQUERY_CREATEPROC_GUILDDISBAND  "\
   CREATE PROCEDURE WriteGuildDisband(   \n\
     IN name      VARCHAR(100))          \n\
   BEGIN                                 \n\
     UPDATE guild                        \n\
      SET                                \n\
      guild_hall = '',                   \n\
      guild_disbanded = 1,               \n\
      guild_disbanded_time = now()       \n\
      WHERE guild_name = name;           \n\
   END"

#define SQLQUERY_CREATEPROC_SPELLS                "\
   CREATE PROCEDURE WriteSpells(                   \
     IN spell_id             INT(4),               \
     IN spell_name           VARCHAR(63),          \
     IN spell_name_ger       VARCHAR(63),          \
     IN spell_icon           VARCHAR(63),          \
     IN spell_desc           TEXT,                 \
     IN spell_desc_ger       TEXT,                 \
     IN spell_school         INT(4),               \
     IN spell_level          INT(4),               \
     IN spell_mana           INT(4),               \
     IN spell_chance         INT(4),               \
     IN spell_mediate_ratio  INT(4),               \
     IN spell_exertion       INT(4),               \
     IN spell_casttime       INT(4),               \
     IN spell_iflag          INT(8))               \
   BEGIN                                           \
     INSERT INTO wiki_spells                       \
      (  spell_id,                                 \
         spell_name,                               \
         spell_name_ger,                           \
         spell_icon,                               \
         spell_desc,                               \
         spell_desc_ger,                           \
         spell_school,                             \
         spell_level,                              \
         spell_mana,                               \
         spell_chance,                             \
         spell_mediate_ratio,                      \
         spell_exertion,                           \
         spell_casttime,                           \
         spell_iflag)                              \
         VALUES (spell_id,                         \
            spell_name,                            \
            spell_name_ger,                        \
            spell_icon,                            \
            spell_desc,                            \
            spell_desc_ger,                        \
            spell_school,                          \
            spell_level,                           \
            spell_mana,                            \
            spell_chance,                          \
            spell_mediate_ratio,                   \
            spell_exertion,                        \
            spell_casttime,                        \
            spell_iflag)                           \
      ON DUPLICATE KEY UPDATE                      \
      spell_name = spell_name,                     \
      spell_name_ger = spell_name_ger,             \
      spell_desc = spell_desc,                     \
      spell_desc_ger = spell_desc_ger,             \
      spell_school = spell_school,                 \
      spell_level = spell_level,                   \
      spell_mana = spell_mana,                     \
      spell_chance = spell_chance,                 \
      spell_mediate_ratio = spell_mediate_ratio,   \
      spell_exertion = spell_exertion,             \
      spell_mediate_ratio = spell_mediate_ratio,   \
      spell_casttime = spell_casttime,             \
      spell_iflag = spell_iflag;                   \
   END"

#define SQLQUERY_CREATEPROC_SKILLS              "\
   CREATE PROCEDURE WriteSkills(                 \
     IN skill_id             INT(4),             \
     IN skill_name           VARCHAR(63),        \
     IN skill_name_ger       VARCHAR(63),        \
     IN skill_icon           VARCHAR(63),        \
     IN skill_desc           TEXT,               \
     IN skill_desc_ger       TEXT,               \
     IN skill_school         INT(4),             \
     IN skill_level          INT(4),             \
     IN skill_chance         INT(4),             \
     IN skill_mediate_ratio  INT(4),             \
     IN skill_exertion       INT(4),             \
     IN skill_iflag          INT(8))             \
   BEGIN                                         \
     INSERT INTO wiki_skills                     \
      (  skill_id,                               \
         skill_name,                             \
         skill_name_ger,                         \
         skill_icon,                             \
         skill_desc,                             \
         skill_desc_ger,                         \
         skill_school,                           \
         skill_level,                            \
         skill_chance,                           \
         skill_mediate_ratio,                    \
         skill_exertion,                         \
         skill_iflag)                            \
         VALUES (skill_id,                       \
            skill_name,                          \
            skill_name_ger,                      \
            skill_icon,                          \
            skill_desc,                          \
            skill_desc_ger,                      \
            skill_school,                        \
            skill_level,                         \
            skill_chance,                        \
            skill_mediate_ratio,                 \
            skill_exertion,                      \
            skill_iflag)                         \
      ON DUPLICATE KEY UPDATE                    \
      skill_name = skill_name,                   \
      skill_name_ger = skill_name_ger,           \
      skill_desc = skill_desc,                   \
      skill_desc_ger = skill_desc_ger,           \
      skill_school = skill_school,               \
      skill_level = skill_level,                 \
      skill_chance = skill_chance,               \
      skill_mediate_ratio = skill_mediate_ratio, \
      skill_exertion = skill_exertion,           \
      skill_iflag = skill_iflag;                 \
   END"

#define SQLQUERY_CREATEPROC_SPELL_REAGENT  "\
   CREATE PROCEDURE WriteSpellReagent(      \
     IN spell_id              INT(4),       \
     IN spell_reagent         VARCHAR(63),  \
     IN spell_reagent_amount  INT(4))       \
   BEGIN                                    \
   INSERT INTO wiki_spell_reagent           \
      (  spell_id,                          \
         spell_reagent,                     \
         spell_reagent_amount)              \
         VALUES (spell_id,                  \
            spell_reagent,                  \
            spell_reagent_amount);          \
   END"

#define SQLQUERY_CREATEPROC_TREASURE_GEN  "\
   CREATE PROCEDURE WriteTreasureGen(      \
     IN treasure_id          INT(4),       \
     IN item_name            VARCHAR(63),  \
     IN item_chance          INT(4))       \
   BEGIN                                   \
   INSERT INTO wiki_treasure_gen           \
      (  treasure_id,                      \
         item_name,                        \
         item_chance)                      \
         VALUES (treasure_id,              \
            item_name,                     \
            item_chance);                  \
   END"

#define SQLQUERY_CREATEPROC_TREASURE_EXTRA  "\
   CREATE PROCEDURE WriteTreasureExtra(      \
     IN treasure_id          INT(4),         \
     IN item_name            VARCHAR(63),    \
     IN item_min_amount      INT(8),         \
     IN item_max_amount      INT(8))         \
   BEGIN                                     \
   INSERT INTO wiki_treasure_extra           \
      (  treasure_id,                        \
         item_name,                          \
         item_min_amount,                    \
         item_max_amount)                    \
         VALUES (treasure_id,                \
            item_name,                       \
            item_min_amount,                 \
            item_max_amount);                \
   END"

#define SQLQUERY_CREATEPROC_TREASURE_MAGIC  "\
   CREATE PROCEDURE WriteTreasureMagic(      \
     IN treasure_id          INT(4),         \
     IN item_name            VARCHAR(63),    \
     IN item_attribute_id    INT(4))         \
   BEGIN                                     \
   INSERT INTO wiki_treasure_magic           \
      (  treasure_id,                        \
         item_name,                          \
         item_attribute_id)                  \
         VALUES (treasure_id,                \
            item_name,                       \
            item_attribute_id);              \
   END"

#define SQLQUERY_CREATEPROC_MONSTER                    "\
   CREATE PROCEDURE WriteMonster(                       \
     IN monster_name            VARCHAR(63),            \
     IN monster_name_ger        VARCHAR(63),            \
     IN monster_icon            VARCHAR(63),            \
     IN monster_desc            TEXT,                   \
     IN monster_desc_ger        TEXT,                   \
     IN monster_level           INT(4),                 \
     IN monster_karma           INT(4),                 \
     IN monster_treasure        INT(4),                 \
     IN monster_speed           INT(4),                 \
     IN monster_behavior        INT(10),                \
     IN monster_difficulty      INT(4),                 \
     IN monster_visiondistance  INT(4),                 \
     IN monster_iflag           INT(8))                 \
   BEGIN                                                \
   INSERT INTO wiki_monster                             \
      (  monster_name,                                  \
         monster_name_ger,                              \
         monster_icon,                                  \
         monster_desc,                                  \
         monster_desc_ger,                              \
         monster_level,                                 \
         monster_karma,                                 \
         monster_treasure,                              \
         monster_speed,                                 \
         monster_behavior,                              \
         monster_difficulty,                            \
         monster_visiondistance,                        \
         monster_iflag)                                 \
         VALUES (monster_name,                          \
            monster_name_ger,                           \
            monster_icon,                               \
            monster_desc,                               \
            monster_desc_ger,                           \
            monster_level,                              \
            monster_karma,                              \
            monster_treasure,                           \
            monster_speed,                              \
            monster_behavior,                           \
            monster_difficulty,                         \
            monster_visiondistance,                     \
            monster_iflag)                              \
      ON DUPLICATE KEY UPDATE                           \
      monster_desc = monster_desc,                      \
      monster_desc_ger = monster_desc_ger,              \
      monster_level = monster_level,                    \
      monster_karma = monster_karma,                    \
      monster_treasure = monster_treasure,              \
      monster_speed = monster_speed,                    \
      monster_behavior = monster_behavior,              \
      monster_difficulty = monster_difficulty,          \
      monster_visiondistance = monster_visiondistance,  \
      monster_iflag = monster_iflag;                    \
   END"

#define SQLQUERY_CREATEPROC_MONSTER_ZONE    "\
   CREATE PROCEDURE WriteMonsterZone(        \
     IN monster_rid          INT(6),         \
     IN monster_name         VARCHAR(63),    \
     IN monster_spawnchance  INT(4))         \
   BEGIN                                     \
   INSERT INTO wiki_monster_zone             \
      (  monster_rid,                        \
         monster_name,                       \
         monster_spawnchance)                \
         VALUES (monster_rid,                \
            monster_name,                    \
            monster_spawnchance);            \
   END"

#define SQLQUERY_CREATEPROC_NPCS               "\
   CREATE PROCEDURE WriteNpcs(                  \
      IN npc_name            VARCHAR(63),       \
      IN npc_name_ger        VARCHAR(63),       \
      IN npc_icon            VARCHAR(63),       \
      IN npc_desc            TEXT,              \
      IN npc_desc_ger        TEXT,              \
      IN npc_merchantmarkup  INT(4),            \
      IN npc_iflag           INT(8))            \
   BEGIN                                        \
   INSERT INTO wiki_npcs                        \
      (  npc_name,                              \
         npc_name_ger,                          \
         npc_icon,                              \
         npc_desc,                              \
         npc_desc_ger,                          \
         npc_merchantmarkup,                    \
         npc_iflag)                             \
         VALUES (npc_name,                      \
            npc_name_ger,                       \
            npc_icon,                           \
            npc_desc,                           \
            npc_desc_ger,                       \
            npc_merchantmarkup,                 \
            npc_iflag)                          \
      ON DUPLICATE KEY UPDATE                   \
      npc_name_ger = npc_name_ger,              \
      npc_desc = npc_desc,                      \
      npc_desc_ger = npc_desc_ger,              \
      npc_merchantmarkup = npc_merchantmarkup,  \
      npc_iflag = npc_iflag;                    \
   END"

#define SQLQUERY_CREATEPROC_WEAPONS      "\
   CREATE PROCEDURE WriteWeapons(         \
     IN weapon_name      VARCHAR(63),     \
     IN weapon_name_ger  VARCHAR(63),     \
     IN weapon_icon      VARCHAR(63),     \
     IN weapon_group     INT(4),          \
     IN weapon_color     INT(4),          \
     IN weapon_desc      TEXT,            \
     IN weapon_desc_ger  TEXT,            \
     IN weapon_value     INT(8),          \
     IN weapon_weight    INT(4),          \
     IN weapon_bulk      INT(4),          \
     IN weapon_range     INT(4),          \
     IN weapon_skill     INT(4),          \
     IN weapon_prof      INT(4),          \
     IN weapon_iflag     INT(8))          \
   BEGIN                                  \
   INSERT INTO wiki_weapons               \
      (  weapon_name,                     \
         weapon_name_ger,                 \
         weapon_icon,                     \
         weapon_group,                    \
         weapon_color,                    \
         weapon_desc,                     \
         weapon_desc_ger,                 \
         weapon_value,                    \
         weapon_weight,                   \
         weapon_bulk,                     \
         weapon_range,                    \
         weapon_skill,                    \
         weapon_prof,                     \
         weapon_iflag)                    \
         VALUES (weapon_name,             \
            weapon_name_ger,              \
            weapon_icon,                  \
            weapon_group,                 \
            weapon_color,                 \
            weapon_desc,                  \
            weapon_desc_ger,              \
            weapon_value,                 \
            weapon_weight,                \
            weapon_bulk,                  \
            weapon_range,                 \
            weapon_skill,                 \
            weapon_prof,                  \
            weapon_iflag)                 \
      ON DUPLICATE KEY UPDATE             \
      weapon_group = weapon_group,        \
      weapon_color = weapon_color,        \
      weapon_desc = weapon_desc,          \
      weapon_desc_ger = weapon_desc_ger,  \
      weapon_value = weapon_value,        \
      weapon_weight = weapon_weight,      \
      weapon_bulk = weapon_bulk,          \
      weapon_range = weapon_range,        \
      weapon_skill = weapon_skill,        \
      weapon_prof = weapon_prof,          \
      weapon_iflag = weapon_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_ROOMS        "\
   CREATE PROCEDURE WriteRooms(           \
     IN room_name      VARCHAR(63),       \
     IN room_name_ger  VARCHAR(63),       \
     IN room_roo       VARCHAR(63),       \
     IN room_number    INT(6),            \
     IN room_region    INT(4),            \
     IN room_iflag     INT(8))            \
   BEGIN                                  \
   INSERT INTO wiki_rooms                 \
      (  room_name,                       \
         room_name_ger,                   \
         room_roo,                        \
         room_number,                     \
         room_region,                     \
         room_iflag)                      \
         VALUES (room_name,               \
            room_name_ger,                \
            room_roo,                     \
            room_number,                  \
            room_region,                  \
            room_iflag)                   \
      ON DUPLICATE KEY UPDATE             \
      room_name = room_name,              \
      room_name_ger = room_name_ger,      \
      room_roo = room_roo,                \
      room_region = room_region,          \
      room_iflag = room_iflag;            \
   END"

#define SQLQUERY_CREATEPROC_NPC_ZONE     "\
   CREATE PROCEDURE WriteNpcZone(         \
     IN npc_name      VARCHAR(63),        \
     IN npc_roomid    INT(6),             \
     IN npc_row       INT(4),             \
     IN npc_col       INT(4))             \
   BEGIN                                  \
   INSERT INTO wiki_npc_zone              \
      (  npc_name,                        \
         npc_roomid,                      \
         npc_row,                         \
         npc_col)                         \
         VALUES (npc_name,                \
            npc_roomid,                   \
            npc_row,                      \
            npc_col);                     \
   END"

#define SQLQUERY_CREATEPROC_NPC_SELLITEM "\
   CREATE PROCEDURE WriteNpcSellItem(     \
     IN npc_name       VARCHAR(63),       \
     IN npc_item_sold  VARCHAR(63),       \
     IN item_color     INT(4))            \
   BEGIN                                  \
   INSERT INTO wiki_npc_sellitem          \
      (  npc_name,                        \
         npc_item_sold,                   \
         item_color)                      \
         VALUES (npc_name,                \
            npc_item_sold,                \
            item_color);                  \
   END"

#define SQLQUERY_CREATEPROC_NPC_SELLSKILL   "\
   CREATE PROCEDURE WriteNpcSellSkill(       \
     IN npc_name        VARCHAR(63),         \
     IN npc_skill_id    INT(4))              \
   BEGIN                                     \
   INSERT INTO wiki_npc_sellskill            \
      (  npc_name,                           \
         npc_skill_id)                       \
         VALUES (npc_name,                   \
            npc_skill_id);                   \
   END"

#define SQLQUERY_CREATEPROC_NPC_SELLSPELL   "\
   CREATE PROCEDURE WriteNpcSellSpell(       \
     IN npc_name        VARCHAR(63),         \
     IN npc_spell_id    INT(4))              \
   BEGIN                                     \
   INSERT INTO wiki_npc_sellspell            \
      (  npc_name,                           \
         npc_spell_id)                       \
         VALUES (npc_name,                   \
            npc_spell_id);                   \
   END"

#define SQLQUERY_CREATEPROC_NPC_SELLCOND "\
   CREATE PROCEDURE WriteNpcSellCond(     \
     IN npc_name       VARCHAR(63),       \
     IN npc_item_sold  VARCHAR(63),       \
     IN item_color     INT(4),            \
     IN item_price     INT(8))            \
   BEGIN                                  \
   INSERT INTO wiki_npc_sellcond          \
      (  npc_name,                        \
         npc_item_sold,                   \
         item_color,                      \
         item_price)                      \
         VALUES (npc_name,                \
            npc_item_sold,                \
            item_color,                   \
            item_price);                  \
   END"

#define SQLQUERY_CREATEPROC_REAGENTS          "\
   CREATE PROCEDURE WriteReagents(             \
     IN reagent_name      VARCHAR(63),         \
     IN reagent_name_ger  VARCHAR(63),         \
     IN reagent_icon      TEXT,                \
     IN reagent_group     INT(4),              \
     IN reagent_color     INT(4),              \
     IN reagent_desc      TEXT,                \
     IN reagent_desc_ger  TEXT,                \
     IN reagent_value     INT(8),              \
     IN reagent_weight    INT(4),              \
     IN reagent_bulk      INT(4),              \
     IN reagent_iflag     INT(8))              \
   BEGIN                                       \
   INSERT INTO wiki_reagents                   \
      (  reagent_name,                         \
         reagent_name_ger,                     \
         reagent_icon,                         \
         reagent_group,                        \
         reagent_color,                        \
         reagent_desc,                         \
         reagent_desc_ger,                     \
         reagent_value,                        \
         reagent_weight,                       \
         reagent_bulk,                         \
         reagent_iflag)                        \
         VALUES (reagent_name,                 \
            reagent_name_ger,                  \
            reagent_icon,                      \
            reagent_group,                     \
            reagent_color,                     \
            reagent_desc,                      \
            reagent_desc_ger,                  \
            reagent_value,                     \
            reagent_weight,                    \
            reagent_bulk,                      \
            reagent_iflag)                     \
         ON DUPLICATE KEY UPDATE               \
         reagent_icon = reagent_icon,          \
         reagent_group = reagent_group,        \
         reagent_color = reagent_color,        \
         reagent_desc = reagent_desc,          \
         reagent_desc_ger = reagent_desc_ger,  \
         reagent_value = reagent_value,        \
         reagent_weight = reagent_weight,      \
         reagent_bulk = reagent_bulk,          \
         reagent_iflag = reagent_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_FOOD         "\
   CREATE PROCEDURE WriteFood(            \
     IN food_name      VARCHAR(63),       \
     IN food_name_ger  VARCHAR(63),       \
     IN food_icon      TEXT,              \
     IN food_group     INT(4),            \
     IN food_color     INT(4),            \
     IN food_desc      TEXT,              \
     IN food_desc_ger  TEXT,              \
     IN food_value     INT(8),            \
     IN food_weight    INT(4),            \
     IN food_bulk      INT(4),            \
     IN food_iflag     INT(8))            \
   BEGIN                                  \
   INSERT INTO wiki_food                  \
      (  food_name,                       \
         food_name_ger,                   \
         food_icon,                       \
         food_group,                      \
         food_color,                      \
         food_desc,                       \
         food_desc_ger,                   \
         food_value,                      \
         food_weight,                     \
         food_bulk,                       \
         food_iflag)                      \
         VALUES (food_name,               \
            food_name_ger,                \
            food_icon,                    \
            food_group,                   \
            food_color,                   \
            food_desc,                    \
            food_desc_ger,                \
            food_value,                   \
            food_weight,                  \
            food_bulk,                    \
            food_iflag)                   \
         ON DUPLICATE KEY UPDATE          \
         food_group = food_group,         \
         food_color = food_color,         \
         food_desc = food_desc,           \
         food_desc_ger = food_desc_ger,   \
         food_value = food_value,         \
         food_weight = food_weight,       \
         food_bulk = food_bulk,           \
         food_iflag = food_iflag;         \
   END"

#define SQLQUERY_CREATEPROC_AMMO         "\
   CREATE PROCEDURE WriteAmmo(            \
     IN ammo_name      VARCHAR(63),       \
     IN ammo_name_ger  VARCHAR(63),       \
     IN ammo_icon      TEXT,              \
     IN ammo_group     INT(4),            \
     IN ammo_color     INT(4),            \
     IN ammo_desc      TEXT,              \
     IN ammo_desc_ger  TEXT,              \
     IN ammo_value     INT(8),            \
     IN ammo_weight    INT(4),            \
     IN ammo_bulk      INT(4),            \
     IN ammo_iflag     INT(8))            \
   BEGIN                                  \
   INSERT INTO wiki_ammo                  \
      (  ammo_name,                       \
         ammo_name_ger,                   \
         ammo_icon,                       \
         ammo_group,                      \
         ammo_color,                      \
         ammo_desc,                       \
         ammo_desc_ger,                   \
         ammo_value,                      \
         ammo_weight,                     \
         ammo_bulk,                       \
         ammo_iflag)                      \
         VALUES (ammo_name,               \
            ammo_name_ger,                \
            ammo_icon,                    \
            ammo_group,                   \
            ammo_color,                   \
            ammo_desc,                    \
            ammo_desc_ger,                \
            ammo_value,                   \
            ammo_weight,                  \
            ammo_bulk,                    \
            ammo_iflag)                   \
         ON DUPLICATE KEY UPDATE          \
         ammo_group = ammo_group,         \
         ammo_color = ammo_color,         \
         ammo_desc = ammo_desc,           \
         ammo_desc_ger = ammo_desc_ger,   \
         ammo_value = ammo_value,         \
         ammo_weight = ammo_weight,       \
         ammo_bulk = ammo_bulk,           \
         ammo_iflag = ammo_iflag;         \
   END"

#define SQLQUERY_CREATEPROC_ARMOR        "\
   CREATE PROCEDURE WriteArmor(           \
     IN armor_name      VARCHAR(63),      \
     IN armor_name_ger  VARCHAR(63),      \
     IN armor_icon      TEXT,             \
     IN armor_group     INT(4),           \
     IN armor_color     INT(4),           \
     IN armor_desc      TEXT,             \
     IN armor_desc_ger  TEXT,             \
     IN armor_value     INT(8),           \
     IN armor_weight    INT(4),           \
     IN armor_bulk      INT(4),           \
     IN armor_iflag     INT(8))           \
   BEGIN                                  \
   INSERT INTO wiki_armor                 \
      (  armor_name,                      \
         armor_name_ger,                  \
         armor_icon,                      \
         armor_group,                     \
         armor_color,                     \
         armor_desc,                      \
         armor_desc_ger,                  \
         armor_value,                     \
         armor_weight,                    \
         armor_bulk,                      \
         armor_iflag)                     \
         VALUES (armor_name,              \
            armor_name_ger,               \
            armor_icon,                   \
            armor_group,                  \
            armor_color,                  \
            armor_desc,                   \
            armor_desc_ger,               \
            armor_value,                  \
            armor_weight,                 \
            armor_bulk,                   \
            armor_iflag)                  \
         ON DUPLICATE KEY UPDATE          \
         armor_group = armor_group,       \
         armor_color = armor_color,       \
         armor_desc = armor_desc,         \
         armor_desc_ger = armor_desc_ger, \
         armor_value = armor_value,       \
         armor_weight = armor_weight,     \
         armor_bulk = armor_bulk,         \
         armor_iflag = armor_iflag;       \
   END"

#define SQLQUERY_CREATEPROC_MISCITEMS    "\
   CREATE PROCEDURE WriteMiscItems(       \
     IN misc_name      VARCHAR(63),       \
     IN misc_name_ger  VARCHAR(63),       \
     IN misc_icon      TEXT,              \
     IN misc_group     INT(4),            \
     IN misc_color     INT(4),            \
     IN misc_desc      TEXT,              \
     IN misc_desc_ger  TEXT,              \
     IN misc_value     INT(8),            \
     IN misc_weight    INT(4),            \
     IN misc_bulk      INT(4),            \
     IN misc_iflag     INT(8))            \
   BEGIN                                  \
   INSERT INTO wiki_miscitems             \
      (  misc_name,                       \
         misc_name_ger,                   \
         misc_icon,                       \
         misc_group,                      \
         misc_color,                      \
         misc_desc,                       \
         misc_desc_ger,                   \
         misc_value,                      \
         misc_weight,                     \
         misc_bulk,                       \
         misc_iflag)                      \
         VALUES (misc_name,               \
            misc_name_ger,                \
            misc_icon,                    \
            misc_group,                   \
            misc_color,                   \
            misc_desc,                    \
            misc_desc_ger,                \
            misc_value,                   \
            misc_weight,                  \
            misc_bulk,                    \
            misc_iflag)                   \
         ON DUPLICATE KEY UPDATE          \
         misc_group = misc_group,         \
         misc_color = misc_color,         \
         misc_desc = misc_desc,           \
         misc_desc_ger = misc_desc_ger,   \
         misc_value = misc_value,         \
         misc_weight = misc_weight,       \
         misc_bulk = misc_bulk,           \
         misc_iflag = misc_iflag;         \
   END"

#define SQLQUERY_CREATEPROC_RINGS        "\
   CREATE PROCEDURE WriteRings(           \
     IN rings_name      VARCHAR(63),      \
     IN rings_name_ger  VARCHAR(63),      \
     IN rings_icon      TEXT,             \
     IN rings_group     INT(4),           \
     IN rings_color     INT(4),           \
     IN rings_desc      TEXT,             \
     IN rings_desc_ger  TEXT,             \
     IN rings_value     INT(8),           \
     IN rings_weight    INT(4),           \
     IN rings_bulk      INT(4),           \
     IN rings_iflag     INT(8))           \
   BEGIN                                  \
   INSERT INTO wiki_rings                 \
      (  rings_name,                      \
         rings_name_ger,                  \
         rings_icon,                      \
         rings_group,                     \
         rings_color,                     \
         rings_desc,                      \
         rings_desc_ger,                  \
         rings_value,                     \
         rings_weight,                    \
         rings_bulk,                      \
         rings_iflag)                     \
         VALUES (rings_name,              \
            rings_name_ger,               \
            rings_icon,                   \
            rings_group,                  \
            rings_color,                  \
            rings_desc,                   \
            rings_desc_ger,               \
            rings_value,                  \
            rings_weight,                 \
            rings_bulk,                   \
            rings_iflag)                  \
         ON DUPLICATE KEY UPDATE          \
         rings_group = rings_group,       \
         rings_color = rings_color,       \
         rings_desc = rings_desc,         \
         rings_desc_ger = rings_desc_ger, \
         rings_value = rings_value,       \
         rings_weight = rings_weight,     \
         rings_bulk = rings_bulk,         \
         rings_iflag = rings_iflag;       \
   END"

#define SQLQUERY_CREATEPROC_RODS         "\
   CREATE PROCEDURE WriteRods(            \
     IN rods_name      VARCHAR(63),       \
     IN rods_name_ger  VARCHAR(63),       \
     IN rods_icon      TEXT,              \
     IN rods_group     INT(4),            \
     IN rods_color     INT(4),            \
     IN rods_desc      TEXT,              \
     IN rods_desc_ger  TEXT,              \
     IN rods_value     INT(8),            \
     IN rods_weight    INT(4),            \
     IN rods_bulk      INT(4),            \
     IN rods_iflag     INT(8))            \
   BEGIN                                  \
   INSERT INTO wiki_rods                  \
      (  rods_name,                       \
         rods_name_ger,                   \
         rods_icon,                       \
         rods_group,                      \
         rods_color,                      \
         rods_desc,                       \
         rods_desc_ger,                   \
         rods_value,                      \
         rods_weight,                     \
         rods_bulk,                       \
         rods_iflag)                      \
         VALUES (rods_name,               \
            rods_name_ger,                \
            rods_icon,                    \
            rods_group,                   \
            rods_color,                   \
            rods_desc,                    \
            rods_desc_ger,                \
            rods_value,                   \
            rods_weight,                  \
            rods_bulk,                    \
            rods_iflag)                   \
         ON DUPLICATE KEY UPDATE          \
         rods_group = rods_group,         \
         rods_color = rods_color,         \
         rods_desc = rods_desc,           \
         rods_desc_ger = rods_desc_ger,   \
         rods_value = rods_value,         \
         rods_weight = rods_weight,       \
         rods_bulk = rods_bulk,           \
         rods_iflag = rods_iflag;         \
   END"

#define SQLQUERY_CREATEPROC_POTIONS         "\
   CREATE PROCEDURE WritePotions(            \
     IN potion_name      VARCHAR(63),        \
     IN potion_name_ger  VARCHAR(63),        \
     IN potion_icon      TEXT,               \
     IN potion_group     INT(4),             \
     IN potion_color     INT(4),             \
     IN potion_desc      TEXT,               \
     IN potion_desc_ger  TEXT,               \
     IN potion_value     INT(8),             \
     IN potion_weight    INT(4),             \
     IN potion_bulk      INT(4),             \
     IN potion_iflag     INT(8))             \
   BEGIN                                     \
   INSERT INTO wiki_potions                  \
      (  potion_name,                        \
         potion_name_ger,                    \
         potion_icon,                        \
         potion_group,                       \
         potion_color,                       \
         potion_desc,                        \
         potion_desc_ger,                    \
         potion_value,                       \
         potion_weight,                      \
         potion_bulk,                        \
         potion_iflag)                       \
         VALUES (potion_name,                \
            potion_name_ger,                 \
            potion_icon,                     \
            potion_group,                    \
            potion_color,                    \
            potion_desc,                     \
            potion_desc_ger,                 \
            potion_value,                    \
            potion_weight,                   \
            potion_bulk,                     \
            potion_iflag)                    \
         ON DUPLICATE KEY UPDATE             \
         potion_group = potion_group,        \
         potion_color = potion_color,        \
         potion_desc = potion_desc,          \
         potion_desc_ger = potion_desc_ger,  \
         potion_value = potion_value,        \
         potion_weight = potion_weight,      \
         potion_bulk = potion_bulk,          \
         potion_iflag = potion_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_SCROLLS           "\
   CREATE PROCEDURE WriteScrolls(              \
    IN scrolls_name      VARCHAR(63),          \
    IN scrolls_name_ger  VARCHAR(63),          \
    IN scrolls_icon      TEXT,                 \
    IN scrolls_group     INT(4),               \
    IN scrolls_color     INT(4),               \
    IN scrolls_desc      TEXT,                 \
    IN scrolls_desc_ger  TEXT,                 \
    IN scrolls_value     INT(8),               \
    IN scrolls_weight    INT(4),               \
    IN scrolls_bulk      INT(4),               \
    IN scrolls_iflag     INT(8))               \
   BEGIN                                       \
   INSERT INTO wiki_scrolls                    \
      (  scrolls_name,                         \
         scrolls_name_ger,                     \
         scrolls_icon,                         \
         scrolls_group,                        \
         scrolls_color,                        \
         scrolls_desc,                         \
         scrolls_desc_ger,                     \
         scrolls_value,                        \
         scrolls_weight,                       \
         scrolls_bulk,                         \
         scrolls_iflag)                        \
         VALUES (scrolls_name,                 \
            scrolls_name_ger,                  \
            scrolls_icon,                      \
            scrolls_group,                     \
            scrolls_color,                     \
            scrolls_desc,                      \
            scrolls_desc_ger,                  \
            scrolls_value,                     \
            scrolls_weight,                    \
            scrolls_bulk,                      \
            scrolls_iflag)                     \
         ON DUPLICATE KEY UPDATE               \
         scrolls_group = scrolls_group,        \
         scrolls_color = scrolls_color,        \
         scrolls_desc = scrolls_desc,          \
         scrolls_desc_ger = scrolls_desc_ger,  \
         scrolls_value = scrolls_value,        \
         scrolls_weight = scrolls_weight,      \
         scrolls_bulk = scrolls_bulk,          \
         scrolls_iflag = scrolls_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_WANDS         "\
   CREATE PROCEDURE WriteWands(            \
    IN wands_name      VARCHAR(63),        \
    IN wands_name_ger  VARCHAR(63),        \
    IN wands_icon      TEXT,               \
    IN wands_group     INT(4),             \
    IN wands_color     INT(4),             \
    IN wands_desc      TEXT,               \
    IN wands_desc_ger  TEXT,               \
    IN wands_value     INT(8),             \
    IN wands_weight    INT(4),             \
    IN wands_bulk      INT(4),             \
    IN wands_iflag     INT(8))             \
   BEGIN                                   \
   INSERT INTO wiki_wands                  \
      (  wands_name,                       \
         wands_name_ger,                   \
         wands_icon,                       \
         wands_group,                      \
         wands_color,                      \
         wands_desc,                       \
         wands_desc_ger,                   \
         wands_value,                      \
         wands_weight,                     \
         wands_bulk,                       \
         wands_iflag)                      \
         VALUES (wands_name,               \
            wands_name_ger,                \
            wands_icon,                    \
            wands_group,                   \
            wands_color,                   \
            wands_desc,                    \
            wands_desc_ger,                \
            wands_value,                   \
            wands_weight,                  \
            wands_bulk,                    \
            wands_iflag)                   \
         ON DUPLICATE KEY UPDATE           \
         wands_group = wands_group,        \
         wands_color = wands_color,        \
         wands_desc = wands_desc,          \
         wands_desc_ger = wands_desc_ger,  \
         wands_value = wands_value,        \
         wands_weight = wands_weight,      \
         wands_bulk = wands_bulk,          \
         wands_iflag = wands_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_QUESTITEMS            "\
   CREATE PROCEDURE WriteQuestItems(               \
    IN questitem_name      VARCHAR(63),            \
    IN questitem_name_ger  VARCHAR(63),            \
    IN questitem_icon      TEXT,                   \
    IN questitem_group     INT(4),                 \
    IN questitem_color     INT(4),                 \
    IN questitem_desc      TEXT,                   \
    IN questitem_desc_ger  TEXT,                   \
    IN questitem_value     INT(8),                 \
    IN questitem_weight    INT(4),                 \
    IN questitem_bulk      INT(4),                 \
    IN questitem_iflag     INT(8))                 \
   BEGIN                                           \
   INSERT INTO wiki_questitems                     \
      (  questitem_name,                           \
         questitem_name_ger,                       \
         questitem_icon,                           \
         questitem_group,                          \
         questitem_color,                          \
         questitem_desc,                           \
         questitem_desc_ger,                       \
         questitem_value,                          \
         questitem_weight,                         \
         questitem_bulk,                           \
         questitem_iflag)                          \
         VALUES (questitem_name,                   \
            questitem_name_ger,                    \
            questitem_icon,                        \
            questitem_group,                       \
            questitem_color,                       \
            questitem_desc,                        \
            questitem_desc_ger,                    \
            questitem_value,                       \
            questitem_weight,                      \
            questitem_bulk,                        \
            questitem_iflag)                       \
         ON DUPLICATE KEY UPDATE                   \
         questitem_group = questitem_group,        \
         questitem_color = questitem_color,        \
         questitem_desc = questitem_desc,          \
         questitem_desc_ger = questitem_desc_ger,  \
         questitem_value = questitem_value,        \
         questitem_weight = questitem_weight,      \
         questitem_bulk = questitem_bulk,          \
         questitem_iflag = questitem_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_NECKLACE            "\
   CREATE PROCEDURE WriteNecklace(               \
    IN necklace_name      VARCHAR(63),           \
    IN necklace_name_ger  VARCHAR(63),           \
    IN necklace_icon      TEXT,                  \
    IN necklace_group     INT(4),                \
    IN necklace_color     INT(4),                \
    IN necklace_desc      TEXT,                  \
    IN necklace_desc_ger  TEXT,                  \
    IN necklace_value     INT(8),                \
    IN necklace_weight    INT(4),                \
    IN necklace_bulk      INT(4),                \
    IN necklace_iflag     INT(8))                \
   BEGIN                                         \
   INSERT INTO wiki_necklace                     \
      (  necklace_name,                          \
         necklace_name_ger,                      \
         necklace_icon,                          \
         necklace_group,                         \
         necklace_color,                         \
         necklace_desc,                          \
         necklace_desc_ger,                      \
         necklace_value,                         \
         necklace_weight,                        \
         necklace_bulk,                          \
         necklace_iflag)                         \
         VALUES (necklace_name,                  \
            necklace_name_ger,                   \
            necklace_icon,                       \
            necklace_group,                      \
            necklace_color,                      \
            necklace_desc,                       \
            necklace_desc_ger,                   \
            necklace_value,                      \
            necklace_weight,                     \
            necklace_bulk,                       \
            necklace_iflag)                      \
         ON DUPLICATE KEY UPDATE                 \
         necklace_group = necklace_group,        \
         necklace_color = necklace_color,        \
         necklace_desc = necklace_desc,          \
         necklace_desc_ger = necklace_desc_ger,  \
         necklace_value = necklace_value,        \
         necklace_weight = necklace_weight,      \
         necklace_bulk = necklace_bulk,          \
         necklace_iflag = necklace_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_INSTRUMENTS              "\
   CREATE PROCEDURE WriteInstruments(                 \
    IN instrument_name      VARCHAR(63),              \
    IN instrument_name_ger  VARCHAR(63),              \
    IN instrument_icon      TEXT,                     \
    IN instrument_group     INT(4),                   \
    IN instrument_color     INT(4),                   \
    IN instrument_desc      TEXT,                     \
    IN instrument_desc_ger  TEXT,                     \
    IN instrument_value     INT(8),                   \
    IN instrument_weight    INT(4),                   \
    IN instrument_bulk      INT(4),                   \
    IN instrument_iflag     INT(8))                   \
   BEGIN                                              \
   INSERT INTO wiki_instruments                       \
      (  instrument_name,                             \
         instrument_name_ger,                         \
         instrument_icon,                             \
         instrument_group,                            \
         instrument_color,                            \
         instrument_desc,                             \
         instrument_desc_ger,                         \
         instrument_value,                            \
         instrument_weight,                           \
         instrument_bulk,                             \
         instrument_iflag)                            \
         VALUES (instrument_name,                     \
            instrument_name_ger,                      \
            instrument_icon,                          \
            instrument_group,                         \
            instrument_color,                         \
            instrument_desc,                          \
            instrument_desc_ger,                      \
            instrument_value,                         \
            instrument_weight,                        \
            instrument_bulk,                          \
            instrument_iflag)                         \
         ON DUPLICATE KEY UPDATE                      \
         instrument_group = instrument_group,         \
         instrument_color = instrument_color,         \
         instrument_desc = instrument_desc,           \
         instrument_desc_ger = instrument_desc_ger,   \
         instrument_value = instrument_value,         \
         instrument_weight = instrument_weight,       \
         instrument_bulk = instrument_bulk,           \
         instrument_iflag = instrument_iflag;         \
   END"

#define SQLQUERY_CREATEPROC_GEMS      "\
   CREATE PROCEDURE WriteGems(         \
    IN gem_name      VARCHAR(63),      \
    IN gem_name_ger  VARCHAR(63),      \
    IN gem_icon      TEXT,             \
    IN gem_group     INT(4),           \
    IN gem_color     INT(4),           \
    IN gem_desc      TEXT,             \
    IN gem_desc_ger  TEXT,             \
    IN gem_value     INT(8),           \
    IN gem_weight    INT(4),           \
    IN gem_bulk      INT(4),           \
    IN gem_iflag     INT(8))           \
   BEGIN                               \
   INSERT INTO wiki_gems               \
      (  gem_name,                     \
         gem_name_ger,                 \
         gem_icon,                     \
         gem_group,                    \
         gem_color,                    \
         gem_desc,                     \
         gem_desc_ger,                 \
         gem_value,                    \
         gem_weight,                   \
         gem_bulk,                     \
         gem_iflag)                    \
         VALUES (gem_name,             \
            gem_name_ger,              \
            gem_icon,                  \
            gem_group,                 \
            gem_color,                 \
            gem_desc,                  \
            gem_desc_ger,              \
            gem_value,                 \
            gem_weight,                \
            gem_bulk,                  \
            gem_iflag)                 \
         ON DUPLICATE KEY UPDATE       \
         gem_group = gem_group,        \
         gem_color = gem_color,        \
         gem_desc = gem_desc,          \
         gem_desc_ger = gem_desc_ger,  \
         gem_value = gem_value,        \
         gem_weight = gem_weight,      \
         gem_bulk = gem_bulk,          \
         gem_iflag = gem_iflag;        \
   END"

#define SQLQUERY_CREATEPROC_OFFERINGS          "\
   CREATE PROCEDURE WriteOfferings(             \
    IN offering_name      VARCHAR(63),          \
    IN offering_name_ger  VARCHAR(63),          \
    IN offering_icon      TEXT,                 \
    IN offering_group     INT(4),               \
    IN offering_color     INT(4),               \
    IN offering_desc      TEXT,                 \
    IN offering_desc_ger  TEXT,                 \
    IN offering_value     INT(8),               \
    IN offering_weight    INT(4),               \
    IN offering_bulk      INT(4),               \
    IN offering_iflag     INT(8))               \
   BEGIN                                        \
   INSERT INTO wiki_offerings                   \
      (  offering_name,                         \
         offering_name_ger,                     \
         offering_icon,                         \
         offering_group,                        \
         offering_color,                        \
         offering_desc,                         \
         offering_desc_ger,                     \
         offering_value,                        \
         offering_weight,                       \
         offering_bulk,                         \
         offering_iflag)                        \
         VALUES (offering_name,                 \
            offering_name_ger,                  \
            offering_icon,                      \
            offering_group,                     \
            offering_color,                     \
            offering_desc,                      \
            offering_desc_ger,                  \
            offering_value,                     \
            offering_weight,                    \
            offering_bulk,                      \
            offering_iflag)                     \
         ON DUPLICATE KEY UPDATE                \
         offering_group = offering_group,       \
         offering_color = offering_color,       \
         offering_desc = offering_desc,         \
         offering_desc_ger = offering_desc_ger, \
         offering_value = offering_value,       \
         offering_weight = offering_weight,     \
         offering_bulk = offering_bulk,         \
         offering_iflag = offering_iflag;       \
   END"

#define SQLQUERY_CREATEPROC_QUESTS          "\
   CREATE PROCEDURE WriteQuests(             \
    IN quest_id           INT(4),            \
    IN quest_name         VARCHAR(63),       \
    IN quest_name_ger     VARCHAR(63),       \
    IN quest_icon         TEXT,              \
    IN quest_icon_group   INT(2),            \
    IN quest_desc         TEXT,              \
    IN quest_desc_ger     TEXT,              \
    IN quest_recent_time  INT(4),            \
    IN quest_schedule_pct INT(4),            \
    IN quest_est_time     INT(4),            \
    IN quest_est_diff     INT(4))            \
   BEGIN                                     \
   INSERT INTO wiki_quests                   \
      (  quest_id,                           \
         quest_name,                         \
         quest_name_ger,                     \
         quest_icon,                         \
         quest_icon_group,                   \
         quest_desc,                         \
         quest_desc_ger,                     \
         quest_recent_time,                  \
         quest_schedule_pct,                 \
         quest_est_time,                     \
         quest_est_diff)                     \
         VALUES (quest_id,                   \
            quest_name,                      \
            quest_name_ger,                  \
            quest_icon,                      \
            quest_icon_group,                \
            quest_desc,                      \
            quest_desc_ger,                  \
            quest_recent_time,               \
            quest_schedule_pct,              \
            quest_est_time,                  \
            quest_est_diff);                 \
   END"

#define SQLQUERY_CREATEPROC_QUESTGIVER      "\
   CREATE PROCEDURE WriteQuestGiver(         \
    IN quest_id           INT(4),            \
    IN quest_npc_name     VARCHAR(63))       \
   BEGIN                                     \
   INSERT INTO wiki_quest_giver              \
      (  quest_id,                           \
         quest_npc_name)                     \
         VALUES (quest_id,                   \
            quest_npc_name);                 \
   END"

#define SQLQUERY_CREATEPROC_ACCOUNT         "\
   CREATE PROCEDURE WriteAccount(IN acct_id INT(8), IN acct_name VARCHAR(63), IN acct_password VARCHAR(63),\
    IN acct_email VARCHAR(255), IN acct_type INT(2), IN acct_loggedin_time INT(12), IN acct_last_login INT(12),\
    IN acct_suspend_time INT(12)) \
   BEGIN \
   INSERT INTO server_accounts (acct_id, acct_name, acct_password, acct_email, acct_type, acct_loggedin_time,\
         acct_last_login, acct_suspend_time) \
      VALUES (acct_id, acct_name, acct_password, acct_email, acct_type, acct_loggedin_time, acct_last_login,\
               acct_suspend_time)\
      ON DUPLICATE KEY UPDATE                \
         acct_name = acct_name, acct_password = acct_password, acct_email = acct_email, acct_type = acct_type,\
         acct_loggedin_time = acct_loggedin_time, acct_last_login = acct_last_login,\
         acct_suspend_time = acct_suspend_time;\
   END"

#define SQLQUERY_CREATEPROC_ACCOUNT_CHAR        "\
   CREATE PROCEDURE WriteAccountChar(IN acct_id INT(8), IN char_name VARCHAR(63))\
   BEGIN \
   INSERT INTO server_account_chars (acct_id, char_name) \
      VALUES (acct_id, char_name); \
   END"

#pragma endregion

/*
 * MySQLGenerateDrop: Assembles a string in the passed buffer which specifies
 *   a SQL procedure to drop based on the sql record type passed.
 */
void MySQLGenerateDrop(sql_recordtype type, char *buffer)
{
   int num_chars = sprintf(buffer, "DROP PROCEDURE IF EXISTS %s;",
      Statistics_Table[type].procedure_name);
   buffer[num_chars] = 0;
}

/*
 * MySQLGenerateTruncate: Assembles a string in the passed buffer which specifies
 *   a SQL table to truncate (empty) based on the sql record type passed.
 */
void MySQLGenerateTruncate(sql_recordtype type, char *buffer)
{
   int num_chars = sprintf(buffer, "TRUNCATE TABLE %s;",
      Statistics_Table[type].table_name);
   buffer[num_chars] = 0;
}

/*
 * MySQLGenerateCall: Assembles a string in the passed buffer which specifies
 *   a call for a SQL procedure based on the sql record type passed.
 */
void MySQLGenerateCall(sql_recordtype type, char *buffer)
{
   if (Statistics_Table[type].num_fields == 0)
   {
      sprintf(buffer, "CALL %s()", Statistics_Table[type].procedure_name);
      return;
   }

   int num_chars = sprintf(buffer, "CALL %s(", Statistics_Table[type].procedure_name);
   for (int i = 0; i < Statistics_Table[type].num_fields - 1; ++i)
      num_chars += sprintf(buffer + num_chars, "?,");
   num_chars += sprintf(buffer + num_chars, "?);");
   buffer[num_chars] = 0;
}

#pragma region Public
/*
 * MySQLDuplicateString: Takes a string and returns a copy allocated with
 *   SQL's memory ID for tracking. Returns NULL if passed NULL.
 */
char *MySQLDuplicateString(char *str)
{
   if (!str)
   {
      bprintf("MySQLDuplicateString could not duplicate null string!");
      return NULL;
   }
   int sLen = strlen(str);
   char *ret_str = (char *)AllocateMemory(MALLOC_ID_SQL, sLen + 1);
   memcpy(ret_str, str, sLen);
   ret_str[sLen] = 0;

   return ret_str;
}

void MySQLInit(char* Host, int Port, char* User, char* Password, char* DB)
{	
	// don't init if already running or a parameter is nullptr
	if (state != STOPPED || !Host || !User || !Password || !DB)
		return;
	
	// set state to starting
	state = STARTING;

	// save connection info
	host		= _strdup(Host);
	user		= _strdup(User);
	password	= _strdup(Password);
	db			= _strdup(DB);
   port = Port;

	// spawn workthread
	hMySQLWorker = (HANDLE) _beginthread(_MySQLWorker, 0, 0);

	// sleep a bit to let workthread get initialized
	// e.g. no one calls MySQLRecordXY right after Init
	Sleep(SLEEPINIT);
};

void MySQLEnd()
{
	// set state to stopping
	state = STOPPING;
};


// How many SQL commands have we run/records sent.
UINT64 MySQLGetRecordCount()
{
   return record_count;
}

/*
 * MySQLTypeNumArgs: Return the number of expected arguments for a given
 *   SQL statistic type.
 */
int MySQLTypeNumArgs(int type)
{
   if (type >= STAT_NONE && type <= STAT_MAXSTAT)
      return Statistics_Table[type].num_fields;

   bprintf("Unknown type received in MySQLTypeNumArgs: %i", type);

   return 0;
}

bool MySQLIsTypeEnabled(int type)
{
   if (type >= STAT_NONE && type <= STAT_MAXSTAT)
      return Statistics_Table[type].enabled;

   bprintf("Unknown type received in MySQLIsTypeEnabled: %i", type);

   return false;
}

bool MySQLSetTypeEnabled(int type, bool enabled)
{
   if (type >= STAT_NONE && type <= STAT_MAXSTAT)
   {
      Statistics_Table[type].enabled = enabled;

      return true;
   }

   bprintf("Unknown type received in MySQLSetTypeEnabled: %i", type);

   return false;
}

/*
 * FreeDataNodeMemory: Frees the memory associated with sql_data_node structure.
 *   Handles freeing strings and then frees the structure itself.
 *   total_fields: size of the array
 *   fields_entered: number of valid array elements (for partially processed data)
 *   data: the data array
 */
void FreeDataNodeMemory(int total_fields, int fields_entered, sql_data_node data[])
{
   for (int i = 0; i < fields_entered; ++i)
   {
      switch (data[i].type)
      {
      case TAG_STRING:
      case TAG_RESOURCE:
         FreeMemory(MALLOC_ID_SQL, data[i].value.str, strlen(data[i].value.str) + 1);
         data[i].value.str = NULL;
         break;
      }
   }
   FreeMemory(MALLOC_ID_SQL, data, sizeof(sql_data_node) * total_fields);
}

/*
 * MySQLRecordGeneric: Enqueues the data for a SQL procedure call.
 *   Frees the memory associated with data if any checks fail.
 *      type: the SQL statistic to enqueue
 *      num_fields: the length of the data array passed in
 *      data: an array of data to log to SQL
 */
BOOL MySQLRecordGeneric(int type, int num_fields, sql_data_node data[])
{

   // Check number of args - not done in C_RecordStat because blakserv
   // can call MySQLRecordGeneric directly.
   if (MySQLTypeNumArgs(type) != num_fields)
   {
      bprintf("Invalid number of items received in MySQLRecordGeneric for statistic %i, found %i expected %i.",
         type, num_fields, MySQLTypeNumArgs(type));

      FreeDataNodeMemory(num_fields, num_fields, data);
      return FALSE;
   }

   if (num_fields > MAX_SQL_PARAMS)
   {
      bprintf("Too many params received in MySQLRecordGeneric for statistic %i, found %i max %i.",
         type, num_fields, MAX_SQL_PARAMS);

      FreeDataNodeMemory(num_fields, num_fields, data);
      return FALSE;
   }

   sql_queue_node* node = (sql_queue_node*)AllocateMemory(MALLOC_ID_SQL, sizeof(sql_queue_node));
   if (!node)
   {
      bprintf("Could not allocate memory for sql queue node in MySQLRecordGeneric.");
      FreeDataNodeMemory(num_fields, num_fields, data);

      return FALSE;
   }
   node->type = (sql_recordtype)type;
   node->num_fields = num_fields;
   node->data = data;

   // try to enqueue
   BOOL enqueued = _MySQLEnqueue(node);

   // cleanup in case of fail
   if (!enqueued)
   {
      FreeDataNodeMemory(num_fields, num_fields, data);
      node->data = NULL;
      FreeMemory(MALLOC_ID_SQL, node, sizeof(sql_queue_node));
   }

   return enqueued;
}

/*
 * MySQLEmptyTable: Enqueues the data for a SQL truncate table call.
 *      type: the SQL statistic to lookup the table
                truncate proc to enqueue
 */
BOOL MySQLEmptyTable(int type)
{
   sql_queue_node* node = (sql_queue_node*)AllocateMemory(MALLOC_ID_SQL, sizeof(sql_queue_node));
   if (!node)
   {
      bprintf("Could not allocate memory for sql queue node in MySQLEmptyTable.");

      return FALSE;
   }

   node->type = (sql_recordtype)type;
   // Set num_fields to negative value to mark it as a table truncate.
   node->num_fields = -1;
   node->data = NULL;

   // try to enqueue
   BOOL enqueued = _MySQLEnqueue(node);

   // cleanup in case of fail
   if (!enqueued)
      FreeMemory(MALLOC_ID_SQL, node, sizeof(sql_queue_node));

   return enqueued;
}
#pragma endregion

#pragma region Internal
void __cdecl _MySQLWorker(void* parameters)
{
	bool reconnect		= 1;
	int		processedcount	= 0;

	/******************************************
	/*            Initialization
	/******************************************/
		
	// init the queue
	queue.mutex = CreateMutex(NULL, FALSE, NULL);
	queue.count = 0;
	queue.first = 0;
	queue.last = 0;

	// init mysql
	mysql = mysql_init(NULL);
	
	if (!mysql)
		return;
	
	// enable auto-reconnect
	mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);

	// check if MySQLEnd was somehow already called and switched state to STOPPING
	// we still expect STARTING here and if so go to INITIALIZED
	if (state == STARTING)
		state = INITIALIZED;

	/******************************************
	/*              Work loop
	/******************************************/
	while(state != STOPPING)
	{	
		/**** Something went wrong with mysql ****/
		if (!mysql)		
			state = STOPPING;
				
		/****** No initial connection yet *****/
      else if (state == INITIALIZED)
      {
         // try connect
         if (mysql == mysql_real_connect(mysql, host, user, password, db, port, NULL, 0))
         {
            state = CONNECTED;
         }

         // not successful, sleep
         else
         {
            lprintf("Failed to connect to mysql: %s", mysql_error(mysql));
            Sleep(SLEEPTIMENOCON);
         }
		}

		/****** At least was connected once *****/
		else if (state >= CONNECTED)
		{
			// verify connection is still up
			// this will auto-reconnect
			if (mysql_ping(mysql) == 0)
			{
				// schema not yet verified/created
				if (state == CONNECTED)
					_MySQLVerifySchema();

				// fully running
				else if (state >= SCHEMAVERIFIED)
				{
					// reset counter for processed items
					processedcount = 0;

					// process up to limit pending items
					while(_MySQLDequeue(TRUE))
					{
						processedcount++;

						// reached limit, do more next loop
						// to get updated state etc.
						if (processedcount >= MAX_ITEMS_UNTIL_LOOP)
							break;						
					}
					
					// defaultsleep if none processed
					if (processedcount == 0)
						Sleep(SLEEPTIME);					
				}
			}
			else
				Sleep(SLEEPTIMENOCON);	
		}
	}

	/******************************************
	/*              Shutdown
	/******************************************/

	// close mysql
	if (mysql)
		mysql_close(mysql);

	// free local connection strings
	if (host)
		free(host);
	
	if (user)
		free(user);
	
	if (password)
		free(password);
	
	if (db)
		free(db);

	// clear queue
	while(_MySQLDequeue(FALSE));

	// delete mutex
	if (queue.mutex)
		CloseHandle(queue.mutex);

	// mark stopped
	state = STOPPED;

	// finalize thread
	_endthread();
};

void _MySQLVerifySchema()
{
   if (!mysql || !state)
      return;

   // Used to check errors.
   int status;

   // create table (won't do it if exist)
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_MONEYTOTAL, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_MONEYCREATED, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_PLAYERLOGINS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_PLAYERDAMAGED, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_PLAYERDEATH, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_LOGPEN, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_LOGPEN_ITEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_PLAYER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_GUILD, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_SPELLS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_SKILLS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_SPELL_REAGENT, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_TREASURE_GEN, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_TREASURE_EXTRA, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_TREASURE_MAGIC, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_MONSTER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_MONSTER_ZONE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NPCS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_WEAPONS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_ROOMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NPC_ZONE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NPC_SELLITEM, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NPC_SELLSKILL, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NPC_SELLSPELL, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NPC_SELLCOND, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_REAGENTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_FOOD, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_AMMO, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_ARMOR, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_MISCITEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_RINGS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_RODS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_POTIONS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_SCROLLS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_WANDS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_QUESTITEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_NECKLACE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_INSTRUMENTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_GEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_OFFERINGS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_QUESTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_QUESTGIVERS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_ACCOUNTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_ACCOUNT_CHARS, status);

   char buffer[128];
   // Drop existing procedures if present.
   for (int i = 1; i <= STAT_MAXSTAT; ++i)
   {
      MySQLGenerateDrop(Statistics_Table[i].stat_type, buffer);
      MYSQL_QUERY_CHECKED(mysql, buffer, status);
   }

   // Recreate procedures.
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_MONEYTOTAL, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_MONEYCREATED, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYERLOGIN, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYERASSESSDAMAGE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYERDEATH, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_LOGPEN, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_LOGPEN_ITEM, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYERSUICIDE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_GUILD, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_GUILDDISBAND, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_SPELLS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_SKILLS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_SPELL_REAGENT, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_TREASURE_GEN, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_TREASURE_EXTRA, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_TREASURE_MAGIC, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_MONSTER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_MONSTER_ZONE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NPCS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_WEAPONS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_ROOMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NPC_ZONE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NPC_SELLITEM, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NPC_SELLSKILL, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NPC_SELLSPELL, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NPC_SELLCOND, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_REAGENTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_FOOD, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_AMMO, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_ARMOR, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_MISCITEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_RINGS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_RODS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_POTIONS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_SCROLLS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_WANDS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_QUESTITEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_NECKLACE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_INSTRUMENTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_GEMS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_OFFERINGS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_QUESTS, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_QUESTGIVER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_ACCOUNT, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_ACCOUNT_CHAR, status);

   // set state to schema verified
   state = SCHEMAVERIFIED;
};

BOOL _MySQLEnqueue(sql_queue_node* Node)
{	
	BOOL enqueued = FALSE;

	if (!Node)
		return enqueued;

	// make sure it has no successor set
	Node->next = 0;

	// try to lock for multithreaded access
	if (WaitForSingleObject(queue.mutex, RECORD_ENQUEUE_TIMEOUT) == WAIT_OBJECT_0)
	{
		// got space in queue left
		if (queue.count < MAX_RECORD_QUEUE)
		{
			// empty queue
			if (queue.count == 0 ||
				queue.first == NULL ||
				queue.last == NULL)
			{
				queue.first = Node;
				queue.last = Node;
				queue.count = 1;
			}

			// not empty
			else
			{
				queue.last->next = Node;
				queue.last = Node;
				queue.count++;
			}

			enqueued = TRUE;
		}
		
		// release lock
		ReleaseMutex(queue.mutex);		
	}

	return enqueued;
};

BOOL _MySQLDequeue(BOOL processNode)
{
	BOOL			dequeued	= FALSE;
	sql_queue_node*	node		= 0;
			
	// try to lock for multithreaded access
	if (WaitForSingleObject(queue.mutex, RECORD_ENQUEUE_TIMEOUT) == WAIT_OBJECT_0)
	{
		// dequeue from the beginning (FIFO)
		if (queue.first)
		{
			// get first element to process
			node = queue.first;
			
			// update queue, set first item to the next one
			queue.first = node->next;
			queue.count--;

			// save that we dequeued a node
			dequeued = TRUE;			
		}
		
		// release lock
		ReleaseMutex(queue.mutex);		
	}
	
	// if we dequeued one, process it
	if (node)
	{
      // process node
      if (node->num_fields < 0)
      {
         _MySQLTruncate(node, processNode);
      }
      else
      {
         _MySQLWriteNode(node, processNode);

         // free memory of processed record and node
         FreeDataNodeMemory(node->num_fields, node->num_fields, (sql_data_node *)node->data);
      }
      FreeMemory(MALLOC_ID_SQL, node, sizeof(sql_queue_node));
	}

	return dequeued;
}

/*
 * _MySQLTruncate: Obtain the correct truncate string for given node type
 *   and pass it on to _MySQLCallProc.
 */
void _MySQLTruncate(sql_queue_node* Node, BOOL ProcessNode)
{
   if (!Node)
      return;

   // really write it, or just free mem at end?
   if (ProcessNode)
   {
      // call stored procedure
      char buffer[128];
      MySQLGenerateTruncate(Node->type, buffer);
      _MySQLCallProc(buffer, NULL, TRUE);
   }
}

/*
 * _MySQLCallProc: Call a MySQL procedure on the database. Parameters can be NULL
 *   if calling a TRUNCATE procedure instead of a WRITE, but nullParams must be
 *   passed as TRUE to allow error-checking NULL Paremeters on WRITE statements.
 */
void _MySQLCallProc(char* ProcName, MYSQL_BIND Parameters[], BOOL nullParams = FALSE)
{
   MYSQL_STMT*	stmt;
   int status;

   if (!ProcName || (!Parameters && !nullParams))
      return;

   // init statement
   stmt = mysql_stmt_init(mysql);

   if (!stmt)
      return;

   // execute stored procedure
   status = mysql_stmt_prepare(stmt, ProcName, strlen(ProcName));
   if (status != 0 && mysql)
   {
      bprintf("MySQL error in mysql_stmt_prepare with procedure %s, status code %i: %s",
         ProcName, status, mysql_error(mysql));
      mysql_stmt_close(stmt);
      return;
   }

   if (!nullParams)
   {
      status = mysql_stmt_bind_param(stmt, Parameters);
      if (status != 0 && mysql)
      {
         bprintf("MySQL error in mysql_stmt_bind_param with procedure %s, status code %i: %s",
            ProcName, status, mysql_error(mysql));
         mysql_stmt_close(stmt);
         return;
      }
   }

   status = mysql_stmt_execute(stmt);
   if (status != 0 && mysql)
   {
      bprintf("MySQL error in mysql_stmt_execute with procedure %s, status code %i: %s",
         ProcName, status, mysql_error(mysql));
   }

   ++record_count;

   // close statement
   mysql_stmt_close(stmt);
};

void _MySQLWriteNode(sql_queue_node* Node, BOOL ProcessNode)
{
   if (!Node || !Node->data)
      return;

   MYSQL_BIND params[MAX_SQL_PARAMS];
   unsigned long str_lens[MAX_SQL_PARAMS];
   my_bool is_null = 1;

   // really write it, or just free mem at end?
   if (ProcessNode)
   {
      sql_data_node *data = (sql_data_node *)Node->data;
      // allocate parameters
      memset(params, 0, sizeof(params));
      for (int i = 0; i < Node->num_fields; ++i)
      {
         switch (data[i].type)
         {
         case TAG_NIL:
            params[i].buffer_type = MYSQL_TYPE_NULL;
            params[i].length = 0;
            params[i].is_null = &is_null;
            break;
         case TAG_INT:
            params[i].buffer_type = MYSQL_TYPE_LONG;
            params[i].buffer = &data[i].value.num;
            params[i].length = 0;
            params[i].is_null = 0;
            break;
         case TAG_STRING:
         case TAG_RESOURCE:
            str_lens[i] = strlen(data[i].value.str);
            params[i].buffer_type = MYSQL_TYPE_STRING;
            params[i].buffer = data[i].value.str;
            params[i].length = &str_lens[i];
            params[i].is_null = 0;
            break;
         }
      }

      // call stored procedure
      char buffer[128];
      MySQLGenerateCall(Node->type, buffer);
      _MySQLCallProc(buffer, params);
   }
};
#pragma endregion
