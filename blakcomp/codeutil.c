// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/* codeutil.c  Subsidiary procedures for use by codegen.c */

#include "blakcomp.h"
#include "bkod.h"
#include "codegen.h"

// file opened in codegen.c -- ugly that it has the same name as a parameter
// to many functions in this file.
extern int outfile;
extern char *current_fname;

// See codegen.c for explanation.
extern char *codegen_buffer;
extern int codegen_buffer_size;
extern int codegen_buffer_warning_size;

/************************************************************************/
/*
 * codegen_warning: Print a warning message during code generation.
 *   Doesn't halt code generation.
 */
void codegen_warning(int linenumber, const char *fmt, ...)
{
   va_list marker;

   printf("%s(%d): warning: ", current_fname, linenumber);

   va_start(marker, fmt);
   vprintf(fmt, marker);
   va_end(marker);
   printf("\n");
}
/************************************************************************/
/* 
 * codegen_error: Print an error message during code generation
 */
void codegen_error(const char *fmt, ...)
{
   va_list marker;

   fprintf(stderr, "%s: error: ", current_fname);

   va_start(marker, fmt);
   vprintf(fmt, marker);
   va_end(marker);
   printf("\nAborting.\n");

   codegen_ok = False;
}
/************************************************************************/
void OutputOpcode(int outfile, opcode_data opcode)
{
   memcpy(&(codegen_buffer[codegen_buffer_position]), &opcode, 1);
   codegen_buffer_position++;

   // Resize buffer at 90%.
   if (codegen_buffer_position > codegen_buffer_warning_size)
      codegen_resize_buffer();
}
/************************************************************************/
void OutputByte(int outfile, BYTE datum)
{
   memcpy(&(codegen_buffer[codegen_buffer_position]), &datum, 1);
   codegen_buffer_position++;

   // Resize buffer at 90%.
   if (codegen_buffer_position > codegen_buffer_warning_size)
      codegen_resize_buffer();
}
/************************************************************************/
void OutputInt(int outfile, int datum)
{
   memcpy(&(codegen_buffer[codegen_buffer_position]), &datum, 4);
   codegen_buffer_position += 4;

   // Resize buffer at 90%.
   if (codegen_buffer_position > codegen_buffer_warning_size)
      codegen_resize_buffer();
}
/************************************************************************/
/*
 * OutputGotoOpcode:  Choose which GOTO opcode to output based on the
 *    goto type (true, false, unconditional) and the type of the data
 *    to be checked (local, constant, property, classvar).
 */
void OutputGotoOpcode(int outfile, int goto_type, int id_type)
{
   switch (goto_type)
   {
   case GOTO_UNCONDITIONAL:
      OutputByte(outfile, (BYTE)OP_GOTO_UNCOND);
      return;
   case GOTO_IF_TRUE:
      switch (id_type)
      {
      case LOCAL_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_TRUE_L);
         return;
      case CONSTANT:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_TRUE_C);
         return;
      case PROPERTY:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_TRUE_P);
         return;
      case CLASS_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_TRUE_V);
         return;
      default:
         codegen_error("Invalid ID type %d encountered in OutputGotoOpcode",
            id_type);
         return;
      }
   case GOTO_IF_FALSE:
      switch (id_type)
      {
      case LOCAL_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_FALSE_L);
         return;
      case CONSTANT:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_FALSE_C);
         return;
      case PROPERTY:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_FALSE_P);
         return;
      case CLASS_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_FALSE_V);
         return;
      default:
         codegen_error("Invalid ID type %d encountered in OutputGotoOpcode",
            id_type);
         return;
      }
   case GOTO_IF_NULL:
      switch (id_type)
      {
      case LOCAL_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NULL_L);
         return;
      case CONSTANT:
         // This shouldn't be possible, but is included to handle
         // all data types.
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NULL_C);
         simple_warning("Found a constant == $ check\n");
         return;
      case PROPERTY:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NULL_P);
         return;
      case CLASS_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NULL_V);
         return;
      default:
         codegen_error("Invalid ID type %d encountered in OutputGotoOpcode",
            id_type);
         return;
      }
   case GOTO_IF_NEQ_NULL:
      switch (id_type)
      {
      case LOCAL_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NEQ_NULL_L);
         return;
      case CONSTANT:
         // This shouldn't be possible, but is included to handle
         // all data types.
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NEQ_NULL_C);
         simple_warning("Found a constant <> $ check\n");
         return;
      case PROPERTY:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NEQ_NULL_P);
         return;
      case CLASS_VAR:
         OutputByte(outfile, (BYTE)OP_GOTO_IF_NEQ_NULL_V);
         return;
      default:
         codegen_error("Invalid ID type %d encountered in OutputGotoOpcode",
            id_type);
         return;
      }
   default:
      codegen_error("Unknown goto type %d encountered in OutputGotoOpcode!",
         goto_type);
   }
}
/************************************************************************/
/*
 * OutputGotoOffset:  Write out jump offset from goto instruction (source)
 *    to destination. sizeof(bkod_type) is subtracted here as when the
 *    interpreter reads this offset, at least that many bytes will have
 *    been read past 'source'.
 */
void OutputGotoOffset(int outfile, int source, int destination)
{
   OutputInt(outfile, destination - source - sizeof(bkod_type));
}
/************************************************************************/
/*
 * const_to_int:  Convert a compiler constant into an object code constant,
 *    and then change this result to an integer and return it.
 */
int const_to_int(const_type c)
{
   constant_type code_const; /* Stuff to be written out */
   int result;

   switch(c->type)
   {
   case C_NUMBER:
      code_const.tag = TAG_INT;
      code_const.data = c->value.numval;
      break;

   case C_NIL:
      code_const.tag = TAG_NIL;
      /* Value is ignored, but put 0 anyway */
      code_const.data = 0;
      break;

   case C_RESOURCE:
      code_const.tag = TAG_RESOURCE;
      code_const.data = c->value.numval;
      break;

   case C_CLASS:
      code_const.tag = TAG_CLASS;
      code_const.data = c->value.numval;
      break;

   case C_MESSAGE:
      code_const.tag = TAG_MESSAGE;
      code_const.data = c->value.numval;
      break;

   case C_STRING:    // Added 8/2/95 for kod debugging strings ARK
      code_const.tag = TAG_DEBUGSTR;
      code_const.data = c->value.numval;
      break;

   case C_OVERRIDE:  // Added 2/16/96 for overriding class variables with properties ARK
      code_const.tag = TAG_OVERRIDE;
      code_const.data = c->value.numval;
      break;

   default:
      simple_error("Unknown constant type (%d) encountered", c->type);
      break;
   }
   
   memcpy(&result, &code_const, sizeof(code_const));
   return result;
}
/************************************************************************/
/*
 * OutputConstant: write out a constant, after adding the appropriate
 *   tag bits to the beginning of the number.  The tag bits indicate the
 *   type of the constant, and are given in bkod.h.
 */
void OutputConstant(int outfile, const_type c)
{
   int outnum = const_to_int(c);

   memcpy(&(codegen_buffer[codegen_buffer_position]), &outnum, sizeof(outnum));
   codegen_buffer_position += sizeof(outnum);

   // Resize buffer at 90%.
   if (codegen_buffer_position > codegen_buffer_warning_size)
      codegen_resize_buffer();
}
/************************************************************************/
/*
 * OutputBaseExpression:  Given a base expression, first write out 1 byte
 *    for its type, and then 4 bytes for its value
 */
void OutputBaseExpression(int outfile, expr_type expr)
{
   id_type id;

   switch(expr->type)
   {
   case E_IDENTIFIER:
      id = expr->value.idval;
      switch (id->type)
      {
      case I_PROPERTY:
	 OutputByte(outfile, (BYTE) PROPERTY);
	 break;
	 
      case I_LOCAL:
	 OutputByte(outfile, (BYTE) LOCAL_VAR);
	 break;

      case I_CLASSVAR:
	 OutputByte(outfile, (BYTE) CLASS_VAR);
	 break;

      default:
	 codegen_error("Bad variable type on %s in OutputBaseExpression", id->name);
      }
      OutputInt(outfile, id->idnum);
      break;

   case E_CONSTANT:
      OutputByte(outfile, (BYTE) CONSTANT);
      OutputConstant(outfile, expr->value.constval);
      break;

   default:
      codegen_error("Bad expression type (%d) in OutputBaseExpression", expr->type);
   }
}
/************************************************************************/
/*
 * BackpatchGotoUnconditional: Go back to the spot "source" in the file,
 *   and write out the offset required to jump to "destination".  Then
 *   return to "destination" in the file.
 */
void BackpatchGotoUnconditional(int outfile, int source, int destination)
{
   FileGoto(outfile, source);
   OutputGotoOffset(outfile, source, destination);
   FileGoto(outfile, destination);
}
/************************************************************************/
/*
 * BackpatchGotoConditional: Go back to the spot "source" in the file,
 *   and write out the offset required to jump to "destination".  Then
 *   return to "destination" in the file. Subract sizeof(bkod_type) to
 *   account for the check data having been read in by the interpreter,
 *   offsetting source from the expected point.
 */
void BackpatchGotoConditional(int outfile, int source, int destination)
{
   FileGoto(outfile, source);
   OutputGotoOffset(outfile, source, destination - sizeof(bkod_type));
   FileGoto(outfile, destination);
}
/************************************************************************/
/*
 * set_source_id:  Set opcode source field to LOCAL_VAR, PROPERTY, CONSTANT,
 *    or CLASS_VAR, depending on type of given expression.  
 *    (The expression must be one of these types).  
 *    An integer is returned, meaning:
 *      The id # of the local variable or property, or
 *      The contant value (from const_to_int), 
 *    depending on the type of expression.
 *    Sourcenum must be either SOURCE1 or SOURCE2; the corresponding field 
 *    of opcode is set.
 */
int set_source_id(opcode_data *opcode, int sourcenum, expr_type e)
{
   id_type id;
   int temp, retval;

   switch (e->type)
   {
   case E_CONSTANT:
      temp = CONSTANT;
      retval = const_to_int(e->value.constval);
      break;

   case E_IDENTIFIER:
      id = e->value.idval;
      retval = id->idnum;
      switch (id->type)
      {
      case I_LOCAL:
         temp = LOCAL_VAR;
         break;

      case I_PROPERTY:
         temp = PROPERTY;
         break;

      case I_CLASSVAR:
         temp = CLASS_VAR;
         break;

      default:
         codegen_error("Identifier in expression not a local or property: %s",
            id->name);
      }
      break;

   default:
      codegen_error("Bad expression type (%d) in set_source_id", e->type);

   }

   if (sourcenum == SOURCE1)
      opcode->source1 = temp;
   else if (sourcenum == SOURCE2)
      opcode->source2 = temp;
   else
      codegen_error("Illegal sourcenum value in set_source_id");

   return retval;
}
/************************************************************************/
/*
 * set_dest_id: Set opcode destination (source2) field to local var or
 *   property, depending on type of given id.
 *   Returns id # of given id.
 */
int set_dest_id(opcode_data *opcode, id_type id)
{
   switch (id->type)
   {
   case I_LOCAL:
      opcode->source2 = LOCAL_VAR;
      break;
   case I_PROPERTY:
      opcode->source2 = PROPERTY;
      break;
   default:
      codegen_error("Identifier in expression not a local or property: %s", 
         id->name);
   }
   return id->idnum;
}
/************************************************************************/
/* 
 * is_base_level: returns True iff e is a base-level expression; i.e. if
 *     it is a leaf of an expression tree. 
 */
int is_base_level(expr_type e)
{
   return e->type == E_IDENTIFIER || e->type == E_CONSTANT;
}
/************************************************************************/
/*
 * make_temp_var: Create & return an id for a local variable with the given id #.
 */
id_type make_temp_var(int idnum)
{
   id_type id = (id_type) SafeMalloc(sizeof(id_struct));
   id->type = I_LOCAL;
   id->idnum = idnum;
   return id;
}
/************************************************************************/
/* 
 * flatten_expr: Generate code to produce the result of the given expression
 *   into the given variable.
 *   maxlocal should be the highest currently used local variable.
 *   Returns highest # local variable used in evaluating expression.
 */
int flatten_expr(expr_type e, id_type destvar, int maxlocal)
{
   opcode_data opcode;
   expr_type tempexpr;
   int sourceval1, sourceval2, destval, our_maxlocal = maxlocal, templocals;
   int op, gotopos, exitpos;
   int binary_dest_type;

   memset(&opcode, 0, sizeof(opcode));  /* Set opcode to all zeros */
   destval = set_dest_id(&opcode, destvar);

   switch (e->type)
   {
   case E_CONSTANT:

      // Operation is assignment (local var = constant)
      if (opcode.source2 == LOCAL_VAR)
         OutputByte(outfile, (BYTE)OP_UNARY_NONE_L);
      else if (opcode.source2 == PROPERTY)
         OutputByte(outfile, (BYTE)OP_UNARY_NONE_P);
      else
         codegen_error("Unknown dest var type (%d) encountered",
            destvar->type);

      // Source is a constant
      opcode.source1 = CONSTANT;
      OutputOpcode(outfile, opcode);

      // Destination is var #destval
      OutputInt(outfile, destval);

      // Source is the constant itself
      OutputConstant(outfile, e->value.constval);

      break;

   case E_IDENTIFIER:
      // Operation is assignment (local var = identifier)
      if (opcode.source2 == LOCAL_VAR)
         OutputByte(outfile, (BYTE)OP_UNARY_NONE_L);
      else if (opcode.source2 == PROPERTY)
         OutputByte(outfile, (BYTE)OP_UNARY_NONE_P);
      else
         codegen_error("Unknown dest var type (%d) encountered",
            destvar->type);

      // Source is an identifier
      set_source_id(&opcode, SOURCE1, e);
      OutputOpcode(outfile, opcode);

      // Destination is local var #destlocal
      OutputInt(outfile, destval);

      // Source is the id # of the identifier
      OutputInt(outfile, e->value.idval->idnum);

      break;

   case E_UNARY_OP:
      /* If operand is simple, compute result directly, else store in temp */
      tempexpr = e->value.unary_opval.exp;
      if (is_base_level(tempexpr))
         sourceval1 = set_source_id(&opcode, SOURCE1, tempexpr);
      else
      {
         /* Evaluate rhs, store in destlocal, then perform operation */
         our_maxlocal = flatten_expr(tempexpr, destvar, our_maxlocal);

         /* Source is same as destination; perform op in place */
         opcode.source1 = opcode.source2;
         sourceval1 = destvar->idnum;
      }

      // Write out operation type
      if (opcode.source2 == LOCAL_VAR)
      {
         switch (e->value.unary_opval.op)
         {
         case NEG_OP:      OutputByte(outfile, (BYTE)OP_UNARY_NEG_L);    break;
         case NOT_OP:      OutputByte(outfile, (BYTE)OP_UNARY_NOT_L);    break;
         case BITNOT_OP:   OutputByte(outfile, (BYTE)OP_UNARY_BITNOT_L); break;
         case PRE_INC_OP:  OutputByte(outfile, (BYTE)OP_UNARY_PREINC);   break;
         case PRE_DEC_OP:  OutputByte(outfile, (BYTE)OP_UNARY_PREDEC);   break;
         case POST_INC_OP: OutputByte(outfile, (BYTE)OP_UNARY_POSTINC);  break;
         case POST_DEC_OP: OutputByte(outfile, (BYTE)OP_UNARY_POSTDEC);  break;
         case FIRST_OP:    OutputByte(outfile, (BYTE)OP_FIRST_L);        break;
         case REST_OP:     OutputByte(outfile, (BYTE)OP_REST_L);         break;
         case GETCLASS_OP: OutputByte(outfile, (BYTE)OP_GETCLASS_L);     break;

         default:
            codegen_error("Unknown unary operator type (%d) encountered",
               e->value.unary_opval.op);
         }
      }
      else if (opcode.source2 == PROPERTY)
      {
         switch (e->value.unary_opval.op)
         {
         case NEG_OP:      OutputByte(outfile, (BYTE)OP_UNARY_NEG_P);    break;
         case NOT_OP:      OutputByte(outfile, (BYTE)OP_UNARY_NOT_P);    break;
         case BITNOT_OP:   OutputByte(outfile, (BYTE)OP_UNARY_BITNOT_P); break;
         case PRE_INC_OP:  OutputByte(outfile, (BYTE)OP_UNARY_PREINC);   break;
         case PRE_DEC_OP:  OutputByte(outfile, (BYTE)OP_UNARY_PREDEC);   break;
         case POST_INC_OP: OutputByte(outfile, (BYTE)OP_UNARY_POSTINC);  break;
         case POST_DEC_OP: OutputByte(outfile, (BYTE)OP_UNARY_POSTDEC);  break;
         case FIRST_OP:    OutputByte(outfile, (BYTE)OP_FIRST_P);        break;
         case REST_OP:     OutputByte(outfile, (BYTE)OP_REST_P);         break;
         case GETCLASS_OP: OutputByte(outfile, (BYTE)OP_GETCLASS_P);     break;

         default:
            codegen_error("Unknown unary operator type (%d) encountered",
               e->value.unary_opval.op);
         }
      }
      else
         codegen_error("Unknown dest var type (%d) encountered",
            destvar->type);

      OutputOpcode(outfile, opcode);

      // Destination is local var #destlocal
      OutputInt(outfile, destval);
      OutputInt(outfile, sourceval1);

      break;

   case E_BINARY_OP:
      tempexpr = e->value.binary_opval.left_exp;
      op = e->value.binary_opval.op;
      templocals = maxlocal;  /* Holds max # used in one subexpression (left or right) */

      if (is_base_level(tempexpr))
         sourceval1 = set_source_id(&opcode, SOURCE1, tempexpr);
      else
      {
         opcode.source1 = LOCAL_VAR;

         /* Evaluate rhs, store in temporary, and assign it to destvar */
         our_maxlocal++;
         templocals++;  /* Lhs must be stored in temp; rhs must not use this temp */
         sourceval1 = our_maxlocal;
         our_maxlocal = flatten_expr(tempexpr, make_temp_var(our_maxlocal), our_maxlocal);
      }

      exitpos = 0;
      /* Check for short circuiting of AND and OR */
      if (op == AND_OP || op == OR_OP)
      {
         /* If must evaluate rhs, create jump over short circuit code */
         // Output the goto opcode
         OutputGotoOpcode(outfile, (op == AND_OP) ? GOTO_IF_TRUE : GOTO_IF_FALSE, opcode.source1);
         gotopos = FileCurPos(outfile);
         OutputInt(outfile, 0);    /* Leave room for destination address */
         OutputInt(outfile, sourceval1);  /* Same as lhs source value above */
         
         /* Short-circuit code: value of expression is known to be true or false */
         flatten_expr(make_expr_from_constant(make_numeric_constant( (op == AND_OP)
                                                                     ? 0 : 1)), 
                      destvar, maxlocal);

         /* Now jump to end of expression evaluation */
         OutputGotoOpcode(outfile, GOTO_UNCONDITIONAL, 0);
         exitpos = FileCurPos(outfile);
         OutputInt(outfile, 0);    /* Leave room for destination */

         /* Backpatch in goto that skipped short circuit code */
         BackpatchGotoConditional(outfile, gotopos, FileCurPos(outfile));
      }

      // opcode.source2 (dest) is about to be clobbered, and we still need it.
      binary_dest_type = opcode.source2;

      tempexpr = e->value.binary_opval.right_exp;
      if (is_base_level(tempexpr))
         sourceval2 = set_source_id(&opcode, SOURCE2, tempexpr);
      else
      {
         opcode.source2 = LOCAL_VAR;
         
         /* Evaluate rhs, store in temporary, and assign it to destvar */
         templocals++;
         sourceval2 = templocals;
         templocals = flatten_expr(tempexpr, make_temp_var(templocals), templocals);
      }

      /* Write out operation type */
      switch(op)
      {
      case AND_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_AND_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_AND_P);
         break;
      case OR_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_OR_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_OR_P);
         break;
      case PLUS_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_ADD_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_ADD_P);
         break;
      case MINUS_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_SUB_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_SUB_P);
         break;
      case MULT_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_MUL_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_MUL_P);
         break;
      case DIV_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_DIV_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_DIV_P);
         break;
      case MOD_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_MOD_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_MOD_P);
         break;
      case NEQ_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_NEQ_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_NEQ_P);
         break;
      case EQ_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_EQ_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_EQ_P);
         break;
      case LT_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_LESS_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_LESS_P);
         break;
      case GT_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_GREATER_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_GREATER_P);
         break;
      case GEQ_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_GEQ_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_GEQ_P);
         break;
      case LEQ_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_LEQ_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_LEQ_P);
         break;
      case BITAND_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_BITAND_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_BITAND_P);
         break;
      case BITOR_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_BINARY_BITOR_L)
            : OutputByte(outfile, (BYTE)OP_BINARY_BITOR_P);
         break;
      case ISCLASS_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_ISCLASS_L)
            : OutputByte(outfile, (BYTE)OP_ISCLASS_P);
         break;
      case ISCLASS_CONST_OP:
         (binary_dest_type == LOCAL_VAR)
            ? OutputByte(outfile, (BYTE)OP_ISCLASS_CONST_L)
            : OutputByte(outfile, (BYTE)OP_ISCLASS_CONST_P);
         break;
      default:
         codegen_error("Unknown unary operator type (%d) encountered", op);
      }
      OutputOpcode(outfile, opcode);
      OutputInt(outfile, destval);         /* Destination is local var #destlocal */
      OutputInt(outfile, sourceval1);      /* Source is variable id # or constant */
      OutputInt(outfile, sourceval2);

      /* If there was short circuit code, fill in the short-circuiting goto */
      if (exitpos != 0)
         BackpatchGotoUnconditional(outfile, exitpos, FileCurPos(outfile));

      /* See which branch used more temps */
      if (templocals > our_maxlocal)
         our_maxlocal = templocals;
      break;

   case E_CALL:
      /* Place return value of call into destination variable */
      our_maxlocal = codegen_call( ((stmt_type) (e->value.callval))->value.call_stmt_val, 
            destvar, e->lineno, our_maxlocal);
      break;

   default:
      codegen_error("Unknown expression type (%d) encountered", e->type);
   }

   return our_maxlocal;
}
/************************************************************************/
/*  
 * simplify_expr: Generate code to simplify the given expression 
 *    until it is an id or a constant.  
 *    If the expression is complicated, this involves
 *    evaluating it into a temporary variable, and then changing the
 *    expression to just evaluate to the temporary variable.
 *   Returns highest # local variable used in code for expression.
 */
int simplify_expr(expr_type expr, int maxlocal)
{
   int our_maxlocal = maxlocal;
   id_type id;

   if (is_base_level(expr))
      return our_maxlocal;
   
   id = make_temp_var(maxlocal + 1); /* Temporary to store expression */
   
   our_maxlocal = flatten_expr(expr, id, maxlocal);
   
   /* Count temp var, if rhs didn't use any new temps */
   if (our_maxlocal == maxlocal)
      our_maxlocal++;
   
   /* Modify expression to make it just a local variable */
   expr->value.idval = id;
   expr->type = E_IDENTIFIER;
      
   return our_maxlocal;
}
