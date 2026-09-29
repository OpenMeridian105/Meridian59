// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * codegen.h: Header file for codegen.c and codeutil.c
 */
 
#ifndef _CODEGEN_H
#define _CODEGEN_H

#include <cstdint>
#include <vector>

 // See codegen.c for explanation.
typedef std::int32_t codegen_offset_t;

extern codegen_offset_t codegen_buffer_end;
extern codegen_offset_t codegen_buffer_position;

/* Inline helpers to move around the buffered output stream. */
inline codegen_offset_t FileCurPos(int /*outfile*/)
{
   return codegen_buffer_position;
}

inline void FileGoto(int /*outfile*/, codegen_offset_t pos)
{
   codegen_buffer_end = codegen_buffer_position;
   codegen_buffer_position = pos;
}

inline void FileGotoEnd(int /*outfile*/)
{
   codegen_buffer_position = codegen_buffer_end;
}

typedef unsigned char BYTE;
enum { SOURCE1 = 1, SOURCE2 = 2 };  /* See set_source_id */

typedef std::vector<codegen_offset_t> codegen_offset_list;

/* Structure for loop addresses */
typedef struct {
   /* File offset of top of loop */
   codegen_offset_t toppos;
   /* List of addresses that need to be filled in
    * with file offset of bottom of loop */
   codegen_offset_list break_list;
   /* Addresses that need to be filled in for continue
   * statements in for loops (in while loops,
   * continue jumps backward, so not necessary).
   */
   codegen_offset_list for_continue_list;
   /* Addresses that need to be filled in but have
    * conditional gotos (whereas breaks are unconditional) */
   codegen_offset_list conditional_goto_list;
} *loop_type, loop_struct;

extern int codegen_ok;          /* Did codegen complete successfully? */

void codegen_resize_buffer(void);
void OutputOpcode(int outfile, opcode_data opcode);
void OutputByte(int outfile, BYTE datum);
void OutputInt(int outfile, int datum);
void OutputConstant(int outfile, const_type c);
void OutputGotoOpcode(int outfile, int goto_type, int id_type);
void OutputGotoOffset(int outfile, codegen_offset_t source, codegen_offset_t destination);
void OutputBaseExpression(int outfile, expr_type expr);
void BackpatchGotoUnconditional(int outfile, codegen_offset_t source, codegen_offset_t destination);
void BackpatchGotoConditional(int outfile, codegen_offset_t source, codegen_offset_t destination);

void codegen_error(const char *fmt, ...);
void codegen_warning(int linenumber, const char *fmt, ...);
int const_to_int(const_type c);
int set_source_id(opcode_data *opcode, int sourcenum, expr_type e);
int set_dest_id(opcode_data *opcode, id_type id);
int is_base_level(expr_type e);
id_type make_temp_var(int idnum);
int flatten_expr(expr_type e, id_type destvar, int maxlocal);
int simplify_expr(expr_type expr, int maxlocal);

void codegen_enter_loop(void);
void codegen_exit_loop(void);

void codegen_header(void);
void codegen_string_table(void);
int codegen_return(expr_type expr, int maxlocal);
int codegen_statement(stmt_type s, int numlocals);
void codegen_parameter(param_type p);
void codegen_property(property_type p);
void codegen_message(message_handler_type m);
void codegen_class(class_type c);
int codegen_call(call_stmt_type c, id_type destvar, int linenumber, int maxlocal);

#endif /* #ifndef _CODEGEN_H */
