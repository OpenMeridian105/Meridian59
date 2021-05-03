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
#define MAX_RECORD_QUEUE       1000
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

// Calls mysql_query with query 'b' and logs an error if query fails.
// Doesn't log 'table already exists' error.
#define MYSQL_QUERY_CHECKED(a, b, c) \
   c = mysql_query(a, b); \
   if (c != 0) { \
      const char *_err = mysql_error(a); \
      if (!(strstr(_err, "Table") && strstr(_err, "already exists"))) { \
         bprintf("MySQL error %i: %s", c, _err); } }

#pragma region SQL
#define SQLQUERY_CREATETABLE_MONEYTOTAL										"\
	CREATE TABLE `player_money_total`										\
	(																		\
	  `idplayer_money_total`		INT(11) NOT NULL AUTO_INCREMENT,		\
	  `player_money_total_time`		DATETIME NOT NULL,						\
	  `player_money_total_amount`	INT(18) NOT NULL,						\
	  PRIMARY KEY (`idplayer_money_total`)									\
	)																		\
	ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYERLOGINS									"\
	CREATE TABLE `player_logins`											\
	(																		\
	  `idplayer_logins`					INT(11) NOT NULL AUTO_INCREMENT,	\
	  `player_logins_account_name`		VARCHAR(45) NOT NULL,				\
	  `player_logins_character_name`	VARCHAR(45) NOT NULL,				\
	  `player_logins_IP`				VARCHAR(45) NOT NULL,				\
	  `player_logins_time`				DATETIME NOT NULL,					\
	  PRIMARY KEY (`idplayer_logins`)										\
	)																		\
	ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_MONEYCREATED							"\
	CREATE TABLE `money_created`									\
	(																\
	  `idmoney_created`			INT(11) NOT NULL AUTO_INCREMENT,	\
	  `money_created_amount`	INT(11) NOT NULL,					\
	  `money_created_time`		DATETIME NOT NULL,					\
	  PRIMARY KEY (`idmoney_created`)								\
	)																\
	ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYERDAMAGED							"\
	CREATE TABLE `player_damaged`									\
	(																\
	  `idplayer_damaged`		INT(11) NOT NULL AUTO_INCREMENT,	\
	  `player_damaged_who`		VARCHAR(45) NOT NULL,				\
	  `player_damaged_attacker` VARCHAR(45) NOT NULL,				\
	  `player_damaged_aspell`	INT(11) NOT NULL,					\
	  `player_damaged_atype`	INT(11) NOT NULL,					\
	  `player_damaged_applied`	INT(11) NOT NULL,					\
	  `player_damaged_original` INT(11)	NOT NULL,					\
	  `player_damaged_weapon`	VARCHAR(45) NOT NULL,				\
	  `player_damaged_time`		DATETIME NOT NULL,					\
	  PRIMARY KEY (`idplayer_damaged`)								\
	)																\
	ENGINE=InnoDB DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYERDEATH						      "\
	CREATE TABLE `player_death`										   \
	(																            \
	  `idplayer_death`			INT(11) NOT NULL AUTO_INCREMENT,	\
	  `player_death_victim`		VARCHAR(45) NOT NULL,				\
	  `player_death_killer`		VARCHAR(45) NOT NULL,				\
     `player_death_room`		VARCHAR(45) NOT NULL,				\
	  `player_death_attack`		VARCHAR(45) NOT NULL,				\
	  `player_death_ispvp`		INT(1) NOT NULL,				      \
	  `player_death_time`		DATETIME NOT NULL,					\
	  PRIMARY KEY (`idplayer_death`)								      \
	)																            \
	ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_PLAYER  						         "\
	CREATE TABLE `player`     										      \
	(																            \
	  `idplayer`			     INT(11) NOT NULL AUTO_INCREMENT,	\
	  `player_account_id`     int(11) NOT NULL,                 \
     `player_name`           VARCHAR(45) NOT NULL,             \
     `player_home`           VARCHAR(255) DEFAULT NULL,        \
     `player_bind`           VARCHAR(255) DEFAULT NULL,        \
     `player_guild`          VARCHAR(45) DEFAULT NULL,         \
     `player_max_health`     INT(4) DEFAULT NULL,              \
     `player_max_mana`       INT(4) DEFAULT NULL,              \
     `player_might`          INT(4) DEFAULT NULL,              \
     `player_int`            INT(4) DEFAULT NULL,              \
     `player_myst`           INT(4) DEFAULT NULL,              \
     `player_stam`           INT(4) DEFAULT NULL,              \
     `player_agil`           INT(4) DEFAULT NULL,              \
     `player_aim`            INT(4) DEFAULT NULL,              \
     `player_suicide`        INT(1) DEFAULT '0',               \
     `player_suicide_time`   DATETIME DEFAULT NULL,            \
     PRIMARY KEY(`idplayer`),								            \
     UNIQUE KEY `player_name` (`player_name`)                  \
	)																            \
	ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=latin1;"

#define SQLQUERY_CREATETABLE_GUILD                             "\
   CREATE TABLE `guild`                                        \
   (                                                           \
      `idguild`               INT(11) NOT NULL AUTO_INCREMENT, \
      `guild_name`            VARCHAR(100) NOT NULL,           \
      `guild_leader`          VARCHAR(45) NOT NULL,            \
      `guild_hall`            VARCHAR(100) DEFAULT NULL,       \
      `guild_rent_paid`       INT(11) NOT NULL,                \
      `guild_disbanded`       INT(1) NOT NULL DEFAULT '0',     \
      `guild_disbanded_time`  DATETIME DEFAULT NULL,           \
      PRIMARY KEY(`idguild`),								            \
      UNIQUE KEY `guild_name` (`guild_name`)                   \
   )                                                           \
   ENGINE = InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET = latin1;"

#define SQLQUERY_CREATEPROC_MONEYTOTAL				"\
	CREATE PROCEDURE WriteTotalMoney(				\n\
	IN total_money INT(11))							\n\
	BEGIN											\n\
	  INSERT INTO `player_money_total`				\n\
      SET											\n\
		`player_money_total_amount` = total_money,	\n\
		`player_money_total_time` = now();			\n\
	END"

#define SQLQUERY_CREATEPROC_MONEYCREATED		"\
	CREATE PROCEDURE WriteMoneyCreated(			\n\
	IN money_created INT(11))					\n\
	BEGIN										\n\
	  INSERT INTO `money_created`				\n\
      SET										\n\
		`money_created_amount` = money_created,	\n\
		`money_created_time` = now();			\n\
	END"

#define SQLQUERY_CREATEPROC_PLAYERLOGIN				"\
	CREATE PROCEDURE WritePlayerLogin(				\n\
	  IN account	VARCHAR(45),					\n\
	  IN charname	VARCHAR(45),					\n\
	  IN ip			VARCHAR(45))					\n\
	BEGIN											\n\
	  INSERT INTO `player_logins`					\n\
      SET											\n\
		`player_logins_account_name` = account,		\n\
		`player_logins_character_name` = charname,	\n\
		`player_logins_IP` = ip,					\n\
		`player_logins_time` = now();				\n\
	END"

#define SQLQUERY_CREATEPROC_PLAYERASSESSDAMAGE "\
	CREATE PROCEDURE WritePlayerAssessDamage(	\n\
	  IN who		VARCHAR(45),				\n\
	  IN attacker	VARCHAR(45),				\n\
	  IN aspell		INT(11),					\n\
	  IN atype		INT(11),					\n\
	  IN applied	INT(11),					\n\
	  IN original	INT(11),					\n\
	  IN weapon		VARCHAR(45))				\n\
	BEGIN										\n\
	  INSERT INTO `player_damaged`				\n\
      SET										\n\
		`player_damaged_who` = who,				\n\
		`player_damaged_attacker` = attacker,	\n\
		`player_damaged_aspell` = aspell,		\n\
		`player_damaged_atype` = atype,			\n\
		`player_damaged_applied` = applied,		\n\
		`player_damaged_original` = original,	\n\
		`player_damaged_weapon` = weapon,		\n\
		`player_damaged_time` = now();			\n\
	END"

#define SQLQUERY_CREATEPROC_PLAYERDEATH			"\
	CREATE PROCEDURE WritePlayerDeath(				\n\
	  IN victim		VARCHAR(45),					   \n\
	  IN killer		VARCHAR(45),					   \n\
	  IN room		VARCHAR(45),					   \n\
	  IN attack 	VARCHAR(45),					   \n\
	  IN ispvp		INT(1))						      \n\
	BEGIN											         \n\
	  INSERT INTO `player_death`					   \n\
      SET											      \n\
		`player_death_victim` = victim,				\n\
		`player_death_killer` = killer,				\n\
		`player_death_room` = room,					\n\
		`player_death_attack` = attack,				\n\
		`player_death_ispvp` = ispvp,				   \n\
		`player_death_time` = now();				   \n\
	END"

#define SQLQUERY_CREATEPROC_PLAYER			      "\
	CREATE PROCEDURE WritePlayer(	               \
     IN account_id   INT(11),					      \
	  IN name		   VARCHAR(45),					\
	  IN home		   VARCHAR(255),					\
	  IN bind		   VARCHAR(255),					\
	  IN guild 	      VARCHAR(45),					\
	  IN max_health   INT(4),						   \
     IN max_mana     INT(4),						   \
     IN might        INT(4),						   \
     IN p_int        INT(4),						   \
     IN myst         INT(4),						   \
     IN stam         INT(4),						   \
     IN agil         INT(4),						   \
     IN aim          INT(4))						   \
	BEGIN											         \
	  INSERT INTO `player` 				            \
      (  `player_account_id`,				         \
         `player_name`, 			               \
         `player_home`, 				            \
         `player_bind`, 				            \
         `player_guild`, 				            \
         `player_max_health`, 				      \
         `player_max_mana`, 				         \
         `player_might`, 				            \
         `player_int`, 				               \
         `player_myst`, 				            \
         `player_stam`, 				            \
         `player_agil`, 				            \
         `player_aim`) 				               \
         VALUES (account_id, 				         \
            name, 				                  \
            home, 				                  \
            bind, 				                  \
            guild, 				                  \
            max_health, 				            \
            max_mana, 				               \
            might, 				                  \
            p_int, 				                  \
            myst, 				                  \
            stam, 				                  \
            agil, 				                  \
            aim)				                     \
		ON DUPLICATE KEY UPDATE                   \
      `player_home` = home,                     \
      `player_bind` = bind,                     \
      `player_guild` = guild,                   \
      `player_max_health` = max_health,         \
      `player_max_mana` = max_mana,             \
      `player_might` = might,                   \
      `player_int` = p_int,                     \
      `player_myst` = myst,                     \
      `player_stam` = stam,                     \
      `player_agil` = agil,                     \
      `player_aim` = aim;				            \
	END"

#define SQLQUERY_CREATEPROC_PLAYERSUICIDE			"\
	CREATE PROCEDURE WritePlayerSuicide(			\n\
     IN account_id   INT(11),					      \n\
     IN name		   VARCHAR(45))					\n\
	BEGIN											         \n\
	  UPDATE `player`					               \n\
      SET											      \n\
		`player_suicide` = 1,			            \n\
      `player_suicide_time` = now()             \n\
      WHERE `player_account_id` = account_id    \n\
         AND `player_name` = name;              \n\
	END"

#define SQLQUERY_CREATEPROC_GUILD               "\
	CREATE PROCEDURE WriteGuild(                 \
     IN name      VARCHAR(100),					   \
     IN leader    VARCHAR(100),			         \
     IN hall      VARCHAR(100),				      \
     IN rent      INT(11))                      \
	BEGIN											         \
	  INSERT INTO `guild` 				            \
      (  `guild_name`,				               \
         `guild_leader`, 			               \
         `guild_hall`,                          \
         `guild_rent_paid`) 				         \
         VALUES (                               \
            name, 				                  \
            leader, 				                  \
            hall,                               \
            rent)				                     \
		ON DUPLICATE KEY UPDATE                   \
      `guild_leader` = leader,                  \
      `guild_hall` = hall,                      \
      `guild_rent_paid` = rent;                 \
	END"

#define SQLQUERY_CREATEPROC_GUILDDISBAND			"\
	CREATE PROCEDURE WriteGuildDisband(		      \n\
     IN name      VARCHAR(100))					   \n\
	BEGIN											         \n\
	  UPDATE `guild`					               \n\
      SET											      \n\
		`guild_hall` = '',		                  \n\
      `guild_disbanded` = 1,			            \n\
      `guild_disbanded_time` = now()            \n\
      WHERE `guild_name` = name;                \n\
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


/*
 * MySQLTypeNumArgs: Return the number of expected arguments for a given
 *   SQL statistic type.
 */
int MySQLTypeNumArgs(int type)
{
   if (type >= STAT_NONE && type <= STAT_MAX)
      return Statistics_Table[type].num_fields;

   bprintf("Unknown type received in MySQLTypeNumArgs: %i", type);

   return 0;
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
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_PLAYER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATETABLE_GUILD, status);

   char buffer[128];
   // Drop existing procedures if present.
   for (int i = 1; i <= STAT_MAX; ++i)
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
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYER, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_PLAYERSUICIDE, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_GUILD, status);
   MYSQL_QUERY_CHECKED(mysql, SQLQUERY_CREATEPROC_GUILDDISBAND, status);

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
		_MySQLWriteNode(node, processNode);

		// free memory of processed record and node
      FreeDataNodeMemory(node->num_fields, node->num_fields, (sql_data_node *)node->data);
      FreeMemory(MALLOC_ID_SQL, node, sizeof(sql_queue_node));

	}

	return dequeued;
}

void _MySQLCallProc(char* ProcName, MYSQL_BIND Parameters[])
{
   MYSQL_STMT*	stmt;
   int status;

   if (!ProcName || !Parameters)
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

   status = mysql_stmt_bind_param(stmt, Parameters);
   if (status != 0 && mysql)
   {
      bprintf("MySQL error in mysql_stmt_bind_param with procedure %s, status code %i: %s",
         ProcName, status, mysql_error(mysql));
      mysql_stmt_close(stmt);
      return;
   }

   status = mysql_stmt_execute(stmt);
   if (status != 0 && mysql)
   {
      bprintf("MySQL error in mysql_stmt_execute with procedure %s, status code %i: %s",
         ProcName, status, mysql_error(mysql));
   }

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
