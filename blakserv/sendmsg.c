// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * sendmsg.c
 *

  This module interprets compiled Blakod.
  
*/

#include "blakserv.h"

/* global debugging and profiling information */

/* stuff to calculate messages & times */
int message_depth = 0;
/* stack has class and bkod ptr for each frame. For current frame, bkod_ptr
   is as of the beginning of the function call */
kod_stack_type stack[MAX_DEPTH];

kod_statistics kod_stat;      /* actual statistics */

char *bkod;
int num_interpreted = 0; /* number of instructions in this top level call */

int trace_session_id = INVALID_ID;

// Static NIL.
static val_type nil_val;

post_queue_type post_q;

// Structs for unary/binary op to read data quicker. Can't include the
// info byte as the alignment will be incorrect. Forcing alignment with
// the extra byte generates slower code.
typedef struct
{
   bkod_type dest;
   bkod_type source;
} unopdata_node;

typedef struct
{
   bkod_type dest;
   bkod_type source1;
   bkod_type source2;
} binopdata_node;

/* return values for InterpretAtMessage */
enum
{
   RETURN_NONE = 0,
   RETURN_PROPAGATE = 1,
   RETURN_NO_PROPAGATE = 2,
};

/* table of pointers to functions to call for ccode functions */

typedef int (*ccall_proc)(int object_id,local_var_type *local_vars,
                    int num_normal_parms,parm_node normal_parm_array[],
                    int num_name_parms,parm_node name_parm_array[]);
ccall_proc ccall_table[MAX_C_FUNCTION];

// Table of pointers to functions to call for opcodes.
typedef void(*op_proc)(int object_id, local_var_type *local_vars);
op_proc opcode_table[NUMBER_OF_OPCODES];
void CreateOpcodeTable(void);

int done;

/* local function prototypes */
// Main interpreter loop.
int InterpretAtMessage(int object_id,class_node* c,message_node* m,
                  int num_sent_parms,parm_node sent_parms[],
                  val_type *ret_val);

// Store functions: one general (local or property) and one specific for each.
__forceinline void StoreValue(int object_id,local_var_type *local_vars,int data_type,int data,
                   val_type new_data);
__forceinline void StoreLocal(local_var_type *local_vars, int data, val_type new_data);
__forceinline void StoreProperty(int object_id, int data, val_type new_data);

void InitProfiling(void)
{
   int i;

   if (done)
      return;

   kod_stat.num_interpreted = 0;
   kod_stat.num_interpreted_highest = 0;
   kod_stat.billions_interpreted = 0;
   kod_stat.num_messages = 0;
   kod_stat.num_top_level_messages = 0;
   kod_stat.system_start_time = GetTime();
   kod_stat.interpreting_time = 0.0;
   kod_stat.interpreting_time_highest = 0;
   kod_stat.interpreting_time_over_second = 0;
   kod_stat.interpreting_time_message_id = INVALID_ID;
   kod_stat.interpreting_time_object_id = INVALID_ID;
   kod_stat.interpreting_time_posts = 0;
   kod_stat.message_depth_highest = 0;
   kod_stat.interpreting_class = INVALID_CLASS;

   for (i = 0; i < MAX_C_FUNCTION; ++i)
   {
      kod_stat.c_count_untimed[i] = 0;
      kod_stat.c_count_timed[i] = 0;
      kod_stat.ccall_total_time[i] = 0;
   }

#if KOD_OPCODE_TESTING
   for (i = 0; i < NUMBER_OF_OPCODES; ++i)
   {
      kod_stat.opcode_total_time[i] = 0;
      kod_stat.opcode_count[i] = 0;
   }
#endif

   message_depth = 0;

   if (ConfigBool(DEBUG_TIME_CALLS))
      InitTimeProfiling();
   else
      kod_stat.debugtime = false;

   done = 1;
}

void InitTimeProfiling(void)
{
   kod_stat.debugtime = true;
}

void EndTimeProfiling(void)
{
   kod_stat.debugtime = false;
}

void InitBkodInterpret(void)
{
   int i;

   bkod = NULL;

   post_q.next = 0;
   post_q.last = 0;
   
   // Init the nil value.
   nil_val.int_val = NIL;

   // Create the opcode table.
   CreateOpcodeTable();

   for (i=0;i<MAX_C_FUNCTION;i++)
      ccall_table[i] = C_Invalid;
   
   ccall_table[CREATEOBJECT] = C_CreateObject;
   
   ccall_table[SENDMESSAGE] = C_SendMessage;
   ccall_table[POSTMESSAGE] = C_PostMessage;
   ccall_table[SENDLISTMSG] = C_SendListMessage;
   ccall_table[SENDLISTMSGBREAK] = C_SendListMessageBreak;
   ccall_table[SENDLISTMSGBYCLASS] = C_SendListMessageByClass;
   ccall_table[SENDLISTMSGBYCLASSBREAK] = C_SendListMessageByClassBreak;

   ccall_table[SAVEGAME] = C_SaveGame;
   ccall_table[LOADGAME] = C_LoadGame;

   ccall_table[ADDPACKET] = C_AddPacket;
   ccall_table[SENDPACKET] = C_SendPacket;
   ccall_table[SENDCOPYPACKET] = C_SendCopyPacket;
   ccall_table[CLEARPACKET] = C_ClearPacket;
   ccall_table[GODLOG] = C_GodLog;
   ccall_table[DEBUG] = C_Debug;
   ccall_table[GETINACTIVETIME] = C_GetInactiveTime;
   ccall_table[DUMPSTACK] = C_DumpStack;

   ccall_table[STRINGEQUAL] = C_StringEqual;
   ccall_table[STRINGCONTAIN] = C_StringContain;
   ccall_table[SETRESOURCE] = C_SetResource;
   ccall_table[PARSESTRING] = C_ParseString;
   ccall_table[SETSTRING] = C_SetString;
   ccall_table[APPENDTEMPSTRING] = C_AppendTempString;
   ccall_table[CLEARTEMPSTRING] = C_ClearTempString;
   ccall_table[GETTEMPSTRING] = C_GetTempString;
   ccall_table[CREATESTRING] = C_CreateString;
   ccall_table[ISSTRING] = C_IsString;
   ccall_table[STRINGSUBSTITUTE] = C_StringSubstitute;
   ccall_table[STRINGLENGTH] = C_StringLength;
   ccall_table[STRINGCONSISTSOF] = C_StringConsistsOf;
   
   ccall_table[CREATETIMER] = C_CreateTimer;
   ccall_table[DELETETIMER] = C_DeleteTimer;
   ccall_table[GETTIMEREMAINING] = C_GetTimeRemaining;
   ccall_table[ISTIMER] = C_IsTimer;
   ccall_table[CREATEROOMDATA] = C_LoadRoom;
   ccall_table[FREEROOM] = C_FreeRoom;
   ccall_table[ROOMDATA] = C_RoomData;
   ccall_table[LINEOFSIGHTVIEW] = C_LineOfSightView;
   ccall_table[LINEOFSIGHTBSP] = C_LineOfSightBSP;
   ccall_table[CANMOVEINROOMBSP] = C_CanMoveInRoomBSP;
   ccall_table[CHANGETEXTUREBSP] = C_ChangeTextureBSP;
   ccall_table[MOVESECTORBSP] = C_MoveSectorBSP;
   ccall_table[CHANGESECTORFLAGBSP] = C_ChangeSectorFlagBSP;
   ccall_table[GETLOCATIONINFOBSP] = C_GetLocationInfoBSP;
   ccall_table[BLOCKERADDBSP] = C_BlockerAddBSP;
   ccall_table[BLOCKERMOVEBSP] = C_BlockerMoveBSP;
   ccall_table[BLOCKERREMOVEBSP] = C_BlockerRemoveBSP;
   ccall_table[BLOCKERCLEARBSP] = C_BlockerClearBSP;
   ccall_table[GETRANDOMPOINTBSP] = C_GetRandomPointBSP;
   ccall_table[GETSTEPTOWARDSBSP] = C_GetStepTowardsBSP;
   ccall_table[GETRANDOMMOVEDESTBSP] = C_GetRandomMoveDestBSP;
   ccall_table[GETSECTORHEIGHTBSP] = C_GetSectorHeightBSP;
   ccall_table[SETROOMDEPTHOVERRIDEBSP] = C_SetRoomDepthOverrideBSP;
   ccall_table[CALCUSERMOVEMENTBUCKET] = C_CalcUserMovementBucket;
   ccall_table[INTERSECTLINECIRCLE] = C_IntersectLineCircle;

   ccall_table[APPENDLISTELEM] = C_AppendListElem;
   ccall_table[CONS] = C_Cons;
   ccall_table[LENGTH] = C_Length;
   ccall_table[LAST] = C_Last;
   ccall_table[NTH] = C_Nth;
   ccall_table[MLIST] = C_List;
   ccall_table[ISLIST] = C_IsList;
   ccall_table[SETFIRST] = C_SetFirst;
   ccall_table[SETNTH] = C_SetNth;
   ccall_table[SWAPLISTELEM] = C_SwapListElem;
   ccall_table[INSERTLISTELEM] = C_InsertListElem;
   ccall_table[DELLISTELEM] = C_DelListElem;
   ccall_table[DELLASTLISTELEM] = C_DelLastListElem;
   ccall_table[FINDLISTELEM] = C_FindListElem;
   ccall_table[ISLISTMATCH] = C_IsListMatch;
   ccall_table[GETLISTELEMBYCLASS] = C_GetListElemByClass;
   ccall_table[GETLISTNODE] = C_GetListNode;
   ccall_table[GETALLLISTNODESBYCLASS] = C_GetAllListNodesByClass;
   ccall_table[LISTCOPY] = C_ListCopy;

   ccall_table[GETTIME] = C_GetTime;
   ccall_table[GETUNIXTIMESTRING] = C_GetUnixTimeString;
   ccall_table[OLDTIMESTAMPFIX] = C_OldTimestampFix;
   ccall_table[GETTICKCOUNT] = C_GetTickCount;
   ccall_table[GETDATEANDTIME] = C_GetDateAndTime;
   ccall_table[SETCLASSVAR] = C_SetClassVar;
   
   ccall_table[CREATETABLE] = C_CreateTable;
   ccall_table[ADDTABLEENTRY] = C_AddTableEntry;
   ccall_table[GETTABLEENTRY] = C_GetTableEntry;
   ccall_table[DELETETABLEENTRY] = C_DeleteTableEntry;
   ccall_table[DELETETABLE] = C_DeleteTable;
   ccall_table[ISTABLE] = C_IsTable;

   ccall_table[ISOBJECT] = C_IsObject;
   
   ccall_table[RECYCLEUSER] = C_RecycleUser;
   
   ccall_table[RANDOM] = C_Random;
   
   ccall_table[RECORDSTAT] = C_RecordStat;
   
   ccall_table[GETSESSIONIP] = C_GetSessionIP;
   
   ccall_table[ABS] = C_Abs;
   ccall_table[BOUND] = C_Bound;
   ccall_table[SQRT] = C_Sqrt;

   ccall_table[STRINGTONUMBER] = C_StringToNumber;
}

kod_statistics * GetKodStats()
{
   return &kod_stat;
}

/* this pointer only makes sense when interpreting (used by bprintf only) */

char * GetBkodPtr(void)
{
   return bkod;
}

/* used by object.c to see if creation of object should call
SendTopLevelBlakodMessage or SendBlakodMessage */
Bool IsInterpreting(void)
{
   return bkod != NULL;
}

void TraceInfo(int session_id,char *class_name,int message_id,int num_parms,
               parm_node parms[])
{
   int i;
   val_type val;
   char trace_buf[BUFFER_SIZE];
   char *buf_ptr = trace_buf;
   int num_chars;
   int buf_len = 0;

   if (num_parms == 0)
      num_chars = sprintf(buf_ptr, "%-15s%-20s\n", class_name, GetNameByID(message_id));
   else
      num_chars = sprintf(buf_ptr, "%-15s%-20s ", class_name, GetNameByID(message_id));
   buf_len += num_chars;
   buf_ptr += num_chars;

   for (i = 0; i < num_parms; ++i)
   {
      val.int_val = parms[i].value;

      if (i == num_parms - 1)
         num_chars = sprintf(buf_ptr, "%s = %s %s\n", GetNameByID(parms[i].name_id),
                        GetTagName(val), GetDataName(val));
      else
         num_chars = sprintf(buf_ptr, "%s = %s %s, ", GetNameByID(parms[i].name_id),
                        GetTagName(val), GetDataName(val));
      buf_len += num_chars;
      buf_ptr += num_chars;

      // Flushing shouldn't be necessary here, but just in case.
      if (buf_len > BUFFER_SIZE * 0.9)
      {
         buf_ptr -= buf_len;
         SendSessionAdminText(session_id, buf_ptr);
         buf_len = 0;
      }
   }
   buf_ptr -= buf_len;
   SendSessionAdminText(session_id, buf_ptr);
}

void PostBlakodMessage(int object_id,int message_id,int num_parms,parm_node parms[])
{
   int i, new_next;

   new_next = (post_q.next + 1) % MAX_POST_QUEUE;
   if (new_next == post_q.last)
   {
      bprintf("PostBlakodMessage can't post MESSAGE %s (%i) to OBJECT %i; queue filled\n",
         GetNameByID(message_id),message_id,object_id);
      return;
   }
   post_q.data[post_q.next].object_id = object_id;
   post_q.data[post_q.next].message_id = message_id;
   post_q.data[post_q.next].num_parms = num_parms;
   for (i=0;i<num_parms;i++)
   {
      post_q.data[post_q.next].parms[i] = parms[i];
   }
   post_q.next = new_next;
}

/* returns the return value of the blakod */
int SendTopLevelBlakodMessage(int object_id,int message_id,int num_parms,parm_node parms[])
{
   int ret_val = 0;
   double start_time = 0;
   double interp_time = 0;
   int posts = 0;
   int accumulated_num_interpreted = 0;

   if (message_depth != 0)
   {
      eprintf("SendTopLevelBlakodMessage called with message_depth %i "
         "and message id %i\n", message_depth,message_id);
   }

   start_time = GetMicroCountDouble();
   kod_stat.num_top_level_messages++;
   trace_session_id = INVALID_ID;
   num_interpreted = 0;

   ret_val = SendBlakodMessage(object_id,message_id,num_parms,parms);

   while (post_q.next != post_q.last)
   {
      posts++;

      accumulated_num_interpreted += num_interpreted;
      num_interpreted = 0;

      if (accumulated_num_interpreted > 10 * MAX_BLAKOD_STATEMENTS)
      {
         bprintf("SendTopLevelBlakodMessage too many instructions in posted followups\n");
         
         dprintf("SendTopLevelBlakodMessage too many instructions in posted followups\n");
         dprintf("  OBJECT %i CLASS %s MESSAGE %s (%i) some followups are being aborted\n",
            object_id, GetClassNameByObjectID(object_id), GetNameByID(message_id), message_id);
         
         break;
      }

      /* posted messages' return value is ignored */
      SendBlakodMessage(post_q.data[post_q.last].object_id,post_q.data[post_q.last].message_id,
         post_q.data[post_q.last].num_parms,post_q.data[post_q.last].parms);

      post_q.last = (post_q.last + 1) % MAX_POST_QUEUE;
   }

   interp_time = GetMicroCountDouble() - start_time;
   kod_stat.interpreting_time += interp_time;
   if (interp_time > kod_stat.interpreting_time_highest)
   {
      kod_stat.interpreting_time_highest = (int)interp_time;
      kod_stat.interpreting_time_message_id = message_id;
      kod_stat.interpreting_time_object_id = object_id;
      kod_stat.interpreting_time_posts = posts;
   }
   if (interp_time > 1000000.0)
   {
      kod_stat.interpreting_time_over_second++;
      kod_stat.interpreting_time_message_id = message_id;
      kod_stat.interpreting_time_object_id = object_id;
      kod_stat.interpreting_time_posts = posts;
   }

   if (num_interpreted > kod_stat.num_interpreted_highest)
      kod_stat.num_interpreted_highest = num_interpreted;
   
   kod_stat.num_interpreted += num_interpreted;
   if (kod_stat.num_interpreted > 1000000000L)
   {
      kod_stat.num_interpreted -= 1000000000L;
      kod_stat.billions_interpreted++;
   }

   if (message_depth != 0)
   {
      eprintf("SendTopLevelBlakodMessage returning with message_depth %i "
         "and message id %i\n", message_depth, message_id);
   }

   return ret_val;
}

typedef struct {
   int class_id;
   int message_id;
   int num_params;
   parm_node *parm;
} ClassMessage, *PClassMessage;

static ClassMessage classMsg;
static int numExecuted;

void SendClassMessage(object_node *object)
{
   class_node *c = GetClassByID(object->class_id);
   do
   {
      if (c->class_id == classMsg.class_id)
      {
         SendBlakodMessage(object->object_id,classMsg.message_id,
                           classMsg.num_params,classMsg.parm);
         numExecuted++;
         return;
      }
      c = c->super_ptr;
   } while (c != NULL);
}

int SendBlakodClassMessage(int class_id,int message_id,int num_params,parm_node parm[])
{
   numExecuted = 0;
   classMsg.class_id = class_id;
   classMsg.message_id = message_id;
   classMsg.num_params = num_params;
   classMsg.parm = parm;
   ForEachObject(SendClassMessage);
   return numExecuted;
}

/* returns the return value of the blakod */
int SendBlakodMessage(int object_id,int message_id,int num_parms,parm_node parms[])
{
   object_node *o;
   class_node *c,*propagate_class;
   message_node *m;
   val_type message_ret;
   
   int prev_interpreting_class;
   char *prev_bkod;

   int propagate_depth = 0;

   prev_bkod = bkod;
   prev_interpreting_class = kod_stat.interpreting_class;

   o = GetObjectByID(object_id);
   if (o == NULL)
   {
      bprintf("SendBlakodMessage can't find OBJECT %i\n",object_id);
      return NIL;
   }

   c = GetClassByID(o->class_id);
   if (c == NULL)
   {
      eprintf("SendBlakodMessage OBJECT %i can't find CLASS %i\n",
         object_id,o->class_id);
      return NIL;
   }

   m = GetMessageByIDFast(c, message_id, &c);

   if (m == NULL)
   {
      bprintf("SendBlakodMessage CLASS %s (%i) OBJECT %i can't find a handler for MESSAGE %s (%i)\n",
         c->class_name,c->class_id,object_id,GetNameByID(message_id),message_id);
      return NIL;
   }

   kod_stat.num_messages++;
   stack[message_depth].class_id = c->class_id;
   stack[message_depth].message_id = m->message_id;
   stack[message_depth].propagate_depth = 0;
   stack[message_depth].num_parms = num_parms;
   memcpy(stack[message_depth].parms,parms,num_parms*sizeof(parm_node));
   stack[message_depth].bkod_ptr = bkod;
   if (message_depth > 0)
      stack[message_depth-1].bkod_ptr = prev_bkod;
   message_depth++;
   if (message_depth > kod_stat.message_depth_highest)
      kod_stat.message_depth_highest = message_depth;

   if (message_depth >= MAX_DEPTH)
   {
      bprintf("SendBlakodMessage sending to CLASS %s (%i), depth is %i, aborted!\n",
         c->class_name,c->class_id,message_depth);
      
      kod_stat.interpreting_class = prev_interpreting_class;
      message_depth--;
      bkod = prev_bkod;

      return NIL;
   }

   if (m->trace_session_id != INVALID_ID)
   {
      trace_session_id = m->trace_session_id;
      m->trace_session_id = INVALID_ID;
   }

   if (trace_session_id != INVALID_ID)
      TraceInfo(trace_session_id,c->class_name,m->message_id,num_parms,parms);

   kod_stat.interpreting_class = c->class_id;

   bkod = m->handler;

   propagate_depth = 1;

   while (InterpretAtMessage(object_id,c,m,num_parms,parms,&message_ret) 
      == RETURN_PROPAGATE)
   {
      propagate_class = m->propagate_class;
      m = m->propagate_message;

      if (m == NULL)
      {
         bprintf("SendBlakodMessage can't propagate MESSAGE %s (%i) in CLASS %s (%i)\n",
            GetNameByID(message_id),message_id,c->class_name,c->class_id);
         message_depth -= propagate_depth;
         kod_stat.interpreting_class = prev_interpreting_class;
         bkod = prev_bkod;
         return NIL;
      }

      if (propagate_class == NULL)
      {
         bprintf("SendBlakodMessage can't find class to propagate to, from "
            "MESSAGE %s (%i) in CLASS %s (%i)\n",GetNameByID(message_id),
            message_id,c->class_name,c->class_id);
         message_depth -= propagate_depth;
         kod_stat.interpreting_class = prev_interpreting_class;
         bkod = prev_bkod;
         return NIL;
      }

      c = propagate_class;

      if (m->trace_session_id != INVALID_ID)
      {
         trace_session_id = m->trace_session_id;
         m->trace_session_id = INVALID_ID;
      }

      if (trace_session_id != INVALID_ID)
         TraceInfo(trace_session_id,"(propagate)",m->message_id,num_parms,parms);

      kod_stat.interpreting_class = c->class_id;

      stack[message_depth-1].bkod_ptr = bkod;

      stack[message_depth].class_id = c->class_id;
      stack[message_depth].message_id = m->message_id;
      stack[message_depth].propagate_depth = propagate_depth;
      stack[message_depth].num_parms = num_parms;
      memcpy(stack[message_depth].parms,parms,num_parms*sizeof(parm_node));
      stack[message_depth].bkod_ptr = m->handler;
      message_depth++;
      propagate_depth++;

      bkod = m->handler;
   }

   message_depth -= propagate_depth;
   kod_stat.interpreting_class = prev_interpreting_class;
   bkod = prev_bkod;

   return message_ret.int_val;
}

/* interpret code below here */

#define get_byte() (*bkod++)

__inline unsigned int get_int()
{
   bkod += 4;
   return *((unsigned int *)(bkod-4));
}

/* before calling this, you MUST set bkod to point to valid bkod. */

/* returns either RETURN_PROPAGATE or RETURN_NO_PROPAGATE.  If no propagate,
* then the return value in ret_val is good.
*/
int InterpretAtMessage(int object_id,class_node* c,message_node* m,
                  int num_sent_parms,
                  parm_node sent_parms[],val_type *ret_val)
{
   int parm_id;
   int i, j;
   double startTime;
   val_type parm_init_value;
   local_var_type local_vars;
   char num_locals, num_parms;
   Bool found_parm;

   // Time messages.
   if (kod_stat.debugtime)
      startTime = GetMicroCountDouble();

   num_locals = get_byte();
   num_parms = get_byte();

   local_vars.num_locals = num_locals + num_parms;

   if (local_vars.num_locals > MAX_LOCALS)
   {
      dprintf("InterpretAtMessage found too many locals and parms for OBJECT %i CLASS %s MESSAGE %s (%s) aborting and returning NIL\n",
         object_id,
         c? c->class_name : "(unknown)",
         m? GetNameByID(m->message_id) : "(unknown)",
         BlakodDebugInfo());
      (*ret_val).int_val = NIL;
      return RETURN_NO_PROPAGATE;
   }

   /* both table and call parms are sorted */

   j = 0;
   i = 0;

   for (;i<num_parms;i++)
   {
      parm_id = get_int(); /* match this with parameters */
      parm_init_value.int_val = get_int();

      /* look if we have a value for this parm */
      found_parm = False;
      j = 0;         /* don't assume sorted for now */
      while (j < num_sent_parms)
      {
         if (sent_parms[j].name_id == parm_id)
         {
         /* assuming no RetrieveValue needed here, since InterpretCall
            does that for us */
            local_vars.locals[i].int_val = sent_parms[j].value;
            found_parm = True;
            j++;
            break;
         }
         j++;
      }

      if (!found_parm)
         local_vars.locals[i].int_val = parm_init_value.int_val;
   }

   // Init all non-parm locals to NIL
   for (;i < local_vars.num_locals; ++i)
      local_vars.locals[i].int_val = NIL;

   for(;;)         /* returns when gets a blakod return */
   {
      /* infinite loop check */
      if (++num_interpreted > MAX_BLAKOD_STATEMENTS)
      {
         bprintf("InterpretAtMessage interpreted too many instructions--infinite loop?\n");

         dprintf("Infinite loop at depth %i\n", message_depth);
         dprintf("  OBJECT %i CLASS %s MESSAGE %s (%s) aborting and returning NIL\n",
            object_id,
            c? c->class_name : "(unknown)",
            m? GetNameByID(m->message_id) : "(unknown)",
            BlakodDebugInfo());

         dprintf("  Local variables:\n");
         for (i=0;i<local_vars.num_locals;i++)
         {
            dprintf("  %3i : %s %5i\n", i,
               GetTagName(local_vars.locals[i]),
               local_vars.locals[i].v.data);
         }

         (*ret_val).int_val = NIL;
         return RETURN_NO_PROPAGATE;
      }

      // Get the opcode for this operation.
      char *op_id = bkod++;

      // Opcode counter - disabled on live server (unnecessary overhead).
#if KOD_OPCODE_TESTING
      kod_stat.opcode_count[*op_id]++;
#endif

      // Zero opcode is return.
      if (!*op_id)
      {
         if (kod_stat.debugtime)
         {
            m->total_call_time += (GetMicroCountDouble() - startTime);
            m->timed_call_count++;
         }
         else
         {
            m->untimed_call_count++;
         }
         opcode_data *opcode = (opcode_data *)bkod++;

         if (opcode->source2 == PROPAGATE)
            return RETURN_PROPAGATE;
         else
         {
            bkod_type data;
            data = get_int();
            *ret_val = RetrieveValue(object_id, &local_vars, opcode->source1, data);
            return RETURN_NO_PROPAGATE;
         }
      }

      /*
      // Error check disabled - if we're not reading opcodes correctly,
      // something has gone very wrong and we shouldn't even save the game
      // before crashing because any recent data changes (e.g. property contents)
      // could be wrong. Easy to debug this using the pdb so an error
      // message isn't necessary
      if (*op_id < 0 || *op_id >= NUMBER_OF_OPCODES)
      {
         // Fatal error.
         bprintf("InterpretAtMessage found INVALID OPCODE command %i.  die.\n",
            *op_id);
         FlushDefaultChannels();
         continue;
      }*/

      // Otherwise call the opcode function.
#if KOD_OPCODE_TESTING
      double startOpTime = GetMicroCountDouble();
#endif
      opcode_table[*op_id](object_id, &local_vars);
#if KOD_OPCODE_TESTING
      kod_stat.opcode_total_time[*op_id] += (GetMicroCountDouble() - startOpTime);
#endif
   }
}

/* RetrieveValue used to be here, but is inline, and used in ccode.c too, so it's
in sendmsg.h now */

__forceinline void StoreValue(int object_id, local_var_type *local_vars,
                         int data_type, int data, val_type new_data)
{
   object_node *o;

   switch (data_type)
   {
   case LOCAL_VAR : 
      if (data < 0 || data >= local_vars->num_locals)
      {
         eprintf("[%s] StoreValue can't write to illegal local var %i\n",
            BlakodDebugInfo(),data);
         return;
      }
      local_vars->locals[data].int_val = new_data.int_val;
      break;

   case PROPERTY : 
      o = GetObjectByID(object_id);
      if (o == NULL)
      {
         eprintf("[%s] StoreValue can't find object %i\n",
            BlakodDebugInfo(),object_id);
         return;
      }
      // num_props includes self, so the max property is stored at [num_props - 1]
      if (data < 0 || data >= o->num_props)
      {
         eprintf("[%s] StoreValue can't write to illegal property %i (max %i)\n",
            BlakodDebugInfo(), data, o->num_props - 1);
         return;
      }
      o->p[data].val.int_val = new_data.int_val; 
      break;

   default :
      eprintf("[%s] StoreValue can't identify type %i\n",
         BlakodDebugInfo(),data_type); 
      break;
   }
}

__forceinline void StoreLocal(local_var_type *local_vars, int data, val_type new_data)
{
   if (data < 0 || data >= local_vars->num_locals)
   {
      eprintf("[%s] StoreLocal can't write to illegal local var %i\n",
         BlakodDebugInfo(), data);
      return;
   }
   local_vars->locals[data].int_val = new_data.int_val;
}

__forceinline void StoreProperty(int object_id, int data, val_type new_data)
{
   object_node *o;

   o = GetObjectByIDInterp(object_id);
   if (o == NULL)
   {
      eprintf("[%s] StoreValue can't find object %i\n",
         BlakodDebugInfo(), object_id);
      return;
   }
   // num_props includes self, so the max property is stored at [num_props - 1]
   if (data < 0 || data >= o->num_props)
   {
      eprintf("[%s] StoreValue can't write to illegal property %i (max %i)\n",
         BlakodDebugInfo(), data, o->num_props - 1);
      return;
   }
   o->p[data].val.int_val = new_data.int_val;
}

char *BlakodDebugInfo()
{
   static char s[100];
   class_node *c;

   if (kod_stat.interpreting_class == INVALID_CLASS)
   {
      sprintf(s,"Server");
   }
   else
   {
      c = GetClassByID(kod_stat.interpreting_class);
      if (c == NULL)
         sprintf(s,"Invalid class %i",kod_stat.interpreting_class);
      else
         sprintf(s,"%s (%i)",c->fname,GetSourceLine(c,bkod));
   }
   return s;
}

char *BlakodStackInfo()
{
   static char buf[5000];
   class_node *c;
   int i;

   buf[0] = '\0';
   for (i=message_depth-1;i>=0;i--)
   {
      char s[1000];
      if (stack[i].class_id == INVALID_CLASS)
      {
         sprintf(s,"Server");
      }
      else
      {
         c = GetClassByID(stack[i].class_id);
         if (c == NULL)
            sprintf(s,"Invalid class %i",stack[i].class_id);
         else
         {
            char *bp;
            char *class_name;
            char buf2[200];
            char parms[800];
            int j;

            /* for current frame, stack[] has pointer at beginning of function;
               use current pointer instead */
            bp = stack[i].bkod_ptr;
            if (i == message_depth-1)
               bp = bkod;

            class_name = "(unknown)";
            if (c->class_name)
               class_name = c->class_name;
            /* use %.*s with a fixed string of pluses to get exactly one plus per
               propagate depth */
            sprintf(s,"%.*s%s::%s",stack[i].propagate_depth,"++++++++++++++++++++++",
               class_name,GetNameByID(stack[i].message_id));
            strcat(s,"(");
            parms[0] = '\0';
            for (j=0;j<stack[i].num_parms;j++)
            {
               val_type val;
               val.int_val = stack[i].parms[j].value;
               sprintf(buf2,"#%s=%s %s",GetNameByID(stack[i].parms[j].name_id),
                       GetTagName(val),GetDataName(val));
               if (j > 0)
                  strcat(parms,",");
               strcat(parms,buf2);
            }
            strcat(s,parms);
            strcat(s,")");
            sprintf(buf2," %s (%i)",c->fname,GetSourceLine(c,bp));
            strcat(s,buf2);
         }
      }
      if (i < message_depth-1)
         strcat(buf,"\n");
      strcat(buf,s);
      if (strlen(buf) > sizeof(buf) - 1000)
      {
         strcat(buf,"\n...and more");
         break;
      }
   }
   return buf;
}

// New opcodes

// Goto instructions, separate implementations depending on what type of data
// is being checked (constant, local, property, classvar) for speed. All
// address offsets take into account position of bkod pointer after reading
// data (i.e. offset is from the point at which the offset is used).

// OP_GOTO_UNCOND: 1 byte instruction, 4 byte address
void InterpretGotoUncond(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   bkod += dest_addr;
}
// OP_GOTO_IF_TRUE_C: 1 byte instruction, 4 byte address, 4 byte constant
void InterpretGotoIfTrueConstant(int object_id, local_var_type *local_vars)
{
   val_type var_check;

   int dest_addr = get_int();
   var_check.int_val = get_int();

   if (var_check.v.data != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_TRUE_L: 1 byte instruction, 4 byte address, 4 byte local ID
void InterpretGotoIfTrueLocal(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   if (local_vars->locals[var_check].v.data != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_TRUE_P: 1 byte instruction, 4 byte address, 4 property ID
void InterpretGotoIfTrueProperty(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   object_node *o = GetObjectByIDInterp(object_id);
   if (!o)
   {
      bprintf("Critical error, NULL object in InterpretGotoIfTrueProperty!\n");
      FlushDefaultChannels();
   }
   else if (o->p[var_check].val.v.data != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_TRUE_V: 1 byte instruction, 4 byte address, 4 classvar ID
void InterpretGotoIfTrueClassVar(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();
   val_type check_data = RetrieveClassVar(object_id, var_check);

   if (check_data.v.data != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_FALSE_C: 1 byte instruction, 4 byte address, 4 byte constant
void InterpretGotoIfFalseConstant(int object_id, local_var_type *local_vars)
{
   val_type var_check;

   int dest_addr = get_int();
   var_check.int_val = get_int();

   if (var_check.v.data == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_FALSE_L: 1 byte instruction, 4 byte address, 4 byte local ID
void InterpretGotoIfFalseLocal(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   if (local_vars->locals[var_check].v.data == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_FALSE_P: 1 byte instruction, 4 byte address, 4 property ID
void InterpretGotoIfFalseProperty(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   object_node *o = GetObjectByIDInterp(object_id);
   if (!o)
   {
      bprintf("Critical error, NULL object in InterpretGotoIfFalseProperty!\n");
      FlushDefaultChannels();
   }
   else if (o->p[var_check].val.v.data == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_FALSE_V: 1 byte instruction, 4 byte address, 4 classvar ID
void InterpretGotoIfFalseClassVar(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();
   val_type check_data = RetrieveClassVar(object_id, var_check);

   if (check_data.v.data == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NULL_C: 1 byte instruction, 4 byte address, 4 byte constant
void InterpretGotoIfNullConstant(int object_id, local_var_type *local_vars)
{
   val_type var_check;

   int dest_addr = get_int();
   var_check.int_val = get_int();

   if (var_check.int_val == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NULL_L: 1 byte instruction, 4 byte address, 4 byte local ID
void InterpretGotoIfNullLocal(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   if (local_vars->locals[var_check].int_val == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NULL_P: 1 byte instruction, 4 byte address, 4 property ID
void InterpretGotoIfNullProperty(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   object_node *o = GetObjectByIDInterp(object_id);
   if (!o)
   {
      bprintf("Critical error, NULL object in InterpretGotoIfNullProperty!\n");
      FlushDefaultChannels();
   }
   else if (o->p[var_check].val.int_val == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NULL_V: 1 byte instruction, 4 byte address, 4 classvar ID
void InterpretGotoIfNullClassVar(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();
   val_type check_data = RetrieveClassVar(object_id, var_check);

   if (check_data.int_val == 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NEQ_NULL_C: 1 byte instruction, 4 byte address, 4 byte constant
void InterpretGotoIfNeqNullConstant(int object_id, local_var_type *local_vars)
{
   val_type var_check;

   int dest_addr = get_int();
   var_check.int_val = get_int();

   if (var_check.int_val != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NEQ_NULL_L: 1 byte instruction, 4 byte address, 4 byte local ID
void InterpretGotoIfNeqNullLocal(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   if (local_vars->locals[var_check].int_val != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NEQ_NULL_P: 1 byte instruction, 4 byte address, 4 property ID
void InterpretGotoIfNeqNullProperty(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   object_node *o = GetObjectByIDInterp(object_id);
   if (!o)
   {
      bprintf("Critical error, NULL object in InterpretGotoIfNeqNullProperty!\n");
      FlushDefaultChannels();
   }
   else if (o->p[var_check].val.int_val != 0)
      bkod += dest_addr;
}
// OP_GOTO_IF_NEQ_NULL_V: 1 byte instruction, 4 byte address, 4 classvar ID
void InterpretGotoIfNeqNullClassVar(int object_id, local_var_type *local_vars)
{
   int dest_addr = get_int();
   int var_check = get_int();

   val_type check_data = RetrieveClassVar(object_id, var_check);
   if (check_data.int_val != 0)
      bkod += dest_addr;
}

// Call instructions, separate implementations for where result is
// stored (none, local, property). Note that the parm arrays no longer
// need to be bounds checked, as the compiler checks this when it
// outputs the opcodes.

// 3 opcodes for calls with no settings (named parameters), 3 opcodes
// for calls with settings.
// OP_CALL_STORE_NONE: 1 byte instruction, 1 byte call ID, call data.
void InterpretCallStoreNone(int object_id, local_var_type *local_vars)
{
   parm_node normal_parm_array[MAX_C_PARMS];
   unsigned char info, num_normal_parms;
   val_type call_return;

   info = get_byte(); /* get function id */

   num_normal_parms = get_byte();

   for (int i = 0; i < num_normal_parms; ++i)
   {
      normal_parm_array[i].type = get_byte();
      normal_parm_array[i].value = get_int();
   }

   // Time messages.
   if (kod_stat.debugtime)
   {
      double startTime = GetMicroCountDouble();
      /* increment timed count of the c function, for profiling info */
      kod_stat.c_count_timed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, 0, 0);
      kod_stat.ccall_total_time[info] += (GetMicroCountDouble() - startTime);
   }
   else
   {
      /* increment untimed count of the c function, for profiling info */
      kod_stat.c_count_untimed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, 0, 0);
   }
}
// OP_CALL_STORE_L: 1 byte instruction, 1 byte call ID, 4 bytes local ID, call data.
void InterpretCallStoreLocal(int object_id, local_var_type *local_vars)
{
   parm_node normal_parm_array[MAX_C_PARMS];
   unsigned char info, num_normal_parms;
   int assign_index;
   val_type call_return;

   info = get_byte(); /* get function id */

   // Local var ID
   assign_index = get_int();

   num_normal_parms = get_byte();

   for (int i = 0; i < num_normal_parms; ++i)
   {
      normal_parm_array[i].type = get_byte();
      normal_parm_array[i].value = get_int();
   }

   // Time messages.
   if (kod_stat.debugtime)
   {
      double startTime = GetMicroCountDouble();
      /* increment timed count of the c function, for profiling info */
      kod_stat.c_count_timed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, 0, 0);
      kod_stat.ccall_total_time[info] += (GetMicroCountDouble() - startTime);
   }
   else
   {
      /* increment untimed count of the c function, for profiling info */
      kod_stat.c_count_untimed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, 0, 0);
   }
   StoreLocal(local_vars, assign_index, call_return);
}
// OP_CALL_STORE_P: 1 byte instruction, 1 byte call ID, 4 bytes property ID, call data.
void InterpretCallStoreProperty(int object_id, local_var_type *local_vars)
{
   parm_node normal_parm_array[MAX_C_PARMS];
   unsigned char info, num_normal_parms;
   int assign_index;
   val_type call_return;

   info = get_byte(); /* get function id */

   // Property ID
   assign_index = get_int();

   num_normal_parms = get_byte();

   for (int i = 0; i < num_normal_parms; ++i)
   {
      normal_parm_array[i].type = get_byte();
      normal_parm_array[i].value = get_int();
   }

   // Time messages.
   if (kod_stat.debugtime)
   {
      double startTime = GetMicroCountDouble();
      /* increment timed count of the c function, for profiling info */
      kod_stat.c_count_timed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, 0, 0);
      kod_stat.ccall_total_time[info] += (GetMicroCountDouble() - startTime);
   }
   else
   {
      /* increment untimed count of the c function, for profiling info */
      kod_stat.c_count_untimed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, 0, 0);
   }
   StoreProperty(object_id, assign_index, call_return);
}
// OP_CALL_SETTINGS_STORE_NONE: 1 byte instruction, 1 byte call ID, call data.
void InterpretCallSettingsStoreNone(int object_id, local_var_type *local_vars)
{
   parm_node normal_parm_array[MAX_C_PARMS], name_parm_array[MAX_NAME_PARMS];
   unsigned char info, num_normal_parms, num_name_parms, initial_type;
   int initial_value;
   val_type call_return, name_val;

   info = get_byte(); /* get function id */

   num_normal_parms = get_byte();

   for (int i = 0; i < num_normal_parms; ++i)
   {
      normal_parm_array[i].type = get_byte();
      normal_parm_array[i].value = get_int();
   }

   num_name_parms = get_byte();

   for (int i = 0; i < num_name_parms; ++i)
   {
      name_parm_array[i].name_id = get_int();

      initial_type = get_byte();
      initial_value = get_int();

      /* translate to literal now, because won't have local vars
      if nested call to sendmessage again */
      /* maybe only need to do this in call to sendmessage and postmessage? */
      name_val = RetrieveValue(object_id, local_vars, initial_type, initial_value);
      name_parm_array[i].value = name_val.int_val;
   }

   // Time messages.
   if (kod_stat.debugtime)
   {
      double startTime = GetMicroCountDouble();
      /* increment timed count of the c function, for profiling info */
      kod_stat.c_count_timed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, num_name_parms, name_parm_array);
      kod_stat.ccall_total_time[info] += (GetMicroCountDouble() - startTime);
   }
   else
   {
      /* increment untimed count of the c function, for profiling info */
      kod_stat.c_count_untimed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, num_name_parms, name_parm_array);
   }
}
// OP_CALL_SETTINGS_STORE_L: 1 byte instruction, 1 byte call ID, 4 bytes local ID, call data.
void InterpretCallSettingsStoreLocal(int object_id, local_var_type *local_vars)
{
   parm_node normal_parm_array[MAX_C_PARMS], name_parm_array[MAX_NAME_PARMS];
   unsigned char info, num_normal_parms, num_name_parms, initial_type;
   int initial_value;
   int assign_index;
   val_type call_return, name_val;

   info = get_byte(); /* get function id */

   // Local var ID
   assign_index = get_int();

   num_normal_parms = get_byte();

   for (int i = 0; i < num_normal_parms; ++i)
   {
      normal_parm_array[i].type = get_byte();
      normal_parm_array[i].value = get_int();
   }

   num_name_parms = get_byte();

   for (int i = 0; i < num_name_parms; ++i)
   {
      name_parm_array[i].name_id = get_int();

      initial_type = get_byte();
      initial_value = get_int();

      /* translate to literal now, because won't have local vars
      if nested call to sendmessage again */
      /* maybe only need to do this in call to sendmessage and postmessage? */
      name_val = RetrieveValue(object_id, local_vars, initial_type, initial_value);
      name_parm_array[i].value = name_val.int_val;
   }

   // Time messages.
   if (kod_stat.debugtime)
   {
      double startTime = GetMicroCountDouble();
      /* increment timed count of the c function, for profiling info */
      kod_stat.c_count_timed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, num_name_parms, name_parm_array);
      kod_stat.ccall_total_time[info] += (GetMicroCountDouble() - startTime);
   }
   else
   {
      /* increment untimed count of the c function, for profiling info */
      kod_stat.c_count_untimed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, num_name_parms, name_parm_array);
   }
   StoreLocal(local_vars, assign_index, call_return);
}
// OP_CALL_SETTINGS_STORE_P: 1 byte instruction, 1 byte call ID, 4 bytes property ID, call data.
void InterpretCallSettingsStoreProperty(int object_id, local_var_type *local_vars)
{
   parm_node normal_parm_array[MAX_C_PARMS], name_parm_array[MAX_NAME_PARMS];
   unsigned char info, num_normal_parms, num_name_parms, initial_type;
   int initial_value;
   int assign_index;
   val_type call_return, name_val;

   info = get_byte(); /* get function id */

   // Property ID
   assign_index = get_int();

   num_normal_parms = get_byte();

   for (int i = 0; i < num_normal_parms; ++i)
   {
      normal_parm_array[i].type = get_byte();
      normal_parm_array[i].value = get_int();
   }

   num_name_parms = get_byte();

   for (int i = 0; i < num_name_parms; ++i)
   {
      name_parm_array[i].name_id = get_int();

      initial_type = get_byte();
      initial_value = get_int();

      /* translate to literal now, because won't have local vars
      if nested call to sendmessage again */
      /* maybe only need to do this in call to sendmessage and postmessage? */
      name_val = RetrieveValue(object_id, local_vars, initial_type, initial_value);
      name_parm_array[i].value = name_val.int_val;
   }

   // Time messages.
   if (kod_stat.debugtime)
   {
      double startTime = GetMicroCountDouble();
      /* increment timed count of the c function, for profiling info */
      kod_stat.c_count_timed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, num_name_parms, name_parm_array);
      kod_stat.ccall_total_time[info] += (GetMicroCountDouble() - startTime);
   }
   else
   {
      /* increment untimed count of the c function, for profiling info */
      kod_stat.c_count_untimed[info]++;
      call_return.int_val = ccall_table[info](object_id, local_vars, num_normal_parms,
         normal_parm_array, num_name_parms, name_parm_array);
   }
   StoreProperty(object_id, assign_index, call_return);
}

// Unary instructions. Two opcodes for each, depending on where we
// store the result (local or property). Each unary instruction has:
// 1 byte opcode, 1 byte source type, 4 bytes source ID, 4 bytes dest ID.

// Macros for building unary instructions.
#define UNARY_OP_INIT \
   opcode_data *opcode = (opcode_data *)bkod++; \
   unopdata_node *opnode = (unopdata_node*)((bkod += sizeof(unopdata_node)) - sizeof(unopdata_node));
#define UNARY_OP_RETRIEVE(a, b, c, d) \
   val_type source_data = RetrieveValue(a, b, c->source1, d->source);
#define INT_CHECK_UNARY(a, b) \
   if (a.v.tag != TAG_INT) \
   { \
      bprintf(b, a.v.tag, a.v.data); \
      return; \
   }

// OP_UNARY_NOT_L: Unary not, store in local.
void InterpretUnaryNot_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryNot_L can't NOT non-int %i,%i\n")

   source_data.v.data = !source_data.v.data;
   StoreLocal(local_vars, opnode->dest, source_data);
}
// OP_UNARY_NOT_P: Unary not, store in property.
void InterpretUnaryNot_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryNot_P can't NOT non-int %i,%i\n")

   source_data.v.data = !source_data.v.data;
   StoreProperty(object_id, opnode->dest, source_data);
}
// OP_UNARY_NEG_L: Unary negation, store in local.
void InterpretUnaryNeg_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryNeg_L can't negate non-int %i,%i\n")

   source_data.v.data = -source_data.v.data;
   StoreLocal(local_vars, opnode->dest, source_data);
}
// OP_UNARY_NEG_P: Unary negation, store in property.
void InterpretUnaryNeg_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryNeg_P can't negate non-int %i,%i\n")

   source_data.v.data = -source_data.v.data;
   StoreProperty(object_id, opnode->dest, source_data);
}
// OP_UNARY_NONE_L: Unary assignment, store in local.
void InterpretUnaryNone_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   StoreLocal(local_vars, opnode->dest, source_data);
}
// OP_UNARY_NONE_P: Unary assignment, store in property.
void InterpretUnaryNone_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   StoreProperty(object_id, opnode->dest, source_data);
}
// OP_UNARY_BITNOT_L: Unary bitwise-not, store in local.
void InterpretUnaryBitNot_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryBitNot_L can't bitwise-not non-int %i,%i\n")

   source_data.v.data = ~source_data.v.data;
   StoreLocal(local_vars, opnode->dest, source_data);
}
// OP_UNARY_BITNOT_P: Unary bitwise-not, store in property.
void InterpretUnaryBitNot_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryBitNot_P can't bitwise-not non-int %i,%i\n")

   source_data.v.data = ~source_data.v.data;
   StoreProperty(object_id, opnode->dest, source_data);
}

// Increment/decrement only have single implementations:
// 1 byte opcode, 1 byte source1/source2 (dest) type, 4 bytes source ID, 4 bytes dest ID.
// OP_UNARY_POSTINC: Unary post-increment.
void InterpretUnaryPostInc(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryPostInc can't post-increment non-int %i,%i\n")

   if (opnode->source != opnode->dest
      || opcode->source1 != opcode->source2)
      StoreValue(object_id, local_vars, opcode->source2, opnode->dest, source_data);
   ++source_data.v.data;
   StoreValue(object_id, local_vars, opcode->source1, opnode->source, source_data);
}
// OP_UNARY_POSTDEC: Unary post-decrement.
void InterpretUnaryPostDec(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryPostDec can't post-decrement non-int %i,%i\n")

   if (opnode->source != opnode->dest
      || opcode->source1 != opcode->source2)
      StoreValue(object_id, local_vars, opcode->source2, opnode->dest, source_data);
   --source_data.v.data;
   StoreValue(object_id, local_vars, opcode->source1, opnode->source, source_data);
}
// OP_UNARY_PREINC: Unary pre-increment.
void InterpretUnaryPreInc(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryPostDec can't pre-increment non-int %i,%i\n")

   ++source_data.v.data;
   if (opnode->source != opnode->dest
      || opcode->source1 != opcode->source2)
      StoreValue(object_id, local_vars, opcode->source1, opnode->source, source_data);
   StoreValue(object_id, local_vars, opcode->source2, opnode->dest, source_data);
}
// OP_UNARY_PREDEC: Unary pre-decrement.
void InterpretUnaryPreDec(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_UNARY(source_data, "InterpretUnaryPreDec can't pre-decrement non-int %i,%i\n")

   --source_data.v.data;
   if (opnode->source != opnode->dest
      || opcode->source1 != opcode->source2)
      StoreValue(object_id, local_vars, opcode->source1, opnode->source, source_data);
   StoreValue(object_id, local_vars, opcode->source2, opnode->dest, source_data);
}

// Unary list instructions.
#define LIST_CHECK_UNARY(a, b) \
   if (a.v.tag != TAG_LIST) \
   { \
      bprintf(b, a.v.tag, a.v.data); \
      return; \
   }
#define INVALID_LIST_CHECK_UNARY(a, b) \
   if (!IsListNodeByID(a.v.data)) \
   { \
      bprintf(b, a.v.tag, a.v.data); \
      return; \
   }

// OP_UNARY_FIRST_L: Unary First (data from list node), store in local.
void InterpretUnaryFirst_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   LIST_CHECK_UNARY(source_data, "InterpretUnaryFirst_L object %i can't take First of a non-list %i,%i\n")
   INVALID_LIST_CHECK_UNARY(source_data, "InterpretUnaryFirst_L object %i can't take First of an invalid list %i,%i\n")

   list_node *l = GetListNodeByID(source_data.v.data);

   StoreLocal(local_vars, opnode->dest, l ? l->first : nil_val);
}
// OP_UNARY_FIRST_P: Unary First (data from list node), store in property.
void InterpretUnaryFirst_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   LIST_CHECK_UNARY(source_data, "InterpretUnaryFirst_P object %i can't take First of a non-list %i,%i\n")
   INVALID_LIST_CHECK_UNARY(source_data, "InterpretUnaryFirst_P object %i can't take First of an invalid list %i,%i\n")

   list_node *l = GetListNodeByID(source_data.v.data);

   StoreProperty(object_id, opnode->dest, l ? l->first : nil_val);
}
// OP_REST_L: Unary Rest (data from list node), store in local.
void InterpretUnaryRest_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   LIST_CHECK_UNARY(source_data, "InterpretUnaryRest_L object %i can't take Rest of a non-list %i,%i\n")
   INVALID_LIST_CHECK_UNARY(source_data, "InterpretUnaryRest_L object %i can't take Rest of an invalid list %i,%i\n")

   list_node *l = GetListNodeByID(source_data.v.data);

   StoreLocal(local_vars, opnode->dest, l ? l->rest : nil_val);
}
// OP_REST_P: Unary Rest (data from list node), store in property.
void InterpretUnaryRest_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   LIST_CHECK_UNARY(source_data, "InterpretUnaryRest_P object %i can't take Rest of a non-list %i,%i\n")
   INVALID_LIST_CHECK_UNARY(source_data, "InterpretUnaryRest_P object %i can't take Rest of an invalid list %i,%i\n")

   list_node *l = GetListNodeByID(source_data.v.data);

   StoreProperty(object_id, opnode->dest, l ? l->rest : nil_val);
}
// OP_GETCLASS_L: Unary GetClass (return class ID or $), store in local.
void InterpretUnaryGetClass_L(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT;
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode);
   val_type store_val;

   if (source_data.v.tag != TAG_OBJECT)
   {
      bprintf("InterpretUnaryGetClass_L can't deal with non-object %i,%i\n",
         source_data.v.tag, source_data.v.data);
      StoreLocal(local_vars, opnode->dest, nil_val);
      return;
   }

   object_node *o = GetObjectByIDInterp(source_data.v.data);
   if (o == NULL)
   {
      bprintf("InterpretUnaryGetClass_L can't find object %i\n", source_data.v.data);
      StoreLocal(local_vars, opnode->dest, nil_val);
      return;
   }

   store_val.v.tag = TAG_CLASS;
   store_val.v.data = o->class_id;

   StoreLocal(local_vars, opnode->dest, store_val);
}
// OP_GETCLASS_P: Unary GetClass (return class ID or $), store in local.
void InterpretUnaryGetClass_P(int object_id, local_var_type *local_vars)
{
   UNARY_OP_INIT;
   UNARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode);
   val_type store_val;

   if (source_data.v.tag != TAG_OBJECT)
   {
      bprintf("InterpretUnaryGetClass_P can't deal with non-object %i,%i\n",
         source_data.v.tag, source_data.v.data);
      StoreProperty(object_id, opnode->dest, nil_val);
      return;
   }

   object_node *o = GetObjectByIDInterp(source_data.v.data);
   if (o == NULL)
   {
      bprintf("InterpretUnaryGetClass_P can't find object %i\n", source_data.v.data);
      StoreProperty(object_id, opnode->dest, nil_val);
      return;
   }
   store_val.v.tag = TAG_CLASS;
   store_val.v.data = o->class_id;

   StoreProperty(object_id, opnode->dest, store_val);
}

// Binary instructions. Two opcodes for each, depending on where we
// store the result (local or property). Each binary instruction has:
// 1 byte opcode, 1 byte source1/source2 type, 8 bytes source IDs, 4 bytes dest ID.

// Macros for building binary instructions.
#define BINARY_OP_INIT \
   opcode_data *opcode = (opcode_data *)bkod++; \
   binopdata_node *opnode = (binopdata_node*)((bkod += sizeof(binopdata_node)) - sizeof(binopdata_node));
#define BINARY_OP_RETRIEVE(a, b, c, d) \
   val_type source1_data = RetrieveValue(a, b, c->source1, d->source1); \
   val_type source2_data = RetrieveValue(a, b, c->source2, d->source2);
#define INT_CHECK_BINARY(a, b, c) \
   if (a.v.tag != TAG_INT || b.v.tag != TAG_INT) \
   { \
      bprintf(c, a.v.tag, a.v.data, b.v.tag, b.v.data); \
      return; \
   }
#define DIV_0_CHECK(a, b) \
   if (a.v.data == 0) \
   { \
      bprintf(b); \
      return; \
   }

// OP_BINARY_ADD_L: Binary add, store in local.
void InterpretBinaryAdd_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode);
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryAdd_L can't add 2 vars %i,%i and %i,%i\n")

   source1_data.v.data += source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_ADD_P: Binary add, store in property.
void InterpretBinaryAdd_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode);
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryAdd_P can't add 2 vars %i,%i and %i,%i\n")

   source1_data.v.data += source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_SUB_L: Binary subtract, store in local.
void InterpretBinarySub_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinarySub_L can't sub 2 vars %i,%i and %i,%i\n")

   source1_data.v.data -= source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_SUB_P: Binary subtract, store in property.
void InterpretBinarySub_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinarySub_P can't sub 2 vars %i,%i and %i,%i\n")

   source1_data.v.data -= source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_MUL_L: Binary multiply, store in local.
void InterpretBinaryMul_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryMul_L can't mul 2 vars %i,%i and %i,%i\n")

   source1_data.v.data *= source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_MUL_P: Binary multiply, store in property.
void InterpretBinaryMul_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryMul_P can't mul 2 vars %i,%i and %i,%i\n")

   source1_data.v.data *= source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_DIV_L: Binary divide, store in local.
void InterpretBinaryDiv_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryDiv_L can't div 2 vars %i,%i and %i,%i\n")
   DIV_0_CHECK(source2_data, "InterpretBinaryDiv_L can't div by 0\n")

   source1_data.v.data /= source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_DIV_P: Binary divide, store in property.
void InterpretBinaryDiv_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryDiv_P can't div 2 vars %i,%i and %i,%i\n")
   DIV_0_CHECK(source2_data, "InterpretBinaryDiv_P can't div by 0\n")

   source1_data.v.data /= source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_MOD_L: Binary modulus, store in local.
void InterpretBinaryMod_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryMod_L can't mod 2 vars %i,%i and %i,%i\n")
   DIV_0_CHECK(source2_data, "InterpretBinaryMod_L can't div by 0\n")

   source1_data.v.data = abs(source1_data.v.data % source2_data.v.data);
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_MOD_P: Binary modulus, store in property.
void InterpretBinaryMod_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryMod_P can't mod 2 vars %i,%i and %i,%i\n")
   DIV_0_CHECK(source2_data, "InterpretBinaryMod_P can't div by 0\n")

   source1_data.v.data = abs(source1_data.v.data % source2_data.v.data);
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_AND_L: Binary AND, store in local.
void InterpretBinaryAnd_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryAnd_L can't AND 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data && source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_AND_P: Binary AND, store in property.
void InterpretBinaryAnd_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryAnd_P can't AND 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data && source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_OR_L: Binary OR, store in local.
void InterpretBinaryOr_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryOr_L can't OR 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data || source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_OR_P: Binary OR, store in property.
void InterpretBinaryOr_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryOr_P can't OR 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data || source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_EQ_L: Binary equality, store in local.
void InterpretBinaryEqual_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)

   if (source1_data.v.tag != source2_data.v.tag)
      source1_data.v.data = false;
   else
      source1_data.v.data = source1_data.v.data == source2_data.v.data;
   source1_data.v.tag = TAG_INT;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_EQ_P: Binary equality, store in property.
void InterpretBinaryEqual_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)

   if (source1_data.v.tag != source2_data.v.tag)
      source1_data.v.data = false;
   else
      source1_data.v.data = source1_data.v.data == source2_data.v.data;
   source1_data.v.tag = TAG_INT;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_NEQ_L: Binary non-equality, store in local.
void InterpretBinaryNEq_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)

   if (source1_data.v.tag != source2_data.v.tag)
      source1_data.v.data = true;
   else
      source1_data.v.data = source1_data.v.data != source2_data.v.data;
   source1_data.v.tag = TAG_INT;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_NEQ_P: Binary non-equality, store in property.
void InterpretBinaryNEq_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)

   if (source1_data.v.tag != source2_data.v.tag)
      source1_data.v.data = true;
   else
      source1_data.v.data = source1_data.v.data != source2_data.v.data;
   source1_data.v.tag = TAG_INT;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_LESS_L: Binary less than, store in local.
void InterpretBinaryLess_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryLess_L can't < 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data < source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_LESS_P: Binary less than, store in property.
void InterpretBinaryLess_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryLess_P can't < 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data < source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_GREATER_L: Binary greater than, store in local.
void InterpretBinaryGreater_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryGreater_L can't > 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data > source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_GREATER_P: Binary greater than, store in property.
void InterpretBinaryGreater_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryGreater_P can't > 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data > source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_LEQ_L: Binary <=, store in local.
void InterpretBinaryLEq_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryLEq_L can't <= 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data <= source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_LEQ_P: Binary <=, store in property.
void InterpretBinaryLEq_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryLEq_P can't <= 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data <= source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_GEQ_L: Binary >=, store in local.
void InterpretBinaryGEq_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryGEq_L can't >= 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data >= source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_GEQ_P: Binary >=, store in property.
void InterpretBinaryGEq_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryGEq_P can't >= 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data >= source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_BITAND_L: Binary bitwise-and, store in local.
void InterpretBinaryBitAnd_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryBitAnd_L can't & 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data & source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_BITAND_P: Binary bitwise-and, store in property.
void InterpretBinaryBitAnd_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryBitAnd_P can't & 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data & source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}
// OP_BINARY_BITOR_L: Binary bitwise-or, store in local.
void InterpretBinaryBitOr_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryBitOr_L can't | 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data | source2_data.v.data;
   StoreLocal(local_vars, opnode->dest, source1_data);
}
// OP_BINARY_BITOR_P: Binary bitwise-or, store in property.
void InterpretBinaryBitOr_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   BINARY_OP_RETRIEVE(object_id, local_vars, opcode, opnode)
   INT_CHECK_BINARY(source1_data, source2_data, "InterpretBinaryBitOr_P can't | 2 vars %i,%i and %i,%i\n")

   source1_data.v.data = source1_data.v.data | source2_data.v.data;
   StoreProperty(object_id, opnode->dest, source1_data);
}

// 'IsClass' binary instructions. Two opcodes used when the class ID is known
// to be a constant (compiler checked), for storing to local or property. Two
// opcodes for when the class ID is either a classvar, property or local,
// storing in either property or local. Layout of instruction is same as other
// binary ops:
// 1 byte opcode, 1 byte source1/source2 type, 8 bytes source IDs, 4 bytes dest ID.

// Macros for building IsClass instructions.
#define ISCLASS_OP_VAR_RETRIEVE(a, b, c, d) \
   val_type object_val = RetrieveValue(a, b, c->source1, d->source1); \
   val_type class_val = RetrieveValue(a, b, c->source2, d->source2);
#define ISCLASS_OP_CONST_RETRIEVE(a, b, c, d) \
   val_type object_val = RetrieveValue(a, b, c->source1, d->source1); \
   val_type class_val = *(val_type *)&opnode->source2;
#define ISCLASS_STORE_FALSE_LOCAL \
   store_val.int_val = KOD_FALSE; \
   StoreLocal(local_vars, opnode->dest, store_val);
#define ISCLASS_STORE_TRUE_LOCAL \
   store_val.int_val = KOD_TRUE; \
   StoreLocal(local_vars, opnode->dest, store_val);
#define ISCLASS_STORE_FALSE_PROP \
   store_val.int_val = KOD_FALSE; \
   StoreProperty(object_id, opnode->dest, store_val);
#define ISCLASS_STORE_TRUE_PROP \
   store_val.int_val = KOD_TRUE; \
   StoreProperty(object_id, opnode->dest, store_val);

// OP_ISCLASS_L: IsClass with variable class ID location, store result in local.
void InterpretIsClass_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   ISCLASS_OP_VAR_RETRIEVE(object_id, local_vars, opcode, opnode)
   val_type store_val;

   if (object_val.v.tag != TAG_OBJECT)
   {
      bprintf("InterpretIsClass_L can't deal with non-object %i,%i\n",
         object_val.v.tag, object_val.v.data);
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   if (class_val.v.tag != TAG_CLASS)
   {
      bprintf("InterpretIsClass_L can't look for non-class %i,%i\n",
         class_val.v.tag, class_val.v.data);
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   object_node *o = GetObjectByID(object_val.v.data);
   if (o == NULL)
   {
      bprintf("InterpretIsClass_L can't find object %i\n", object_val.v.data);
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   class_node *c = GetClassByID(o->class_id);
   if (c == NULL)
   {
      bprintf("InterpretIsClass_L can't find class %i, DIE totally\n", o->class_id);
      FlushDefaultChannels();
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   do
   {
      if (c->class_id == class_val.v.data)
      {
         ISCLASS_STORE_TRUE_LOCAL
         return;
      }
      c = c->super_ptr;
   } while (c != NULL);

   ISCLASS_STORE_FALSE_LOCAL
}
// OP_ISCLASS_P: IsClass with variable class ID location, store result in property.
void InterpretIsClass_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   ISCLASS_OP_VAR_RETRIEVE(object_id, local_vars, opcode, opnode)
   val_type store_val;

   if (object_val.v.tag != TAG_OBJECT)
   {
      bprintf("InterpretIsClass_P can't deal with non-object %i,%i\n",
         object_val.v.tag, object_val.v.data);
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   if (class_val.v.tag != TAG_CLASS)
   {
      bprintf("InterpretIsClass_P can't look for non-class %i,%i\n",
         class_val.v.tag, class_val.v.data);
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   object_node *o = GetObjectByID(object_val.v.data);
   if (o == NULL)
   {
      bprintf("InterpretIsClass_P can't find object %i\n", object_val.v.data);
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   class_node *c = GetClassByID(o->class_id);
   if (c == NULL)
   {
      bprintf("InterpretIsClass_P can't find class %i, DIE totally\n", o->class_id);
      FlushDefaultChannels();
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   do
   {
      if (c->class_id == class_val.v.data)
      {
         ISCLASS_STORE_TRUE_PROP
         return;
      }
      c = c->super_ptr;
   } while (c != NULL);

   ISCLASS_STORE_FALSE_PROP
}
// OP_ISCLASS_CONST_L: IsClass with constant class ID, store result in local.
void InterpretIsClassConst_L(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   ISCLASS_OP_CONST_RETRIEVE(object_id, local_vars, opcode, opnode)
   val_type store_val;

   if (object_val.v.tag != TAG_OBJECT)
   {
      bprintf("InterpretIsClassConst_L can't deal with non-object %i,%i\n",
         object_val.v.tag, object_val.v.data);
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   object_node *o = GetObjectByID(object_val.v.data);
   if (o == NULL)
   {
      bprintf("InterpretIsClassConst_L can't find object %i\n", object_val.v.data);
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   class_node *c = GetClassByID(o->class_id);
   if (c == NULL)
   {
      bprintf("InterpretIsClassConst_L can't find class %i, DIE totally\n", o->class_id);
      FlushDefaultChannels();
      ISCLASS_STORE_FALSE_LOCAL
      return;
   }

   do
   {
      if (c->class_id == class_val.v.data)
      {
         ISCLASS_STORE_TRUE_LOCAL
         return;
      }
      c = c->super_ptr;
   } while (c != NULL);

   ISCLASS_STORE_FALSE_LOCAL
}
// OP_ISCLASS_CONST_P: IsClass with constant class ID, store result in property.
void InterpretIsClassConst_P(int object_id, local_var_type *local_vars)
{
   BINARY_OP_INIT
   ISCLASS_OP_CONST_RETRIEVE(object_id, local_vars, opcode, opnode)
   val_type store_val;

   if (object_val.v.tag != TAG_OBJECT)
   {
      bprintf("InterpretIsClassConst_P can't deal with non-object %i,%i\n",
         object_val.v.tag, object_val.v.data);
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   object_node *o = GetObjectByID(object_val.v.data);
   if (o == NULL)
   {
      bprintf("InterpretIsClassConst_P can't find object %i\n", object_val.v.data);
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   class_node *c = GetClassByID(o->class_id);
   if (c == NULL)
   {
      bprintf("InterpretIsClassConst_P can't find class %i, DIE totally\n", o->class_id);
      FlushDefaultChannels();
      ISCLASS_STORE_FALSE_PROP
      return;
   }

   do
   {
      if (c->class_id == class_val.v.data)
      {
         ISCLASS_STORE_TRUE_PROP
         return;
      }
      c = c->super_ptr;
   } while (c != NULL);

   ISCLASS_STORE_FALSE_PROP
}

void CreateOpcodeTable(void)
{
   opcode_table[OP_GOTO_UNCOND] = InterpretGotoUncond;
   opcode_table[OP_GOTO_IF_TRUE_C] = InterpretGotoIfTrueConstant;
   opcode_table[OP_GOTO_IF_TRUE_L] = InterpretGotoIfTrueLocal;
   opcode_table[OP_GOTO_IF_TRUE_P] = InterpretGotoIfTrueProperty;
   opcode_table[OP_GOTO_IF_TRUE_V] = InterpretGotoIfTrueClassVar;
   opcode_table[OP_GOTO_IF_FALSE_C] = InterpretGotoIfFalseConstant;
   opcode_table[OP_GOTO_IF_FALSE_L] = InterpretGotoIfFalseLocal;
   opcode_table[OP_GOTO_IF_FALSE_P] = InterpretGotoIfFalseProperty;
   opcode_table[OP_GOTO_IF_FALSE_V] = InterpretGotoIfFalseClassVar;

   opcode_table[OP_GOTO_IF_NULL_C] = InterpretGotoIfNullConstant;
   opcode_table[OP_GOTO_IF_NULL_L] = InterpretGotoIfNullLocal;
   opcode_table[OP_GOTO_IF_NULL_P] = InterpretGotoIfNullProperty;
   opcode_table[OP_GOTO_IF_NULL_V] = InterpretGotoIfNullClassVar;

   opcode_table[OP_GOTO_IF_NEQ_NULL_C] = InterpretGotoIfNeqNullConstant;
   opcode_table[OP_GOTO_IF_NEQ_NULL_L] = InterpretGotoIfNeqNullLocal;
   opcode_table[OP_GOTO_IF_NEQ_NULL_P] = InterpretGotoIfNeqNullProperty;
   opcode_table[OP_GOTO_IF_NEQ_NULL_V] = InterpretGotoIfNeqNullClassVar;

   opcode_table[OP_CALL_STORE_NONE] = InterpretCallStoreNone;
   opcode_table[OP_CALL_STORE_L] = InterpretCallStoreLocal;
   opcode_table[OP_CALL_STORE_P] = InterpretCallStoreProperty;
   opcode_table[OP_CALL_SETTINGS_STORE_NONE] = InterpretCallSettingsStoreNone;
   opcode_table[OP_CALL_SETTINGS_STORE_L] = InterpretCallSettingsStoreLocal;
   opcode_table[OP_CALL_SETTINGS_STORE_P] = InterpretCallSettingsStoreProperty;
   opcode_table[OP_UNARY_NOT_L] = InterpretUnaryNot_L;
   opcode_table[OP_UNARY_NOT_P] = InterpretUnaryNot_P;
   opcode_table[OP_UNARY_NEG_L] = InterpretUnaryNeg_L;
   opcode_table[OP_UNARY_NEG_P] = InterpretUnaryNeg_P;
   opcode_table[OP_UNARY_NONE_L] = InterpretUnaryNone_L;
   opcode_table[OP_UNARY_NONE_P] = InterpretUnaryNone_P;
   opcode_table[OP_UNARY_BITNOT_L] = InterpretUnaryBitNot_L;
   opcode_table[OP_UNARY_BITNOT_P] = InterpretUnaryBitNot_P;
   opcode_table[OP_UNARY_POSTINC] = InterpretUnaryPostInc;
   opcode_table[OP_UNARY_POSTDEC] = InterpretUnaryPostDec;
   opcode_table[OP_UNARY_PREINC] = InterpretUnaryPreInc;
   opcode_table[OP_UNARY_PREDEC] = InterpretUnaryPreDec;
   opcode_table[OP_BINARY_ADD_L] = InterpretBinaryAdd_L;
   opcode_table[OP_BINARY_ADD_P] = InterpretBinaryAdd_P;
   opcode_table[OP_BINARY_SUB_L] = InterpretBinarySub_L;
   opcode_table[OP_BINARY_SUB_P] = InterpretBinarySub_P;
   opcode_table[OP_BINARY_MUL_L] = InterpretBinaryMul_L;
   opcode_table[OP_BINARY_MUL_P] = InterpretBinaryMul_P;
   opcode_table[OP_BINARY_DIV_L] = InterpretBinaryDiv_L;
   opcode_table[OP_BINARY_DIV_P] = InterpretBinaryDiv_P;
   opcode_table[OP_BINARY_MOD_L] = InterpretBinaryMod_L;
   opcode_table[OP_BINARY_MOD_P] = InterpretBinaryMod_P;
   opcode_table[OP_BINARY_AND_L] = InterpretBinaryAnd_L;
   opcode_table[OP_BINARY_AND_P] = InterpretBinaryAnd_P;
   opcode_table[OP_BINARY_OR_L] = InterpretBinaryOr_L;
   opcode_table[OP_BINARY_OR_P] = InterpretBinaryOr_P;
   opcode_table[OP_BINARY_EQ_L] = InterpretBinaryEqual_L;
   opcode_table[OP_BINARY_EQ_P] = InterpretBinaryEqual_P;
   opcode_table[OP_BINARY_NEQ_L] = InterpretBinaryNEq_L;
   opcode_table[OP_BINARY_NEQ_P] = InterpretBinaryNEq_P;
   opcode_table[OP_BINARY_LESS_L] = InterpretBinaryLess_L;
   opcode_table[OP_BINARY_LESS_P] = InterpretBinaryLess_P;
   opcode_table[OP_BINARY_GREATER_L] = InterpretBinaryGreater_L;
   opcode_table[OP_BINARY_GREATER_P] = InterpretBinaryGreater_P;
   opcode_table[OP_BINARY_LEQ_L] = InterpretBinaryLEq_L;
   opcode_table[OP_BINARY_LEQ_P] = InterpretBinaryLEq_P;
   opcode_table[OP_BINARY_GEQ_L] = InterpretBinaryGEq_L;
   opcode_table[OP_BINARY_GEQ_P] = InterpretBinaryGEq_P;
   opcode_table[OP_BINARY_BITAND_L] = InterpretBinaryBitAnd_L;
   opcode_table[OP_BINARY_BITAND_P] = InterpretBinaryBitAnd_P;
   opcode_table[OP_BINARY_BITOR_L] = InterpretBinaryBitOr_L;
   opcode_table[OP_BINARY_BITOR_P] = InterpretBinaryBitOr_P;
   opcode_table[OP_ISCLASS_L] = InterpretIsClass_L;
   opcode_table[OP_ISCLASS_P] = InterpretIsClass_P;
   opcode_table[OP_ISCLASS_CONST_L] = InterpretIsClassConst_L;
   opcode_table[OP_ISCLASS_CONST_P] = InterpretIsClassConst_P;
   opcode_table[OP_FIRST_L] = InterpretUnaryFirst_L;
   opcode_table[OP_FIRST_P] = InterpretUnaryFirst_P;
   opcode_table[OP_REST_L] = InterpretUnaryRest_L;
   opcode_table[OP_REST_P] = InterpretUnaryRest_P;
   opcode_table[OP_GETCLASS_L] = InterpretUnaryGetClass_L;
   opcode_table[OP_GETCLASS_P] = InterpretUnaryGetClass_P;
}
