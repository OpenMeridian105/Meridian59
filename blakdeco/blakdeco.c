// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/* Blakston .bof (blakod object format) dump utility */

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include "bkod.h"

#define BOF_VERSION 11

#define MAX_CLASSES 100
#define MAX_HANDLERS 500

enum {
   GOTO_IF_FALSE,
   GOTO_IF_TRUE,
   GOTO_IF_NULL,
   GOTO_IF_NEQ_NULL
   
};
enum
{
   NOT = 0,
   NEGATE = 1,
   NONE = 2,
   BITWISE_NOT = 3,
   POST_INCREMENT = 4,
   POST_DECREMENT = 5,
   PRE_INCREMENT = 6,
   PRE_DECREMENT = 7,
   UNARY_FIRST = 8,
   UNARY_REST = 9,
   UNARY_GETCLASS = 10,
};
enum
{
   ADD = 0,
   SUBTRACT = 1,
   MULTIPLY = 2,
   DIV = 3,
   MOD = 4,
   AND = 5,
   OR = 6,
   EQUAL = 7,
   NOT_EQUAL = 8,
   LESS_THAN = 9,
   GREATER_THAN = 10,
   LESS_EQUAL = 11,
   GREATER_EQUAL = 12,
   BITWISE_AND = 13,
   BITWISE_OR = 14,
   BINARY_ISCLASS = 15,
};

/* function prototypes */
BOOL memmap_file(char *s);
void release_file();
unsigned char get_byte();
unsigned int get_int();
void dump_bof();
void dump_bkod();
char * name_unary_operation(int unary_op);
char * name_binary_operation(int binary_op);
char * name_var_type(int parm_type);
char * name_goto_cond(int cond);
char * name_function(int fnum);
void print_hex_byte(unsigned char ch);
char *str_constant(int type, int num);
int  find_linenum(int offset);

/* global variables */
HANDLE fh;                      /* handle to the open file */
int file_size;                  /* length of th file */
HANDLE mapfh;                   /* handle to the file mapping */
char *file_mem;                 /* ptr to the memory mapped file */
int index;                      /* current location in file */
int dump_hex = 0;               /* dump the raw hex */
int inst_start;                 /* start of current instruction for dump_xxx to use */

unsigned int debug_offset;      /* start of debugging info in file */

static BYTE bof_magic[] = { 0x42, 0x4F, 0x46, 0xFF };

// Opcode table and function pointer.
typedef void(*op_dump)(char *text);
op_dump opcode_table[NUMBER_OF_OPCODES];
void CreateOpcodeTable(void);

// For counting opcodes.
int opcode_count[NUMBER_OF_OPCODES];

int main(int argc,char *argv[])
{
   if (argc != 2)
   {
      fprintf(stderr,"Usage:  blakdeco filename.bof\n");
      exit(1);
   }
   
   if (memmap_file(argv[1]))
   {
      dump_bof();
      release_file();
   }
}

BOOL memmap_file(char *s)
{
   fh = CreateFile(s,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
   if (fh == INVALID_HANDLE_VALUE)
   {
      fprintf(stderr,"Can't open file %s\n",s);
      return FALSE;
   }

   file_size = GetFileSize(fh,NULL);
   
   mapfh = CreateFileMapping(fh,NULL,PAGE_READONLY,0,file_size,NULL);
   if (mapfh == NULL)
   {
      fprintf(stderr,"Can't create a file mapping\n");
      CloseHandle(fh);
      return FALSE;
   }

   file_mem = (char *) MapViewOfFile(mapfh,FILE_MAP_READ,0,0,0);
   if (file_mem == NULL)
   {
      fprintf(stderr,"Can't map a view of file\n");
      CloseHandle(mapfh);
      CloseHandle(fh);
      return FALSE;
   }
   return TRUE;
}

void release_file()
{
   UnmapViewOfFile(file_mem);
   CloseHandle(mapfh);
   CloseHandle(fh);
}  

unsigned char get_byte()
{
   if (index >= file_size)
   {
      fprintf(stderr,"Read past end of file.  Die.\n");
      release_file();
      exit(1);
   }

   return file_mem[index++];
}

unsigned int get_int()
{
   if (index+3 >= file_size)
   {
      fprintf(stderr,"Read past end of file on int read.  Die.\n");
      release_file();
      exit(1);
   }

   index += 4;
   return *((unsigned int *)(file_mem + index-4));
}

void get_string(char *str, int max_chars)
{
   int i;

   for (i=0; i < max_chars; i++)
   {
      if (index > file_size)
      {
	 fprintf(stderr,"Read past end of file on string read.  Die.\n");
	 release_file();
	 exit(1);
      }

      /* Copy byte of string and look for null termination */
      str[i] = *(file_mem + index);
      index++;
      if (str[i] == 0)
	 return;
   }
}

void dump_bof()
{
   unsigned int c,i,j,superclass_id;
   unsigned int num_properties, prop_num,prop_defaults;
   unsigned int num_cvars, cvar_num,cvar_defaults;
   unsigned int handler[MAX_HANDLERS],num_messages;
   unsigned int classes[MAX_CLASSES],num_classes;
   unsigned int strtable_offset, fname_offset;

   unsigned int num_strings;

   int classes_id[MAX_CLASSES];

   char *default_str, str[500];

   index = 0;

   // Check magic number.
   for (i=0; i < 4; i++)
   {
      BYTE b = get_byte();
      if (b != bof_magic[i])
      {
         printf("Bad magic number--this is not a BOF file\n");
         release_file();
         exit(1);
      }
   }

   // Check BOF version.
   int bof_vers = get_int();
   if (bof_vers != BOF_VERSION)
   {
      printf("Incorrect BOF version - found %i but require %i\n",
         bof_vers, BOF_VERSION);
      release_file();
      exit(1);
   }
   printf(".bof version: %i\n", bof_vers);

   fname_offset = get_int();
   if (fname_offset >= (unsigned int) file_size)
   {
      fprintf(stderr,"Read past end of file on kod filename.  Die.\n");
      release_file();
      exit(1);
   }
   printf("Source file = %s\n", file_mem + fname_offset);

   strtable_offset = get_int();
   printf("String table at offset %08X\n", strtable_offset);

   debug_offset = get_int();
   if (debug_offset == 0)
      printf("No line number debugging information\n");
   else printf("Debugging information at offset %08X\n", debug_offset);

   num_classes = get_int();
   printf("Classes: %i\n",num_classes);

   if (num_classes > MAX_CLASSES-1)
   {
      printf("Can only handle %i classes\n",MAX_CLASSES-1);
      num_classes = MAX_CLASSES-1;
   }
   for (i=0;i<num_classes;i++)
   {
      classes_id[i] = get_int();
      classes[i] = get_int();
      printf("class id %i at offset %08X\n",classes_id[i],classes[i]);
   }
   classes[i] = -1;
  
   for (c=0;c<num_classes;c++)
   {
      printf("Class id %i:\n",classes_id[c]);
      superclass_id = get_int();
      printf("Superclass %i\n",superclass_id);
      
      printf("Property table offset: %08X\n", get_int());
      printf("Message handler offset: %08X\n", get_int());
      
      num_cvars = get_int();
      printf("Classvars: %i\n", num_cvars);
      
      cvar_defaults = get_int();
      printf("Classvar default values: %i\n",cvar_defaults);
      for (i=0;i<cvar_defaults;i++)
      {
	 cvar_num = get_int();
	 default_str = strdup(str_constant(CONSTANT, get_int()));
	 printf("  classvar %2i init value: %s\n",cvar_num,default_str);
      }

      num_properties = get_int();
      printf("Properties: %i\n",num_properties);
      
      prop_defaults = get_int();
      printf("Property default values: %i\n",prop_defaults);
      for (i=0;i<prop_defaults;i++)
      {
	 prop_num = get_int();
	 default_str = strdup(str_constant(CONSTANT, get_int()));
	 printf("  property %2i init value: %s\n",prop_num,default_str);
      }
      
      num_messages = get_int();
      printf("Message handlers: %i\n",num_messages);
      if (num_messages > MAX_HANDLERS-1)
      {
	 printf("Can only handle %i handlers\n",MAX_HANDLERS-1);
	 num_messages = MAX_HANDLERS-1;
      }

      for (i=0;i<num_messages;i++)
      {
	 int id, comment;
	 
	 id = get_int();
	 handler[i] = get_int();
	 comment = get_int();
	 
	 printf(" message %5i at offset %08X\n",id,handler[i]);
	 if (comment != -1)
	    printf("  Comment string #%5i\n", comment);

      }
      handler[i] = -1;
      
      printf("BKOD data:\n");
      for (i=0;i<num_messages;i++)
      {
         unsigned char locals,num_parms;
         int parm_id,parm_default;
	 
	 locals = get_byte();
	 num_parms = get_byte();
	 
	 printf("\nMessage handler, %i local vars\n",locals);
	 
	 for (j=0;j<(unsigned int) num_parms;j++)
	 {
	    parm_id = get_int();
	    parm_default = get_int();
	    default_str = strdup(str_constant(CONSTANT, parm_default));
	    printf("  parm id %i = %s\n",parm_id,default_str);
	 }
	 
   
    // Create the opcode table
    CreateOpcodeTable();

	 // printf("at ofs %08X, next handler %08X, next class %08X\n",
    // index,handler[i+1],classes[c+1]); 
	 while ((unsigned int) index <  handler[i+1] &&
           (unsigned int) index < classes[c+1] &&
           index < file_size)
	 {
	    dump_bkod();
	    if (index == strtable_offset)
	       break;
	 }
      }
      printf("--------------------------------------------\n");
      num_strings = get_int();
      printf("Strings: %d\n", num_strings);
      for (i=0; i < num_strings; i++)
         printf("String %d at offset %08X\n", i, get_int());
      
      for (i=0; i < num_strings; i++)
      {
         get_string(str, 500);
         printf("String %d = %s\n", i, str);
      }
      
      printf("\n");
   }

   // Print opcode counts.
   int total_opcodes = 0;
   for (int i = 0; i < NUMBER_OF_OPCODES; ++i)
   {
      total_opcodes += opcode_count[i];
      printf("Opcode: %i, count: %i\n", i, opcode_count[i]);
   }
   printf("Total instructions: %i\n\n", total_opcodes);
}

void dump_bkod()
{
   int i, line;
   char text[50000];
   static int last_line = 0;

//   printf("at %08X\n",index);
   
   inst_start = index;

   line = find_linenum(index);
   if (line != last_line)
   {
      printf("*** Line %d\n", line);
      last_line = line;
   }

   char opcode_char = get_byte();

   // Call the opcode handler for this opcode.
   opcode_table[opcode_char](text);

   // Increment count for this opcode.
   opcode_count[opcode_char]++;

   if (dump_hex)
   {
      printf("BKOD raw: ");
      for (i=0;i<index-inst_start;i++)
	 print_hex_byte(file_mem[inst_start+i]);
      printf("\n");
   }
   printf("@%08X: ",inst_start);
   printf("%s\n",text);
}

char * name_unary_operation(int unary_op)
{
   switch (unary_op)
   {
      case NOT : return "NOT";
      case NEGATE : return "-";
      case NONE : return "";
      case BITWISE_NOT : return "~";
      case PRE_INCREMENT :
      case POST_INCREMENT : return "++";
      case PRE_DECREMENT :
      case POST_DECREMENT : return "--";
      case UNARY_FIRST: return "First elem";
      case UNARY_REST: return "Rest elem";
      case UNARY_GETCLASS: return "GetClass";
      default : return "INVALID";
   }
}

char * name_binary_operation(int binary_op)
{
   switch (binary_op)
   {
   case ADD : return "+";
   case SUBTRACT : return "-";
   case MULTIPLY : return "*";
   case DIV : return "/";
   case MOD : return "%";
   case AND : return "and";
   case OR : return "or";
   case EQUAL : return "=";
   case NOT_EQUAL : return "!=";
   case LESS_THAN : return "<";
   case GREATER_THAN : return ">";
   case LESS_EQUAL : return "<=";
   case GREATER_EQUAL : return ">=";
   case BITWISE_AND : return "&";
   case BITWISE_OR : return "|";
   case BINARY_ISCLASS: return "is";
   default : return "INVALID";
   }
}

char * name_var_type(int parm_type)
{
   switch (parm_type)
   {
      case LOCAL_VAR : return "Local var";
      case PROPERTY : return "Property";
      case CONSTANT : return "Constant";
      case CLASS_VAR : return "Class var";
      default : return "INVALID";
   }
}

char * name_goto_cond(int cond)
{
   switch (cond)
   {
   case GOTO_IF_TRUE: return "!= 0";
   case GOTO_IF_FALSE: return "== 0";
   case GOTO_IF_NULL: return "== $";
   case GOTO_IF_NEQ_NULL: return "!= $";
   default : return "INVALID";
   }
}

char * name_function(int fnum)
{
   static char s[25];

   switch (fnum)
   {
   case CREATEOBJECT : return "Create";

   case SENDMESSAGE : return "Send";
   case POSTMESSAGE : return "Post";
   case SENDLISTMSG : return "SendList";
   case SENDLISTMSGBREAK : return "SendListBreak";
   case SENDLISTMSGBYCLASS : return "SendListByClass";
   case SENDLISTMSGBYCLASSBREAK : return "SendListByClassBreak";

   case SAVEGAME : return "SaveGame";
   case LOADGAME : return "LoadGame";

   case GODLOG : return "GodLog";
   case DEBUG : return "Debug";  
   case ADDPACKET : return "AddPacket";
   case SENDPACKET : return "SendPacket";
   case SENDCOPYPACKET : return "SendCopyPacket";
   case CLEARPACKET : return "ClearPacket";
   case GETINACTIVETIME : return "GetInactiveTime";
   case DUMPSTACK : return "DumpStack";

   case ISSTRING : return "IsString";
   case STRINGEQUAL : return "StringEqual";
   case STRINGCONTAIN : return "StringContain";
   case SETRESOURCE : return "SetResource";
   case PARSESTRING : return "ParseString";
   case SETSTRING : return "SetString";
   case CREATESTRING : return "CreateString";
   case STRINGSUBSTITUTE : return "StringSubstitute";
   case APPENDTEMPSTRING : return "AppendTempString";
   case CLEARTEMPSTRING : return "ClearTempString";
   case GETTEMPSTRING : return "GetTempString";
   case STRINGLENGTH : return "StringLength";
   case STRINGCONSISTSOF : return "StringConsistsOf";

   case SETCLASSVAR : return "SetClassVar";

   case CREATETIMER : return "CreateTimer";
   case DELETETIMER : return "DeleteTimer";
   case GETTIMEREMAINING : return "GetTimeRemaining";
   case ISTIMER : return "IsTimer";

   case CHANGESECTORFLAGBSP : return "ChangeSectorFlagBSP";
   case MOVESECTORBSP : return "MoveSectorBSP";
   case CHANGETEXTUREBSP : return "ChangeTextureBSP";
   case CREATEROOMDATA : return "LoadRoom";
   case FREEROOM : return "FreeRoom";
   case ROOMDATA : return "RoomData";
   case LINEOFSIGHTVIEW : return "LineOfSightView";
   case LINEOFSIGHTBSP : return "LineOfSightBSP";
   case GETLOCATIONINFOBSP: return "GetLocationInfoBSP";
   case BLOCKERADDBSP: return "BlockerAddBSP";
   case BLOCKERMOVEBSP: return "BlockerMoveBSP";
   case BLOCKERREMOVEBSP: return "BlockerRemoveBSP";
   case BLOCKERCLEARBSP: return "BlockerClearBSP";
   case GETRANDOMPOINTBSP: return "GetRandomPointBSP";
   case GETSTEPTOWARDSBSP: return "GetStepTowardsBSP";
   case GETRANDOMMOVEDESTBSP: return "GetRandomMoveDestBSP";
   case GETSECTORHEIGHTBSP: return "GetSectorHeightBSP";
   case SETROOMDEPTHOVERRIDEBSP: return "SetRoomDepthOverrideBSP";
   case CALCUSERMOVEMENTBUCKET: return "CalcUserMovementBucket";
   case INTERSECTLINECIRCLE: return "IntersectLineCircle";
   case STRINGTONUMBER : return "StringToNumber";

   case CANMOVEINROOMBSP: return "CanMoveInRoomBSP";

   case APPENDLISTELEM : return "AppendListElem";
   case CONS  : return "Cons";
   case LENGTH  : return "Length";
   case ISLISTMATCH : return "IsListMatch";
   case NTH  : return "Nth";
   case MLIST  : return "List";
   case ISLIST : return "IsList";
   case SETFIRST : return "SetFirst";
   case SETNTH : return "SetNth";
   case DELLISTELEM : return "DelListElem";
   case DELLASTLISTELEM: return "DelLastListElem";
   case FINDLISTELEM : return "FindListElem";
   case SWAPLISTELEM : return "SwapListElem";
   case INSERTLISTELEM : return "InsertListElem";
   case LAST : return "Last";
   case GETLISTELEMBYCLASS : return "GetListElemByClass";
   case GETLISTNODE : return "GetListNode";
   case GETALLLISTNODESBYCLASS : return "C_GetAllListNodesByClass";
   case LISTCOPY : return "ListCopy";

   case GETTIME : return "GetTime";
   case GETUNIXTIMESTRING: return "GetUnixTimeString";
   case OLDTIMESTAMPFIX: return "OldTimestampFix";
   case GETTICKCOUNT : return "GetTickCount";
   case GETDATEANDTIME : return "GetDateAndTime";

   case ABS : return "Abs";
   case BOUND : return "Bound";
   case SQRT : return "Sqrt";

   case CREATETABLE : return "CreateTable";
   case ADDTABLEENTRY : return "AddTableEntry";
   case GETTABLEENTRY : return "GetTableEntry";
   case DELETETABLEENTRY : return "DeleteTableEntry";
   case DELETETABLE : return "DeleteTable";
   case ISTABLE : return "IsTable";

   case RECYCLEUSER : return "RecycleUser";

   case ISOBJECT : return "IsObject";

   case RANDOM  : return "Random";
   case RECORDSTAT : return "RecordStat";
   case GETSESSIONIP : return "GetSessionIP";
   default : sprintf(s,"Unknown function %i",fnum); return s;
   }

   /* can't get here */
   return NULL;
}

void print_hex_byte(unsigned char ch)
{
   int b1,b2;

   b1 = ch / 16;
   b2 = ch & (16-1);
   if (b1 >= 10)
      printf("%c",b1-10+'A');
   else
      printf("%i",b1);

   if (b2 >= 10)
      printf("%c",b2-10+'A');
   else
      printf("%i",b2);
}

char *str_constant(int type, int num)
{
   static char s[64];
   constant_type bc;

   if (type == LOCAL_VAR || type == PROPERTY || type == CLASS_VAR)
   {
      sprintf(s, "%i", num);
      return s;
   }

   memcpy(&bc, &num, sizeof(constant_type));
   switch (bc.tag)
   {
   case TAG_NIL : 
      if (bc.data == 0) 
	 sprintf(s,"$");
      else
	 sprintf(s,"%i",bc.data);
      break;
   case TAG_INT :
      sprintf(s,"(int %i)",bc.data);
      break;
   case TAG_RESOURCE :
      sprintf(s,"(rsc %i)",bc.data);
      break;
   case TAG_CLASS :
      sprintf(s,"(class %i)",bc.data);
      break;
   case TAG_MESSAGE :
      sprintf(s,"(message %i)",bc.data);
      break;
   case TAG_DEBUGSTR :
      sprintf(s,"(debugging string %i)",bc.data);
      break;
   case TAG_OVERRIDE :
      sprintf(s,"(overridden by property %i)",bc.data);
      break;
   default :
      sprintf(s,"INVALID");
      break;
   }
   return s;
}

int find_linenum(int offset)
{
   char *pos;
   int line_num, line_offset;

   pos = file_mem + debug_offset + 4;  // +4 to skip # of debug entries

   while (1)
   {
      if ((pos - file_mem) + 7 >= file_size)
      {
	 printf("Read past end of file looking for debug position %d", offset);
	 exit(1);
      }
      memcpy(&line_offset, pos, 4);
      memcpy(&line_num, pos + 4, 4);
      pos += 8;
      if (offset <= line_offset)
	 return line_num;
   }
}

// Dump true/false gotos.
void dump_goto(char *text, int goto_type, int var_type)
{
   int dest_addr, var;

   dest_addr = get_int();

   var = get_int();
   sprintf(text, "If %s %s %s goto absolute %08X",
      name_var_type(var_type), str_constant(var_type, var),
      name_goto_cond(goto_type), dest_addr + index);
}
// Dump the body of a call statement.
void dump_call(char *text)
{
   unsigned char num_parms, parm_type;
   int i, parm_value;

   num_parms = get_byte();
   for (i = 0;i<num_parms;i++)
   {
      parm_type = get_byte();
      parm_value = get_int();
      if (i != 0)
         strcat(text, ", ");
      strcat(text, name_var_type(parm_type));
      strcat(text, " ");
      strcat(text, str_constant(parm_type, parm_value));
   }
}
// Dump the body of a call statement with settings.
void dump_call_settings(char *text)
{
   unsigned char num_parms, parm_type;
   int i, parm_value, name_id;
   char tempbuf[15];

   num_parms = get_byte();
   for (i = 0;i<num_parms;i++)
   {
      parm_type = get_byte();
      parm_value = get_int();
      if (i != 0)
         strcat(text, ", ");
      strcat(text, name_var_type(parm_type));
      strcat(text, " ");
      strcat(text, str_constant(parm_type, parm_value));
   }

   strcat(text, "\n  : ");

   num_parms = get_byte();
   for (i = 0;i<num_parms;i++)
   {
      name_id = get_int();
      parm_type = get_byte();
      parm_value = get_int();
      if (i != 0)
         strcat(text, ", ");
      strcat(text, "Parm id ");
      strcat(text, itoa(name_id, tempbuf, 10));
      strcat(text, " ");
      strcat(text, name_var_type(parm_type));
      strcat(text, " ");
      strcat(text, str_constant(parm_type, parm_value));
   }
}
// Dump the given unary instruction.
void dump_unary(char *text, int dest_type, int operation)
{
   char *source_str;
   int dest, source;

   /*   printf("at %08X in unary assign\n",index); */

   char op_char = get_byte();
   opcode_data *opcode = (opcode_data *)&op_char;

   dest = get_int();
   source = get_int();
   source_str = _strdup(str_constant(opcode->source1, source));
   switch (operation)
   {
   case PRE_INCREMENT:
   case PRE_DECREMENT:
      if (source != dest || opcode->source1 != opcode->source2)
      {
         sprintf(text, "%s %i = %s %s %s", name_var_type(opcode->source1), source,
            name_unary_operation(operation), name_var_type(opcode->source1),
            source_str);
         printf("@%08X: ", inst_start);
         printf("%s\n", text);
      }
      sprintf(text, "%s %i = %s %s %s", name_var_type(opcode->source2), dest,
         name_unary_operation(operation), name_var_type(opcode->source1),
         source_str);
      break;
   case POST_INCREMENT:
   case POST_DECREMENT:
      if (source != dest || opcode->source1 != opcode->source2)
      {
         sprintf(text, "%s %i = %s %s %s", name_var_type(opcode->source2), dest,
            name_var_type(opcode->source1), source_str,
            name_unary_operation(NONE)); // NONE because this is just an assignment to new local
         printf("@%08X: ", inst_start);
         printf("%s\n", text);
      }
      sprintf(text, "%s %i = %s %s %s", name_var_type(opcode->source1), source,
         name_var_type(opcode->source1), source_str,
         name_unary_operation(operation));
      break;
   default:
      sprintf(text, "%s %i = %s %s %s", name_var_type(dest_type), dest,
         name_unary_operation(operation), name_var_type(opcode->source1),
         source_str);
   }
}
// Dump the given binary instruction.
void dump_binary(char *text, int dest_type, int operation)
{
   char *source1_str, *source2_str;
   int dest, source1, source2;

   char op_char = get_byte();
   opcode_data *opcode = (opcode_data *)&op_char;

   dest = get_int();
   source1 = get_int();
   source2 = get_int();
   source1_str = _strdup(str_constant(opcode->source1, source1));
   source2_str = _strdup(str_constant(opcode->source2, source2));
   sprintf(text, "%s %i = %s %s %s %s %s", name_var_type(dest_type), dest,
      name_var_type(opcode->source1), source1_str, name_binary_operation(operation),
      name_var_type(opcode->source2), source2_str);
}
// Return
void dump_return(char *text)
{
   int ret_val;

   char op_char = get_byte();
   opcode_data *opcode = (opcode_data *)&op_char;

   switch (opcode->source2)
   {
   case PROPAGATE:
   {
      sprintf(text, "Return propagate");
      break;
   }
   case NO_PROPAGATE:
   {
      ret_val = get_int();
      sprintf(text, "Return %s %s", name_var_type(opcode->source1),
         str_constant(opcode->source1, ret_val));
      break;
   }
   }
}
// Goto
void dump_goto_uncond(char *text)
{
   int dest_addr;
   dest_addr = get_int();

   sprintf(text, "Goto absolute %08X", dest_addr + index);
}
void dump_goto_if_true_constant(char *text)
{
   dump_goto(text, GOTO_IF_TRUE, CONSTANT);
}
void dump_goto_if_true_local(char *text)
{
   dump_goto(text, GOTO_IF_TRUE, LOCAL_VAR);
}
void dump_goto_if_true_property(char *text)
{
   dump_goto(text, GOTO_IF_TRUE, PROPERTY);
}
void dump_goto_if_true_classvar(char *text)
{
   dump_goto(text, GOTO_IF_TRUE, CLASS_VAR);
}
void dump_goto_if_false_constant(char *text)
{
   dump_goto(text, GOTO_IF_FALSE, CONSTANT);
}
void dump_goto_if_false_local(char *text)
{
   dump_goto(text, GOTO_IF_FALSE, LOCAL_VAR);
}
void dump_goto_if_false_property(char *text)
{
   dump_goto(text, GOTO_IF_FALSE, PROPERTY);
}
void dump_goto_if_false_classvar(char *text)
{
   dump_goto(text, GOTO_IF_FALSE, CLASS_VAR);
}
void dump_goto_if_null_constant(char *text)
{
   dump_goto(text, GOTO_IF_NULL, CONSTANT);
}
void dump_goto_if_null_local(char *text)
{
   dump_goto(text, GOTO_IF_NULL, LOCAL_VAR);
}
void dump_goto_if_null_property(char *text)
{
   dump_goto(text, GOTO_IF_NULL, PROPERTY);
}
void dump_goto_if_null_classvar(char *text)
{
   dump_goto(text, GOTO_IF_NULL, CLASS_VAR);
}
void dump_goto_if_neq_null_constant(char *text)
{
   dump_goto(text, GOTO_IF_NEQ_NULL, CONSTANT);
}
void dump_goto_if_neq_null_local(char *text)
{
   dump_goto(text, GOTO_IF_NEQ_NULL, LOCAL_VAR);
}
void dump_goto_if_neq_null_property(char *text)
{
   dump_goto(text, GOTO_IF_NEQ_NULL, PROPERTY);
}
void dump_goto_if_neq_null_classvar(char *text)
{
   dump_goto(text, GOTO_IF_NEQ_NULL, CLASS_VAR);
}
// Calls
void dump_call_store_none(char *text)
{
   unsigned char info;

   info = get_byte();

   sprintf(text, "Call (no store) %s--", name_function(info));
   dump_call(text);
}
void dump_call_store_local(char *text)
{
   unsigned char info;
   int assign_index;

   info = get_byte();
   assign_index = get_int();

   sprintf(text, "%s %s = Call (local store) %s\n  : ", name_var_type(LOCAL_VAR),
      str_constant(LOCAL_VAR, assign_index), name_function(info));
   dump_call(text);
}
void dump_call_store_property(char *text)
{
   unsigned char info;
   int assign_index;

   info = get_byte();
   assign_index = get_int();

   sprintf(text, "%s %s = Call (prop store) %s\n  : ", name_var_type(PROPERTY),
      str_constant(PROPERTY, assign_index), name_function(info));
   dump_call(text);
}
void dump_call_settings_store_none(char *text)
{
   unsigned char info;

   info = get_byte();

   sprintf(text, "Call (settings, no store) %s--", name_function(info));
   dump_call_settings(text);
}
void dump_call_settings_store_local(char *text)
{
   unsigned char info;
   int assign_index;

   info = get_byte();
   assign_index = get_int();

   sprintf(text, "%s %s = Call (settings, local store) %s\n  : ", name_var_type(LOCAL_VAR),
      str_constant(LOCAL_VAR, assign_index), name_function(info));
   dump_call_settings(text);
}
void dump_call_settings_store_property(char *text)
{
   unsigned char info;
   int assign_index;

   info = get_byte();
   assign_index = get_int();

   sprintf(text, "%s %s = Call (settings, prop store) %s\n  : ", name_var_type(PROPERTY),
      str_constant(PROPERTY, assign_index), name_function(info));
   dump_call_settings(text);
}
// Unary
void dump_unary_not_L(char *text)
{
   dump_unary(text, LOCAL_VAR, NOT);
}
void dump_unary_not_P(char *text)
{
   dump_unary(text, PROPERTY, NOT);
}
void dump_unary_neg_L(char *text)
{
   dump_unary(text, LOCAL_VAR, NEGATE);
}
void dump_unary_neg_P(char *text)
{
   dump_unary(text, PROPERTY, NEGATE);
}
void dump_unary_none_L(char *text)
{
   dump_unary(text, LOCAL_VAR, NONE);
}
void dump_unary_none_P(char *text)
{
   dump_unary(text, PROPERTY, NONE);
}
void dump_unary_bitnot_L(char *text)
{
   dump_unary(text, LOCAL_VAR, BITWISE_NOT);
}
void dump_unary_bitnot_P(char *text)
{
   dump_unary(text, PROPERTY, BITWISE_NOT);
}
void dump_unary_postinc(char *text)
{
   dump_unary(text, 0, POST_INCREMENT);
}
void dump_unary_postdec(char *text)
{
   dump_unary(text, 0, POST_DECREMENT);
}
void dump_unary_preinc(char *text)
{
   dump_unary(text, 0, PRE_INCREMENT);
}
void dump_unary_predec(char *text)
{
   dump_unary(text, 0, PRE_DECREMENT);
}
void dump_unary_first_L(char *text)
{
   dump_unary(text, LOCAL_VAR, UNARY_FIRST);
}
void dump_unary_first_P(char *text)
{
   dump_unary(text, PROPERTY, UNARY_FIRST);
}
void dump_unary_rest_L(char *text)
{
   dump_unary(text, LOCAL_VAR, UNARY_REST);
}
void dump_unary_rest_P(char *text)
{
   dump_unary(text, PROPERTY, UNARY_REST);
}
void dump_unary_getclass_L(char *text)
{
   dump_unary(text, LOCAL_VAR, UNARY_GETCLASS);
}
void dump_unary_getclass_P(char *text)
{
   dump_unary(text, PROPERTY, UNARY_GETCLASS);
}
// Binary
void dump_binary_add_L(char *text)
{
   dump_binary(text, LOCAL_VAR, ADD);
}
void dump_binary_add_P(char *text)
{
   dump_binary(text, PROPERTY, ADD);
}
void dump_binary_sub_L(char *text)
{
   dump_binary(text, LOCAL_VAR, SUBTRACT);
}
void dump_binary_sub_P(char *text)
{
   dump_binary(text, PROPERTY, SUBTRACT);
}
void dump_binary_mul_L(char *text)
{
   dump_binary(text, LOCAL_VAR, MULTIPLY);
}
void dump_binary_mul_P(char *text)
{
   dump_binary(text, PROPERTY, MULTIPLY);
}
void dump_binary_div_L(char *text)
{
   dump_binary(text, LOCAL_VAR, DIV);
}
void dump_binary_div_P(char *text)
{
   dump_binary(text, PROPERTY, DIV);
}
void dump_binary_mod_L(char *text)
{
   dump_binary(text, LOCAL_VAR, MOD);
}
void dump_binary_mod_P(char *text)
{
   dump_binary(text, PROPERTY, MOD);
}
void dump_binary_and_L(char *text)
{
   dump_binary(text, LOCAL_VAR, AND);
}
void dump_binary_and_P(char *text)
{
   dump_binary(text, PROPERTY, AND);
}
void dump_binary_or_L(char *text)
{
   dump_binary(text, LOCAL_VAR, OR);
}
void dump_binary_or_P(char *text)
{
   dump_binary(text, PROPERTY, OR);
}
void dump_binary_equal_L(char *text)
{
   dump_binary(text, LOCAL_VAR, EQUAL);
}
void dump_binary_equal_P(char *text)
{
   dump_binary(text, PROPERTY, EQUAL);
}
void dump_binary_neq_L(char *text)
{
   dump_binary(text, LOCAL_VAR, NOT_EQUAL);
}
void dump_binary_neq_P(char *text)
{
   dump_binary(text, PROPERTY, NOT_EQUAL);
}
void dump_binary_less_L(char *text)
{
   dump_binary(text, LOCAL_VAR, LESS_THAN);
}
void dump_binary_less_P(char *text)
{
   dump_binary(text, PROPERTY, LESS_THAN);
}
void dump_binary_greater_L(char *text)
{
   dump_binary(text, LOCAL_VAR, GREATER_THAN);
}
void dump_binary_greater_P(char *text)
{
   dump_binary(text, PROPERTY, GREATER_THAN);
}
void dump_binary_leq_L(char *text)
{
   dump_binary(text, LOCAL_VAR, LESS_EQUAL);
}
void dump_binary_leq_P(char *text)
{
   dump_binary(text, PROPERTY, LESS_EQUAL);
}
void dump_binary_geq_L(char *text)
{
   dump_binary(text, LOCAL_VAR, GREATER_EQUAL);
}
void dump_binary_geq_P(char *text)
{
   dump_binary(text, PROPERTY, GREATER_EQUAL);
}
void dump_binary_bitand_L(char *text)
{
   dump_binary(text, LOCAL_VAR, BITWISE_AND);
}
void dump_binary_bitand_P(char *text)
{
   dump_binary(text, PROPERTY, BITWISE_AND);
}
void dump_binary_bitor_L(char *text)
{
   dump_binary(text, LOCAL_VAR, BITWISE_OR);
}
void dump_binary_bitor_P(char *text)
{
   dump_binary(text, PROPERTY, BITWISE_OR);
}
void dump_isclass_L(char *text)
{
   dump_binary(text, LOCAL_VAR, BINARY_ISCLASS);
}
void dump_isclass_P(char *text)
{
   dump_binary(text, PROPERTY, BINARY_ISCLASS);
}
void dump_isclass_const_L(char *text)
{
   dump_binary(text, LOCAL_VAR, BINARY_ISCLASS);
}
void dump_isclass_const_P(char *text)
{
   dump_binary(text, PROPERTY, BINARY_ISCLASS);
}

void CreateOpcodeTable(void)
{
   opcode_table[OP_RETURN] = dump_return;
   opcode_table[OP_GOTO_UNCOND] = dump_goto_uncond;
   opcode_table[OP_GOTO_IF_TRUE_C] = dump_goto_if_true_constant;
   opcode_table[OP_GOTO_IF_TRUE_L] = dump_goto_if_true_local;
   opcode_table[OP_GOTO_IF_TRUE_P] = dump_goto_if_true_property;
   opcode_table[OP_GOTO_IF_TRUE_V] = dump_goto_if_true_classvar;
   opcode_table[OP_GOTO_IF_FALSE_C] = dump_goto_if_false_constant;
   opcode_table[OP_GOTO_IF_FALSE_L] = dump_goto_if_false_local;
   opcode_table[OP_GOTO_IF_FALSE_P] = dump_goto_if_false_property;
   opcode_table[OP_GOTO_IF_FALSE_V] = dump_goto_if_false_classvar;
   opcode_table[OP_GOTO_IF_NULL_C] = dump_goto_if_null_constant;
   opcode_table[OP_GOTO_IF_NULL_L] = dump_goto_if_null_local;
   opcode_table[OP_GOTO_IF_NULL_P] = dump_goto_if_null_property;
   opcode_table[OP_GOTO_IF_NULL_V] = dump_goto_if_null_classvar;
   opcode_table[OP_GOTO_IF_NEQ_NULL_C] = dump_goto_if_neq_null_constant;
   opcode_table[OP_GOTO_IF_NEQ_NULL_L] = dump_goto_if_neq_null_local;
   opcode_table[OP_GOTO_IF_NEQ_NULL_P] = dump_goto_if_neq_null_property;
   opcode_table[OP_GOTO_IF_NEQ_NULL_V] = dump_goto_if_neq_null_classvar;

   opcode_table[OP_CALL_STORE_NONE] = dump_call_store_none;
   opcode_table[OP_CALL_STORE_L] = dump_call_store_local;
   opcode_table[OP_CALL_STORE_P] = dump_call_store_property;
   opcode_table[OP_CALL_SETTINGS_STORE_NONE] = dump_call_settings_store_none;
   opcode_table[OP_CALL_SETTINGS_STORE_L] = dump_call_settings_store_local;
   opcode_table[OP_CALL_SETTINGS_STORE_P] = dump_call_settings_store_property;

   opcode_table[OP_UNARY_NOT_L] = dump_unary_not_L;
   opcode_table[OP_UNARY_NOT_P] = dump_unary_not_P;
   opcode_table[OP_UNARY_NEG_L] = dump_unary_neg_L;
   opcode_table[OP_UNARY_NEG_P] = dump_unary_neg_P;
   opcode_table[OP_UNARY_NONE_L] = dump_unary_none_L;
   opcode_table[OP_UNARY_NONE_P] = dump_unary_none_P;
   opcode_table[OP_UNARY_BITNOT_L] = dump_unary_bitnot_L;
   opcode_table[OP_UNARY_BITNOT_P] = dump_unary_bitnot_P;
   opcode_table[OP_UNARY_POSTINC] = dump_unary_postinc;
   opcode_table[OP_UNARY_POSTDEC] = dump_unary_postdec;
   opcode_table[OP_UNARY_PREINC] = dump_unary_preinc;
   opcode_table[OP_UNARY_PREDEC] = dump_unary_predec;

   opcode_table[OP_BINARY_ADD_L] = dump_binary_add_L;
   opcode_table[OP_BINARY_ADD_P] = dump_binary_add_P;
   opcode_table[OP_BINARY_SUB_L] = dump_binary_sub_L;
   opcode_table[OP_BINARY_SUB_P] = dump_binary_sub_P;
   opcode_table[OP_BINARY_MUL_L] = dump_binary_mul_L;
   opcode_table[OP_BINARY_MUL_P] = dump_binary_mul_P;
   opcode_table[OP_BINARY_DIV_L] = dump_binary_div_L;
   opcode_table[OP_BINARY_DIV_P] = dump_binary_div_P;
   opcode_table[OP_BINARY_MOD_L] = dump_binary_mod_L;
   opcode_table[OP_BINARY_MOD_P] = dump_binary_mod_P;
   opcode_table[OP_BINARY_AND_L] = dump_binary_and_L;
   opcode_table[OP_BINARY_AND_P] = dump_binary_and_P;
   opcode_table[OP_BINARY_OR_L] = dump_binary_or_L;
   opcode_table[OP_BINARY_OR_P] = dump_binary_or_P;
   opcode_table[OP_BINARY_EQ_L] = dump_binary_equal_L;
   opcode_table[OP_BINARY_EQ_P] = dump_binary_equal_P;
   opcode_table[OP_BINARY_NEQ_L] = dump_binary_neq_L;
   opcode_table[OP_BINARY_NEQ_P] = dump_binary_neq_P;
   opcode_table[OP_BINARY_LESS_L] = dump_binary_less_L;
   opcode_table[OP_BINARY_LESS_P] = dump_binary_less_P;
   opcode_table[OP_BINARY_GREATER_L] = dump_binary_greater_L;
   opcode_table[OP_BINARY_GREATER_P] = dump_binary_greater_P;
   opcode_table[OP_BINARY_LEQ_L] = dump_binary_leq_L;
   opcode_table[OP_BINARY_LEQ_P] = dump_binary_leq_P;
   opcode_table[OP_BINARY_GEQ_L] = dump_binary_geq_L;
   opcode_table[OP_BINARY_GEQ_P] = dump_binary_geq_P;
   opcode_table[OP_BINARY_BITAND_L] = dump_binary_bitand_L;
   opcode_table[OP_BINARY_BITAND_P] = dump_binary_bitand_P;
   opcode_table[OP_BINARY_BITOR_L] = dump_binary_bitor_L;
   opcode_table[OP_BINARY_BITOR_P] = dump_binary_bitor_P;
   opcode_table[OP_ISCLASS_L] = dump_isclass_L;
   opcode_table[OP_ISCLASS_P] = dump_isclass_P;
   opcode_table[OP_ISCLASS_CONST_L] = dump_isclass_const_L;
   opcode_table[OP_ISCLASS_CONST_P] = dump_isclass_const_P;
   opcode_table[OP_FIRST_L] = dump_unary_first_L;
   opcode_table[OP_FIRST_P] = dump_unary_first_P;
   opcode_table[OP_REST_L] = dump_unary_rest_L;
   opcode_table[OP_REST_P] = dump_unary_rest_P;
   opcode_table[OP_GETCLASS_L] = dump_unary_getclass_L;
   opcode_table[OP_GETCLASS_P] = dump_unary_getclass_P;
}
