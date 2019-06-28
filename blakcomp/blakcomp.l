	/* Lex file for Blakod compiler */

%option stack

%{
#include "blakcomp.h"
#include "blakcomp.tab.h"

#ifdef __cplusplus
extern "C" {
#endif
int yywrap(void);
#ifdef __cplusplus
}
#endif

extern int yyparse(void);
static FILE *open_file(char *fname);

static void compile_file_mode();
static void usage(void);
static void usage_directory(void);

int lineno;
Bool generate_code = True; /* True if we should generate code */
extern int codegen_ok;     // True if codegen completed without errors
extern int yydebug;        /* Set to nonzero to get yacc debug info */
int directory_mode;        /* Whether we are compiling in directory mode or not */
list_type directory_list;  /* If compiling in directory mode, the list of dirs */
list_type file_list;       /* List of files to compile */
char *current_fname;
char current_path[_MAX_PATH];
char *bof_fname;           /* Object code filename */
int debug_bof;             /* Should we put debugging info in .bof file? */
int print_dup_eng_ger;     /* Whether we print duplicate Eng-Ger rscs */
int print_missing_ger;     /* Whether we print missing German rscs */
int print_unref_rscs;      /* Whether we print out rscs with no references */
int print_unref_locals;    /* Whether we print out local vars with no references */
int add_eol = 0;           /* Hack to deal with EOL being significant in INITIAL */
int comment_depth = 0;     /* Allow nested comments */

// Keep track of whether we are processing constants or not, because we can
// handle constant include files in a much faster way.
Bool in_constants = False;

// bofs go into /output_dir/loadkod/
// rscs go into /output_dir/rsc/
char bof_output_dir[_MAX_PATH];
char rsc_output_dir[_MAX_PATH];

struct file_state {
   YY_BUFFER_STATE buffer;
   char *filename;
   int lineno;
   FILE *file_ptr;
};


#define MAX_INCLUDE_DEPTH 10
#define MAX_INCLUDE_DIRS  20

struct file_state include_stack[MAX_INCLUDE_DEPTH + 1];  /* stack of nested included files */
int include_depth;                                       /* nesting depth of include files */

static char *include_dirs[MAX_INCLUDE_DIRS];/* include directory names */             
static int num_include_dirs;               /* # of include directories specified on command line */
%}

	/* A single line of a multiple-line string constant, minus newline & initial quote */
strline  (([^\"\n\\]|\\.)*\")[ \t\r]*

   /* String mode is for inside strings.  Code mode is for when we reach the messages block */
%s STRING CODE COMMENT

%%

	/* strings -- allow backslash to introduce escaped characters or extend line */

<STRING>(([^\"\n\\]|\\.)*$)	{ yyerror("Unterminated string"); 
			  yylval.string_val = assemble_string(yytext);
			  yy_pop_state();
			  return STRING_CONSTANT; 
			}

   /* This nastiness allows multiple-line C-like strings to be interpreted as a single string */
<STRING>{strline}(\n[ \t\r]*\"{strline})*   {
			  yylval.string_val = assemble_string(yytext);
			  yy_pop_state();
			  return STRING_CONSTANT; 
			}			

\"			{ yy_push_state(STRING); }

   /* Handle comments */
   /* Start multi-line comments */
<CODE>"/*" {
   ++comment_depth;
   yy_push_state(COMMENT);
}

   /* INITIAL requires we keep track of whether to add EOL after comment */
<INITIAL>^[ \t\r]*"/*" {
   add_eol = false;
   ++comment_depth;
   yy_push_state(COMMENT);
}

   /* If we matched something other than nothing or whitespace before */
   /* the comment starts, we have to return EOL when the comment ends.*/
<INITIAL>"/*" {
   add_eol = true;
   ++comment_depth;
   yy_push_state(COMMENT);
}

   /* Close multi-line comments */
<COMMENT>"*/" {
   comment_depth--;
   if (!comment_depth)
   {
      yy_pop_state();
      if (YYSTATE == INITIAL && add_eol)
         return EOL;
   }
}

   /* Close multi-line comments with newline */
<COMMENT>"*/"[ \t\r]*\n {
   comment_depth--;
   lineno++;
   if (!comment_depth)
   {
      yy_pop_state();
      if (YYSTATE == INITIAL && add_eol)
         return EOL;
   }
}

   /* If two comments are separated by whitespace, join them. */
<COMMENT>"*/"[ \t\r]*"/*" ;
   /* Allow nested comments */
<COMMENT>"/*" comment_depth++;
   /* Single line comments take precedence over multilines. */
<COMMENT>"//"[^\n]* ;
   /* Throw error if we hit EOF while in comment mode. */
<COMMENT><<EOF>> yyerror("Unclosed multi-line comment ");
   /* Match any line not containing * / and newline (faster) */
<COMMENT>[^*/\n]* ;
   /* Ignore newline, but increment lineno. */
<COMMENT>\n lineno++;
   /* Ignore other characters. */
<COMMENT>. ;

   /* Old kod style comments - removed in BOF version 7 */
   /*   Kod-style single-line comments */
   /* Ignore comment-only lines completely */
   /* ^[ \t\r]*"\%".*\n	lineno++; */
   /* Otherwise just delete comment part of line */
   /* "\%".* ; */

   /* C++-style single-line comments */
   /* Ignore comment-only lines completely */
^[ \t\r]*"//".*\n lineno++;
   /* Otherwise just delete comment part of line */
"//".* ;

   /* Region folding, disallow = and ; in region name to avoid */
   /* conflict with # parameter names */
^[ \t\r]*"#region"[^\=;\n]*\n lineno++;
^[ \t\r]*"#endregion"[^\=;\n]*\n lineno++;

	/* reserved words */
and		{ return AND; }
break		{ return BREAK; }
classvars	{ in_constants = False; return CLASSVARS; }
continue	{ return CONTINUE; }
constants	{ in_constants = True; return CONSTANTS; }
else            { return ELSE;}
end		{ in_constants = False; yy_push_state(INITIAL); return END; } 
for		{ return FOR; }
foreach	{ return FOREACH; }
if		{ return IF; }
in		{ return IN; }
is		{ return IS; }
isclass { return ISCLASS; }
getclass { return GETCLASS; }
first { return FIRST; }
rest { return REST; }
local		{ return LOCAL; }
messages	{ in_constants = False; yy_push_state(CODE); return MESSAGES; }
not		{ return NOT; }
or		{ return OR; }
propagate	{ return PROPAGATE; }
properties	{ in_constants = False; return PROPERTIES; }
resources	{ in_constants = False;  return RESOURCES; }
return	        { return RETURN; }
while		{ return WHILE; }
include         { return INCLUDE; }
do { return DO; }
case { return CASE; }
switch { return SWITCH; }
default { return DEFAULT; }

	/* punctuation */
"<>"	{ yylval.int_val = NEQ_OP; return REL_OP; }
"<"	{ yylval.int_val = LT_OP; return REL_OP; }
">"	{ yylval.int_val = GT_OP; return REL_OP; }
"<="	{ yylval.int_val = LEQ_OP; return REL_OP; }
">="	{ yylval.int_val = GEQ_OP; return REL_OP; }
"--"	{ yylval.int_val = DEC_OP; return DEC_OP; }
"++"	{ yylval.int_val = INC_OP; return INC_OP; }
"+="  { yylval.int_val = PLUS_OP; return ASSIGN_OP; }
"-="  { yylval.int_val = MINUS_OP; return ASSIGN_OP; }
"*="  { yylval.int_val = MULT_OP; return ASSIGN_OP; }
"/="  { yylval.int_val = DIV_OP; return ASSIGN_OP; }
"%="  { yylval.int_val = MOD_OP; return ASSIGN_OP; }
"&="  { yylval.int_val = BITAND_OP; return ASSIGN_OP; }
"|="  { yylval.int_val = BITOR_OP; return ASSIGN_OP; }

[-+*/%,:#&@()={}\$\[\]\|~]	{ return yytext[0]; }	/* return character itself */
\;                              { return SEP; }  /* Separator */

	/* names */

[A-Za-z][A-Za-z0-9_]*	{ yylval.idname = strdup(yytext); return IDENTIFIER; }
[A-Za-z0-9_\-]*\.[A-Za-z0-9_\-]*	{ yylval.idname = strdup(yytext); return FILENAME; }

	/* numbers */

[0-9]+			{ yylval.int_val = atoi(yytext); return NUMBER; }
0x[0-9a-fA-F]+		{ sscanf(yytext, "%x", &yylval.int_val); return NUMBER; }

  	/* line continuation */
\\[ \t\r]*\n		lineno++;	/* Allow multiple-line statements */

	/* whitespace */
^[ \t\r]*\n	lineno++;	/* Skip completely blank lines */
[ \t\r]+	;
<CODE>\n    lineno++;       /* When parsing messages, throw out newlines */
<INITIAL>\n	{ lineno++; return EOL; } /* Parser needs to know about other newlines */

	/* anything else */
.	yyerror("Invalid character");

%%

/************************************************************************/
void handle_error(void)
{
   static int numerrors = 0;
   generate_code = False; /* Errors; don't generate code */
   if (++numerrors > MAXERRORS)
   {
      printf("Too many errors. Aborting.\n");
      exit(1);
   }
}
/************************************************************************/
/* yyerror should be used for lexing & parsing errors. */
void yyerror(const char *s)
{
   // Kod files store full path, include files don't.
   char *fname = strrchr(current_fname, '\\');
   if (fname)
      fname++; // Move past the slash.
   else
      fname = current_fname;

   if (!s || !yytext)
   {
      printf("%s(%d): fatal error: Error occurred but no data reported from parser\n",
         fname, lineno);
   }
   else if (!strcmp(yytext, "\n"))
   {
      /* If token is EOL, give useful error message */
      printf("%s(%d): fatal error: Premature end of line\n",
         fname, lineno - 1);
   }
   else if (!strcmp(s,"syntax error, unexpected ISCLASS") && !stricmp(yytext,"isclass"))
   {
      // Bad token ISCLASS and yytext is IsClass, likely non-assignment of IsClass call.
      printf("%s(%d): fatal error: result of IsClass MUST be assigned to local or property\n",
         fname, lineno);
   }
   else if (!strcmp(s,"syntax error, unexpected FIRST") && !stricmp(yytext,"first"))
   {
      // Bad token FIRST and yytext is first, likely non-assignment of First call.
      printf("%s(%d): fatal error: result of First MUST be assigned to local or property\n",
         fname, lineno);
   }
   else if (!strcmp(s,"syntax error, unexpected REST") && !stricmp(yytext,"rest"))
   {
      // Bad token REST and yytext is rest, likely non-assignment of Rest call.
      printf("%s(%d): fatal error: result of Rest MUST be assigned to local or property\n",
         fname, lineno);
   }
   else if (!strcmp(s,"syntax error, unexpected GETCLASS") && !stricmp(yytext,"getclass"))
   {
      // Bad token GETCLASS and yytext is getclass, likely non-assignment of GetClass call.
      printf("%s(%d): fatal error: result of GetClass MUST be assigned to local or property\n",
         fname, lineno);
   }
   else
   {
      printf("%s(%d): fatal error: %s on token %s\n",
         fname, lineno, s, yytext);
   }

   // These are critical errors, so just exit. Continuing will potentially
   // crash blakcomp.
   exit(1);
}
/************************************************************************/
/* action_error should be used for semantic errors. */
void action_error(const char *fmt, ...)
{
   int errline = lineno;
   va_list marker;

   /* If last token was EOL, report error on previous line */
   if (!strcmp(yytext, "\n"))
      errline--;

   // Kod files store full path, include files don't.
   char *fname = strrchr(current_fname, '\\');
   if (fname)
      fprintf(stderr, "%s(%d): error: ", fname + 1, errline);
   else
      fprintf(stderr, "%s(%d): error: ", current_fname, errline);

   va_start(marker, fmt);
   vprintf(fmt, marker);
   va_end(marker);
   printf("\n");

   handle_error();
}
/************************************************************************/
/* simple_error should be used for other errors. */
void simple_error(const char *fmt, ...)
{
   va_list marker;

   printf("  error: ");
   va_start(marker, fmt);
   vprintf(fmt, marker);
   va_end(marker);
   printf("\n");

   handle_error();   
}
/************************************************************************/
/* simple_warning should be used for all warnings. */
void simple_warning(const char *fmt, ...)
{
   va_list marker;

   printf("  warning: ");
   va_start(marker, fmt);
   vprintf(fmt, marker);
   va_end(marker);
   printf("\n");
}
/************************************************************************/
/*
 * assemble_string:  Convert a Blakod multiple-line string into a
 *   plain string, without starting and ending quotes, and with escape
 *   sequences replaced.
 */
char *assemble_string(char *str)
{
   char *new_str = strdup(str);
   char *rd, *wt;
   int inside_quotes = 1;

   rd = new_str;
   wt = new_str;

   while (*rd != '\0')
   {
      /* Throw out junk outside quotes */
      if (!inside_quotes && *rd != '"')
      {
	 if (*rd == '\n')
	    lineno++;
	 rd++;
	 continue;
      }
	 
      switch (*rd)
      {
      case '\\':
	 rd++;
	 
	 /* Look for special characters */
	 switch (*rd)
	 {
	 case 'n':
	    *wt = '\r';    /* Ugh.. CR-LF pair for Windows ugliness */
	    wt++;
	    *wt = '\n';
	    wt++;
	    break;

	 default:
	    *wt = *rd;
	    wt++;
	    break;
	    
	 }
	 break;

      case '"':  /* Ignore quotes */
	 inside_quotes = 1 - inside_quotes;
	 break;

      case '\n': /* Ignore newlines */
	 lineno++;
	 break;

      default:
	 *wt = *rd;
	 wt++;
	 break;
      }

      rd++;
   }
   *wt = '\0';
   return new_str;
}
/************************************************************************/
/* 
 * recompile_warnings:  Give warning messages for each class that should
 *    be recompiled because one of its ancestors was recompiled.
 */
void recompile_warnings(list_type l)
{
   if (l == NULL)
      return;

   // Too much spam.

   /*
   int column = 0;
   for (; l != NULL; l = l->next)
   {
      const char *name = ((recompile_type) (l->data))->class_id->name;
      
      if (column + strlen(name) >= 75)
      {
         printf("\n");
         column = 0;
      }
      printf("%s  ", name);
      column += strlen(name) + 2;

      // ((recompile_type) (l->data))->superclass->name); 
   }
   printf("\n");*/
}
/************************************************************************/
int yywrap(void)
{
   fclose(include_stack[include_depth].file_ptr);

   yy_delete_buffer(YY_CURRENT_BUFFER);
   /* XXX Should close file; but this causes access violation, because
      FILE pointer is out of scope.  
    */

   /* If in an included file, move back to including file */
   if (include_depth > 0)
   {
      // Could be parsing a constants include file.
      if (include_depth == 1)
         include_const_file_parse_finished();
      include_depth--;
      lineno = include_stack[include_depth].lineno;
      current_fname = include_stack[include_depth].filename;
      yy_switch_to_buffer(include_stack[include_depth].buffer);

      return 0;
   }

   return 1; /* Otherwise stop reading the current file */
}
/************************************************************************/
/*
 * open_file:  Open the given filename and return a FILE pointer to it.
 *    If the file has no extension, we add a ".kod" extension.
 *    Also set bof_fname to the corresponding ".bof" filename.
 */
FILE *open_file(char *fname)
{
   char temp[256];
   FILE *fin;

   /* Add .kod extension if necessary */
   set_extension(temp, fname, ".kod");
   
   if ((fin = fopen(temp, "r")) == NULL)
   {
      perror(fname);
      exit(1);
   }

   current_fname = strdup(temp);

   /* Construct object code filename */
   bof_fname = strdup(fname);
   set_extension(bof_fname, bof_fname, ".bof");

   return fin;
}
/************************************************************************/
/*
 * open_include_file:  Attempt to open given filename in all include
 *   directories.  Return file pointer, or NULL on failure.
 */
FILE *open_include_file(char *filename)
{
   FILE *fin;
   int i;
   char tempbuf[512];

   /* Loop through include directories and try to open file */
   for (i=0; i < num_include_dirs; i++)
   {
      sprintf(tempbuf, "%s/%s", include_dirs[i], filename);
      if ((fin = fopen(tempbuf, "r")) != NULL)
	 return fin;
   }

   /* Finally, just try current directory */
   sprintf(tempbuf, "%s/%s", current_path, filename);
   fin = fopen(tempbuf, "r");

   return fin;
}
/************************************************************************/
/*
 * include_file:  Switch flex over to the given file, parse it, and then
 *   return to the original file.  Include files may be nested.
 */
void include_file(char *filename)
{
   FILE *fin;

   if (include_depth >= MAX_INCLUDE_DEPTH)
   {
      action_error("Maximum nesting depth of include files is %d", MAX_INCLUDE_DEPTH);
      return;
   }

   if (directory_mode && in_constants)
   {
      // Record which constants files are parsed in directory mode
      // because we only need to parse them once. Constants are saved
      // in the ID table so we can access them again.
      if (!include_const_file_parse(filename))
      {
         return;
      }
   }

   if ((fin = open_include_file(filename)) == NULL)
   {
      action_error("Can't find include file %s", filename);
      return;
   }

   /* Save away current state, and move on to included file */

   include_stack[include_depth].buffer   = YY_CURRENT_BUFFER;
   include_stack[include_depth].filename = current_fname;
   include_stack[include_depth].lineno   = lineno;
   include_depth++;

   yy_switch_to_buffer(yy_create_buffer(fin, YY_BUF_SIZE));
   include_stack[include_depth].file_ptr = fin;
   current_fname = strdup(filename);
   lineno = 1;
}
/************************************************************************/
/*
 * add_include_directory:  Add given directory to list of directories
 *   that are searched for include files.  These directories can be
 *   specified on the command line.
 */
void add_include_directory(char *dirname)
{
   if (num_include_dirs == MAX_INCLUDE_DIRS)
      simple_error("Too many include directories!");
   else
   {
      include_dirs[num_include_dirs] = dirname;
      num_include_dirs++;
   }
}
/************************************************************************/
/*
* check_output_directory:  Create given directory if it doesn't exist.
*   Also create the two child directories we need (loadkod, rsc).
*/
void check_output_directory(char *dirname)
{
   sprintf(bof_output_dir, "%s\\loadkod", dirname);
   sprintf(rsc_output_dir, "%s\\rsc", dirname);

   if (access(dirname, 4) != 0)
   {
      // Try create it.
      if (_mkdir(dirname) != 0)
      {
         printf("Failed to create output directory %s!\n", dirname);
         exit(1);
      }
   }
   if (access(bof_output_dir, 4) != 0)
   {
      // Try create it.
      if (_mkdir(bof_output_dir) != 0)
      {
         printf("Failed to create output directory %s!\n", bof_output_dir);
         exit(1);
      }
   }
   if (access(rsc_output_dir, 4) != 0)
   {
      // Try create it.
      if (_mkdir(rsc_output_dir) != 0)
      {
         printf("Failed to create output directory %s!\n", rsc_output_dir);
         exit(1);
      }
   }
}
/************************************************************************/
void usage(void)
{
   fprintf(stderr, "usage: bc <options> filename ...\n");
   fprintf(stderr, "     possible options are:\n");
   fprintf(stderr, "     -d            Put debugging info in .bof file\n");
   fprintf(stderr, "     -K file       Specify kodbase file\n");
   fprintf(stderr, "     -I dir        Add dir to include path\n");
}
/************************************************************************/
void usage_directory(void)
{
#ifdef BLAK_PLATFORM_WINDOWS
   fprintf(stderr, "\nOR to compile a directory recursively:\n\n");
   fprintf(stderr, "usage: bc -r <options> directory ...\n");
   fprintf(stderr, "     possible options are:\n");
   fprintf(stderr, "     -d            Put debugging info in .bof file\n");
   fprintf(stderr, "     -K file       Specify kodbase file\n");
   fprintf(stderr, "     -I dir        Add dir to include path\n");
   fprintf(stderr, "     -O dir        Output directory for bof/rsc (e.g. \\run\\server)\n");
#endif
}
/************************************************************************/
void usage_debug(void)
{
   fprintf(stderr, "     -A            Print duplicate English-German resources\n");
   fprintf(stderr, "     -B            Print missing German resources\n");
   fprintf(stderr, "     -C            Print resources with no references beyond declaration\n");
   fprintf(stderr, "     -L            Print local vars with no references beyond declaration\n");
}
/************************************************************************/
int main(int argc, char **argv)
{
   int i;
   char *arg;

   if (argc == 1)
   {
      usage();
      usage_directory();
      usage_debug();
      exit(1);
   }

   file_list = NULL;
   directory_list = NULL;
   debug_bof = False;
   print_dup_eng_ger = False;
   print_missing_ger = False;
   print_unref_rscs = False;
   print_unref_locals = False;
   directory_mode = False;
   bof_output_dir[0] = 0;
   rsc_output_dir[0] = 0;

   num_include_dirs = 0;

   /* Parse command line */
   for (i = 1; i < argc; i++)
   {
      arg = argv[i];
      if (*arg == '-' || *arg == '/')
      {
         /* Switch on command-line options */
         switch(toupper(*(arg + 1)))
         {
         case 'A': /* Print out duplicate Eng-Ger resources */
            print_dup_eng_ger = True;
            break;

         case 'B': /* Print out missing German resources */
            print_missing_ger = True;
            break;

         case 'C': /* Print out resources that don't have any references */
            print_unref_rscs = True;
            break;

         case 'D':               /* Debugging on */
            debug_bof = True;
            break;

         case 'I':               /* Specify include file directory */
            if (i == argc - 1)
               printf("Switch -%c needs a directory (ignoring)\n",*(arg+1));
            else
            {
               i++;
               arg = argv[i];
               if (access(arg,4) == 0) /* 4 means read access */
                  add_include_directory(arg);
               else
               {
                  printf("Unable to read include file directory %s\n",arg);
                  exit(1);
               }
            }
            break;

         case 'K' :              /* Specify kodbase filename */
            if (i == argc - 1)
               printf("Switch -%c needs a filename (ignoring)\n",*(arg+1));
            else
            {
               i++;
               arg = argv[i];
               set_kodbase_filename(arg);
            }
            break;

         case 'L':               /* Print out local vars that don't have any references */
            print_unref_locals = True;
            break;
            
         case 'O': /* Output directory */
            if (i == argc - 1)
               printf("Switch -%c needs a directory (ignoring)\n",*(arg+1));
            else
            {
               i++;
               arg = argv[i];
               // Will exit if it doesn't succeed.
               check_output_directory(arg);
            }
            break;

         case 'R': /* Compiling using a directory instead of a filename */
            directory_mode = True;
            break;

         default:
            printf("Ignoring unknown switch -%c.\n", *(arg + 1));
         }
      }
      else
      {
         if (directory_mode)
            directory_list = list_add_item(directory_list, argv[i]);
         else
            file_list = list_add_item(file_list, argv[i]);
      }
   }

#ifdef BLAK_PLATFORM_WINDOWS
   if (directory_mode)
      compile_directory_mode();
   else
#endif
   compile_file_list(".\\", file_list);

   return !generate_code;
}

void compile_file_list(char *path, list_type l)
{
   if (l == NULL)
   {
      printf("No files specified!\n");
      return;
   }

   /* Uncomment the next line to get parser trace */
   /* yydebug = 1; */

   // In directory mode, these are handled once before compilation.
   if (!directory_mode)
   {
      initialize_parser();

      /* Read in database file */
      if (!load_kodbase())
         simple_error("Error loading database; continuing with compilation");
   }

   // Set to True here in case any start-up processes used simple_error.
   // If compilation shouldn't be performed at this stage, blakcomp
   // should be exited at the appropriate point.
   generate_code = True;

   // Save current path for include files.
   sprintf(current_path,"%s",path);

   for (; l != NULL; l = l->next)
   {
      // Reset per-file counts
      include_depth = 0;
      st.num_strings = 0;
      st.strings = NULL;

      include_stack[0].file_ptr = open_file((char *)l->data);
      include_stack[0].buffer = yy_create_buffer(include_stack[0].file_ptr,
         YY_BUF_SIZE);
      include_stack[0].filename = current_fname;
      yy_switch_to_buffer(include_stack[0].buffer);
      /*
      printf("File %s:\n", (char *) current_file_ptr->data);
      */
      lineno = 1;

      yyparse();

      /* Get rid of buffer for original file */
      // yy_delete_buffer(include_stack[0].buffer);

      if (generate_code)
         codegen(current_fname, bof_fname);

      if (!generate_code || !codegen_ok)
         compile_failed(current_fname);

      generate_code = True;
   }

   // Kodbase saves once at the end (much faster). Also, incremental kodbase
   // with compile errors can lead to failed future builds.
   if (codegen_ok && !directory_mode)
      save_kodbase();

   /* Give warnings for classes that should be recompiled */
   recompile_warnings(st.recompile_list);
}
