// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/* bkod.h
 * header file for the bkod byte compiled format
 */

#ifndef _BKOD_H
#define _BKOD_H

/* special byte for message handler header */
enum
{
   HANDLER = 0,
   CONSTRUCTOR = 1,
   DESTRUCTOR = 2,
};

// Opcodes
enum opcode_id {
   OP_RETURN,
   // Goto instructions
   OP_GOTO_UNCOND,
   OP_GOTO_IF_TRUE_C,
   OP_GOTO_IF_TRUE_L,
   OP_GOTO_IF_TRUE_P,
   OP_GOTO_IF_TRUE_V,
   OP_GOTO_IF_FALSE_C,
   OP_GOTO_IF_FALSE_L,
   OP_GOTO_IF_FALSE_P,
   OP_GOTO_IF_FALSE_V,
   OP_GOTO_IF_NULL_C,
   OP_GOTO_IF_NULL_L,
   OP_GOTO_IF_NULL_P,
   OP_GOTO_IF_NULL_V,
   OP_GOTO_IF_NEQ_NULL_C,
   OP_GOTO_IF_NEQ_NULL_L,
   OP_GOTO_IF_NEQ_NULL_P,
   OP_GOTO_IF_NEQ_NULL_V,
   // Call instructions
   // C calls without settings (First, Nth etc)
   OP_CALL_STORE_NONE,
   OP_CALL_STORE_L,
   OP_CALL_STORE_P,
   // C calls with settings (Send, Create etc)
   OP_CALL_SETTINGS_STORE_NONE,
   OP_CALL_SETTINGS_STORE_L,
   OP_CALL_SETTINGS_STORE_P,
   // Unary instructions
   OP_UNARY_NOT_L,
   OP_UNARY_NOT_P,
   OP_UNARY_NEG_L,
   OP_UNARY_NEG_P,
   OP_UNARY_NONE_L,
   OP_UNARY_NONE_P,
   OP_UNARY_BITNOT_L,
   OP_UNARY_BITNOT_P,
   OP_UNARY_POSTINC,
   OP_UNARY_POSTDEC,
   OP_UNARY_PREINC,
   OP_UNARY_PREDEC,
   // Binary instructions
   OP_BINARY_ADD_L,
   OP_BINARY_ADD_P,
   OP_BINARY_SUB_L,
   OP_BINARY_SUB_P,
   OP_BINARY_MUL_L,
   OP_BINARY_MUL_P,
   OP_BINARY_DIV_L,
   OP_BINARY_DIV_P,
   OP_BINARY_MOD_L,
   OP_BINARY_MOD_P,
   OP_BINARY_AND_L,
   OP_BINARY_AND_P,
   OP_BINARY_OR_L,
   OP_BINARY_OR_P,
   OP_BINARY_EQ_L,
   OP_BINARY_EQ_P,
   OP_BINARY_NEQ_L,
   OP_BINARY_NEQ_P,
   OP_BINARY_LESS_L,
   OP_BINARY_LESS_P,
   OP_BINARY_GREATER_L,
   OP_BINARY_GREATER_P,
   OP_BINARY_LEQ_L,
   OP_BINARY_LEQ_P,
   OP_BINARY_GEQ_L,
   OP_BINARY_GEQ_P,
   OP_BINARY_BITAND_L,
   OP_BINARY_BITAND_P,
   OP_BINARY_BITOR_L,
   OP_BINARY_BITOR_P,
   OP_ISCLASS_L,
   OP_ISCLASS_P,
   OP_ISCLASS_CONST_L,
   OP_ISCLASS_CONST_P,
   OP_FIRST_L,
   OP_FIRST_P,
   OP_REST_L,
   OP_REST_P,
   OP_GETCLASS_L,
   OP_GETCLASS_P,
   NUMBER_OF_OPCODES // Must be last.
};

/* source1 & source2 bits of the opcode for assign and return,
 * and some legal values for type byte of each parameter in a call
 */
enum 
{
   LOCAL_VAR = 0,
   PROPERTY = 1,
   CONSTANT = 2,
   CLASS_VAR = 3,
};

/* source2 bit of the opcode for return */
enum
{
   NO_PROPAGATE = 0,
   PROPAGATE = 1,
};

/* function ID's */
enum
{
   CREATEOBJECT = 1,

   SENDMESSAGE = 11,
   POSTMESSAGE = 12,
   SENDLISTMSG = 13,
   SENDLISTMSGBREAK = 14,
   SENDLISTMSGBYCLASS = 15,
   SENDLISTMSGBYCLASSBREAK = 16,

   SAVEGAME = 19,
   LOADGAME = 20,

   GODLOG = 21,
   DEBUG = 22,
   ADDPACKET = 23,
   SENDPACKET = 24,
   SENDCOPYPACKET = 25,
   CLEARPACKET = 26,
   GETINACTIVETIME = 27,
   DUMPSTACK = 28,

   ISSTRING = 30,
   STRINGEQUAL = 31,
   STRINGCONTAIN = 32,
   SETRESOURCE = 33,
   PARSESTRING = 34,
   SETSTRING = 35,
   CREATESTRING = 36,
   STRINGSUBSTITUTE = 37,
   APPENDTEMPSTRING = 38,
   CLEARTEMPSTRING = 39,
   GETTEMPSTRING = 40,
   STRINGLENGTH = 43,
   STRINGCONSISTSOF = 44,

   SETCLASSVAR = 45,

   CREATETIMER = 51,
   DELETETIMER = 52,
   GETTIMEREMAINING = 53,
   ISTIMER = 54,

   CHANGESECTORFLAGBSP = 58,
   MOVESECTORBSP = 59,
   CHANGETEXTUREBSP = 60,
   CREATEROOMDATA = 61,
   FREEROOM = 62,
   ROOMDATA = 63,

   LINEOFSIGHTVIEW = 69,
   LINEOFSIGHTBSP = 70,
   INTERSECTLINECIRCLE = 71,
   STRINGTONUMBER = 72,
   CANMOVEINROOMBSP = 73,
   GETLOCATIONINFOBSP = 74,
   BLOCKERADDBSP = 75,
   BLOCKERMOVEBSP = 76,
   BLOCKERREMOVEBSP = 77,
   BLOCKERCLEARBSP = 78,
   GETRANDOMPOINTBSP = 79,
   GETSTEPTOWARDSBSP = 80,
   GETRANDOMMOVEDESTBSP = 81,
   GETSECTORHEIGHTBSP = 82,
   SETROOMDEPTHOVERRIDEBSP = 83,
   CALCUSERMOVEMENTBUCKET = 84,

   DELLASTLISTELEM = 98,
   GETALLLISTNODESBYCLASS = 99,
   APPENDLISTELEM = 100,
   CONS = 101,

   LENGTH = 104,
   NTH = 105,
   MLIST = 106,
   ISLIST = 107,
   SETFIRST = 108,
   SETNTH = 109,
   DELLISTELEM = 110,
   FINDLISTELEM = 111,
   SWAPLISTELEM = 112,
   INSERTLISTELEM = 113,
   LAST = 114,
   ISLISTMATCH = 115,
   GETLISTELEMBYCLASS = 116,
   GETLISTNODE = 117,
   LISTCOPY = 118,

   GETTIME = 120,
   GETTICKCOUNT = 121,
   GETDATEANDTIME = 122,
   GETUNIXTIMESTRING = 123,
   OLDTIMESTAMPFIX = 124,

   ABS = 131,
   BOUND = 132,
   SQRT = 133,

   CREATETABLE = 141,
   ADDTABLEENTRY = 142,
   GETTABLEENTRY = 143,
   DELETETABLEENTRY = 144,
   DELETETABLE = 145,
   ISTABLE = 146,

   RECYCLEUSER = 151,

   ISOBJECT = 161,

   RANDOM = 201,
   
   RECORDSTAT = 210,
   
   GETSESSIONIP = 220,
};

enum
{
   NIL = 0,
   TAG_NIL = 0,
   TAG_INT = 1,
   TAG_OBJECT = 2,
   TAG_LIST = 3,
   TAG_RESOURCE = 4,
   TAG_TIMER = 5,
   TAG_SESSION = 6,
   TAG_ROOM_DATA = 7,
   TAG_TEMP_STRING = 8,
   TAG_STRING = 9,
   TAG_CLASS = 10,
   TAG_MESSAGE = 11,
   TAG_DEBUGSTR = 12,
   TAG_TABLE = 13,
   TAG_OVERRIDE = 14,  // For overriding a class variable with a property
   TAG_EMPTY = 15,     // Empty tag available for use
};

#define MAX_TAG 12
#define MAX_KOD_INT ((1<<27)-1)  // 28th bit is sign. 0x07ffffff == kod +134217727
#define MASK_KOD_INT ((1<<28)-1) // 28th bit is sign. 0x0fffffff == kod -1
#define MIN_KOD_INT (1<<27)      // 28th bit is sign. 0x08000000 == kod -134217728
#define KODFINENESS 64           // how many fine rows give a full row

// num bits to shift a tag to get it to the correct position.
// 28 for 32-bit kod data types, presumably 56 for 64-bit.
#define KOD_SHIFT 28

#define KOD_FALSE (TAG_INT << KOD_SHIFT)
#define KOD_TRUE ((TAG_INT << KOD_SHIFT)+1)

// Defined here for sendmsg.h/c and codegen.c.
// Max number of locals in a message.
#define MAX_LOCALS 50
// Max number of named parameters in message header.
#define MAX_NAME_PARMS 45
// Max number of parameters to a call.
#define MAX_C_PARMS 40

typedef struct
{
   int data:28;
   unsigned int tag:4;
} constant_type;

// New opcode data - usually only require source bits.
// Opcodes that require a source and dest use source2 for the dest bits.
typedef struct
{
   unsigned int source2 : 4;
   unsigned int source1 : 4;
} opcode_data;

// This is the type of any kod data structure, i.e. kod integers (data and tag).
// Added as a separate typedef for possible future transition to 64-bit types.
typedef int bkod_type;

#endif
