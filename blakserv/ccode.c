// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* ccode.c
*

  This module has all of the C functions that are callable from Blakod.
  The parameters describe what class/message called the function, its
  parameters, and its local variables.  The return value of these
  functions is really a val_type, and is returned to the Blakod.
  
*/

#include "blakserv.h"

#define iswhite(c) ((c)==' ' || (c)=='\t' || (c)=='\n' || (c)=='\r')

// Simplify retrieval/error checking of values.
// a == val_type, b == array index, c == return value,
//    d == kod type, e == error message
// Uses __func__ and #variable to print error messages.
#define RETRIEVEVALUETYPEMSG(_a, _b, _c, _d, _e) \
   _a = RetrieveValue(object_id, local_vars, normal_parm_array[_b].type, \
   normal_parm_array[_b].value); \
   if (_a.v.tag != _d) \
   { \
      bprintf(_e, __func__, _a.v.tag, _a.v.data, #_a, object_id, \
         GetClassNameByObjectID(object_id)); \
      return _c; \
   }

// a == val_type, b == array index, c == return value
// Handles case where a list is retrieved, but NIL is also valid.
#define RETRIEVEVALUELISTNIL(_a, _b, _c) \
   _a = RetrieveValue(object_id,local_vars,normal_parm_array[_b].type, \
      normal_parm_array[_b].value); \
   if (_a.v.tag != TAG_LIST) \
   { \
      if (_a.v.tag == TAG_NIL) \
      { \
         return _c; \
      } \
      bprintf("%s can't get elem in non-list %i,%i for %s, obj:%i %s\n", \
         __func__, _a.v.tag, _a.v.data, #_a, object_id, \
         GetClassNameByObjectID(object_id)); \
      return _c; \
   }

// a == val_type, b == array index, c == return value
#define RETRIEVEVALUEINT(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_INT, \
      "%s can't use non-int %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUEOBJECT(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_OBJECT, \
      "%s can't use non-object %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUELIST(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_LIST, \
      "%s can't use non-list %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUERESOURCE(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_RESOURCE, \
      "%s can't use non-resource %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUETIMER(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_TIMER, \
      "%s can't use non-timer %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUEROOMDATA(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_ROOM_DATA, \
      "%s can't use non-room %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUESTRING(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_STRING, \
      "%s can't use non-string %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUECLASS(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_CLASS, \
      "%s can't use non-class %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUEMESSAGE(_a, _b, _c) \
RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_MESSAGE, \
      "%s can't use non-message %i,%i for %s, obj:%i %s\n")

#define RETRIEVEVALUETABLE(_a, _b, _c) \
   RETRIEVEVALUETYPEMSG(_a, _b, _c, TAG_TABLE, \
      "%s can't use non-table %i,%i for %s, obj:%i %s\n")

// _h == room_node, _i == TAG_ROOM_DATA, _j == return value
#define GETROOMBYID(_h, _i, _j) \
   _h = GetRoomDataByID(_i.v.data); \
   if (_h == NULL) \
   { \
      bprintf("%s can't find room %i in obj:%i %s\n",\
         __func__, _i.v.data, object_id, \
         GetClassNameByObjectID(object_id)); \
      return _j; \
   }

// global buffers for zero-terminated string manipulation
static char buf0[LEN_MAX_CLIENT_MSG+1];
static char buf1[LEN_MAX_CLIENT_MSG+1];

/* just like strstr, except any case-insensitive match will be returned */
const char* stristr(const char* pSource, const char* pSearch)
{
   if (!pSource || !pSearch || !*pSearch)
      return NULL;
	
   int nSearch = strlen(pSearch);
   // Don't search past the end of pSource
   const char *pEnd = pSource + strlen(pSource) - nSearch;
   while (pSource <= pEnd)
   {
      if (0 == strnicmp(pSource, pSearch, nSearch))
         return pSource;

      pSource++;
   }
	
   return NULL;
}


int C_Invalid(int object_id,local_var_type *local_vars,
			  int num_normal_parms,parm_node normal_parm_array[],
			  int num_name_parms,parm_node name_parm_array[])
{
	bprintf("C_Invalid called--bad C function number");
	return NIL;
}

int C_SetTrace(int object_id,local_var_type *local_vars,
               int num_normal_parms,parm_node normal_parm_array[],
               int num_name_parms,parm_node name_parm_array[])
{
#if defined BLAKDEBUG || defined DEBUG
   SetTraceOn();
#else
   dprintf("SetTrace disabled for release build.");
#endif
   return NIL;
}

/*
 * C_SaveGame: Performs a system save, but without garbage collection. We
 *    can't garbage collect when the game is saved from blakod as object,
 *    list, timer and string references (in local vars) may be incorrect when
 *    control passes back to the calling message. Returns a blakod string
 *    containing the time of the saved game if successful.
 */
int C_SaveGame(int object_id,local_var_type *local_vars,
               int num_normal_parms,parm_node normal_parm_array[],
               int num_name_parms,parm_node name_parm_array[])
{
   val_type ret_val;
   int save_time = 0;
   string_node *snod;
   char timeStr[15];

   PauseTimers();
   lprintf("C_SaveGame saving game\n");
   save_time = SaveAll();
   UnpauseTimers();

   // Check for a sane time value.
   if (save_time < 0 || save_time > INT_MAX)
   {
      bprintf("C_SaveGame got invalid save game time!");
      return NIL;
   }

   ret_val.v.tag = TAG_STRING;
   ret_val.v.data = CreateString("");

   snod = GetStringByID(ret_val.v.data);
   if (snod == NULL)
   {
      bprintf("C_SaveGame can't set invalid string %i,%i\n",
         ret_val.v.tag, ret_val.v.data);
      return NIL;
   }

   // Make a string with the save game time.
   sprintf(timeStr, "%d", save_time);

   // Make a blakod string using the string value of the save game time.
   SetString(snod, timeStr, 10);

   return ret_val.int_val;
}

/*
 * C_LoadGame: Takes a blakod string as a parameter, which contains a save
 *    game time.  Posts a message to the blakserv main thread which triggers
 *    a load game, using the save game time value sent in the message. All
 *    users are disconnected when the game reload triggers.
 */
int C_LoadGame(int object_id, local_var_type *local_vars,
               int num_normal_parms, parm_node normal_parm_array[],
               int num_name_parms, parm_node name_parm_array[])
{
   val_type game_val;
   string_node *snod;
   int save_time = 0;

   RETRIEVEVALUESTRING(game_val, 0, NIL);

   snod = GetStringByID(game_val.v.data);
   if (snod == NULL)
   {
      bprintf("C_LoadGame can't get invalid string %i,%i\n",
         game_val.v.tag, game_val.v.data);
      return NIL;
   }

   // Convert string time to integer.
   save_time = atoi(snod->data);

   // Check for a sane time value.
   if (save_time < 0 || save_time > INT_MAX)
   {
      bprintf("C_LoadGame got invalid save game time!");
      return NIL;
   }

   MessagePost(main_thread_id, WM_BLAK_MAIN_LOAD_GAME, 0, save_time);

   return NIL;
}

int C_AddPacket(int object_id,local_var_type *local_vars,
				int num_normal_parms,parm_node normal_parm_array[],
				int num_name_parms,parm_node name_parm_array[])
{
	int i;
	val_type send_len,send_data;
	
	i = 0;
	while (i < num_normal_parms)
	{
		send_len = RetrieveValue(object_id,local_vars,normal_parm_array[i].type,
			normal_parm_array[i].value);
		i++;
		if (i >= num_normal_parms)
		{
			bprintf("C_AddPacket has # of bytes, needs object\n");
			break;
		}
		
		send_data = RetrieveValue(object_id,local_vars,normal_parm_array[i].type,
			normal_parm_array[i].value);
		i++;
		AddBlakodToPacket(send_len,send_data);
	}
	
	return NIL;
}

int C_SendPacket(int object_id,local_var_type *local_vars,
				 int num_normal_parms,parm_node normal_parm_array[],
				 int num_name_parms,parm_node name_parm_array[])
{
	val_type temp;
	
	temp = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
	if (temp.v.tag != TAG_SESSION)
	{
		bprintf("C_SendPacket object %i can't send to non-session %i,%i\n",
			object_id,temp.v.tag,temp.v.data);
		return NIL;
	}
	
	SendPacket(temp.v.data);
	
	return NIL;
}

int C_SendCopyPacket(int object_id,local_var_type *local_vars,
					 int num_normal_parms,parm_node normal_parm_array[],
					 int num_name_parms,parm_node name_parm_array[])
{
	val_type temp;
	
	temp = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
	if (temp.v.tag != TAG_SESSION)
	{
		bprintf("C_SendPacket object %i can't send to non-session %i,%i\n",
			object_id,temp.v.tag,temp.v.data);
		return NIL;
	}
	
	SendCopyPacket(temp.v.data);
	
	return NIL;
}

int C_ClearPacket(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	ClearPacket();
	
	return NIL;
}

int C_GodLog(int object_id,local_var_type *local_vars,
			int num_normal_parms,parm_node normal_parm_array[],
			int num_name_parms,parm_node name_parm_array[])
{
	int i;
	val_type each_val;
	class_node *c;
	char buf[2000];
	kod_statistics *kstat;
	
	/* need the current interpreting class in case there are debug strings,
	which are stored in the class. */

	kstat = GetKodStats();
	
	c = GetClassByID(kstat->interpreting_class);
	if (c == NULL)
	{
		bprintf("C_GodLog can't find class %i, can't print out debug strs\n",
			kstat->interpreting_class);
		return NIL;
	}
	
	sprintf(buf,"[%s] ",BlakodDebugInfo());
	
	for (i=0;i<num_normal_parms;i++)
	{
		each_val = RetrieveValue(object_id,local_vars,normal_parm_array[i].type,
			normal_parm_array[i].value);
		
		switch (each_val.v.tag)
		{
		case TAG_DEBUGSTR :
			sprintf(buf+strlen(buf),"%s",GetClassDebugStr(c,each_val.v.data));
			break;
			
		case TAG_RESOURCE :
			{
				resource_node *r;
				r = GetResourceByID(each_val.v.data);
				if (r == NULL)
				{
					sprintf(buf+strlen(buf),"<unknown RESOURCE %i>",each_val.v.data);
				}
				else
				{
					sprintf(buf+strlen(buf),"%s",r->resource_val[0]);
				}
			}
			break;
			
		case TAG_INT :
			sprintf(buf+strlen(buf),"%d",(int)each_val.v.data);
			break;
			
		case TAG_CLASS :
			{
				class_node *c;
				c = GetClassByID(each_val.v.data);
				if (c == NULL)
				{
					sprintf(buf+strlen(buf),"<unknown CLASS %i>",each_val.v.data);
				}
				else
				{
					strcat(buf,"&");
					strcat(buf,c->class_name);
				}
			}
			break;
			
		case TAG_STRING :
			{
				int lenBuffer;
				string_node *snod = GetStringByID(each_val.v.data);
				
				if (snod == NULL)
				{
					bprintf("C_GodLog can't find string %i\n",each_val.v.data);
					return NIL;
				}
				lenBuffer = strlen(buf);
				memcpy(buf + lenBuffer,snod->data,snod->len_data);
				*(buf + lenBuffer + snod->len_data) = 0;
			}
			break;
			
		case TAG_TEMP_STRING :
			{
				int len_buf;
				string_node *snod;
				
				snod = GetTempString();
				len_buf = strlen(buf);
				memcpy(buf + len_buf,snod->data,snod->len_data);
				*(buf + len_buf + snod->len_data) = 0;
			}
			break;
			
		case TAG_OBJECT :
			{
				object_node *o;
				class_node *c;
				user_node *u;
				
				/* for objects, print object number */
				
				o = GetObjectByID(each_val.v.data);
				if (o == NULL)
				{
					sprintf(buf+strlen(buf),"<OBJECT %i invalid>",each_val.v.data);
					break;
				}
				c = GetClassByID(o->class_id);
				if (c == NULL)
				{
					sprintf(buf+strlen(buf),"<OBJECT %i unknown class>",each_val.v.data);
					break;
				}
				
				if (c->class_id == USER_CLASS || c->class_id == DM_CLASS ||
					 c->class_id == ADMIN_CLASS)
				{
					u = GetUserByObjectID(o->object_id);
					if (u == NULL)
					{
						sprintf(buf+strlen(buf),"<OBJECT %i broken user>",each_val.v.data);
						break;
					}
					sprintf(buf+strlen(buf),"OBJECT %i",each_val.v.data);
					break;
				}
			}
			//FALLTHRU
		default :
			sprintf(buf+strlen(buf),"%s %s",GetTagName(each_val),GetDataName(each_val));
			break;
      }
   }
   gprintf("%s\n",buf);
   return NIL;
}

int C_Debug(int object_id,local_var_type *local_vars,
			int num_normal_parms,parm_node normal_parm_array[],
			int num_name_parms,parm_node name_parm_array[])
{
	int i;
	val_type each_val;
	class_node *c;
	char buf[2000];
	kod_statistics *kstat;
	
	/* need the current interpreting class in case there are debug strings,
	which are stored in the class. */

	kstat = GetKodStats();
	
	c = GetClassByID(kstat->interpreting_class);
	if (c == NULL)
	{
		bprintf("C_Debug can't find class %i, can't print out debug strs\n",
			kstat->interpreting_class);
		return NIL;
	}
	
	sprintf(buf,"[%s] ",BlakodDebugInfo());
	
	for (i=0;i<num_normal_parms;i++)
	{
		each_val = RetrieveValue(object_id,local_vars,normal_parm_array[i].type,
			normal_parm_array[i].value);
		
		switch (each_val.v.tag)
		{
		case TAG_DEBUGSTR :
			sprintf(buf+strlen(buf),"%s",GetClassDebugStr(c,each_val.v.data));
			break;
			
		case TAG_RESOURCE :
			{
				resource_node *r;
				r = GetResourceByID(each_val.v.data);
				if (r == NULL)
				{
					sprintf(buf+strlen(buf),"<unknown RESOURCE %i>",each_val.v.data);
				}
				else
				{
					sprintf(buf+strlen(buf),"%s",r->resource_val[0]);
				}
			}
			break;
			
		case TAG_INT :
			sprintf(buf+strlen(buf),"%d",(int)each_val.v.data);
			break;
			
		case TAG_CLASS :
			{
				class_node *c;
				c = GetClassByID(each_val.v.data);
				if (c == NULL)
				{
					sprintf(buf+strlen(buf),"<unknown CLASS %i>",each_val.v.data);
				}
				else
				{
					strcat(buf,"&");
					strcat(buf,c->class_name);
				}
			}
			break;
			
		case TAG_STRING :
			{
				int lenBuffer;
				string_node *snod = GetStringByID(each_val.v.data);
				
				if (snod == NULL)
				{
					bprintf("C_Debug can't find string %i\n",each_val.v.data);
					return NIL;
				}
				lenBuffer = strlen(buf);
				memcpy(buf + lenBuffer,snod->data,snod->len_data);
				*(buf + lenBuffer + snod->len_data) = 0;
			}
			break;
			
		case TAG_TEMP_STRING :
			{
				int len_buf;
				string_node *snod;
				
				snod = GetTempString();
				len_buf = strlen(buf);
				memcpy(buf + len_buf,snod->data,snod->len_data);
				*(buf + len_buf + snod->len_data) = 0;
			}
			break;
			
		case TAG_OBJECT :
			{
				object_node *o;
				class_node *c;
				user_node *u;
				
				/* for objects, print account if it's a user */
				
				o = GetObjectByID(each_val.v.data);
				if (o == NULL)
				{
					sprintf(buf+strlen(buf),"<OBJECT %i invalid>",each_val.v.data);
					break;
				}
				c = GetClassByID(o->class_id);
				if (c == NULL)
				{
					sprintf(buf+strlen(buf),"<OBJECT %i unknown class>",each_val.v.data);
					break;
				}
				
				if (c->class_id == USER_CLASS || c->class_id == DM_CLASS ||
					 c->class_id == ADMIN_CLASS)
				{
					u = GetUserByObjectID(o->object_id);
					if (u == NULL)
					{
						sprintf(buf+strlen(buf),"<OBJECT %i broken user>",each_val.v.data);
						break;
					}
					sprintf(buf+strlen(buf),"ACCOUNT %i OBJECT %i",u->account_id,each_val.v.data);
					break;
				}
			}
			//FALLTHRU
		default :
			sprintf(buf+strlen(buf),"%s %s",GetTagName(each_val),GetDataName(each_val));
			break;
      }
      
      if (i != num_normal_parms-1)
		  sprintf(buf+strlen(buf),",");
   }
   dprintf("%s\n",buf);
   return NIL;
}

int C_GetInactiveTime(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type session_val, ret_val;
   session_node *s;

   session_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   if (session_val.v.tag != TAG_SESSION)
   {
      bprintf("C_GetInactiveTime can't use non-session %i,%i\n",
         session_val.v.tag, session_val.v.data);
      return NIL;
   }

   s = GetSessionByID(session_val.v.data);
   if (s == NULL)
   {
      bprintf("C_GetInactiveTime can't find session %i\n", session_val.v.data);
      return NIL;
   }
   if (s->state != STATE_GAME)
   {
      bprintf("C_GetInactiveTime can't use session %i in state %i\n",
         session_val.v.data, s->state);
      return NIL;
   }

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = GetSecondCount() - s->game->game_last_message_time;

   return ret_val.int_val;
}

int C_DumpStack(int object_id,local_var_type *local_vars,
					  int num_normal_parms,parm_node normal_parm_array[],
					  int num_name_parms,parm_node name_parm_array[])
{
   if (ConfigBool(DEBUG_DUMPSTACK))
      PrintStackToDebug();
   else
      dprintf("DumpStack() disabled in server config, <set config bool [Debug] DumpStack Yes> to enable.");

   return NIL;
}

int C_SendMessage(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type object_val,message_val;

   /* Get the object (or class or int) to which we are sending the message */
   /* Not to be confused with object_id, which is the 'self' object sending the message */
   object_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   /* Handle the message to send first; that way other errors are more descriptive */
   message_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
      normal_parm_array[1].value);
   if (message_val.v.tag != TAG_MESSAGE)
   {
      // Handle message names passed as strings.
      if (message_val.v.tag == TAG_STRING)
      {
         message_val.v.data = GetIDByName(GetStringByID(message_val.v.data)->data);
         if (message_val.v.data == INVALID_ID)
         {
            bprintf("C_SendMessage OBJECT %i can't use bad string message %i,\n",
               object_id, message_val.v.tag);
            return NIL;
         }
      }
      else
      {
         bprintf("C_SendMessage OBJECT %i can't send non-message %i,%i\n",
               object_id, message_val.v.tag, message_val.v.data);
         return NIL;
      }
   }

   if (object_val.v.tag == TAG_OBJECT)
      return SendBlakodMessage(object_val.v.data, message_val.v.data, num_name_parms, name_parm_array);

   if (object_val.v.tag == TAG_INT)
   {
      /* Can send to built-in objects using constants. */
      object_val.v.data = GetBuiltInObjectID(object_val.v.data);
      if (object_val.v.data > INVALID_OBJECT)
         return SendBlakodMessage(object_val.v.data, message_val.v.data,
                     num_name_parms, name_parm_array);
   }

   if (object_val.v.tag == TAG_CLASS)
      return SendBlakodClassMessage(object_val.v.data, message_val.v.data, num_name_parms, name_parm_array);

   /* Assumes object_id (the current 'self') is a valid object */
   bprintf("C_SendMessage OBJECT %i CLASS %s can't send MESSAGE %s (%i) to non-object %i,%i\n",
      object_id,
      GetClassByID(GetObjectByID(object_id)->class_id)->class_name,
      GetNameByID(message_val.v.data), message_val.v.data,
      object_val.v.tag,object_val.v.data);
   return NIL;
}

int C_PostMessage(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type object_val, message_val;

   object_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);

   /* Handle the message to send first; that way other errors are more descriptive */
   message_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
      normal_parm_array[1].value);
   if (message_val.v.tag != TAG_MESSAGE)
   {
      // Handle message names passed as strings.
      if (message_val.v.tag == TAG_STRING)
      {
         message_val.v.data = GetIDByName(GetStringByID(message_val.v.data)->data);
         if (message_val.v.data == INVALID_ID)
         {
            bprintf("C_PostMessage OBJECT %i can't use bad string message %i,\n",
               object_id, message_val.v.tag);
            return NIL;
         }
      }
      else
      {
         bprintf("C_PostMessage OBJECT %i can't send non-messsage %i,%i\n",
            object_id,message_val.v.tag,message_val.v.data);
         return NIL;
      }
   }

   if (object_val.v.tag == TAG_OBJECT)
   {
      PostBlakodMessage(object_val.v.data, message_val.v.data, num_name_parms,
         name_parm_array);
   }
   else if (object_val.v.tag == TAG_INT)
   {
      /* Can post to built-in objects using constants. */
      int post_obj_id = GetBuiltInObjectID(object_val.v.data);
      if (post_obj_id > INVALID_OBJECT)
      {
         PostBlakodMessage(post_obj_id, message_val.v.data, num_name_parms,
            name_parm_array);
      }
      else
      {
         /* Assumes object_id (the current 'self') is a valid object */
         bprintf("C_PostMessage OBJECT %i CLASS %s can't send MESSAGE %s (%i) to bad built-in object %i,%i\n",
            object_id,
            GetClassByID(GetObjectByID(object_id)->class_id)->class_name,
            GetNameByID(message_val.v.data), message_val.v.data,
            object_val.v.tag, object_val.v.data);
      }
   }
   else
   {
      /* Assumes object_id (the current 'self') is a valid object */
      bprintf("C_PostMessage OBJECT %i CLASS %s can't send MESSAGE %s (%i) to non-object %i,%i\n",
         object_id,
         GetClassByID(GetObjectByID(object_id)->class_id)->class_name,
         GetNameByID(message_val.v.data), message_val.v.data,
         object_val.v.tag,object_val.v.data);
      return NIL;
   }

   return NIL;
}

/*
* C_SendListMessage: Takes a list, a list position (n), a message and message
*   parameters. If n = 0, sends the message to all objects in the list given.
*   If n > 1, sends the message to the Nth object in each element of the list
*   given, which should be a list containing sublists. Handles TRUE and FALSE
*   returns from the messages, returns TRUE by default and FALSE if any called
*   object returns FALSE. Rationale is that a call to multiple objects would
*   be looking for a FALSE condition, not a TRUE one.
*/
int C_SendListMessage(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, pos_val, message_val, ret_val;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = True;

   // Get the list we're going to use.
   list_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   // If $ list, just return.
   if (list_val.v.tag == TAG_NIL)
      return ret_val.int_val;

   // List 'position', 0 for obj in top list, >0 for obj = Nth(list,pos).
   pos_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[1].value);
   if (pos_val.v.tag != TAG_INT || pos_val.v.data < 0)
   {
      bprintf("C_SendListMessage OBJECT %i can't use non-int list pos %i, %i\n",
         object_id, pos_val.v.tag, pos_val.v.data);
      return ret_val.int_val;
   }

   // Get the message to send, either a message ID or a string containing a message name.
   message_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
      normal_parm_array[2].value);
   if (message_val.v.tag != TAG_MESSAGE)
   {
      // Handle message names passed as strings.
      if (message_val.v.tag == TAG_STRING)
      {
         message_val.v.data = GetIDByName(GetStringByID(message_val.v.data)->data);
         if (message_val.v.data == INVALID_ID)
         {
            bprintf("C_SendListMessage OBJECT %i can't use bad string message %i,\n",
               object_id, message_val.v.tag);
            return ret_val.int_val;
         }
      }
      else
      {
         bprintf("C_SendListMessage OBJECT %i can't send non-message %i,%i\n",
               object_id, message_val.v.tag, message_val.v.data);
         return ret_val.int_val;
      }
   }

   if (list_val.v.tag != TAG_LIST)
   {
      bprintf("C_SendListMessage OBJECT %i can't send to non-list %i, %i\n",
         object_id, list_val.v.tag, list_val.v.data);
      return ret_val.int_val;
   }

   // Separate functions handle each of the cases: objects in top list, objects
   // as first element of sublist, objects as nth element of sublist.
   if (pos_val.v.data == 0)
      ret_val.v.data = SendListMessage(list_val.v.data, False, message_val.v.data,
         num_name_parms, name_parm_array);
   else if (pos_val.v.data == 1)
      ret_val.v.data = SendFirstListMessage(list_val.v.data, False, message_val.v.data,
         num_name_parms, name_parm_array);
   else
      ret_val.v.data = SendNthListMessage(list_val.v.data, pos_val.v.data, False,
         message_val.v.data, num_name_parms, name_parm_array);

   return ret_val.int_val;
}

/*
* C_SendListMessageBreak: Works the same as SendListMessage, except
*   breaks on the first FALSE return from the sent messages. Used to
*   speed up algorithms that rely on calling every object in a list
*   until reaching a FALSE return.
*/
int C_SendListMessageBreak(int object_id, local_var_type *local_vars,
            int num_normal_parms, parm_node normal_parm_array[],
            int num_name_parms, parm_node name_parm_array[])
{
   val_type list_val, pos_val, message_val, ret_val;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = True;

   // Get the list we're going to use.
   list_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   // If $ list, just return.
   if (list_val.v.tag == TAG_NIL)
      return ret_val.int_val;

   // List 'position', 0 for obj in top list, >0 for obj = Nth(list,pos).
   pos_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[1].value);
   if (pos_val.v.tag != TAG_INT || pos_val.v.data < 0)
   {
      bprintf("C_SendListMessageBreak OBJECT %i can't use non-int list pos %i, %i\n",
         object_id, pos_val.v.tag, pos_val.v.data);
      return ret_val.int_val;
   }

   // Get the message to send, either a message ID or a string containing a message name.
   message_val = RetrieveValue(object_id, local_vars, normal_parm_array[2].type,
      normal_parm_array[2].value);
   if (message_val.v.tag != TAG_MESSAGE)
   {
      // Handle message names passed as strings.
      if (message_val.v.tag == TAG_STRING)
      {
         message_val.v.data = GetIDByName(GetStringByID(message_val.v.data)->data);
         if (message_val.v.data == INVALID_ID)
         {
            bprintf("C_SendListMessageBreak OBJECT %i can't use bad string message %i,\n",
               object_id, message_val.v.tag);
            return ret_val.int_val;
         }
      }
      else
      {
         bprintf("C_SendListMessageBreak OBJECT %i can't send non-message %i,%i\n",
            object_id, message_val.v.tag, message_val.v.data);
         return ret_val.int_val;
      }
   }

   if (list_val.v.tag != TAG_LIST)
   {
      bprintf("C_SendListMessageBreak OBJECT %i can't send to non-list %i, %i\n",
         object_id, list_val.v.tag, list_val.v.data);
      return ret_val.int_val;
   }

   // Separate functions handle each of the cases: objects in top list, objects
   // as first element of sublist, objects as nth element of sublist.
   if (pos_val.v.data == 0)
      ret_val.v.data = SendListMessage(list_val.v.data, True, message_val.v.data,
      num_name_parms, name_parm_array);
   else if (pos_val.v.data == 1)
      ret_val.v.data = SendFirstListMessage(list_val.v.data, True, message_val.v.data,
      num_name_parms, name_parm_array);
   else
      ret_val.v.data = SendNthListMessage(list_val.v.data, pos_val.v.data, True,
      message_val.v.data, num_name_parms, name_parm_array);

   return ret_val.int_val;
}

/*
* C_SendListMessageByClass: Takes a list, a list position (n), a class, a
*   message and message parameters. Works the same as C_SendListMessage,
*   except the message is only sent to objects of the given class.
*/
int C_SendListMessageByClass(int object_id, local_var_type *local_vars,
            int num_normal_parms, parm_node normal_parm_array[],
            int num_name_parms, parm_node name_parm_array[])
{
   val_type list_val, pos_val, message_val, class_val, ret_val;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = True;

   // Get the list we're going to use.
   list_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   // If $ list, just return.
   if (list_val.v.tag == TAG_NIL)
      return ret_val.int_val;

   // List 'position', 0 for obj in top list, >0 for obj = Nth(list,pos).
   pos_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[1].value);
   if (pos_val.v.tag != TAG_INT || pos_val.v.data < 0)
   {
      bprintf("C_SendListMessageByClass OBJECT %i can't use non-int list pos %i, %i\n",
         object_id, pos_val.v.tag, pos_val.v.data);
      return ret_val.int_val;
   }

   // Get the class we want to send to.
   class_val = RetrieveValue(object_id, local_vars, normal_parm_array[2].type,
      normal_parm_array[2].value);
   if (class_val.v.tag != TAG_CLASS)
   {
      bprintf("C_SendListMessageByClass OBJECT %i can't use non-class %i, %i\n",
         object_id, class_val.v.tag, class_val.v.data);
      return ret_val.int_val;
   }

   // Get the message to send, either a message ID or a string containing a message name.
   message_val = RetrieveValue(object_id, local_vars, normal_parm_array[3].type,
      normal_parm_array[3].value);
   if (message_val.v.tag != TAG_MESSAGE)
   {
      // Handle message names passed as strings.
      if (message_val.v.tag == TAG_STRING)
      {
         message_val.v.data = GetIDByName(GetStringByID(message_val.v.data)->data);
         if (message_val.v.data == INVALID_ID)
         {
            bprintf("C_SendListMessageByClass OBJECT %i can't use bad string message %i,\n",
               object_id, message_val.v.tag);
            return ret_val.int_val;
         }
      }
      else
      {
         bprintf("C_SendListMessageByClass OBJECT %i can't send non-message %i,%i\n",
            object_id, message_val.v.tag, message_val.v.data);
         return ret_val.int_val;
      }
   }

   if (list_val.v.tag != TAG_LIST)
   {
      bprintf("C_SendListMessageByClass OBJECT %i can't send to non-list %i, %i\n",
         object_id, list_val.v.tag, list_val.v.data);
      return ret_val.int_val;
   }

   // Separate functions handle each of the cases: objects in top list, objects
   // as first element of sublist, objects as nth element of sublist.
   if (pos_val.v.data == 0)
      ret_val.v.data = SendListMessageByClass(list_val.v.data, class_val.v.data, False,
         message_val.v.data, num_name_parms, name_parm_array);
   else if (pos_val.v.data == 1)
      ret_val.v.data = SendFirstListMessageByClass(list_val.v.data, class_val.v.data, False,
         message_val.v.data, num_name_parms, name_parm_array);
   else
      ret_val.v.data = SendNthListMessageByClass(list_val.v.data, pos_val.v.data, 
         class_val.v.data, False, message_val.v.data, num_name_parms, name_parm_array);

   return ret_val.int_val;
}

/*
* C_SendListMessageByClassBreak: Works the same as SendListMessageByClass,
*   except breaks on the first FALSE return from the sent messages. Used to
*   speed up algorithms that rely on calling every object in a list until
*   reaching a FALSE return.
*/
int C_SendListMessageByClassBreak(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type list_val, pos_val, message_val, class_val, ret_val;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = True;

   // Get the list we're going to use.
   list_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   // If $ list, just return.
   if (list_val.v.tag == TAG_NIL)
      return ret_val.int_val;

   // List 'position', 0 for obj in top list, >0 for obj = Nth(list,pos).
   pos_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[1].value);
   if (pos_val.v.tag != TAG_INT || pos_val.v.data < 0)
   {
      bprintf("C_SendListMessageByClassBreak OBJECT %i can't use non-int list pos %i, %i\n",
         object_id, pos_val.v.tag, pos_val.v.data);
      return ret_val.int_val;
   }

   // Get the class we want to send to.
   class_val = RetrieveValue(object_id, local_vars, normal_parm_array[2].type,
      normal_parm_array[2].value);
   if (class_val.v.tag != TAG_CLASS)
   {
      bprintf("C_SendListMessageByClassBreak OBJECT %i can't use non-class %i, %i\n",
         object_id, class_val.v.tag, class_val.v.data);
      return ret_val.int_val;
   }

   // Get the message to send, either a message ID or a string containing a message name.
   message_val = RetrieveValue(object_id, local_vars, normal_parm_array[3].type,
      normal_parm_array[3].value);
   if (message_val.v.tag != TAG_MESSAGE)
   {
      // Handle message names passed as strings.
      if (message_val.v.tag == TAG_STRING)
      {
         message_val.v.data = GetIDByName(GetStringByID(message_val.v.data)->data);
         if (message_val.v.data == INVALID_ID)
         {
            bprintf("C_SendListMessageByClassBreak OBJECT %i can't use bad string message %i,\n",
               object_id, message_val.v.tag);
            return ret_val.int_val;
         }
      }
      else
      {
         bprintf("C_SendListMessageByClassBreak OBJECT %i can't send non-message %i,%i\n",
            object_id, message_val.v.tag, message_val.v.data);
         return ret_val.int_val;
      }
   }

   if (list_val.v.tag != TAG_LIST)
   {
      bprintf("C_SendListMessageByClassBreak OBJECT %i can't send to non-list %i, %i\n",
         object_id, list_val.v.tag, list_val.v.data);
      return ret_val.int_val;
   }

   // Separate functions handle each of the cases: objects in top list, objects
   // as first element of sublist, objects as nth element of sublist.
   if (pos_val.v.data == 0)
      ret_val.v.data = SendListMessageByClass(list_val.v.data, class_val.v.data, True,
      message_val.v.data, num_name_parms, name_parm_array);
   else if (pos_val.v.data == 1)
      ret_val.v.data = SendFirstListMessageByClass(list_val.v.data, class_val.v.data, True,
      message_val.v.data, num_name_parms, name_parm_array);
   else
      ret_val.v.data = SendNthListMessageByClass(list_val.v.data, pos_val.v.data,
      class_val.v.data, True, message_val.v.data, num_name_parms, name_parm_array);

   return ret_val.int_val;
}

int C_CreateObject(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type ret_val, class_val;

   RETRIEVEVALUECLASS(class_val, 0, NIL);

   ret_val.v.tag = TAG_OBJECT;
   ret_val.v.data = CreateObject(class_val.v.data, num_name_parms, name_parm_array);
   return ret_val.int_val;
}

// Look up the string given by val.  If found, return true and set *str and *len
// to the string value and length respectively.  function_name is the C function
// name used in reporting errors.
// If the string isn't found (including if val corresponds to NIL), false is returned.
bool LookupString(val_type val, const char *function_name, const char **str, int *len)
{
	string_node *snod;

	switch(val.v.tag)
	{
	case TAG_STRING :
		snod = GetStringByID(val.v.data);
		if (snod == NULL)
		{
			bprintf( "%s can't use invalid string %i,%i\n",
                  function_name, val.v.tag, val.v.data );
			return false;
		}
		*str = snod->data;
		break;
		
	case TAG_TEMP_STRING :
		snod = GetTempString();
		*str = snod->data;
		break;
		
	case TAG_RESOURCE :
      *str = GetResourceStrByLanguageID(val.v.data, ConfigInt(RESOURCE_LANGUAGE));
      if (*str == NULL)
		{
			bprintf( "%s can't use invalid resource %i as string\n",
                  function_name, val.v.data );
			return false;
		}
		break;
		
	case TAG_DEBUGSTR :
   {
      kod_statistics *kstat;
      class_node *c;
		
      kstat = GetKodStats();
		
      c = GetClassByID(kstat->interpreting_class);
      if (c == NULL)
      {
         bprintf("%s can't find class %i, can't get debug str\n",
                 function_name, kstat->interpreting_class);
         return false;
      }
      *str = GetClassDebugStr(c, val.v.data);
      break;
   }

   case TAG_NIL:
		bprintf( "%s can't use nil as string\n", function_name );
      return false;
   
	default :
		bprintf( "%s can't use with non-string thing %i,%i\n",
               function_name, val.v.tag, val.v.data );
		return false;
	}

   if (*str == NULL)
      return false;
   *len = strlen(*str);
   
   return true;
}


int C_StringEqual(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
   val_type s1_val, s2_val, ret_val;
   const char *s1 = NULL, *s2 = NULL;
   int len1, len2;
   resource_node *r1 = NULL, *r2 = NULL;
   Bool s1_resource = False, s2_resource = False;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = False;

   s1_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);
   s2_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[1].value);

   if (s1_val.v.tag == TAG_RESOURCE)
   {
      r1 = GetResourceByID(s1_val.v.data);
      if (r1 == NULL)
      {
         bprintf("C_StringEqual can't use invalid resource %i as string\n",
            s1_val.v.data);
         return ret_val.int_val;
      }
      s1_resource = True;
   }
   else
   {
      if (!LookupString(s1_val, "C_StringEqual", &s1, &len1))
         return NIL;
   }

   if (s2_val.v.tag == TAG_RESOURCE)
   {
      r2 = GetResourceByID(s2_val.v.data);
      if (r2 == NULL)
      {
         bprintf("C_StringEqual can't use invalid resource %i as string\n",
            s2_val.v.data);
         return ret_val.int_val;
      }
      s2_resource = True;
   }
   else
   {
      if (!LookupString(s2_val, "C_StringEqual", &s2, &len2))
         return NIL;
   }

   // If both are resources, just compare the English string (first array position).
   if (s1_resource && s2_resource)
   {
      s1 = r1->resource_val[0];
      if (s1 == NULL)
      {
         bprintf("C_StringEqual got NULL string resource 1");
         return ret_val.int_val;
      }
      s2 = r2->resource_val[0];
      if (s2 == NULL)
      {
         bprintf("C_StringEqual got NULL string resource 2");
         return ret_val.int_val;
      }

      len1 = strlen(s1);
      len2 = strlen(s2);
      ret_val.v.data = FuzzyBufferEqual(s1, len1, s2, len2);

      return ret_val.int_val;
   }

   // First string is resource, second isn't.
   if (s1_resource)
   {
      len2 = strlen(s2);
      for (int i = 0; i < MAX_LANGUAGE_ID; i++)
      {
         s1 = r1->resource_val[i];
         if (s1 == NULL)
         {
            if (i == 0)
            {
               bprintf("C_StringEqual got NULL string resource 1");
               return ret_val.int_val;
            }
            continue;
         }

         len1 = strlen(s1);
         if (FuzzyBufferEqual(s1, len1, s2, len2))
         {
            ret_val.v.data = True;
            return ret_val.int_val;
         }
      }
      return ret_val.int_val;
   }

   // Second string is resource, first isn't.
   if (s2_resource)
   {
      len1 = strlen(s1);
      for (int i = 0; i < MAX_LANGUAGE_ID; i++)
      {
         s2 = r2->resource_val[i];
         if (s2 == NULL)
         {
            if (i == 0)
            {
               bprintf("C_StringEqual got NULL string resource 2");
               return ret_val.int_val;
            }
            continue;
         }

         len2 = strlen(s2);
         if (FuzzyBufferEqual(s1, len1, s2, len2))
         {
            ret_val.v.data = True;
            return ret_val.int_val;
         }
      }
      return ret_val.int_val;
   }

   // Neither strings are resources.
   len1 = strlen(s1);
   len2 = strlen(s2);
   ret_val.v.data = FuzzyBufferEqual(s1, len1, s2, len2);
   return ret_val.int_val;
}

void FuzzyCollapseString(char* pTarget, const char* pSource, int len)
{
	if (!pTarget || !pSource || len <= 0)
	{
		*pTarget = '\0';
		return;
	}
	
	// skip over leading and trailing whitespace
	while (len && iswhite(*pSource)) { pSource++; len--; }
	while (len && iswhite(pSource[len-1])) { len--; }
	
	// copy the core string in uppercase
	while (len)
	{
		*pTarget++ = toupper(*pSource++);
		len--;
	}
	
	*pTarget = '\0';
}

bool FuzzyBufferEqual(const char *s1,int len1,const char *s2,int len2)
{
	if (!s1 || !s2 || len1 <= 0 || len2 <= 0)
		return false;
	
	// skip over leading whitespace
	while (len1 && iswhite(*s1)) { s1++; len1--; }
	while (len2 && iswhite(*s2)) { s2++; len2--; }
	
	// cut off trailing whitespace
	while (len1 && iswhite(s1[len1-1])) { len1--; }
	while (len2 && iswhite(s2[len2-1])) { len2--; }
	
	// empty strings can't match anything
	if (!len1 || !len2)
		return false;
	
	// walk the strings until we find a mismatch or an end
	while (len1 && len2 && toupper(*s1) == toupper(*s2))
	{
		s1++;
		s2++;
		len1--;
		len2--;
	}
	
	// we matched only if we finished both strings at the same time
	return (len1 == 0 && len2 == 0);
}

//	Blakod parameters; string0, string1, string2
//	Substitute first occurrence of string1 in string0 with string2
//	Returns 1 if substituted, 0 if not found, NIL if error
int C_StringSubstitute(int object_id,local_var_type *local_vars,
                       int num_normal_parms,parm_node normal_parm_array[],
                       int num_name_parms,parm_node name_parm_array[])
{
   val_type s0_val; // TAG_STRING or temp string.
   val_type s1_val; // Value we're subbing out.
   val_type s2_val; // Replacement string.
   val_type ret_val;
   string_node *snod0; // String we replace into.
   string_node *snod1; // String we're subbing out.
   char buf0[LEN_MAX_CLIENT_MSG + 1]; // Array for s0 (used for string we're modifying).
   char buf1[LEN_MAX_CLIENT_MSG + 1]; // Array for s1
   char *s0; // Pointer for buf0 (used for string we're modifying).
   const char *s1; // Pointer for string to remove.
   const char *s2; // Pointer for string to add.
   const char *subspot; // Pointer to the position in s0 to add substitute str.
   int len1; // Length of string we're subbing out.
   int len2; // Length of string we're adding.
   int new_len; // Length of the final string.

   s0 = buf0;
   s1 = buf1;
   s2 = subspot = NULL;

   s0_val = RetrieveValue( object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   if (s0_val.v.tag == TAG_STRING)
      snod0 = GetStringByID(s0_val.v.data);
   else if (s0_val.v.tag == TAG_TEMP_STRING)
      snod0 = GetTempString();
   else
      snod0 = NULL;

   if (!snod0)
   {
      bprintf("C_StringSub can't modify first argument non-string %i,%i\n",
         s0_val.v.tag, s0_val.v.data);
      return NIL;
   }

   s1_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[1].value);

   switch(s1_val.v.tag)
   {
   case TAG_STRING :
      snod1 = GetStringByID(s1_val.v.data);
      if (!snod1)
      {
         bprintf( "C_StringSub can't sub for invalid string %i,%i\n",
            s1_val.v.tag, s1_val.v.data );
         return NIL;
      }

      // Make a zero-terminated scratch copy of string1.
      len1 = snod1->len_data;
      memcpy(buf1, snod1->data, len1);
      buf1[len1] = 0;
      break;

   case TAG_TEMP_STRING :
      snod1 = GetTempString();

      // Make a zero-terminated scratch copy of string1.
      len1 = snod1->len_data;
      memcpy(buf1, snod1->data, len1);
      buf1[len1] = 0;
      break;

   case TAG_RESOURCE :
      s1 = GetResourceStrByLanguageID(s1_val.v.data, ConfigInt(RESOURCE_LANGUAGE));
      if (!s1)
      {
         bprintf( "C_StringSub can't sub for invalid resource %i\n", s1_val.v.data );
         return NIL;
      }
      len1 = strlen(s1);
      break;

   case TAG_DEBUGSTR :
      kod_statistics *kstat;
      class_node *c;

      kstat = GetKodStats();

      c = GetClassByID(kstat->interpreting_class);
      if (c == NULL)
      {
         bprintf("C_StringSub can't find class %i, can't get debug str\n",
            kstat->interpreting_class);
         return NIL;
      }
      s1 = GetClassDebugStr(c,s1_val.v.data);
      len1 = 0;
      if (s1)
         len1 = strlen(s1);
      break;

   case TAG_NIL :
      bprintf( "C_StringSub can't sub for nil\n" );
      return NIL;

   default :
      bprintf( "C_StringSub can't sub for non-string thing %i,%i\n",
         s1_val.v.tag, s1_val.v.data );
      return NIL;
   }

   if (len1 < 1 || len1 > LEN_MAX_CLIENT_MSG)
   {
      bprintf( "C_StringSub can't sub for null string %i,%i\n",
         s1_val.v.tag, s1_val.v.data );
      return NIL;
   }

   s2_val = RetrieveValue( object_id, local_vars, normal_parm_array[2].type,
      normal_parm_array[2].value );

   if (!LookupString(s2_val, "C_StringSub", &s2, &len2))
      return NIL;

   new_len = snod0->len_data - len1 + len2;
   if (new_len > LEN_MAX_CLIENT_MSG)
   {
      bprintf("C_StringSub can't sub, string too long.");
      return NIL;
   }

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = 0;

   // Make a zero-terminated scratch copy of string0 so we can use stristr
   // to find the location in s0 where s1 starts. If we don't find it,
   // stristr will be NULL.
   memcpy(s0, snod0->data, snod0->len_data);
   s0[snod0->len_data] = 0;
   subspot = stristr(s0, s1);

   if (subspot) // only substitute if string1 is found in string0
   {
      if (snod0 != GetTempString())
      {
         // free the old string0 and allocate a new (possibly longer) string0
         FreeMemory(MALLOC_ID_STRING, snod0->data, snod0->len_data);
         snod0->data = (char *)AllocateMemory(MALLOC_ID_STRING, new_len + 1);
      }

      // Copy the before and after pieces of the original string back from s0,
      // which is a duplicate of the original.

      // Copy the piece of original string before the sub point.
      memcpy(snod0->data, s0, subspot - s0);
      // Add the new part (string2).
      memcpy(snod0->data + (subspot - s0), s2, len2);
      // Copy the piece after the end of string1 (the subbed out string).
      memcpy(snod0->data + (subspot - s0) + len2, subspot + len1, new_len - (subspot - s0) - len2);
      snod0->len_data = new_len;
      snod0->data[snod0->len_data] = '\0';

      ret_val.v.data = 1;
   }

   return ret_val.int_val;
}

int C_StringContain(int object_id,local_var_type *local_vars,
               int num_normal_parms,parm_node normal_parm_array[],
               int num_name_parms,parm_node name_parm_array[])
{
   val_type s1_val, s2_val, ret_val;
   const char *s1 = NULL,*s2 = NULL;
   int len1, len2;
   resource_node *r1 = NULL, *r2 = NULL;
   Bool s1_resource = False, s2_resource = False;
   
   ret_val.v.tag = TAG_INT;
   ret_val.v.data = False;

   s1_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);
   s2_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
      normal_parm_array[1].value);

   if (s1_val.v.tag == TAG_RESOURCE)
   {
      r1 = GetResourceByID(s1_val.v.data);
      if (r1 == NULL)
      {
         bprintf("C_StringContain can't use invalid resource %i as string\n",
            s1_val.v.data);
         return ret_val.int_val;
      }
      s1_resource = True;
   }
   else
   {
      if (!LookupString(s1_val, "C_StringContain", &s1, &len1))
         return NIL;
   }

   if (s2_val.v.tag == TAG_RESOURCE)
   {
      r2 = GetResourceByID(s2_val.v.data);
      if (r2 == NULL)
      {
         bprintf("C_StringContain can't use invalid resource %i as string\n",
            s2_val.v.data);
         return ret_val.int_val;
      }
      s2_resource = True;
   }
   else
   {
      if (!LookupString(s2_val, "C_StringContain", &s2, &len2))
         return NIL;
   }

   // If both are resources, just compare the English string (first array position).
   if (s1_resource && s2_resource)
   {
      s1 = r1->resource_val[0];
      if (s1 == NULL)
      {
         bprintf("C_StringContain got NULL string resource 1");
         return ret_val.int_val;
      }
      s2 = r2->resource_val[0];
      if (s2 == NULL)
      {
         bprintf("C_StringContain got NULL string resource 2");
         return ret_val.int_val;
      }

      len1 = strlen(s1);
      len2 = strlen(s2);
      ret_val.v.data = FuzzyBufferContain(s1, len1, s2, len2);

      return ret_val.int_val;
   }

   // First string is resource, second isn't.
   if (s1_resource)
   {
      len2 = strlen(s2);
      ret_val.v.tag = TAG_INT;
      for (int i = 0; i < MAX_LANGUAGE_ID; i++)
      {
         s1 = r1->resource_val[i];
         if (s1 == NULL)
         {
            if (i == 0)
            {
               bprintf("C_StringContain got NULL string resource 1");
               return ret_val.int_val;
            }
            continue;
         }

         len1 = strlen(s1);
         if (FuzzyBufferContain(s1, len1, s2, len2))
         {
            ret_val.v.data = True;
            return ret_val.int_val;
         }
      }
      return ret_val.int_val;
   }

   // Second string is resource, first isn't.
   if (s2_resource)
   {
      len1 = strlen(s1);
      ret_val.v.tag = TAG_INT;
      for (int i = 0; i < MAX_LANGUAGE_ID; i++)
      {
         s2 = r2->resource_val[i];
         if (s2 == NULL)
         {
            if (i == 0)
            {
               bprintf("C_StringContain got NULL string resource 2");
               return ret_val.int_val;
            }
            continue;
         }

         len2 = strlen(s2);
         if (FuzzyBufferContain(s1, len1, s2, len2))
         {
            ret_val.v.data = True;
            return ret_val.int_val;
         }
      }
      return ret_val.int_val;
   }

   // Neither strings are resources.
   len1 = strlen(s1);
   len2 = strlen(s2);
   ret_val.v.data = FuzzyBufferContain(s1, len1, s2, len2);
   return ret_val.int_val;
}

/*
"   orc teeth" == "orc teeth"
" orc  teeth" == "orc teeth"
"orcteeth" == "orc teeth"
*/

// return true if s1 contains s2,
//	first converting to uppercase and squashing tabs and spaces to a single space
bool FuzzyBufferContain(const char *s1,int len_s1,const char *s2,int len_s2)
{
	if (!s1 || !s2 || len_s1 <= 0 || len_s2 <= 0)
		return false;
	
	FuzzyCollapseString(buf0, s1, len_s1);
	FuzzyCollapseString(buf1, s2, len_s2);
	
	return (NULL != strstr(buf0, buf1));
}

int C_SetResource(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type drsc_val,str_val;
	resource_node *r;
	string_node *snod;
	int new_len;
	char *new_str;

   RETRIEVEVALUERESOURCE(drsc_val, 0, NIL);

	if (drsc_val.v.data < MIN_DYNAMIC_RSC)
	{
		bprintf("C_SetResource can't set non-dynamic resource %i,%i\n",
			drsc_val.v.tag,drsc_val.v.data);
		return NIL;
	}
	
	str_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	switch (str_val.v.tag)
	{
	case TAG_TEMP_STRING :
		snod = GetTempString();
		new_len = snod->len_data;
		new_str = snod->data;
		break;
		
	case TAG_RESOURCE :
		{
			r = GetResourceByID(str_val.v.data);
			if (r == NULL)
			{
				bprintf("C_SetResource can't set from bad resource %i\n",
					str_val.v.data);
				return NIL;
			}
			new_len = strlen(r->resource_val[0]);
			new_str = r->resource_val[0];
			break;
		}
	case TAG_STRING :
		snod = GetStringByID(str_val.v.data);
		if (snod == NULL)
		{
			bprintf( "C_SetResource can't set from bad string %i\n",
				str_val.v.data);
			return NIL;
		}
		new_len = snod->len_data;
		new_str = snod->data;
		break;
	default :
		bprintf("C_SetResource can't set from non temp string %i,%i\n",
			str_val.v.tag,str_val.v.data);
		return NIL;
	}
	
	r = GetResourceByID(drsc_val.v.data);
	if (r == NULL)
	{
		eprintf("C_SetResource got dyna rsc number that doesn't exist\n");
		return NIL;
	}
	
	ChangeDynamicResource(r,new_str,new_len);
	
	return NIL;
}

int C_ParseString(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type parse_str_val,separator_str_val,callback_val,string_val;
	parm_node p[1];
	string_node *snod;
	const char *separators;
	kod_statistics *kstat;
	class_node *c;
	char *each_str;
	
	kstat = GetKodStats();
	
	if (kstat->interpreting_class == INVALID_CLASS)
	{
		eprintf("C_ParseString can't find current class\n");
		return NIL;
	}
	
	c = GetClassByID(kstat->interpreting_class);
	if (c == NULL)
	{
		eprintf("C_ParseString can't find class %i\n",kstat->interpreting_class);
		return NIL;
	}
	
	parse_str_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
	if (parse_str_val.v.tag != TAG_TEMP_STRING)
	{
		bprintf("C_ParseString can't parse non-temp string %i,%i\n",
			parse_str_val.v.tag,parse_str_val.v.data);
		return NIL;
	}
	
	snod = GetTempString();
	/* null terminate it to do strtok */
	snod->data[std::min(LEN_TEMP_STRING-1,snod->len_data)] = 0;
	
	separator_str_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	if (separator_str_val.v.tag != TAG_DEBUGSTR)
	{
		bprintf("C_ParseString can't use separator non-debugstr %i,%i\n",
			separator_str_val.v.tag,separator_str_val.v.data);
		return NIL;
	}
	separators = GetClassDebugStr(c,separator_str_val.v.data);
	
	callback_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
		normal_parm_array[2].value);
	if (callback_val.v.tag != TAG_MESSAGE)
	{
		bprintf("C_ParseString can't callback non-message %i,%i\n",
			callback_val.v.tag,callback_val.v.data);
		return NIL;
	}
	
	/* setup our parameter to callback */
	string_val.v.tag = TAG_TEMP_STRING;
	string_val.v.data = 0;
	
	p[0].type = CONSTANT;
	p[0].value = string_val.int_val;
	p[0].name_id = STRING_PARM;
	
	each_str = strtok(snod->data,separators);
	while (each_str != NULL)
	{
	/* move the parsed string to beginning of the temp string for kod's use.
	also, fake the length on it for kod's sake.  Doesn't matter to
		us because we null terminated the real string*/
		
		strcpy(snod->data,each_str);
		snod->len_data = strlen(snod->data);
		
		SendBlakodMessage(object_id,callback_val.v.data,1,p);
		
		each_str = strtok(NULL,separators);
	}
	return NIL;
}

int C_SetString(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type s1_val,s2_val;
   string_node *snod,*snod2;
   class_node *c;
   const char *str;

   s1_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);

   if (s1_val.v.tag != TAG_STRING)
   {
      /* If we're passed a NULL string, create one and use that. Allows
       * us to create and set a string with one call.*/
      if (s1_val.v.tag == TAG_NIL)
      {
         s1_val.v.tag = TAG_STRING;
         s1_val.v.data = CreateString("");
      }
      else
      {
         bprintf("C_SetString can't set non-string %i,%i\n",
            s1_val.v.tag,s1_val.v.data);
         return NIL;
      }
   }

   snod = GetStringByID(s1_val.v.data);
   if (snod == NULL)
   {
      bprintf("C_SetString can't set invalid string %i,%i\n",
         s1_val.v.tag,s1_val.v.data);
      return NIL;
   }

   s2_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
      normal_parm_array[1].value);
   switch (s2_val.v.tag)
   {
   case TAG_STRING :
      snod2 = GetStringByID( s2_val.v.data);
      if( snod2 == NULL )
      {
         bprintf( "C_SetString can't find string %i,%i\n",
            s2_val.v.tag, s2_val.v.data );
         return NIL;
      }
      //bprintf("SetString string%i<--string%i\n",s1_val.v.data,s2_val.v.data);
      SetString(snod,snod2->data,snod2->len_data);
      break;

   case TAG_TEMP_STRING :
      snod2 = GetTempString();
      //bprintf("SetString string%i<--tempstring\n",s1_val.v.data);
      SetString(snod,snod2->data,snod2->len_data);
      break;

   case TAG_RESOURCE :
      str = GetResourceStrByLanguageID(s2_val.v.data, ConfigInt(RESOURCE_LANGUAGE));
      if (str == NULL)
      {
         bprintf("C_SetString can't set from invalid resource %i\n",s2_val.v.data);
         return NIL;
      }
      //bprintf("SetString string%i<--resource%i\n",s1_val.v.data,s2_val.v.data);
      SetString(snod, (char*)str, strlen(str));
      break;

   case TAG_MESSAGE :
      str = GetNameByID(s2_val.v.data);
      if (str == NULL)
      {
         bprintf("C_SetString can't set from invalid message %i\n",s2_val.v.data);
         return NIL;
      }
      SetString(snod,GetNameByID(s2_val.v.data),strlen(GetNameByID(s2_val.v.data)));
      break;

   case TAG_CLASS :
      c = GetClassByID(s2_val.v.data);
      if (c == NULL)
      {
         bprintf("C_SetString can't set from invalid class %i\n", s2_val.v.data);
         return NIL;
      }

      if (c->class_name == NULL)
      {
         bprintf("C_SetString can't set from invalid class name %i\n",s2_val.v.data);
         return NIL;
      }

      SetString(snod, c->class_name, strlen(c->class_name));
      break;

   case TAG_DEBUGSTR :
      str = GetClassDebugStr(GetClassByID(GetKodStats()->interpreting_class),s2_val.v.data);
      if (str == NULL)
      {
         bprintf("C_SetString can't set from invalid debug string %i\n",s2_val.v.data);
         return NIL;
      }
      SetString(snod,(char*)str,strlen(str));
      break;

   case TAG_INT:
      char buf[21];
      snprintf(buf, 21, "%i", s2_val.v.data);
      SetString(snod, buf, strlen(buf));
      break;

   default :
      bprintf("C_SetString can't set from non-string thing %i,%i\n",
         s2_val.v.tag,s2_val.v.data);
      return NIL;
   }

   return s1_val.int_val;
}

int C_ClearTempString(int object_id,local_var_type *local_vars,
					  int num_normal_parms,parm_node normal_parm_array[],
					  int num_name_parms,parm_node name_parm_array[])
{
	val_type ret_val;
	
	ClearTempString();
	
	ret_val.v.tag = TAG_TEMP_STRING;
	ret_val.v.data = 0;		/* doesn't matter for TAG_TEMP_STRING */
	return ret_val.int_val;
}

int C_GetTempString(int object_id,local_var_type *local_vars,
					int num_normal_parms,parm_node normal_parm_array[],
					int num_name_parms,parm_node name_parm_array[])
{
	val_type ret_val;
	
	ret_val.v.tag = TAG_TEMP_STRING;
	ret_val.v.data = 0;		/* doesn't matter for TAG_TEMP_STRING */
	return ret_val.int_val;
}

int C_AppendTempString(int object_id,local_var_type *local_vars,
					   int num_normal_parms,parm_node normal_parm_array[],
					   int num_name_parms,parm_node name_parm_array[])
{
	val_type s_val;
	string_node *snod;
	
	s_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
	
	switch (s_val.v.tag)
	{
	case TAG_INT :
		AppendNumToTempString(s_val.v.data);
		break;
		
	case TAG_STRING :
		snod = GetStringByID( s_val.v.data);
		if(snod == NULL )
		{
			bprintf( "C_AppendTempString can't find string %i,%i\n", s_val.v.tag, s_val.v.data );
			return NIL;
		}
		AppendTempString(snod->data,snod->len_data);
		break;
		
	case TAG_TEMP_STRING :
		bprintf("C_AppendTempString attempting to append temp string to itself!\n");
		return NIL;
		
   case TAG_RESOURCE:
   {
      const char *pStrConst;
      pStrConst = GetResourceStrByLanguageID(s_val.v.data, ConfigInt(RESOURCE_LANGUAGE));
      if (pStrConst == NULL)
      {
         bprintf("C_AppendTempString can't set from invalid resource %i\n", s_val.v.data);
         return NIL;
      }
      AppendTempString(pStrConst, strlen(pStrConst));
   }
      break;
	case TAG_DEBUGSTR :
		{
			kod_statistics *kstat = GetKodStats();
			class_node *c = GetClassByID(kstat->interpreting_class);
			const char *pStrConst;
			int strLen = 0;
			
			if (c == NULL)
			{
				bprintf("C_AppendTempString can't find class %i, can't get debug str\n",kstat->interpreting_class);
				return NIL;
			}
			pStrConst = GetClassDebugStr(c,s_val.v.data);
			strLen = 0;
			if (pStrConst != NULL)
			{
				strLen = strlen(pStrConst);
				AppendTempString(pStrConst,strLen);
			}
			else
			{
				bprintf("C_AppendTempString: GetClassDebugStr returned NULL");
				return NIL;
			}
		}
		break;
		
	case TAG_NIL :
		bprintf("C_AppendTempString can't set from NIL\n");
		break;
		
	default :
		bprintf("C_AppendTempString can't set from non-string thing %i,%i\n",s_val.v.tag,s_val.v.data);
		return NIL;
	}
	return NIL;
}

int C_CreateString(int object_id,local_var_type *local_vars,
				   int num_normal_parms,parm_node normal_parm_array[],
				   int num_name_parms,parm_node name_parm_array[])
{
	val_type ret_val;
	
	ret_val.v.tag = TAG_STRING;
	ret_val.v.data = CreateString("");
	
	return ret_val.int_val;
}

int C_IsString(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type var_check;

   var_check = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
            normal_parm_array[0].value);

   return (var_check.v.tag == TAG_STRING) ? KOD_TRUE : KOD_FALSE;
}

int C_StringLength(int object_id,local_var_type *local_vars,
				   int num_normal_parms,parm_node normal_parm_array[],
				   int num_name_parms,parm_node name_parm_array[])
{
	val_type s1_val,ret_val;
	const char *s1;
	int len;
	
	s1_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
   if (!LookupString(s1_val, "C_StringLength", &s1, &len))
      return NIL;
   
	ret_val.v.tag = TAG_INT;
	ret_val.v.data = len;
	
	return ret_val.int_val;
}

int C_StringConsistsOf(int object_id,local_var_type *local_vars,
                       int num_normal_parms,parm_node normal_parm_array[],
                       int num_name_parms,parm_node name_parm_array[])
{
	val_type s1_val,s2_val,ret_val;
	const char *s1,*s2;
	int len1,len2;
	
	s1_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
   if (!LookupString(s1_val, "C_StringConsistsOf", &s1, &len1))
      return NIL;
	
	s2_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
   if (!LookupString(s2_val, "C_StringConsistsOf", &s2, &len2))
      return NIL;

   // See if all characters in s1 are from s2.
   bool all_found = true;
   for (int i = 0; i < len1; ++i)
   {
      if (strchr(s2, s1[i]) == NULL)
      {
         all_found = false;
         break;
      }
   }
   
	ret_val.v.tag = TAG_INT;
	ret_val.v.data = (int) all_found;
	
	return ret_val.int_val;
}

int C_CreateTimer(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type object_val,message_val,time_val,ret_val;
	object_node *o;
	
	o = GetObjectByID(object_id);
	if (o == NULL)
	{
		eprintf("C_CreateTimer can't find object %i, critical error\n",
         object_id);
		return NIL;
	}

   RETRIEVEVALUEOBJECT(object_val, 0, NIL);
   RETRIEVEVALUEMESSAGE(message_val, 1, NIL);

	time_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
		normal_parm_array[2].value);
	
	if (time_val.v.tag != TAG_INT || time_val.v.data < 0)
	{
		bprintf("C_CreateTimer can't create timer in negative int %i,%i milliseconds in obj:%i %s\n",
			time_val.v.tag,time_val.v.data, object_id, GetClassNameByObjectID(object_id));
		return NIL;
	}
	
	if (GetMessageByID(o->class_id,message_val.v.data,NULL) == NULL)
	{
		bprintf("C_CreateTimer can't create timer w/ message %i not for class %i in obj:%i %s\n",
			message_val.v.data,o->class_id, object_id, GetClassNameByObjectID(object_id));
		return NIL;
	}
	
	ret_val.v.tag = TAG_TIMER;
	ret_val.v.data = CreateTimer(o->object_id,message_val.v.data,time_val.v.data);
	/*   dprintf("create timer %i\n",ret_val.v.data); */
	
	return ret_val.int_val;
}

int C_DeleteTimer(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type timer_val,ret_val;
	
   RETRIEVEVALUETIMER(timer_val, 0, NIL);

	ret_val.v.tag = TAG_INT; /* really a boolean */
	ret_val.v.data = DeleteTimer(timer_val.v.data);
	
	return ret_val.int_val;
}

int C_GetTimeRemaining(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type timer_val, ret_val;
   timer_node *t;

   RETRIEVEVALUETIMER(timer_val, 0, NIL);

   t = GetTimerByID(timer_val.v.data);
   if (t == NULL)
   {
      bprintf("C_GetTimeRemaining can't find timer %i,%i in obj:%i %s\n",
         timer_val.v.tag, timer_val.v.data, object_id,
         GetClassNameByObjectID(object_id));
      return NIL;
   }

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = (int)(t->time - GetMilliCount());
   if (ret_val.v.data < 0)
      ret_val.v.data = 0;

   return ret_val.int_val;
}

int C_IsTimer(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type var_check;

   var_check = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   if (var_check.v.tag == TAG_NIL)
   {
      bprintf("C_IsTimer called with NIL timer by object %i %s",
         object_id,GetClassNameByObjectID(object_id));
      return KOD_FALSE;
   }

   return (var_check.v.tag == TAG_TIMER) ? KOD_TRUE : KOD_FALSE;
}

int C_LoadRoom(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type room_val;

   RETRIEVEVALUERESOURCE(room_val, 0, NIL);

   return LoadRoom(room_val.v.data);
}

/*
 * C_FreeRoom: Takes a room's room data (TAG_ROOM_DATA) and removes the
 *             room from the server's list of rooms. Frees the memory
 *             associated with the room.
 */
int C_FreeRoom(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type room_val;
   room_node *room;

   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   GETROOMBYID(room, room_val, NIL);

   UnloadRoom(room);

   return NIL;
}

int C_RoomData(int object_id,local_var_type *local_vars,
			   int num_normal_parms,parm_node normal_parm_array[],
			   int num_name_parms,parm_node name_parm_array[])
{
	val_type room_val,ret_val,rows,cols,security,rowshighres,colshighres;
	room_node *room;

   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   GETROOMBYID(room, room_val, NIL);
	
	rows.v.tag = TAG_INT;
	rows.v.data = room->data.rows;
	cols.v.tag = TAG_INT;
	cols.v.data = room->data.cols;
	security.v.tag = TAG_INT;
	security.v.data = room->data.security;
	rowshighres.v.tag = TAG_INT;
	rowshighres.v.data = room->data.rowshighres;
	colshighres.v.tag = TAG_INT;
	colshighres.v.data = room->data.colshighres;

	ret_val.int_val = NIL;
	
	ret_val.v.data = Cons(colshighres,ret_val);
	ret_val.v.tag = TAG_LIST;
	
	ret_val.v.data = Cons(rowshighres,ret_val);
	ret_val.v.tag = TAG_LIST;
	
	ret_val.v.data = Cons(security,ret_val);
	ret_val.v.tag = TAG_LIST;
	
	ret_val.v.data = Cons(cols,ret_val);
	ret_val.v.tag = TAG_LIST;
	
	ret_val.v.data = Cons(rows,ret_val);
	ret_val.v.tag = TAG_LIST;
	
	return ret_val.int_val;
}

int C_GetLocationInfoBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type room_val, queryflags, row, col, finerow, finecol;
	val_type returnflags, floorheight, floorheightwd, ceilingheight, serverid;
	room_node *r;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(queryflags, 1, KOD_FALSE);
   RETRIEVEVALUEINT(row, 2, KOD_FALSE);
   RETRIEVEVALUEINT(col, 3, KOD_FALSE);
   RETRIEVEVALUEINT(finerow, 4, KOD_FALSE);
   RETRIEVEVALUEINT(finecol, 5, KOD_FALSE);
   // local 'out' vars
   RETRIEVEVALUEINT(returnflags, 6, KOD_FALSE);
   RETRIEVEVALUEINT(floorheight, 7, KOD_FALSE);
   RETRIEVEVALUEINT(floorheightwd, 8, KOD_FALSE);
   RETRIEVEVALUEINT(ceilingheight, 9, KOD_FALSE);
   RETRIEVEVALUEINT(serverid, 10, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

	V2 p;
	p.X = GRIDCOORDTOROO(col.v.data, finecol.v.data);
	p.Y = GRIDCOORDTOROO(row.v.data, finerow.v.data);

	// params of query
	unsigned int qflags = (unsigned int)queryflags.v.data;
	unsigned int rflags;
	float heightF, heightFWD, heightC;
	BspLeaf* leaf = NULL;
	
	// query
	bool ok = BSPGetLocationInfo(&r->data, &p, qflags, &rflags, &heightF, &heightFWD, &heightC, &leaf);

	if (ok)
	{
		// set output vars
		local_vars->locals[returnflags.v.data].v.tag = TAG_INT;
		local_vars->locals[returnflags.v.data].v.data = (int)rflags;

		local_vars->locals[floorheight.v.data].v.tag = TAG_INT;
		local_vars->locals[floorheight.v.data].v.data = FLOATTOKODINT(FINENESSROOTOKOD(heightF));

		local_vars->locals[floorheightwd.v.data].v.tag = TAG_INT;
		local_vars->locals[floorheightwd.v.data].v.data = FLOATTOKODINT(FINENESSROOTOKOD(heightFWD));

		local_vars->locals[ceilingheight.v.data].v.tag = TAG_INT;
		local_vars->locals[ceilingheight.v.data].v.data = FLOATTOKODINT(FINENESSROOTOKOD(heightC));

		if (leaf && leaf->Sector)
		{
			local_vars->locals[serverid.v.data].v.tag = TAG_INT;
			local_vars->locals[serverid.v.data].v.data = leaf->Sector->ServerID;
		}

		// mark succeeded
      return KOD_TRUE;
	}

	return KOD_FALSE;
}


int C_GetSectorHeightBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type ret_val, room_val, serverid, animation;
   room_node *r;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = 0;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(serverid, 1, KOD_FALSE);
   RETRIEVEVALUEINT(animation, 2, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

   float height;
   bool is_floor = (animation.v.data == ANIMATE_FLOOR_LIFT);

   // query
   ret_val.v.data = (BSPGetSectorHeightByID(&r->data, serverid.v.data, is_floor, &height)) ?
      (int)FINENESSROOTOKOD(height) : 0;

   return ret_val.int_val;
}

int C_SetRoomDepthOverrideBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type ret_val, room_val, flags, depth1, depth2, depth3;
   room_node *r;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = 0;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(flags, 1, KOD_FALSE);
   RETRIEVEVALUEINT(depth1, 2, KOD_FALSE);
   RETRIEVEVALUEINT(depth2, 3, KOD_FALSE);
   RETRIEVEVALUEINT(depth3, 4, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

   // update flags and depth values
   r->data.DepthFlags = flags.v.data;
   r->data.OverrideDepth1 = FINENESSKODTOROO(depth1.v.data);
   r->data.OverrideDepth2 = FINENESSKODTOROO(depth2.v.data);
   r->data.OverrideDepth3 = FINENESSKODTOROO(depth3.v.data);

   return ret_val.int_val;
}

int C_CalcUserMovementBucket(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type bucket, bucket_max, new_bucket, speed, delta;
   val_type row_start, col_start, finerow_start, finecol_start;
   val_type row_end, col_end, finerow_end, finecol_end;

   RETRIEVEVALUEINT(bucket, 0, KOD_FALSE);
   RETRIEVEVALUEINT(new_bucket, 1, KOD_FALSE);
   RETRIEVEVALUEINT(bucket_max, 2, KOD_FALSE);
   RETRIEVEVALUEINT(speed, 3, KOD_FALSE);
   RETRIEVEVALUEINT(delta, 4, KOD_FALSE);
   RETRIEVEVALUEINT(row_start, 5, KOD_FALSE);
   RETRIEVEVALUEINT(col_start, 6, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_start, 7, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_start, 8, KOD_FALSE);
   RETRIEVEVALUEINT(row_end, 9, KOD_FALSE);
   RETRIEVEVALUEINT(col_end, 10, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_end, 11, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_end, 12, KOD_FALSE);

   // Calculate the movesize for the claimed speed in this dt.
   // SPEED is defined as # of big squares per 10000ms
   // The unit here is fine upscaled by another *256 (same is done below on iDy, iDx)
   double iMaxMoveRun = ((double)speed.v.data * (double)KODFINENESS * 256.0 * (double)delta.v.data) / 10000.0;

   // Fill up the movement bucket with tokens for the squared distance
   // one could have travelled at maximum legal speed. Not bound at this
   // stage as a large valid move length could consume the extra tokens.
   double iBucket = (double)bucket.v.data + iMaxMoveRun;

   // Get move-deltas in FINENESS units and scale up further (*256) for precision.
   // Same was done with iMaxMoveRun above. Calculate the squared vector length from the deltas.
   double iDy = 256.0 * (double)(((row_end.v.data * KODFINENESS) + finerow_end.v.data) - ((row_start.v.data * KODFINENESS) + finerow_start.v.data));
   double iDx = 256.0 * (double)(((col_end.v.data * KODFINENESS) + finecol_end.v.data) - ((col_start.v.data * KODFINENESS) + finecol_start.v.data));
   double iMoveLength = sqrt((iDy * iDy) + (iDx * iDx));

   // This move would consume more tokens than we have left -> deny
   if (iBucket - iMoveLength <= 0)
   {
      // Set upper bound on how many tokens we can have.
      iBucket = MIN(iBucket, bucket_max.v.data);
      local_vars->locals[new_bucket.v.data].v.tag = TAG_INT;
      local_vars->locals[new_bucket.v.data].v.data = (int)iBucket;

      return KOD_FALSE;
   }

   // Subtract the tokens used in this move.
   iBucket -= iMoveLength;
   // Set upper bound on how many tokens we can have.
   iBucket = MIN(iBucket, bucket_max.v.data);
   local_vars->locals[new_bucket.v.data].v.tag = TAG_INT;
   local_vars->locals[new_bucket.v.data].v.data = (int)iBucket;

   return KOD_TRUE;
}

int C_IntersectLineCircle(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   // Center point.
   val_type row_point, col_point, finerow_point, finecol_point;
   // Line start point.
   val_type row_start, col_start, finerow_start, finecol_start;
   // Line end point.
   val_type row_end, col_end, finerow_end, finecol_end;
   // Radius to check within.
   val_type radius;

   RETRIEVEVALUEINT(row_point, 0, KOD_FALSE);
   RETRIEVEVALUEINT(col_point, 1, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_point, 2, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_point, 3, KOD_FALSE);
   RETRIEVEVALUEINT(row_start, 4, KOD_FALSE);
   RETRIEVEVALUEINT(col_start, 5, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_start, 6, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_start, 7, KOD_FALSE);
   RETRIEVEVALUEINT(row_end, 8, KOD_FALSE);
   RETRIEVEVALUEINT(col_end, 9, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_end, 10, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_end, 11, KOD_FALSE);
   RETRIEVEVALUEINT(radius, 12, KOD_FALSE);

   V2 center, start, end;
   center.X = GRIDCOORDTOROO(row_point.v.data, finerow_point.v.data);
   center.Y = GRIDCOORDTOROO(col_point.v.data, finecol_point.v.data);
   start.X  = GRIDCOORDTOROO(row_start.v.data, finerow_start.v.data);
   start.Y  = GRIDCOORDTOROO(col_start.v.data, finecol_start.v.data);
   end.X    = GRIDCOORDTOROO(row_end.v.data, finerow_end.v.data);
   end.Y    = GRIDCOORDTOROO(col_end.v.data, finecol_end.v.data);

   bool retval = IntersectLineCircle(&center, FINENESSKODTOROO(radius.v.data), &start, &end);
   return retval ? KOD_TRUE : KOD_FALSE;
}

int C_CanMoveInRoomBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type ret_val, room_val;
	val_type row_source, col_source, finerow_source, finecol_source, height_source;
	val_type row_dest, col_dest, finerow_dest, finecol_dest;
	val_type speed, objectid, move_flags;
	room_node *r;
	bool is_player, move_outside_bsp, ignore_blockers;

	ret_val.v.tag = TAG_INT;
	ret_val.v.data = false;

   RETRIEVEVALUEROOMDATA(room_val, 0, ret_val.int_val);
   RETRIEVEVALUEINT(row_source, 1, ret_val.int_val);
   RETRIEVEVALUEINT(col_source, 2, ret_val.int_val);
   RETRIEVEVALUEINT(finerow_source, 3, ret_val.int_val);
   RETRIEVEVALUEINT(finecol_source, 4, ret_val.int_val);
   RETRIEVEVALUEINT(height_source, 5, ret_val.int_val);
   RETRIEVEVALUEINT(row_dest, 6, ret_val.int_val);
   RETRIEVEVALUEINT(col_dest, 7, ret_val.int_val);
   RETRIEVEVALUEINT(finerow_dest, 8, ret_val.int_val);
   RETRIEVEVALUEINT(finecol_dest, 9, ret_val.int_val);
   RETRIEVEVALUEINT(speed, 10, ret_val.int_val);
   RETRIEVEVALUEOBJECT(objectid, 11, ret_val.int_val);
   RETRIEVEVALUEINT(move_flags, 12, ret_val.int_val);

   is_player = (move_flags.v.data & CANMOVE_IS_PLAYER) == CANMOVE_IS_PLAYER;
   move_outside_bsp = (move_flags.v.data & CANMOVE_MOVE_OUTSIDE_BSP) == CANMOVE_MOVE_OUTSIDE_BSP;
   ignore_blockers = (move_flags.v.data & CANMOVE_IGNORE_BLOCKERS) == CANMOVE_IGNORE_BLOCKERS;

   GETROOMBYID(r, room_val, KOD_FALSE);

	V2 s;
	s.X = GRIDCOORDTOROO(col_source.v.data, finecol_source.v.data);
	s.Y = GRIDCOORDTOROO(row_source.v.data, finerow_source.v.data);

	V2 e;
	e.X = GRIDCOORDTOROO(col_dest.v.data, finecol_dest.v.data);
	e.Y = GRIDCOORDTOROO(row_dest.v.data, finerow_dest.v.data);

   float height = FINENESSKODTOROO(height_source.v.data);
   float fSpeed = SPEEDKODTOROO(speed.v.data);

	Wall* blockWall;

   // todo: consider making "ignoreEndBlocker" a KOD param, "false" means any query using an object position as "END" will return FALSE
   ret_val.v.data = (is_player) ?
      BSPCanMoveInRoom3D<true>(&r->data, &s, &e, height, fSpeed, objectid.v.data, move_outside_bsp, ignore_blockers, false, &blockWall) :
      BSPCanMoveInRoom3D<false>(&r->data, &s, &e, height, fSpeed, objectid.v.data, move_outside_bsp, ignore_blockers, false, &blockWall);

#if DEBUGMOVE
	//dprintf("MOVE:%i R:%i S:(%1.2f/%1.2f) E:(%1.2f/%1.2f)", ret_val.v.data, r->data.roomdata_id, s.X, s.Y, e.X, e.Y);
#endif

	return ret_val.int_val;
}

int C_LineOfSightView(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type ret_val, angle_val;
   val_type row_source, col_source, finerow_source, finecol_source;
   val_type row_dest, col_dest, finerow_dest, finecol_dest;

   RETRIEVEVALUEINT(angle_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(row_source, 1, KOD_FALSE);
   RETRIEVEVALUEINT(col_source, 2, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_source, 3, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_source, 4, KOD_FALSE);
   RETRIEVEVALUEINT(row_dest, 5, KOD_FALSE);
   RETRIEVEVALUEINT(col_dest, 6, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_dest, 7, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_dest, 8, KOD_FALSE);

   V2 source;
   source.X = GRIDCOORDTOROO(col_source.v.data, finecol_source.v.data);
   source.Y = GRIDCOORDTOROO(row_source.v.data, finerow_source.v.data);

   V2 target;
   target.X = GRIDCOORDTOROO(col_dest.v.data, finecol_dest.v.data);
   target.Y = GRIDCOORDTOROO(row_dest.v.data, finerow_dest.v.data);

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = BSPLineOfSightView(&source, &target, angle_val.v.data);

   return ret_val.int_val;
}

int C_LineOfSightBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type ret_val, room_val;
	val_type row_source, col_source, finerow_source, finecol_source, height_source;
	val_type row_dest, col_dest, finerow_dest, finecol_dest, height_dest;
	room_node *r;
 
	ret_val.v.tag = TAG_INT;
	ret_val.v.data = false;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(row_source, 1, KOD_FALSE);
   RETRIEVEVALUEINT(col_source, 2, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_source, 3, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_source, 4, KOD_FALSE);
   RETRIEVEVALUEINT(height_source, 5, KOD_FALSE);
   RETRIEVEVALUEINT(row_dest, 6, KOD_FALSE);
   RETRIEVEVALUEINT(col_dest, 7, KOD_FALSE);
   RETRIEVEVALUEINT(finerow_dest, 8, KOD_FALSE);
   RETRIEVEVALUEINT(finecol_dest, 9, KOD_FALSE);
   RETRIEVEVALUEINT(height_dest, 10, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

   V3 s;
   s.X = GRIDCOORDTOROO(col_source.v.data, finecol_source.v.data);
   s.Y = GRIDCOORDTOROO(row_source.v.data, finerow_source.v.data);
   s.Z = FINENESSKODTOROO(height_source.v.data);

   V3 e;
   e.X = GRIDCOORDTOROO(col_dest.v.data, finecol_dest.v.data);
   e.Y = GRIDCOORDTOROO(row_dest.v.data, finerow_dest.v.data);
   e.Z = FINENESSKODTOROO(height_dest.v.data);

	ret_val.v.data = BSPLineOfSight(&r->data, &s, &e);

#if DEBUGLOS
	dprintf("LOS:%i S:(%1.2f/%1.2f/%1.2f) E:(%1.2f/%1.2f/%1.2f)", ret_val.v.data, s.X, s.Y, s.Z, e.X, e.Y, e.Z);
#endif

	return ret_val.int_val;
}

int C_ChangeTextureBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type room_val, server_id, new_texnum, flags;
   room_node *r;

   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   RETRIEVEVALUEINT(server_id, 1, NIL);
   RETRIEVEVALUEINT(new_texnum, 2, NIL);
   RETRIEVEVALUEINT(flags, 3, NIL);
   GETROOMBYID(r, room_val, NIL);

   BSPChangeTexture(&r->data, (unsigned short)server_id.v.data,
      (unsigned short)new_texnum.v.data, flags.v.data);

   return NIL;
}

// C_ChangeSectorFlagBSP: Allows changing sector flags from kod. Also allows
//    resetting a flag to a default value. Only some flags can reasonably be
//    changed from kod, i.e. depth and nomove status.
int C_ChangeSectorFlagBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type room_val, server_id, change_flag;
   room_node *r;


   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   RETRIEVEVALUEINT(server_id, 1, NIL);
   RETRIEVEVALUEINT(change_flag, 2, NIL);
   GETROOMBYID(r, room_val, NIL);

   BSPChangeSectorFlag(&r->data, (unsigned int)server_id.v.data,
      (unsigned int)change_flag.v.data);

   return NIL;
}

int C_MoveSectorBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type room_val, server_id, animation, height, speed;
   room_node *r;

   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   RETRIEVEVALUEINT(server_id, 1, NIL);
   RETRIEVEVALUEINT(animation, 2, NIL);
   RETRIEVEVALUEINT(height, 3, NIL);
   RETRIEVEVALUEINT(speed, 4, NIL);
   GETROOMBYID(r, room_val, NIL);

   bool is_floor = (animation.v.data == ANIMATE_FLOOR_LIFT);
   float fheight = FINENESSKODTOROO((float)height.v.data);
   float fspeed = 0.0f; // todo, but always instant anyways atm

   BSPMoveSector(&r->data, (unsigned int)server_id.v.data, is_floor, fheight, fspeed);

   return NIL;
}

int C_BlockerAddBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type ret_val, room_val, obj_val;
	val_type row, col, finerow, finecol;
	room_node *r;

	ret_val.v.tag = TAG_INT;
	ret_val.v.data = false;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEOBJECT(obj_val, 1, KOD_FALSE);
   RETRIEVEVALUEINT(row, 2, KOD_FALSE);
   RETRIEVEVALUEINT(col, 3, KOD_FALSE);
   RETRIEVEVALUEINT(finerow, 4, KOD_FALSE);
   RETRIEVEVALUEINT(finecol, 5, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

	V2 p;
	p.X = GRIDCOORDTOROO(col.v.data, finecol.v.data);
	p.Y = GRIDCOORDTOROO(row.v.data, finerow.v.data);

	// query
	ret_val.v.data = BSPBlockerAdd(&r->data, obj_val.v.data, &p);

	return ret_val.int_val;
}

int C_BlockerMoveBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type ret_val, room_val, obj_val;
	val_type row, col, finerow, finecol;
	room_node *r;

	ret_val.v.tag = TAG_INT;
	ret_val.v.data = false;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEOBJECT(obj_val, 1, KOD_FALSE);
   RETRIEVEVALUEINT(row, 2, KOD_FALSE);
   RETRIEVEVALUEINT(col, 3, KOD_FALSE);
   RETRIEVEVALUEINT(finerow, 4, KOD_FALSE);
   RETRIEVEVALUEINT(finecol, 5, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

	V2 p;
	p.X = GRIDCOORDTOROO(col.v.data, finecol.v.data);
	p.Y = GRIDCOORDTOROO(row.v.data, finerow.v.data);

	// query
	ret_val.v.data = BSPBlockerMove(&r->data, obj_val.v.data, &p);

	return ret_val.int_val;
}

int C_BlockerRemoveBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type ret_val, room_val, obj_val;
	room_node *r;

	ret_val.v.tag = TAG_INT;
	ret_val.v.data = false;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEOBJECT(obj_val, 1, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

	// query
	ret_val.v.data = BSPBlockerRemove(&r->data, obj_val.v.data);

	return ret_val.int_val;
}

int C_BlockerClearBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type room_val;
   room_node *r;

   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   GETROOMBYID(r, room_val, NIL);

   // query
   BSPBlockerClear(&r->data);

   return NIL;
}

int C_GetRandomPointBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type room_val, maxattempts_val, unblockedradius_val;
	val_type row, col, finerow, finecol;
	room_node *r;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(maxattempts_val, 1, KOD_FALSE);
   RETRIEVEVALUEINT(unblockedradius_val, 2, KOD_FALSE);
   RETRIEVEVALUEINT(row, 3, KOD_FALSE);
   RETRIEVEVALUEINT(col, 4, KOD_FALSE);
   RETRIEVEVALUEINT(finerow, 5, KOD_FALSE);
   RETRIEVEVALUEINT(finecol, 6, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

	V2 p;
	bool ok = BSPGetRandomPoint(&r->data, maxattempts_val.v.data, FINENESSKODTOROO((float)unblockedradius_val.v.data), &p);

	if (ok)
	{
		// set output vars
		local_vars->locals[finecol.v.data].v.tag = TAG_INT;
		local_vars->locals[finecol.v.data].v.data = ROOCOORDTOGRIDFINE(p.X);

		local_vars->locals[finerow.v.data].v.tag = TAG_INT;
		local_vars->locals[finerow.v.data].v.data = ROOCOORDTOGRIDFINE(p.Y);

		local_vars->locals[col.v.data].v.tag = TAG_INT;
		local_vars->locals[col.v.data].v.data = ROOCOORDTOGRIDBIG(p.X);

		local_vars->locals[row.v.data].v.tag = TAG_INT;
		local_vars->locals[row.v.data].v.data = ROOCOORDTOGRIDBIG(p.Y);

		// mark succeeded
      return KOD_TRUE;
	}

	return KOD_FALSE;
}

int C_GetRandomMoveDestBSP(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type room_val, maxattempts_val, mindistance_val, maxdistance_val;
   val_type row, col, finerow, finecol, objrow, objcol, objfinerow, objfinecol;
   room_node *r;

   RETRIEVEVALUEROOMDATA(room_val, 0, KOD_FALSE);
   RETRIEVEVALUEINT(maxattempts_val, 1, KOD_FALSE);
   RETRIEVEVALUEINT(mindistance_val, 2, KOD_FALSE);
   RETRIEVEVALUEINT(maxdistance_val, 3, KOD_FALSE);
   RETRIEVEVALUEINT(objrow, 4, KOD_FALSE);
   RETRIEVEVALUEINT(objcol, 5, KOD_FALSE);
   RETRIEVEVALUEINT(objfinerow, 6, KOD_FALSE);
   RETRIEVEVALUEINT(objfinecol, 7, KOD_FALSE);
   RETRIEVEVALUEINT(row, 8, KOD_FALSE);
   RETRIEVEVALUEINT(col, 9, KOD_FALSE);
   RETRIEVEVALUEINT(finerow, 10, KOD_FALSE);
   RETRIEVEVALUEINT(finecol, 11, KOD_FALSE);
   GETROOMBYID(r, room_val, KOD_FALSE);

   float mindist = FINENESSKODTOROO((float)mindistance_val.v.data);
   float maxdist = FINENESSKODTOROO((float)maxdistance_val.v.data);

   V2 s;
   s.X = GRIDCOORDTOROO(objcol.v.data, objfinecol.v.data);
   s.Y = GRIDCOORDTOROO(objrow.v.data, objfinerow.v.data);

   V2 e;
   bool ok = BSPGetRandomMoveDest(&r->data, maxattempts_val.v.data, mindist, maxdist, &s, &e);

   if (ok)
   {
      // set output vars
      local_vars->locals[finecol.v.data].v.tag = TAG_INT;
      local_vars->locals[finecol.v.data].v.data = ROOCOORDTOGRIDFINE(e.X);

      local_vars->locals[finerow.v.data].v.tag = TAG_INT;
      local_vars->locals[finerow.v.data].v.data = ROOCOORDTOGRIDFINE(e.Y);

      local_vars->locals[col.v.data].v.tag = TAG_INT;
      local_vars->locals[col.v.data].v.data = ROOCOORDTOGRIDBIG(e.X);

      local_vars->locals[row.v.data].v.tag = TAG_INT;
      local_vars->locals[row.v.data].v.data = ROOCOORDTOGRIDBIG(e.Y);

      // mark succeeded
      return KOD_TRUE;
   }

   return KOD_FALSE;
}

int C_GetStepTowardsBSP(int object_id, local_var_type *local_vars,
	int num_normal_parms, parm_node normal_parm_array[],
	int num_name_parms, parm_node name_parm_array[])
{
	val_type ret_val, room_val;
	val_type row_source, col_source, finerow_source, finecol_source, height_source;
	val_type row_dest, col_dest, finerow_dest, finecol_dest;
	val_type speed, state_flags, objectid;
	room_node *r;

	// in case it fails
	ret_val.int_val = NIL;

   RETRIEVEVALUEROOMDATA(room_val, 0, NIL);
   RETRIEVEVALUEINT(row_source, 1, NIL);
   RETRIEVEVALUEINT(col_source, 2, NIL);
   RETRIEVEVALUEINT(finerow_source, 3, NIL);
   RETRIEVEVALUEINT(finecol_source, 4, NIL);
   RETRIEVEVALUEINT(height_source, 5, NIL);
   RETRIEVEVALUEINT(row_dest, 6, NIL);
   RETRIEVEVALUEINT(col_dest, 7, NIL);
   RETRIEVEVALUEINT(finerow_dest, 8, NIL);
   RETRIEVEVALUEINT(finecol_dest, 9, NIL);
   RETRIEVEVALUEINT(speed, 10, NIL);
   RETRIEVEVALUEINT(state_flags, 11, NIL);
   RETRIEVEVALUEOBJECT(objectid, 12, NIL);
   GETROOMBYID(r, room_val, NIL);

	V2 s;
	s.X = GRIDCOORDTOROO(col_source.v.data, finecol_source.v.data);
	s.Y = GRIDCOORDTOROO(row_source.v.data, finerow_source.v.data);

	V2 e;
   e.X = GRIDCOORDTOROO(local_vars->locals[col_dest.v.data].v.data, local_vars->locals[finecol_dest.v.data].v.data);
   e.Y = GRIDCOORDTOROO(local_vars->locals[row_dest.v.data].v.data, local_vars->locals[finerow_dest.v.data].v.data);

   float height = FINENESSKODTOROO(height_source.v.data);
   float fSpeed = SPEEDKODTOROO(speed.v.data);

	V2 p;
	unsigned int flags = (unsigned int)state_flags.v.data;
	bool ok = BSPGetStepTowards(&r->data, &s, &e, &p, &flags, objectid.v.data, fSpeed, height);

	if (ok)
	{
		ret_val.v.tag = TAG_INT;
		ret_val.v.data = flags;

		local_vars->locals[finecol_dest.v.data].v.tag = TAG_INT;
		local_vars->locals[finecol_dest.v.data].v.data = ROOCOORDTOGRIDFINE(p.X);

		local_vars->locals[finerow_dest.v.data].v.tag = TAG_INT;
		local_vars->locals[finerow_dest.v.data].v.data = ROOCOORDTOGRIDFINE(p.Y);

		local_vars->locals[col_dest.v.data].v.tag = TAG_INT;
		local_vars->locals[col_dest.v.data].v.data = ROOCOORDTOGRIDBIG(p.X);

		local_vars->locals[row_dest.v.data].v.tag = TAG_INT;
		local_vars->locals[row_dest.v.data].v.data = ROOCOORDTOGRIDBIG(p.Y);
	}

	return ret_val.int_val;
}

/*
 * C_AppendListElem: takes a list and an item to be appended to the list,
 *    appends the item to the end of the list. Returns the original list
 *    with the item appended to the end.
 */
int C_AppendListElem(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type source_val, list_val, ret_val;
   
   source_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);
   list_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
      normal_parm_array[1].value);

   if (list_val.v.tag != TAG_LIST)
   {
      if (list_val.v.tag != TAG_NIL)
      {
         bprintf("C_AppendListElem can't add to non-list %i,%i in obj:%i %s\n",
            list_val.v.tag,list_val.v.data, object_id, GetClassNameByObjectID(object_id));
         return list_val.int_val;
      }
   }

   ret_val.v.tag = TAG_LIST;
   ret_val.v.data = AppendListElem(source_val,list_val);
   return ret_val.int_val;
}

int C_Cons(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type source_val,dest_val,ret_val;

   source_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);
   dest_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
      normal_parm_array[1].value);

   if (dest_val.v.tag != TAG_LIST)
   {
      if (dest_val.v.tag != TAG_NIL)
      {
         bprintf("C_Cons can't add to non-list %i,%i in obj:%i %s\n",
            dest_val.v.tag, dest_val.v.data, object_id,
            GetClassNameByObjectID(object_id));
         return dest_val.int_val;
      }
   }

   ret_val.v.tag = TAG_LIST;
   ret_val.v.data = Cons(source_val,dest_val);
   return ret_val.int_val;
}

int C_Length(int object_id,local_var_type *local_vars,
			 int num_normal_parms,parm_node normal_parm_array[],
			 int num_name_parms,parm_node name_parm_array[])
{
	val_type list_val,ret_val;
	
	list_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
		normal_parm_array[0].value);
	
	if (list_val.v.tag == TAG_NIL)
	{
		ret_val.v.tag = TAG_INT;
		ret_val.v.data = 0;
		return ret_val.int_val;
	}
	
	if (list_val.v.tag != TAG_LIST)
	{
		bprintf("C_Length can't take Length of a non-list %i,%i in obj:%i %s\n",
			list_val.v.tag,list_val.v.data, object_id, GetClassNameByObjectID(object_id));
		return NIL;
	}
	ret_val.v.tag = TAG_INT;
	ret_val.v.data = Length(list_val.v.data);
	return ret_val.int_val;
}

int C_Last(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val;

   RETRIEVEVALUELIST(list_val, 0, NIL);

   return Last(list_val.v.data);
}

int C_Nth(int object_id,local_var_type *local_vars,
		  int num_normal_parms,parm_node normal_parm_array[],
		  int num_name_parms,parm_node name_parm_array[])
{
	val_type n_val,list_val;
	
   RETRIEVEVALUELIST(list_val, 0, NIL);

	if (!IsListNodeByID(list_val.v.data))
	{
		bprintf("C_Nth can't take Nth of an invalid list %i,%i in obj:%i %s\n",
			list_val.v.tag,list_val.v.data, object_id, GetClassNameByObjectID(object_id));
		return NIL;
	}

   RETRIEVEVALUEINT(n_val, 1, NIL);

	return Nth(n_val.v.data,list_val.v.data);
}

/*
 * C_IsListMatch:  takes two lists, checks the values of each element
 *    in the lists and returns TRUE if the elements are identical, FALSE
 *    otherwise. Elements are identical if they have the same int_val
 *    (tag AND data type) except in the case of TAG_LIST, in which case
 *    the list contents must be identical but the list node number does not
 *    need to be.
 */
int C_IsListMatch(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type ret_val, list_one, list_two;

   RETRIEVEVALUELIST(list_one, 0, KOD_FALSE);
   RETRIEVEVALUELIST(list_two, 1, KOD_FALSE);

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = IsListMatch(list_one.v.data, list_two.v.data);
   return ret_val.int_val;
}

int C_List(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type temp, ret_val;

   if (num_normal_parms == 0)
      return NIL;

   ret_val.int_val = NIL;
   for (int i = num_normal_parms - 1; i >= 0; --i)
   {
      extern list_node *list_nodes;
      temp = RetrieveValue(object_id, local_vars, normal_parm_array[i].type,
         normal_parm_array[i].value);
      ret_val.v.data = Cons(temp, ret_val);
      ret_val.v.tag = TAG_LIST; /* do this AFTER the cons call or DIE */
   }
   return ret_val.int_val;
}

int C_IsList(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type var_check;

   var_check = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   return (var_check.v.tag == TAG_LIST || var_check.v.tag == TAG_NIL)
      ? KOD_TRUE : KOD_FALSE;
}

int C_SetFirst(int object_id,local_var_type *local_vars,
			   int num_normal_parms,parm_node normal_parm_array[],
			   int num_name_parms,parm_node name_parm_array[])
{
	val_type list_val,set_val;
	
   RETRIEVEVALUELIST(list_val, 0, NIL);
	
	set_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	
	return SetFirst(list_val.v.data,set_val);
}

int C_SetNth(int object_id,local_var_type *local_vars,
			 int num_normal_parms,parm_node normal_parm_array[],
			 int num_name_parms,parm_node name_parm_array[])
{
	val_type n_val,list_val,set_val;
	
   RETRIEVEVALUELIST(list_val, 0, NIL);
   RETRIEVEVALUEINT(n_val, 1, NIL);

	set_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
		normal_parm_array[2].value);
	
	return SetNth(n_val.v.data,list_val.v.data,set_val);
}

/*
 * C_InsertListElem:  takes a list, a list position and one piece of data, adds
 *    the data at the given position. If the list position is larger than the
 *    list, it is added to the end. If list position 0 is sent, just returns
 *    the initial list, otherwise returns the altered list.
 */
int C_InsertListElem(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type n_val,list_val,set_val, ret_val;

   list_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);
   if (list_val.v.tag != TAG_LIST)
   {
      if (list_val.v.tag != TAG_NIL)
      {
         bprintf("C_InsertListElem can't add elem to non-list %i,%i in obj:%i %s \n",
            list_val.v.tag,list_val.v.data, object_id, GetClassNameByObjectID(object_id));
         return list_val.int_val;
      }
   }

   RETRIEVEVALUEINT(n_val, 1, list_val.int_val);

   set_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
      normal_parm_array[2].value);

   ret_val.v.tag = TAG_LIST;

   // Handle the case where the new element should be in the first position.
   // Should have called Cons to do this. Cons also adds to $ lists.
   if (n_val.v.data == 1 || list_val.v.tag == TAG_NIL)
      ret_val.v.data = Cons(set_val,list_val);
   else
      ret_val.v.data = InsertListElem(n_val.v.data,list_val.v.data,set_val);

   return ret_val.int_val;
}

/*
 * C_SwapListElem: takes a list and two integers representing elements
 *                 in the list, swaps the data in the two elements.
 *                 Returns NIL.
 */
int C_SwapListElem(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, n_val, m_val;

   RETRIEVEVALUELIST(list_val, 0, NIL);
   RETRIEVEVALUEINT(n_val, 1, NIL);
   RETRIEVEVALUEINT(m_val, 2, NIL);

   if (n_val.v.data == 0 || m_val.v.data == 0)
   {
      bprintf("C_SwapListElem given invalid list element, elements are %i,%i in obj:%i %s\n",
         n_val.v.data, m_val.v.data, object_id, GetClassNameByObjectID(object_id));
      return NIL;
   }

   return SwapListElem(list_val.v.data,n_val.v.data,m_val.v.data);
}

int C_DelListElem(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type list_val,list_elem,ret_val;

   RETRIEVEVALUELIST(list_val, 0, NIL);

	list_elem = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
			     normal_parm_array[1].value);
	
	ret_val.int_val = DelListElem(list_val,list_elem);
	
	return ret_val.int_val;
}

int C_DelLastListElem(int object_id, local_var_type *local_vars,
                      int num_normal_parms, parm_node normal_parm_array[],
                      int num_name_parms, parm_node name_parm_array[])
{
   val_type list_val, ret_val;

   RETRIEVEVALUELIST(list_val, 0, NIL);

   ret_val.int_val = DelLastListElem(list_val);

   return ret_val.int_val;
}

int C_FindListElem(int object_id,local_var_type *local_vars,
               int num_normal_parms,parm_node normal_parm_array[],
               int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, list_elem, ret_val;

   list_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);

   if (list_val.v.tag == TAG_NIL)
      return KOD_FALSE;

   if (list_val.v.tag != TAG_LIST)
   {
      bprintf("C_FindListElem can't find elem in non-list %i,%i obj:%i %s\n",
         list_val.v.tag,list_val.v.data, object_id, GetClassNameByObjectID(object_id));
      return NIL;
   }

   list_elem = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
              normal_parm_array[1].value);

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = FindListElem(list_val,list_elem);

   return ret_val.int_val;
}

/*
 * C_GetAllListNodesByClass: takes a list, a position and a class. Checks each
 *                sub-list in the parent list and if the class is present at
 *                the position, adds it to a new list. Returns the new list or
 *                NIL.
 */
int C_GetAllListNodesByClass(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, class_val, pos_val;

   RETRIEVEVALUELISTNIL(list_val, 0, NIL);

   pos_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
               normal_parm_array[1].value);
   if (pos_val.v.tag != TAG_INT || pos_val.v.data < 1)
   {
      bprintf("C_GetAllListNodesByClass object %i can't use non-int position %i,%i\n",
         object_id, pos_val.v.tag, pos_val.v.data);
      return NIL;
   }

   RETRIEVEVALUECLASS(class_val, 2, NIL);

   return GetAllListNodesByClass(list_val.v.data, pos_val.v.data, class_val.v.data);
}

/*
 * C_GetListNode: takes a list, a position and an object. Checks each
 *                sub-list in the parent list and returns the first list node
 *                containing the object at that position. Returns NIL if the
 *                object wasn't found in any sub-lists.
 */
int C_GetListNode(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, list_elem, pos_val;

   RETRIEVEVALUELISTNIL(list_val, 0, NIL);

   pos_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
               normal_parm_array[1].value);
   if (pos_val.v.tag != TAG_INT || pos_val.v.data < 1)
   {
      bprintf("C_GetListNode can't use non-int position %i,%i in obj:%i %s\n",
         pos_val.v.tag, pos_val.v.data, object_id, GetClassNameByObjectID(object_id));
      return NIL;
   }

   list_elem = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
                  normal_parm_array[2].value);

   if (list_elem.v.tag == TAG_NIL)
   {
      bprintf("C_GetListNode can't find $ in list %i,%i, obj:%i %s\n",
         list_val.v.tag, list_val.v.data, object_id, GetClassNameByObjectID(object_id));
      return NIL;
   }

   return GetListNode(list_val, pos_val.v.data, list_elem);
}

/*
 * C_GetListElemByClass: takes a list and a class, returns the element of the
 *                        list with that class if found, NIL otherwise.
 */
int C_GetListElemByClass(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, class_val;

   RETRIEVEVALUELISTNIL(list_val, 0, NIL);
   RETRIEVEVALUECLASS(class_val, 1, NIL);

   return GetListElemByClass(list_val, class_val.v.data);
}

/*
 * C_ListCopy: takes a list, makes a copy and returns the copy.
 */
int C_ListCopy(int object_id,local_var_type *local_vars,
         int num_normal_parms,parm_node normal_parm_array[],
         int num_name_parms,parm_node name_parm_array[])
{
   val_type list_val, ret_val;

   RETRIEVEVALUELISTNIL(list_val, 0, NIL);

   ret_val.v.data = ListCopy(list_val.v.data);
   ret_val.v.tag = TAG_LIST;

   return ret_val.int_val;
}

int C_GetTime(int object_id,local_var_type *local_vars,
			  int num_normal_parms,parm_node normal_parm_array[],
			  int num_name_parms,parm_node name_parm_array[])
{
	val_type ret_val;
	
	ret_val.v.tag = TAG_INT;

    /*  We must subtract a number from the system time due to size
        limitations within the blakod.  Blakod uses 32 bit values,
        -4 bits for type and -1 bit for sign.  This leaves us with
        27 bits for value,  This only allows us to have 134M or so
        as a positive value.  Current system time is a bit larger
        than that.  So, we subtract off time to compensate.
    */

	ret_val.v.data = GetTime() - 1632000000L;    // Offset to Sept 2021
	
	return ret_val.int_val;
}

/*
 * C_GetUnixTimeString: Returns unix time as a kod string, to work around
 *    the kod integer limit issues.
 */
int C_GetUnixTimeString(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type ret_val;
   string_node *snod;
   char timeStr[15];

   ret_val.v.tag = TAG_STRING;
   ret_val.v.data = CreateString("");

   snod = GetStringByID(ret_val.v.data);
   if (snod == NULL)
   {
      bprintf("C_GetUnixTimeString can't set invalid string %i,%i\n",
         ret_val.v.tag, ret_val.v.data);
      return NIL;
   }

   // Make a string with unix time.
   int len = sprintf(timeStr, "%i", GetTime());

   if (len <= 0)
   {
      bprintf("C_GetUnixTimeString got invalid time from GetTime(), returning $.");

      return NIL;
   }

   // Make a blakod string using the string value of the unix time.
   SetString(snod, timeStr, len);

   return ret_val.int_val;
}

// Temporary C call used to fix instances of GetTime() with offset being used
// as a 'permanent' timestamp. As these break every 4 years when stored as int,
// they should instead be stored as strings. This function converts the old int
// to the new string.
int C_OldTimestampFix(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type ret_val, time_val;
   string_node *snod;
   char timeStr[15];

   time_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   if (time_val.v.tag != TAG_INT)
   {
      bprintf("C_OldTimestampFix can only set timestamp from TAG_INT, received %i,%i\n",
         time_val.v.tag, time_val.v.data);
      return NIL;
   }

   ret_val.v.tag = TAG_STRING;
   ret_val.v.data = CreateString("");

   snod = GetStringByID(ret_val.v.data);
   if (snod == NULL)
   {
      bprintf("C_OldTimestampFix can't set invalid string %i,%i\n",
         ret_val.v.tag, ret_val.v.data);
      return NIL;
   }

   // Make a string with unix time by adding the time offset to the old timestamp.
   int len = sprintf(timeStr, "%i", 1388534400 + time_val.v.data);

   if (len <= 0)
   {
      bprintf("C_OldTimestampFix got invalid time from GetTime(), returning $.");

      return NIL;
   }

   // Make a blakod string using the string value of the timestamp.
   SetString(snod, timeStr, len);

   return ret_val.int_val;
}

int C_GetTickCount(int object_id,local_var_type *local_vars,
			  int num_normal_parms,parm_node normal_parm_array[],
			  int num_name_parms,parm_node name_parm_array[])
{
	val_type ret_val;
	
	// GetMilliCount is from blakerv/time.c. 
	// Its return is in ms and with a precision of 1ms.
	// It also provides Windows & Linux implementations.
	UINT64 tick = GetMilliCount();
	
	// but tick is unsigned 64-bit integer
	// and blakserv integers are signed with only 28-bits
	// the high-bit is the sign at bit-index 27/31
	// recapitulate:
	// 0x00000000 = 0000 0000 0000 0000 0000 0000 0000 = 0
	// 0x07FFFFFF = 0111 1111 1111 1111 1111 1111 1111 = 134217727
	// 0x08000000 = 1000 0000 0000 0000 0000 0000 0000 = -134217728
	// 0x0FFFFFFF = 1111 1111 1111 1111 1111 1111 1111 = -1
	
	// convert:
	// 1) We grab the low 32-bits by casting to unsigned int (so next & can easily be done in 32-bit registers)
	// 2) We grab the value within the positive blakserv-integer mask by &
	// 3) This means our returned tick rolls over every 134217.728s (~37 hrs)
	// 4) Roll-Over means anything calculating the timespan of something before and after the roll-over
	//    will return a negative timespan (but only once).
	ret_val.v.tag = TAG_INT;
	ret_val.v.data = (int)((unsigned int)tick & MAX_KOD_INT);
	
	return ret_val.int_val;
}

/*
 * C_GetDateAndTime: Gets the date and time, places them into passed local vars.
 *    Gets local or UTC time depending on type requested by kod.
 */
int C_GetDateAndTime(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type type_val, year_val, month_val, day_val, hour_val, minute_val, second_val;

   RETRIEVEVALUEINT(type_val, 0, NIL);

   year_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[1].value);
   month_val = RetrieveValue(object_id, local_vars, normal_parm_array[1].type,
      normal_parm_array[2].value);
   day_val = RetrieveValue(object_id, local_vars, normal_parm_array[2].type,
      normal_parm_array[3].value);
   hour_val = RetrieveValue(object_id, local_vars, normal_parm_array[3].type,
      normal_parm_array[4].value);
   minute_val = RetrieveValue(object_id, local_vars, normal_parm_array[4].type,
      normal_parm_array[5].value);
   second_val = RetrieveValue(object_id, local_vars, normal_parm_array[5].type,
      normal_parm_array[6].value);

   time_t t = time(NULL);
   struct tm tm_time;
   
   if (type_val.v.data == BTIME_LOCAL)
      tm_time = *localtime(&t);
   else
      tm_time = *gmtime(&t);

   // Only set the local vars if we're passed an integer - allow leaving
   // these as null in the function call.
   if (year_val.v.tag == TAG_INT)
   {
      // tm_year is number of years after 1900.
      local_vars->locals[year_val.v.data].v.data = tm_time.tm_year + 1900;
      local_vars->locals[year_val.v.data].v.tag = TAG_INT;
   }
   if (month_val.v.tag == TAG_INT)
   {
      // tm_mon ranges from 0-11, 0 is Jan.
      local_vars->locals[month_val.v.data].v.data = tm_time.tm_mon + 1;
      local_vars->locals[month_val.v.data].v.tag = TAG_INT;
   }
   if (day_val.v.tag == TAG_INT)
   {
      // tm_mday ranges from 1-31.
      local_vars->locals[day_val.v.data].v.data = tm_time.tm_mday;
      local_vars->locals[day_val.v.data].v.tag = TAG_INT;
   }
   if (hour_val.v.tag == TAG_INT)
   {
      // tm_hour ranges from 0-23
      local_vars->locals[hour_val.v.data].v.data = tm_time.tm_hour;
      local_vars->locals[hour_val.v.data].v.tag = TAG_INT;
   }
   if (minute_val.v.tag == TAG_INT)
   {
      // tm_min ranges from 0-59.
      local_vars->locals[minute_val.v.data].v.data = tm_time.tm_min;
      local_vars->locals[minute_val.v.data].v.tag = TAG_INT;
   }
   if (second_val.v.tag == TAG_INT)
   {
      // tm_sec ranges from 0-60, due to leap seconds.
      local_vars->locals[second_val.v.data].v.data = tm_time.tm_sec;
      local_vars->locals[second_val.v.data].v.tag = TAG_INT;
   }

   return NIL;
}

int C_Random(int object_id,local_var_type *local_vars,
			 int num_normal_parms,parm_node normal_parm_array[],
			 int num_name_parms,parm_node name_parm_array[])
{
	val_type low_bound,high_bound;
	val_type ret_val;
	int randomValue;
	
	low_bound = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
			     normal_parm_array[0].value);
	high_bound = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	if (low_bound.v.tag != TAG_INT || high_bound.v.tag != TAG_INT)
	{
		bprintf("C_Random got an invalid boundary %i,%i or %i,%i in obj:%i %s\n",
			low_bound.v.tag,low_bound.v.data,high_bound.v.tag,
			high_bound.v.data, object_id, GetClassNameByObjectID(object_id));
		return NIL;
	}
	if (low_bound.v.data > high_bound.v.data)
	{
		bprintf("C_Random got low > high boundary %i and %i in obj:%i %s\n",
			low_bound.v.data,high_bound.v.data, object_id,
         GetClassNameByObjectID(object_id));
		return NIL;
	}
	ret_val.v.tag = TAG_INT;
#if 0
	ret_val.v.data = low_bound.v.data + rand() % (high_bound.v.data -
		low_bound.v.data + 1);
#else
	// The rand() function returns number between 0 and 0x7fff (MAX_RAND)
	// we have to scale this to fit our range. -- call twice to fill all the bits
	// and mask to a 28 bit positive kod integer
	randomValue = MAX_KOD_INT & ((rand() << 15) + rand());
	ret_val.v.data = low_bound.v.data + randomValue % (high_bound.v.data - 
		low_bound.v.data + 1);
#endif
	return ret_val.int_val;
	
}

int C_Abs(int object_id,local_var_type *local_vars,
		  int num_normal_parms,parm_node normal_parm_array[],
		  int num_name_parms,parm_node name_parm_array[])
{
	val_type int_val,ret_val;

   RETRIEVEVALUEINT(int_val, 0, NIL);

	ret_val.v.tag = TAG_INT;
	if (int_val.v.data & (1 << 27))
		ret_val.v.data = -int_val.v.data;
	else
		ret_val.v.data = int_val.v.data;
	
	return ret_val.int_val;  
}

int C_Sqrt(int object_id,local_var_type *local_vars,
		   int num_normal_parms,parm_node normal_parm_array[],
		   int num_name_parms,parm_node name_parm_array[])
{
	val_type int_val,ret_val;

   RETRIEVEVALUEINT(int_val, 0, NIL);

	if (int_val.v.data & (1 << 27))
	{
		bprintf("C_Sqrt result undefined for negative value in obj:%i %s\n",
         object_id, GetClassNameByObjectID(object_id));
		return NIL;
	}
	
	ret_val.v.tag = TAG_INT;
	ret_val.v.data = (int)sqrt((double)int_val.v.data);
	
	return ret_val.int_val;  
}

int C_Bound(int object_id,local_var_type *local_vars,
			int num_normal_parms,parm_node normal_parm_array[],
			int num_name_parms,parm_node name_parm_array[])
{
	val_type int_val,min_val,max_val;
	
   RETRIEVEVALUEINT(int_val, 0, NIL);

	min_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	if (min_val.v.tag != TAG_NIL)
	{
		if (min_val.v.tag != TAG_INT)
		{
			bprintf("C_Bound can't use min bound %i,%i in obj:%i %s\n",
            min_val.v.tag,min_val.v.data, object_id,
            GetClassNameByObjectID(object_id));
			return NIL;
		}
		if (int_val.v.data < min_val.v.data)
			int_val.v.data = min_val.v.data;
	}	 
	
	max_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
		normal_parm_array[2].value);
	if (max_val.v.tag != TAG_NIL)
	{
		if (max_val.v.tag != TAG_INT)
		{
			bprintf("C_Bound can't use max bound %i,%i in obj:%i %s\n",
            max_val.v.tag,max_val.v.data, object_id,
            GetClassNameByObjectID(object_id));
			return NIL;
		}
		if (int_val.v.data > max_val.v.data)
			int_val.v.data = max_val.v.data;
	}	 
	
	return int_val.int_val;
}

int C_CreateTable(int object_id,local_var_type *local_vars,
                  int num_normal_parms,parm_node normal_parm_array[],
                  int num_name_parms,parm_node name_parm_array[])
{
   val_type ret_val, size_val;

   if (num_normal_parms == 0)
      size_val.v.data = 73;
   else
   {
      size_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
         normal_parm_array[0].value);
      if (size_val.v.tag != TAG_INT)
      {
         bprintf("C_CreateTable can't use non-int %i,%i for size in obj:%i %s\n",
            size_val.v.tag, size_val.v.data, object_id,
            GetClassNameByObjectID(object_id));
         size_val.v.data = 73;
      }
   }

   ret_val.v.tag = TAG_TABLE;
   ret_val.v.data = CreateTable(size_val.v.data);

   return ret_val.int_val;
}

int C_AddTableEntry(int object_id,local_var_type *local_vars,
					int num_normal_parms,parm_node normal_parm_array[],
					int num_name_parms,parm_node name_parm_array[])
{
	val_type table_val,key_val,data_val;

   RETRIEVEVALUETABLE(table_val, 0, NIL);

	key_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	
   // Can't use key value that might change. Strings are okay,
   // because the string itself is hashed.
   if (key_val.v.tag == TAG_OBJECT || key_val.v.tag == TAG_LIST
      || key_val.v.tag == TAG_TIMER || key_val.v.tag == TAG_TABLE
      || key_val.v.tag == TAG_CLASS)
   {
      bprintf("C_AddTableEntry can't use key id %i,%i in obj:%i %s\n",
         key_val.v.tag, key_val.v.data, object_id,
         GetClassNameByObjectID(object_id));
      return NIL;
   }

	data_val = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
		normal_parm_array[2].value);
	
	InsertTable(table_val.v.data,key_val,data_val);
	return NIL;
}

int C_GetTableEntry(int object_id,local_var_type *local_vars,
					int num_normal_parms,parm_node normal_parm_array[],
					int num_name_parms,parm_node name_parm_array[])
{
	val_type table_val,key_val;

   RETRIEVEVALUETABLE(table_val, 0, NIL);

	key_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);

	return GetTableEntry(table_val.v.data, key_val);
}

int C_DeleteTableEntry(int object_id,local_var_type *local_vars,
					   int num_normal_parms,parm_node normal_parm_array[],
					   int num_name_parms,parm_node name_parm_array[])
{
	val_type table_val,key_val;

   RETRIEVEVALUETABLE(table_val, 0, NIL);

	key_val = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
		normal_parm_array[1].value);
	
	DeleteTableEntry(table_val.v.data,key_val);
	return NIL;
}

int C_DeleteTable(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type table_val;

   RETRIEVEVALUETABLE(table_val, 0, NIL);
	bprintf("C_DeleteTable is deprecated, tables are deleted at GC.\n");
	//DeleteTable(table_val.v.data);
	return NIL;
}

int C_IsTable(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type check_val;

   check_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);

   return (check_val.v.tag == TAG_TABLE && GetTableByID(check_val.v.data))
      ? KOD_TRUE : KOD_FALSE;
}

int C_RecycleUser(int object_id,local_var_type *local_vars,
				  int num_normal_parms,parm_node normal_parm_array[],
				  int num_name_parms,parm_node name_parm_array[])
{
	val_type object_val;
	object_node *o;
	user_node *old_user;
	user_node *new_user;
	
   RETRIEVEVALUEOBJECT(object_val, 0, NIL);
	
	o = GetObjectByID(object_val.v.data);
	if (o == NULL)
	{
		bprintf("C_RecycleUser can't find object %i\n",object_val.v.data);
		return NIL;
	}
	
	// Find the old user from the object that KOD gives us.
	old_user = GetUserByObjectID(o->object_id);
	if (old_user == NULL)
	{
		bprintf("C_RecycleUser can't find user which is object %i\n",object_val.v.data);
		return NIL;
	}
	
	// Create another user which matches the old one.
	new_user = CreateNewUser(old_user->account_id,o->class_id);
	//bprintf("C_RecycleUser made new user, got object %i\n",new_user->object_id);
	
	// Delete the old user/object.
	// KOD:  post(old_user_object,@Delete);
	//
	PostBlakodMessage(old_user->object_id,DELETE_MSG,0,NULL);
	DeleteUserByObjectID(old_user->object_id);
	//bprintf("C_RecycleUser deleted old user and object %i\n",old_user->object_id);
	
	object_val.v.tag = TAG_OBJECT;
	object_val.v.data = new_user->object_id;
	
	return object_val.int_val;
}

int C_IsObject(int object_id,local_var_type *local_vars,
			   int num_normal_parms,parm_node normal_parm_array[],
			   int num_name_parms,parm_node name_parm_array[])
{
	val_type var_check;
	
	var_check = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
			     normal_parm_array[0].value);

	return (var_check.v.tag == TAG_OBJECT && GetObjectByID(var_check.v.data))
      ? KOD_TRUE : KOD_FALSE;
}

int C_StringToNumber(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
   val_type s1_val, ret_val;
   string_node *snod;

   s1_val = RetrieveValue(object_id, local_vars, normal_parm_array[0].type,
      normal_parm_array[0].value);

   switch (s1_val.v.tag)
   {
   case TAG_STRING:
      snod = GetStringByID(s1_val.v.data);
      break;
   case TAG_TEMP_STRING:
      snod = GetTempString();
      break;
   default:
      bprintf("C_StringToNumber can't use non-string %i,%i\n",
         s1_val.v.tag, s1_val.v.data);
      return NIL;
   }

   if (!snod)
   {
      bprintf("C_StringToNumber can't use invalid string %i,%i\n",
         s1_val.v.tag, s1_val.v.data);
      return NIL;
   }

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = (int)strtol(snod->data, NULL, 10);

   return ret_val.int_val;
}

// This is a hack to display a german TAG_RESOURCE in RecordStat.
#define RESOURCE_GER 16

int C_RecordStat(int object_id,local_var_type *local_vars,
				int num_normal_parms,parm_node normal_parm_array[],
				int num_name_parms,parm_node name_parm_array[])
{
#ifdef BLAK_PLATFORM_WINDOWS
   val_type stat_type, val, val_check;
   resource_node *rsc_node;
   session_node *session;
   string_node *snod;
   sql_data_node *data;
   int count = 0, num_expected;

   // Don't do anything if SQL isn't enabled.
   static bool bEnabled = ConfigBool(MYSQL_ENABLED);
   if (!bEnabled)
      return NIL;

   // The first paramenter to RecordStat() should always be a STAT_TYPE.
   // STAT_TYPE enum located in database.h, Also defined in blakston.khd
   // to match between C code and Kod code.
   stat_type = RetrieveValue(object_id,local_vars,normal_parm_array[0].type, normal_parm_array[0].value);
   if (stat_type.v.tag != TAG_INT)
   {
      bprintf("STAT_TYPE expected in C_RecordStat() as first parameter");
      return NIL;
   }

   // Must be a valid statistic type.
   if (stat_type.v.data <= 0 || stat_type.v.data > STAT_MAXSTAT)
   {
      bprintf("C_RecordStat received unknown statistic type %i", stat_type.v.data);
      return NIL;
   }

   // Have to allocate it on the heap because this data structure gets placed
   // on a queue in database.c. Memory free handled in database.c unless
   // something goes wrong parsing the values from kod here.
   num_expected = (num_normal_parms - 1) / 2;

   data = (sql_data_node *) AllocateMemory(MALLOC_ID_SQL, sizeof(sql_data_node) * num_expected);
 
   for (int i = 1; i < num_normal_parms - 1; ++count, i += 2)
   {
      // Check parameter types here.
      val_check = RetrieveValue(object_id, local_vars, normal_parm_array[i].type, normal_parm_array[i].value);
      val = RetrieveValue(object_id, local_vars, normal_parm_array[i + 1].type, normal_parm_array[i + 1].value);
      if (val_check.v.data != val.v.tag)
      {
         if (!(val_check.v.data == RESOURCE_GER && val.v.tag == TAG_RESOURCE))
         {
            val_type check_tag;
            check_tag.v.tag = val_check.v.data;
            bprintf("Wrong Type of Parameter in C_RecordStat(), expected %s found %s",
               ((val_check.v.data == RESOURCE_GER) ? "RESOURCE_GER" : GetTagName(check_tag)),
                  GetTagName(val));
            FreeDataNodeMemory(num_expected, count, data);
            return NIL;
         }
      }

      data[count].type = (val_check.v.data == RESOURCE_GER) ? RESOURCE_GER : val.v.tag;
      switch (data[count].type)
      {
      case TAG_NIL:
      case TAG_INT:
         data[count].value.num = val.v.data;
         break;
      case TAG_RESOURCE:
         rsc_node = GetResourceByID(val.v.data);
         if (!rsc_node || !rsc_node->resource_val[0])
         {
            bprintf("Couldn't lookup resource node %i in C_RecordStat()", val.v.data);
            FreeDataNodeMemory(num_expected, count, data);
            return NIL;
         }
         data[count].value.str = MySQLDuplicateString(rsc_node->resource_val[0]);
         break;
      case RESOURCE_GER:
         // Switch data type to TAG_RESOURCE, and retrieve the German resource if present.
         data[count].type = TAG_RESOURCE;
         rsc_node = GetResourceByID(val.v.data);
         if (!rsc_node || !rsc_node->resource_val[0])
         {
            bprintf("Couldn't lookup resource node %i in C_RecordStat()", val.v.data);
            FreeDataNodeMemory(num_expected, count, data);
            return NIL;
         }
         if (!rsc_node->resource_val[1])
         {
            bprintf("Missing German rsc %i in C_RecordStat(), using English rsc", val.v.data);
            data[count].value.str = MySQLDuplicateString(rsc_node->resource_val[0]);
         }
         else
            data[count].value.str = MySQLDuplicateString(rsc_node->resource_val[1]);
         break;
      case TAG_STRING:
         snod = GetStringByID(val.v.data);
         if (!snod || !snod->data)
         {
            bprintf("C_RecordStat got null string for ID %i", val.v.data);
            FreeDataNodeMemory(num_expected, count, data);
            return NIL;
         }
         data[count].value.str = MySQLDuplicateString(snod->data);
         break;
      case TAG_SESSION:
         session = GetSessionByID(val.v.data);
         if (!session || !session->account)
         {
            bprintf("C_RecordStat got null session or account for session ID %i", val.v.data);
            FreeDataNodeMemory(num_expected, count, data);
            return NIL;
         }
         data[count].type = TAG_INT;
         data[count].value.num = session->account->account_id;
         break;
      default:
         bprintf("C_RecordStat got type %s which cannot be handled, aborting call",
            GetTagName(val));
         FreeDataNodeMemory(num_expected, count, data);
         return NIL;
      }
   }

   // All types okay, try insert it.
   MySQLRecordGeneric(stat_type.v.data, count, data);
#endif
   return NIL;
}

int C_EmptyStatTable(int object_id, local_var_type *local_vars,
   int num_normal_parms, parm_node normal_parm_array[],
   int num_name_parms, parm_node name_parm_array[])
{
#ifdef BLAK_PLATFORM_WINDOWS
   val_type stat_type;

   // Don't do anything if SQL isn't enabled.
   static bool bEnabled = ConfigBool(MYSQL_ENABLED);
   if (!bEnabled)
      return NIL;

   // STAT_TYPE enum located in database.h, Also defined in blakston.khd
   // to match between C code and Kod code.
   stat_type = RetrieveValue(object_id, local_vars, normal_parm_array[0].type, normal_parm_array[0].value);
   if (stat_type.v.tag != TAG_INT)
   {
      bprintf("STAT_TYPE expected in C_EmptyStatTable(), got %s %i",
         GetTagName(stat_type), stat_type.v.data);
      return NIL;
   }

   // Must be a valid statistic type.
   if (stat_type.v.data <= 0 || stat_type.v.data > STAT_MAXSTAT)
   {
      bprintf("C_EmptyStatTable received unknown statistic type %i",
         stat_type.v.data);
      return NIL;
   }

   MySQLEmptyTable(stat_type.v.data);
#endif
   return NIL;
}

int C_GetSessionIP(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   val_type session_id, temp, ret_val;
   session_node* session = NULL;
   
   session_id = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
      normal_parm_array[0].value);
      
   
   if (session_id.v.tag != TAG_SESSION)
   {
      bprintf("C_GetSessionIP can't use non session %i,%i\n",session_id.v.tag,session_id.v.data);
      return NIL;
   }

   session = GetSessionByID(session_id.v.data);
   
   if (!session)
   {
      bprintf("C_GetSessionIP can't find session for %i,%i\n", session_id.v.tag, session_id.v.data);
      return NIL;
   }
   
   ret_val.int_val = NIL;
   temp.v.tag = TAG_INT;
   
   // reverse the order, because the address is stored in network order in in6_addr
   for (int i = sizeof(struct in6_addr) - 1; i >= 0; i--)
   {
#ifdef BLAK_PLATFORM_WINDOWS
      temp.v.data = session->conn.addr.u.Byte[i];
#else
      temp.v.data = session->conn.addr.s6_addr[i];
#endif
      ret_val.v.data = Cons(temp, ret_val);
      ret_val.v.tag = TAG_LIST;
   }

   return ret_val.int_val;
}

int C_SetClassVar(int object_id,local_var_type *local_vars,
            int num_normal_parms,parm_node normal_parm_array[],
            int num_name_parms,parm_node name_parm_array[])
{
   // Note that setting a class var is only temporary until the server restarts.
   class_node *c;
   object_node *o;
   val_type ret_val, class_val, data_str, var_name;
   int var_id;
   const char *pStrConst;

   ret_val.v.tag = TAG_INT;
   ret_val.v.data = False;

   class_val = RetrieveValue(object_id,local_vars,normal_parm_array[0].type,
                  normal_parm_array[0].value);
   var_name = RetrieveValue(object_id,local_vars,normal_parm_array[1].type,
                  normal_parm_array[1].value);
   data_str = RetrieveValue(object_id,local_vars,normal_parm_array[2].type,
                  normal_parm_array[2].value);

   if (class_val.v.tag == TAG_OBJECT)
   {
      o = GetObjectByID(class_val.v.data);
      if (o == NULL)
      {
         bprintf("C_SetClassVar can't find the class of object %i\n",
               class_val.v.data);
         return ret_val.int_val;
      }

      class_val.v.tag = TAG_CLASS;
      class_val.v.data = o->class_id;
   }

   if (class_val.v.tag != TAG_CLASS)
   {
      bprintf("C_SetClassVar can't look for non-class %i,%i\n",
         class_val.v.tag,class_val.v.data);
      return ret_val.int_val;
   }

   c = GetClassByID(class_val.v.data);
   if (c == NULL)
   {
      bprintf("C_SetClassVar cannot find class %i.\n",class_val.v.data);
      return ret_val.int_val;
   }

   if (var_name.v.tag != TAG_DEBUGSTR)
   {
      bprintf("C_SetClassVar passed bad class var string, tag %i.\n",
         var_name.v.tag);
      return ret_val.int_val;
   }

   kod_statistics *kstat = GetKodStats();
   class_node *c2 = GetClassByID(kstat->interpreting_class);
   if (c2 == NULL)
   {
      bprintf("C_SetClassVar can't find class %i, can't get debug str\n",
            kstat->interpreting_class);
      return ret_val.int_val;
   }

   pStrConst = GetClassDebugStr(c2,var_name.v.data);
   if (pStrConst == NULL)
   {
      bprintf("C_SetClassVar: GetClassDebugStr returned NULL\n");
      return ret_val.int_val;
   }

   var_id = GetClassVarIDByName(c, pStrConst);
   if (var_id == INVALID_CLASSVAR)
   {
      bprintf("C_SetClassVar cannot find classvar named %s in class %i.\n",
            pStrConst,class_val.v.data);
      return ret_val.int_val;
   }

   c->vars[var_id].val = data_str;
   ret_val.v.data = True;

   return ret_val.int_val;
}
