// Meridian 59 - PostgreSQL database implementation for Linux
// Replaces MySQL database.c with libpq-based PostgreSQL support.

#include "blakserv.h"
#include <pthread.h>
#include <libpq-fe.h>

// Worker thread state
enum { DB_STOPPED=0, DB_STOPPING, DB_STARTING, DB_CONNECTED, DB_READY };
static int db_state = DB_STOPPED;

// Connection
static PGconn *pg_conn = NULL;
static char *db_host = NULL;
static int db_port = 0;
static char *db_user = NULL;
static char *db_pass = NULL;
static char *db_name = NULL;

// Queue
static sql_queue queue = { PTHREAD_MUTEX_INITIALIZER, 0, NULL, NULL };
static UINT64 record_count = 0;

// Worker thread
static pthread_t db_thread;

#define MAX_RECORD_QUEUE 15000
#define MAX_SQL_PARAMS 45
#define DB_WORKER_SLEEP_MS 10
#define DB_RECONNECT_SLEEP_MS 5000
#define MAX_ITEMS_PER_LOOP 10

// Statistics table - maps STAT_TYPE to table name and expected field count
static sql_statistic_type Statistics_Table[] = {
   {STAT_NONE,             0, false, NULL},
   {STAT_TOTALMONEY,       1, true,  "money_total"},
   {STAT_MONEYCREATED,     1, true,  "money_created"},
   {STAT_PLAYERLOGIN,      3, true,  "player_logins"},
   {STAT_ASSESS_DAM,       7, true,  "player_damaged"},
   {STAT_PLAYERDEATH,      5, true,  "player_death"},
   {STAT_PLAYER,          13, true,  "player"},
   {STAT_PLAYERSUICIDE,    2, true,  "player"},
   {STAT_GUILD,            4, true,  "guild"},
   {STAT_GUILDDISBAND,     1, true,  "guild"},
   {STAT_SPELLS,          14, true,  "wiki_spells"},
   {STAT_SPELL_REAGENT,    3, true,  "wiki_spell_reagent"},
   {STAT_TREASURE_GEN,     3, true,  "wiki_treasure_gen"},
   {STAT_MONSTER,         13, true,  "wiki_monster"},
   {STAT_MONSTER_ZONE,     3, true,  "wiki_monster_zone"},
   {STAT_NPCS,             7, true,  "wiki_npcs"},
   {STAT_WEAPONS,         14, true,  "wiki_weapons"},
   {STAT_ROOMS,            6, true,  "wiki_rooms"},
   {STAT_NPC_ZONE,         4, true,  "wiki_npc_zone"},
   {STAT_NPC_SELLITEM,     3, true,  "wiki_npc_sellitem"},
   {STAT_NPC_SELLSKILL,    2, true,  "wiki_npc_sellskill"},
   {STAT_NPC_SELLSPELL,    2, true,  "wiki_npc_sellspell"},
   {STAT_REAGENTS,        11, true,  "wiki_reagents"},
   {STAT_FOOD,            11, true,  "wiki_food"},
   {STAT_AMMO,            11, true,  "wiki_ammo"},
   {STAT_ARMOR,           11, true,  "wiki_armor"},
   {STAT_MISCITEMS,       11, true,  "wiki_miscitems"},
   {STAT_RINGS,           11, true,  "wiki_rings"},
   {STAT_RODS,            11, true,  "wiki_rods"},
   {STAT_POTIONS,         11, true,  "wiki_potions"},
   {STAT_SCROLLS,         11, true,  "wiki_scrolls"},
   {STAT_WANDS,           11, true,  "wiki_wands"},
   {STAT_QUESTITEMS,      11, true,  "wiki_questitems"},
   {STAT_SKILLS,          12, true,  "wiki_skills"},
   {STAT_NECKLACE,        11, true,  "wiki_necklace"},
   {STAT_INSTRUMENTS,     11, true,  "wiki_instruments"},
   {STAT_GEMS,            11, true,  "wiki_gems"},
   {STAT_OFFERINGS,       11, true,  "wiki_offerings"},
   {STAT_QUESTS,          11, true,  "wiki_quests"},
   {STAT_TREASURE_EXTRA,   4, true,  "wiki_treasure_extra"},
   {STAT_TREASURE_MAGIC,   3, true,  "wiki_treasure_magic"},
   {STAT_NPC_SELLCOND,     4, true,  "wiki_npc_sellcond"},
   {STAT_LOGPEN,           7, true,  "player_logpen"},
   {STAT_LOGPEN_ITEM,      3, true,  "player_logpen_items"},
   {STAT_QUESTGIVER,       2, true,  "wiki_quest_giver"},
   {STAT_ACCOUNTS,         8, true,  "server_accounts"},
   {STAT_ACCOUNT_CHARS,    2, true,  "server_account_chars"},
   {STAT_COMPL_QUESTS,     5, true,  "player_completed_quests"},
   {STAT_COMPL_QUEST_ITEMS,4, true,  "player_quest_reward_items"}
};

/******************************************************************************/
// Schema creation - all tables
/******************************************************************************/

static const char *schema_sql =
"CREATE TABLE IF NOT EXISTS money_total ("
"  money_total_time TIMESTAMP NOT NULL DEFAULT NOW(),"
"  money_total_amount VARCHAR(18) NOT NULL);"

"CREATE TABLE IF NOT EXISTS money_created ("
"  money_created_time TIMESTAMP NOT NULL DEFAULT NOW(),"
"  money_created_amount INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS player_logins ("
"  plogin_account_name VARCHAR(45) NOT NULL,"
"  plogin_character_name VARCHAR(45) NOT NULL,"
"  plogin_IP VARCHAR(45) NOT NULL,"
"  plogin_time TIMESTAMP NOT NULL DEFAULT NOW());"

"CREATE TABLE IF NOT EXISTS player_damaged ("
"  idpdamaged SERIAL PRIMARY KEY,"
"  pdamaged_who VARCHAR(45) NOT NULL,"
"  pdamaged_attacker VARCHAR(45) NOT NULL,"
"  pdamaged_aspell INT NOT NULL,"
"  pdamaged_atype INT NOT NULL,"
"  pdamaged_applied INT NOT NULL,"
"  pdamaged_original INT NOT NULL,"
"  pdamaged_weapon VARCHAR(45) NOT NULL,"
"  pdamaged_time TIMESTAMP NOT NULL DEFAULT NOW());"

"CREATE TABLE IF NOT EXISTS player_death ("
"  pdeath_victim VARCHAR(45) NOT NULL,"
"  pdeath_killer VARCHAR(45) NOT NULL,"
"  pdeath_room VARCHAR(63) NOT NULL,"
"  pdeath_attack VARCHAR(45) NOT NULL,"
"  pdeath_ispvp INT NOT NULL,"
"  pdeath_time TIMESTAMP NOT NULL DEFAULT NOW());"

"CREATE TABLE IF NOT EXISTS player ("
"  player_account_id INT NOT NULL,"
"  player_name VARCHAR(45) NOT NULL PRIMARY KEY,"
"  player_home VARCHAR(128),"
"  player_bind VARCHAR(128),"
"  player_guild VARCHAR(45),"
"  player_max_health INT,"
"  player_max_mana INT,"
"  player_might INT,"
"  player_int INT,"
"  player_myst INT,"
"  player_stam INT,"
"  player_agil INT,"
"  player_aim INT,"
"  player_suicide INT DEFAULT 0,"
"  player_suicide_time TIMESTAMP);"

"CREATE TABLE IF NOT EXISTS guild ("
"  guild_name VARCHAR(100) NOT NULL PRIMARY KEY,"
"  guild_leader VARCHAR(45) NOT NULL,"
"  guild_hall VARCHAR(100),"
"  guild_rent_paid INT NOT NULL,"
"  guild_disbanded INT NOT NULL DEFAULT 0,"
"  guild_disbanded_time TIMESTAMP);"

"CREATE TABLE IF NOT EXISTS wiki_spells ("
"  spell_id INT NOT NULL PRIMARY KEY,"
"  spell_name VARCHAR(63) NOT NULL,"
"  spell_name_ger VARCHAR(63) NOT NULL,"
"  spell_icon VARCHAR(63) NOT NULL,"
"  spell_desc TEXT,"
"  spell_desc_ger TEXT,"
"  spell_school INT,"
"  spell_level INT,"
"  spell_mana INT,"
"  spell_chance INT,"
"  spell_mediate_ratio INT,"
"  spell_exertion INT,"
"  spell_casttime INT,"
"  spell_iflag INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS wiki_spell_reagent ("
"  spell_id INT NOT NULL,"
"  spell_reagent VARCHAR(63) NOT NULL,"
"  spell_reagent_amount INT,"
"  PRIMARY KEY (spell_id, spell_reagent));"

"CREATE TABLE IF NOT EXISTS wiki_treasure_gen ("
"  treasure_id INT NOT NULL,"
"  item_name VARCHAR(63) NOT NULL,"
"  item_chance INT,"
"  PRIMARY KEY (treasure_id, item_name));"

"CREATE TABLE IF NOT EXISTS wiki_monster ("
"  monster_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  monster_name_ger VARCHAR(63) NOT NULL,"
"  monster_icon VARCHAR(63) NOT NULL,"
"  monster_desc TEXT,"
"  monster_desc_ger TEXT,"
"  monster_level INT,"
"  monster_karma INT,"
"  monster_treasure INT,"
"  monster_speed INT,"
"  monster_behavior INT,"
"  monster_difficulty INT,"
"  monster_visiondistance INT,"
"  monster_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_monster_zone ("
"  monster_rid INT NOT NULL,"
"  monster_name VARCHAR(63) NOT NULL,"
"  monster_spawnchance INT,"
"  PRIMARY KEY (monster_rid, monster_name));"

"CREATE TABLE IF NOT EXISTS wiki_npcs ("
"  npc_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  npc_name_ger VARCHAR(63) NOT NULL,"
"  npc_icon VARCHAR(63) NOT NULL,"
"  npc_desc TEXT,"
"  npc_desc_ger TEXT,"
"  npc_merchantmarkup INT,"
"  npc_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_weapons ("
"  weapon_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  weapon_name_ger VARCHAR(63) NOT NULL,"
"  weapon_icon VARCHAR(63) NOT NULL,"
"  weapon_group INT NOT NULL,"
"  weapon_color INT NOT NULL,"
"  weapon_desc TEXT,"
"  weapon_desc_ger TEXT,"
"  weapon_value INT,"
"  weapon_weight INT,"
"  weapon_bulk INT,"
"  weapon_range INT,"
"  weapon_skill INT,"
"  weapon_prof INT,"
"  weapon_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_rooms ("
"  room_name VARCHAR(63) NOT NULL,"
"  room_name_ger VARCHAR(63) NOT NULL,"
"  room_roo VARCHAR(63) NOT NULL,"
"  room_number INT NOT NULL PRIMARY KEY,"
"  room_region INT NOT NULL,"
"  room_iflag INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS wiki_npc_zone ("
"  npc_name VARCHAR(63) NOT NULL,"
"  npc_roomid INT NOT NULL,"
"  npc_row INT NOT NULL,"
"  npc_col INT NOT NULL,"
"  PRIMARY KEY (npc_name, npc_roomid, npc_row, npc_col));"

"CREATE TABLE IF NOT EXISTS wiki_npc_sellitem ("
"  npc_name VARCHAR(63) NOT NULL,"
"  npc_item_sold VARCHAR(63) NOT NULL,"
"  item_color INT NOT NULL,"
"  PRIMARY KEY (npc_name, npc_item_sold, item_color));"

"CREATE TABLE IF NOT EXISTS wiki_npc_sellskill ("
"  npc_name VARCHAR(63) NOT NULL,"
"  npc_skill_id INT NOT NULL,"
"  PRIMARY KEY (npc_name, npc_skill_id));"

"CREATE TABLE IF NOT EXISTS wiki_npc_sellspell ("
"  npc_name VARCHAR(63) NOT NULL,"
"  npc_spell_id INT NOT NULL,"
"  PRIMARY KEY (npc_name, npc_spell_id));"

"CREATE TABLE IF NOT EXISTS wiki_npc_sellcond ("
"  npc_name VARCHAR(63) NOT NULL,"
"  npc_item_sold VARCHAR(63) NOT NULL,"
"  item_color INT NOT NULL,"
"  item_price INT NOT NULL,"
"  PRIMARY KEY (npc_name, npc_item_sold, item_color));"

"CREATE TABLE IF NOT EXISTS wiki_reagents ("
"  reagent_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  reagent_name_ger VARCHAR(63) NOT NULL,"
"  reagent_icon VARCHAR(63) NOT NULL,"
"  reagent_group INT NOT NULL,"
"  reagent_color INT NOT NULL,"
"  reagent_desc TEXT,"
"  reagent_desc_ger TEXT,"
"  reagent_value INT,"
"  reagent_weight INT,"
"  reagent_bulk INT,"
"  reagent_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_food ("
"  food_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  food_name_ger VARCHAR(63) NOT NULL,"
"  food_icon VARCHAR(63) NOT NULL,"
"  food_group INT NOT NULL,"
"  food_color INT NOT NULL,"
"  food_desc TEXT,"
"  food_desc_ger TEXT,"
"  food_value INT,"
"  food_weight INT,"
"  food_bulk INT,"
"  food_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_ammo ("
"  ammo_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  ammo_name_ger VARCHAR(63) NOT NULL,"
"  ammo_icon VARCHAR(63) NOT NULL,"
"  ammo_group INT NOT NULL,"
"  ammo_color INT NOT NULL,"
"  ammo_desc TEXT,"
"  ammo_desc_ger TEXT,"
"  ammo_value INT,"
"  ammo_weight INT,"
"  ammo_bulk INT,"
"  ammo_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_armor ("
"  armor_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  armor_name_ger VARCHAR(63) NOT NULL,"
"  armor_icon VARCHAR(63) NOT NULL,"
"  armor_group INT NOT NULL,"
"  armor_color INT NOT NULL,"
"  armor_desc TEXT,"
"  armor_desc_ger TEXT,"
"  armor_value INT,"
"  armor_weight INT,"
"  armor_bulk INT,"
"  armor_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_miscitems ("
"  misc_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  misc_name_ger VARCHAR(63) NOT NULL,"
"  misc_icon VARCHAR(63) NOT NULL,"
"  misc_group INT NOT NULL,"
"  misc_color INT NOT NULL,"
"  misc_desc TEXT,"
"  misc_desc_ger TEXT,"
"  misc_value INT,"
"  misc_weight INT,"
"  misc_bulk INT,"
"  misc_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_rings ("
"  rings_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  rings_name_ger VARCHAR(63) NOT NULL,"
"  rings_icon VARCHAR(63) NOT NULL,"
"  rings_group INT NOT NULL,"
"  rings_color INT NOT NULL,"
"  rings_desc TEXT,"
"  rings_desc_ger TEXT,"
"  rings_value INT,"
"  rings_weight INT,"
"  rings_bulk INT,"
"  rings_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_rods ("
"  rods_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  rods_name_ger VARCHAR(63) NOT NULL,"
"  rods_icon VARCHAR(63) NOT NULL,"
"  rods_group INT NOT NULL,"
"  rods_color INT NOT NULL,"
"  rods_desc TEXT,"
"  rods_desc_ger TEXT,"
"  rods_value INT,"
"  rods_weight INT,"
"  rods_bulk INT,"
"  rods_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_potions ("
"  potion_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  potion_name_ger VARCHAR(63) NOT NULL,"
"  potion_icon VARCHAR(63) NOT NULL,"
"  potion_group INT NOT NULL,"
"  potion_color INT NOT NULL,"
"  potion_desc TEXT,"
"  potion_desc_ger TEXT,"
"  potion_value INT,"
"  potion_weight INT,"
"  potion_bulk INT,"
"  potion_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_scrolls ("
"  scrolls_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  scrolls_name_ger VARCHAR(63) NOT NULL,"
"  scrolls_icon VARCHAR(63) NOT NULL,"
"  scrolls_group INT NOT NULL,"
"  scrolls_color INT NOT NULL,"
"  scrolls_desc TEXT,"
"  scrolls_desc_ger TEXT,"
"  scrolls_value INT,"
"  scrolls_weight INT,"
"  scrolls_bulk INT,"
"  scrolls_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_wands ("
"  wands_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  wands_name_ger VARCHAR(63) NOT NULL,"
"  wands_icon VARCHAR(63) NOT NULL,"
"  wands_group INT NOT NULL,"
"  wands_color INT NOT NULL,"
"  wands_desc TEXT,"
"  wands_desc_ger TEXT,"
"  wands_value INT,"
"  wands_weight INT,"
"  wands_bulk INT,"
"  wands_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_questitems ("
"  questitem_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  questitem_name_ger VARCHAR(63) NOT NULL,"
"  questitem_icon VARCHAR(63) NOT NULL,"
"  questitem_group INT NOT NULL,"
"  questitem_color INT NOT NULL,"
"  questitem_desc TEXT,"
"  questitem_desc_ger TEXT,"
"  questitem_value INT,"
"  questitem_weight INT,"
"  questitem_bulk INT,"
"  questitem_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_skills ("
"  skill_id INT NOT NULL PRIMARY KEY,"
"  skill_name VARCHAR(63) NOT NULL,"
"  skill_name_ger VARCHAR(63) NOT NULL,"
"  skill_icon VARCHAR(63) NOT NULL,"
"  skill_desc TEXT,"
"  skill_desc_ger TEXT,"
"  skill_school INT,"
"  skill_level INT,"
"  skill_chance INT,"
"  skill_mediate_ratio INT,"
"  skill_exertion INT,"
"  skill_iflag INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS wiki_necklace ("
"  necklace_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  necklace_name_ger VARCHAR(63) NOT NULL,"
"  necklace_icon VARCHAR(63) NOT NULL,"
"  necklace_group INT NOT NULL,"
"  necklace_color INT NOT NULL,"
"  necklace_desc TEXT,"
"  necklace_desc_ger TEXT,"
"  necklace_value INT,"
"  necklace_weight INT,"
"  necklace_bulk INT,"
"  necklace_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_instruments ("
"  instrument_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  instrument_name_ger VARCHAR(63) NOT NULL,"
"  instrument_icon VARCHAR(63) NOT NULL,"
"  instrument_group INT NOT NULL,"
"  instrument_color INT NOT NULL,"
"  instrument_desc TEXT,"
"  instrument_desc_ger TEXT,"
"  instrument_value INT,"
"  instrument_weight INT,"
"  instrument_bulk INT,"
"  instrument_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_gems ("
"  gem_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  gem_name_ger VARCHAR(63) NOT NULL,"
"  gem_icon VARCHAR(63) NOT NULL,"
"  gem_group INT NOT NULL,"
"  gem_color INT NOT NULL,"
"  gem_desc TEXT,"
"  gem_desc_ger TEXT,"
"  gem_value INT,"
"  gem_weight INT,"
"  gem_bulk INT,"
"  gem_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_offerings ("
"  offering_name VARCHAR(63) NOT NULL PRIMARY KEY,"
"  offering_name_ger VARCHAR(63) NOT NULL,"
"  offering_icon VARCHAR(63) NOT NULL,"
"  offering_group INT NOT NULL,"
"  offering_color INT NOT NULL,"
"  offering_desc TEXT,"
"  offering_desc_ger TEXT,"
"  offering_value INT,"
"  offering_weight INT,"
"  offering_bulk INT,"
"  offering_iflag INT);"

"CREATE TABLE IF NOT EXISTS wiki_quests ("
"  quest_id INT NOT NULL PRIMARY KEY,"
"  quest_name VARCHAR(63) NOT NULL,"
"  quest_name_ger VARCHAR(63) NOT NULL,"
"  quest_icon VARCHAR(63) NOT NULL,"
"  quest_icon_group INT NOT NULL,"
"  quest_desc TEXT,"
"  quest_desc_ger TEXT,"
"  quest_recent_time INT,"
"  quest_schedule_pct INT,"
"  quest_est_time INT,"
"  quest_est_diff INT);"

"CREATE TABLE IF NOT EXISTS wiki_treasure_extra ("
"  treasure_id INT NOT NULL,"
"  item_name VARCHAR(63) NOT NULL,"
"  item_min_amount INT,"
"  item_max_amount INT,"
"  PRIMARY KEY (treasure_id, item_name));"

"CREATE TABLE IF NOT EXISTS wiki_treasure_magic ("
"  treasure_id INT NOT NULL,"
"  item_name VARCHAR(63) NOT NULL,"
"  item_attribute_id INT NOT NULL,"
"  PRIMARY KEY (treasure_id, item_name, item_attribute_id));"

"CREATE TABLE IF NOT EXISTS player_logpen ("
"  player_name VARCHAR(63) NOT NULL,"
"  logpen_time TIMESTAMP NOT NULL DEFAULT NOW(),"
"  room_id INT,"
"  logpen_xp INT NOT NULL,"
"  logpen_hp INT NOT NULL,"
"  logpen_spellpct INT NOT NULL,"
"  logpen_skillpct INT NOT NULL,"
"  logpen_numitems INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS player_logpen_items ("
"  logpen_item_id SERIAL PRIMARY KEY,"
"  player_name VARCHAR(63) NOT NULL,"
"  logpen_time TIMESTAMP NOT NULL DEFAULT NOW(),"
"  item_name VARCHAR(63) NOT NULL);"

"CREATE TABLE IF NOT EXISTS player_completed_quests ("
"  quest_id INT NOT NULL,"
"  player_name VARCHAR(63) NOT NULL,"
"  completion_time TIMESTAMP NOT NULL DEFAULT NOW(),"
"  num_seconds_taken INT NOT NULL,"
"  xp_rewarded INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS player_quest_reward_items ("
"  quest_rewarditem_id SERIAL PRIMARY KEY,"
"  quest_id INT NOT NULL,"
"  player_name VARCHAR(63) NOT NULL,"
"  completion_time TIMESTAMP NOT NULL DEFAULT NOW(),"
"  item_name VARCHAR(63) NOT NULL);"

"CREATE TABLE IF NOT EXISTS wiki_quest_giver ("
"  quest_id INT NOT NULL,"
"  quest_npc_name VARCHAR(63) NOT NULL,"
"  PRIMARY KEY (quest_id, quest_npc_name));"

"CREATE TABLE IF NOT EXISTS server_accounts ("
"  acct_id INT NOT NULL PRIMARY KEY,"
"  acct_name VARCHAR(63) NOT NULL,"
"  acct_password VARCHAR(63) NOT NULL,"
"  acct_email VARCHAR(255) NOT NULL,"
"  acct_type INT NOT NULL,"
"  acct_loggedin_time INT NOT NULL,"
"  acct_last_login INT NOT NULL,"
"  acct_suspend_time INT NOT NULL);"

"CREATE TABLE IF NOT EXISTS server_account_chars ("
"  entry_id SERIAL PRIMARY KEY,"
"  acct_id INT NOT NULL,"
"  char_name VARCHAR(63) NOT NULL);"
;

/******************************************************************************/
// Internal functions
/******************************************************************************/

static bool PgConnect(void)
{
   char conninfo[512];
   snprintf(conninfo, sizeof(conninfo),
      "host=%s port=%d user=%s password=%s dbname=%s",
      db_host, db_port, db_user, db_pass, db_name);

   pg_conn = PQconnectdb(conninfo);
   if (PQstatus(pg_conn) != CONNECTION_OK)
   {
      eprintf("PostgreSQL connection failed: %s\n", PQerrorMessage(pg_conn));
      PQfinish(pg_conn);
      pg_conn = NULL;
      return false;
   }

   // Set client encoding to LATIN1 for German umlauts and legacy data
   PQsetClientEncoding(pg_conn, "LATIN1");

   lprintf("PostgreSQL connected to %s:%d/%s\n", db_host, db_port, db_name);
   return true;
}

static bool PgVerifySchema(void)
{
   PGresult *res = PQexec(pg_conn, schema_sql);
   if (PQresultStatus(res) != PGRES_COMMAND_OK)
   {
      eprintf("PostgreSQL schema creation failed: %s\n", PQerrorMessage(pg_conn));
      PQclear(res);
      return false;
   }
   PQclear(res);
   dprintf("PostgreSQL schema verified\n");
   return true;
}

static bool PgIsConnected(void)
{
   if (!pg_conn)
      return false;
   if (PQstatus(pg_conn) != CONNECTION_OK)
   {
      // Try to reset the connection
      PQreset(pg_conn);
      return PQstatus(pg_conn) == CONNECTION_OK;
   }
   return true;
}

static void PgWriteNode(sql_queue_node *node)
{
   sql_data_node *data = (sql_data_node *)node->data;
   int type = node->type;
   int nf = node->num_fields;

   if (type <= STAT_NONE || type > STAT_MAXSTAT)
      return;

   if (!Statistics_Table[type].enabled)
      return;

   const char *table = Statistics_Table[type].table_name;
   if (!table)
      return;

   // Build parameterized INSERT
   // Convert all fields to string params for PQexecParams
   const char *paramValues[MAX_SQL_PARAMS];
   char numbufs[MAX_SQL_PARAMS][20];
   int nParams = 0;

   for (int i = 0; i < nf && i < MAX_SQL_PARAMS; i++)
   {
      switch (data[i].type)
      {
      case TAG_NIL:
         paramValues[nParams] = NULL;
         break;
      case TAG_INT:
         snprintf(numbufs[nParams], sizeof(numbufs[0]), "%d", data[i].value.num);
         paramValues[nParams] = numbufs[nParams];
         break;
      case TAG_STRING:
      case TAG_RESOURCE:
         paramValues[nParams] = data[i].value.str ? data[i].value.str : "";
         break;
      case TAG_SESSION:
         snprintf(numbufs[nParams], sizeof(numbufs[0]), "%d", data[i].value.num);
         paramValues[nParams] = numbufs[nParams];
         break;
      default:
         paramValues[nParams] = NULL;
         break;
      }
      nParams++;
   }

   // Build INSERT query with $1, $2, ... placeholders
   char sql[4096];
   char placeholders[1024];
   placeholders[0] = 0;
   for (int i = 0; i < nParams; i++)
   {
      char ph[8];
      snprintf(ph, sizeof(ph), "%s$%d", i > 0 ? "," : "", i + 1);
      strcat(placeholders, ph);
   }
   snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES (%s) ON CONFLICT DO NOTHING", table, placeholders);

   PGresult *res = PQexecParams(pg_conn, sql, nParams, NULL,
      paramValues, NULL, NULL, 0);

   if (PQresultStatus(res) != PGRES_COMMAND_OK)
   {
      // Don't spam errors - just log once
      static int error_count = 0;
      if (error_count++ < 10)
         eprintf("PostgreSQL INSERT into %s failed: %s\n", table, PQerrorMessage(pg_conn));
   }
   else
   {
      record_count++;
   }
   PQclear(res);
}

static void PgTruncateTable(int type)
{
   if (type <= STAT_NONE || type > STAT_MAXSTAT)
      return;

   const char *table = Statistics_Table[type].table_name;
   if (!table)
      return;

   char sql[256];
   snprintf(sql, sizeof(sql), "TRUNCATE TABLE %s", table);
   PGresult *res = PQexec(pg_conn, sql);
   if (PQresultStatus(res) != PGRES_COMMAND_OK)
      eprintf("PostgreSQL TRUNCATE %s failed: %s\n", table, PQerrorMessage(pg_conn));
   PQclear(res);
}

static bool PgDequeue(void)
{
   pthread_mutex_lock(&queue.mutex);
   if (queue.count == 0)
   {
      pthread_mutex_unlock(&queue.mutex);
      return false;
   }

   sql_queue_node *node = queue.first;
   queue.first = node->next;
   if (queue.first == NULL)
      queue.last = NULL;
   queue.count--;
   pthread_mutex_unlock(&queue.mutex);

   if (node->num_fields < 0)
      PgTruncateTable(node->type);
   else
      PgWriteNode(node);

   // Free data
   if (node->data)
      FreeDataNodeMemory(abs(node->num_fields),
         abs(node->num_fields), (sql_data_node *)node->data);
   FreeMemory(MALLOC_ID_SQL, node, sizeof(sql_queue_node));

   return true;
}

static void *PgWorker(void *arg)
{
   db_state = DB_STARTING;

   // Connect
   while (db_state != DB_STOPPING)
   {
      if (PgConnect())
      {
         if (PgVerifySchema())
         {
            db_state = DB_READY;
            break;
         }
         PQfinish(pg_conn);
         pg_conn = NULL;
      }
      // Retry after delay
      usleep(DB_RECONNECT_SLEEP_MS * 1000);
   }

   // Main work loop
   while (db_state != DB_STOPPING)
   {
      if (db_state == DB_READY && PgIsConnected())
      {
         int processed = 0;
         for (int i = 0; i < MAX_ITEMS_PER_LOOP; i++)
         {
            if (PgDequeue())
               processed++;
            else
               break;
         }
         if (processed == 0)
            usleep(DB_WORKER_SLEEP_MS * 1000);
      }
      else
      {
         // Try reconnect
         if (!PgIsConnected())
         {
            eprintf("PostgreSQL connection lost, reconnecting...\n");
            if (!PgConnect())
               usleep(DB_RECONNECT_SLEEP_MS * 1000);
            else
               db_state = DB_READY;
         }
      }
   }

   // Drain remaining queue
   while (PgDequeue()) {}

   if (pg_conn)
   {
      PQfinish(pg_conn);
      pg_conn = NULL;
   }
   db_state = DB_STOPPED;
   return NULL;
}

/******************************************************************************/
// Public API
/******************************************************************************/

void MySQLInit(char *Host, int Port, char *User, char *Password, char *DB)
{
   if (db_state != DB_STOPPED)
      return;

   if (!Host || !User || !Password || !DB)
   {
      eprintf("PostgreSQL init failed: missing connection parameters\n");
      return;
   }

   db_host = strdup(Host);
   db_port = Port;
   db_user = strdup(User);
   db_pass = strdup(Password);
   db_name = strdup(DB);

   pthread_create(&db_thread, NULL, PgWorker, NULL);
   usleep(100000); // 100ms for thread startup
}

void MySQLEnd()
{
   if (db_state == DB_STOPPED)
      return;

   db_state = DB_STOPPING;
   pthread_join(db_thread, NULL);

   free(db_host); db_host = NULL;
   free(db_user); db_user = NULL;
   free(db_pass); db_pass = NULL;
   free(db_name); db_name = NULL;
}

BOOL MySQLRecordGeneric(int type, int num_fields, sql_data_node data[])
{
   if (type <= STAT_NONE || type > STAT_MAXSTAT)
      return FALSE;

   if (!Statistics_Table[type].enabled)
   {
      FreeDataNodeMemory(num_fields, num_fields, data);
      return FALSE;
   }

   if (db_state < DB_READY)
   {
      FreeDataNodeMemory(num_fields, num_fields, data);
      return FALSE;
   }

   sql_queue_node *node = (sql_queue_node *)AllocateMemory(MALLOC_ID_SQL,
      sizeof(sql_queue_node));
   node->type = (sql_recordtype)type;
   node->num_fields = num_fields;
   node->data = data;
   node->next = NULL;

   pthread_mutex_lock(&queue.mutex);
   if (queue.count >= MAX_RECORD_QUEUE)
   {
      pthread_mutex_unlock(&queue.mutex);
      FreeDataNodeMemory(num_fields, num_fields, data);
      FreeMemory(MALLOC_ID_SQL, node, sizeof(sql_queue_node));
      eprintf("PostgreSQL queue full, dropping record\n");
      return FALSE;
   }

   if (queue.last)
      queue.last->next = node;
   else
      queue.first = node;
   queue.last = node;
   queue.count++;
   pthread_mutex_unlock(&queue.mutex);

   return TRUE;
}

BOOL MySQLEmptyTable(int type)
{
   sql_queue_node *node = (sql_queue_node *)AllocateMemory(MALLOC_ID_SQL,
      sizeof(sql_queue_node));
   node->type = (sql_recordtype)type;
   node->num_fields = -1;
   node->data = NULL;
   node->next = NULL;

   pthread_mutex_lock(&queue.mutex);
   if (queue.last)
      queue.last->next = node;
   else
      queue.first = node;
   queue.last = node;
   queue.count++;
   pthread_mutex_unlock(&queue.mutex);

   return TRUE;
}

UINT64 MySQLGetRecordCount()
{
   return record_count;
}

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

bool MySQLIsTypeEnabled(int type)
{
   if (type >= STAT_NONE && type <= STAT_MAXSTAT)
      return Statistics_Table[type].enabled;
   return false;
}

bool MySQLSetTypeEnabled(int type, bool enabled)
{
   if (type >= STAT_NONE && type <= STAT_MAXSTAT)
   {
      Statistics_Table[type].enabled = enabled;
      return true;
   }
   return false;
}

void FreeDataNodeMemory(int total_fields, int fields_entered, sql_data_node data[])
{
   for (int i = 0; i < fields_entered; i++)
   {
      if ((data[i].type == TAG_STRING || data[i].type == TAG_RESOURCE)
         && data[i].value.str)
      {
         FreeMemory(MALLOC_ID_SQL, data[i].value.str, strlen(data[i].value.str) + 1);
      }
   }
   FreeMemory(MALLOC_ID_SQL, data, sizeof(sql_data_node) * total_fields);
}
