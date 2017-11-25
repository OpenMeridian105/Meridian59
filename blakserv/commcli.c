// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * commcli.c
 *

 This module has a few functions that concern communication from the
 server and blakod to the clients.  This is done with a buffer list
 and functions to add various types of data to the list.

 */

#include "blakserv.h"

#define NUMBER_OBJECT 5     /* writes 4 bytes, but diff "tag" */
#define STRING_RESOURCE 6   /* writes actual string, even though it's a resource */
#define STRING_AS_INTEGER 7 /* converts string to 32-bit int, writes int */

static buffer_node *blist;

void InitCommCli()
{
   blist = NULL;
}

void AddBlakodToPacket(val_type obj_size,val_type obj_data)
{
   int num_bytes;
   char byte1;
   short byte2;
   int byte4;
   string_node *snod;
   const char *pStrConst;
   val_type temp_val;

   if (obj_size.v.tag != TAG_INT)
   {
      bprintf("AddBlakodToPacket looking for int, # of bytes, got %i,%i\n",
         obj_size.v.tag,obj_size.v.data);
      return;
   }

   num_bytes = obj_size.v.data;

   if (obj_data.v.tag == TAG_NIL)
      bprintf("AddBlakodToPacket looking for value, got NIL\n");

/*   dprintf("Send %i bytes from %i,%i\n",obj_size.data,obj_data.v.tag,obj_data.v.data); */
   switch (obj_data.v.tag)
   {
   case TAG_STRING :
      snod = GetStringByID(obj_data.v.data);
      if (snod == NULL)
      {
         bprintf("AddBlakodToPacket can't find string id %i\n",obj_data.v.data);
         break;
      }
      if (num_bytes == STRING_AS_INTEGER)
         AddStringToPacketAsInt(snod->data);
      else
         AddStringToPacket(snod->len_data,snod->data);
      break;
      
   case TAG_TEMP_STRING :
      snod = GetTempString();
      AddStringToPacket(snod->len_data,snod->data);
      break;

   default :
      switch (num_bytes)
      {
      case 1 :
         byte1 = (char) obj_data.v.data;
         AddByteToPacket(byte1);
         break;
      case 2 :
         byte2 = (short) obj_data.v.data;
         AddShortToPacket(byte2);
         break;
      case 4 :
         byte4 = (int) obj_data.v.data;
         AddIntToPacket(byte4);
         break;
      case NUMBER_OBJECT :
         temp_val.int_val = obj_data.int_val;
         temp_val.v.tag = CLIENT_TAG_NUMBER;
         byte4 = temp_val.int_val;
         AddIntToPacket(byte4);
         break;
      case STRING_RESOURCE :
         if (obj_data.v.tag != TAG_RESOURCE)
         {
            bprintf("AddBlakodToPacket can't send %i,%i as a resource/string\n",
               obj_data.v.tag,obj_data.v.data);
            return;
         }
         pStrConst = GetResourceStrByLanguageID(obj_data.v.data, ConfigInt(RESOURCE_LANGUAGE));
         if (pStrConst == NULL)
         {
            bprintf("AddBlakodToPacket can't find resource %i as a resource/string\n",
               obj_data.v.data);
            return;
         }
         AddStringToPacket(strlen(pStrConst),pStrConst);
         break;
      default :
         bprintf("AddBlakodToPacket can't send %i bytes\n",num_bytes);
         break;
      }
   }
}

/* these few functions are for synched mode */
void AddByteToPacket(unsigned char byte1)
{
   blist = AddToBufferList(blist,&byte1,1);
}

void AddShortToPacket(short byte2)
{
   blist = AddToBufferList(blist,&byte2,2);
}

void AddIntToPacket(int byte4)
{
   blist = AddToBufferList(blist,&byte4,4);
}

void AddStringToPacketAsInt(const char *ptr)
{
   int number = strtol(ptr, NULL, 10);

   blist = AddToBufferList(blist, &number, 4);
}

void AddStringToPacket(int int_len,const char *ptr)
{
   unsigned short len;

   len = int_len;

   blist = AddToBufferList(blist,&len,2);
   blist = AddToBufferList(blist,(void *) ptr,int_len);
}

void SecurePacketBufferList(int session_id, buffer_node *bl)
{
   session_node *s = GetSessionByID(session_id);
   char* pRedbook;

   if (!session_id || !s || !s->account || !s->account->account_id ||
       s->conn.type == CONN_CONSOLE)
   {
      //dprintf("SecurePacketBufferList cannot find session %i", session_id);
      return;
   }
   if (s->version_major < 4)
   {
      return;
   }
   if (bl == NULL || bl->buf == NULL)
   {
//      dprintf("SecurePacketBufferList can't use invalid buffer list");
      return;
   }

//   dprintf("Securing msg %u with %u", (unsigned char)bl->buf[0], (unsigned char)(s->secure_token & 0xFF));

   bl->buf[0] ^= (unsigned char)(s->secure_token & 0xFF);
   pRedbook = GetSecurityRedbook();
   if (s->sliding_token && pRedbook)
   {
      if (s->sliding_token < pRedbook ||
         s->sliding_token > pRedbook+strlen(pRedbook))
      {
         lprintf("SecurePacketBufferList lost redbook on session %i account %i (%s), may break session\n",
         session_id, s->account->account_id, s->account->name);
         s->sliding_token = pRedbook;
      }

      s->secure_token += ((*s->sliding_token) & 0x7F);
      s->sliding_token++;
      if (*s->sliding_token == '\0')
         s->sliding_token = pRedbook;
   }
}

void SendPacket(int session_id)
{
/*
   int i;
   dprintf("sending packet len %i\n",len_buf);
   for (i=0;i<len_buf;i++)
      dprintf("%i ",(unsigned char)buf[i]);
   dprintf("\n");
*/

//   dprintf("SendPacket msg %u", (unsigned char)blist->buf[0]);
   SecurePacketBufferList(session_id,blist);
   SendClientBufferList(session_id,blist);
   blist = NULL;
}

void SendCopyPacket(int session_id)
{
   buffer_node *bl = CopyBufferList(blist);
//   dprintf("SendCopyPacket msg %u", (unsigned char)bl->buf[0]);
   SecurePacketBufferList(session_id,bl);
   SendClientBufferList(session_id,bl);
}

void ClearPacket()
{
   DeleteBufferList(blist);
   blist = NULL;
}

void ClientHangupToBlakod(session_node *session)
{
   val_type command,parm_list;
   parm_node parms[1];

   command.v.tag = TAG_INT;
   command.v.data = BP_REQ_QUIT;

   parm_list.int_val = NIL;
   parm_list.v.data = Cons(command,parm_list);
   parm_list.v.tag = TAG_LIST;

   parms[0].type = CONSTANT;
   parms[0].value = parm_list.int_val;
   parms[0].name_id = CLIENT_PARM;

   SendTopLevelBlakodMessage(session->game->object_id,RECEIVE_CLIENT_MSG,1,parms);
}

void SendBlakodBeginSystemEvent(int type)
{
   val_type int_val;
   parm_node p[1];

   int_val.v.tag = TAG_INT;
   int_val.v.data = type;

   p[0].type = CONSTANT;
   p[0].value = int_val.int_val;
   p[0].name_id = TYPE_PARM;

   SendTopLevelBlakodMessage(GetSystemObjectID(),GARBAGE_MSG,1,p);
}

void SendBlakodEndSystemEvent(int type)
{
   val_type int_val;
   parm_node p[1];

   int_val.v.tag = TAG_INT;
   int_val.v.data = type;

   p[0].type = CONSTANT;
   p[0].value = int_val.int_val;
   p[0].name_id = TYPE_PARM;

   SendTopLevelBlakodMessage(GetSystemObjectID(),GARBAGE_DONE_MSG,1,p);
}

#define BLAK_TAG_PARM_CREATE(a, b, c, d, e) \
   do \
   { \
      a[b].type = CONSTANT; \
      a[b].value = (c << KOD_SHIFT) + d; \
      a[b].name_id = GetIDByName(e); \
   } while (0)

#define BLAK_PARM_CREATE(a, b, c, d) \
   do \
   { \
      a[b].type = CONSTANT; \
      a[b].value = c.int_val; \
      a[b].name_id = GetIDByName(d); \
   } while (0)

void SendBlakodRegisterCallback(blakod_reg_callback *reg)
{
   parm_node p[12];

   // Obj ID.
   BLAK_TAG_PARM_CREATE(p, 0, TAG_OBJECT, reg->object_id, "oObject");
   // Msg ID.
   BLAK_TAG_PARM_CREATE(p, 1, TAG_MESSAGE, reg->message_id, "message");

   // Second.
   BLAK_TAG_PARM_CREATE(p, 2, TAG_INT, reg->second, "iSecond");
   // Minute.
   BLAK_TAG_PARM_CREATE(p, 3, TAG_INT, reg->minute, "iMinute");
   // Hour.
   BLAK_TAG_PARM_CREATE(p, 4, TAG_INT, reg->hour, "iHour");
   // Day.
   BLAK_TAG_PARM_CREATE(p, 5, TAG_INT, reg->day, "iDay");
   // Month.
   BLAK_TAG_PARM_CREATE(p, 6, TAG_INT, reg->month, "iMonth");
   // Year.
   BLAK_TAG_PARM_CREATE(p, 7, TAG_INT, reg->year, "iYear");

   // Optional parameters.
   // parm1
   BLAK_PARM_CREATE(p, 8, reg->parm1, "parm1");
   // parm2
   BLAK_PARM_CREATE(p, 9, reg->parm2, "parm2");
   // parm3
   BLAK_PARM_CREATE(p, 10, reg->parm3, "parm3");
   // parm4
   BLAK_PARM_CREATE(p, 11, reg->parm4, "parm4");

   SendTopLevelBlakodMessage(GetRealTimeObjectID(), GetIDByName("registercallback"), 12, p);
}
