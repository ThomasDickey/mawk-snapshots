/********************************************
symtype.h
copyright 2009-2012,2016, Thomas E. Dickey
copyright 1991, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: symtype.h,v 1.18 2016/09/30 13:22:32 tom Exp $
 */

/* types related to symbols are defined here */

#ifndef  SYMTYPE_H
#define  SYMTYPE_H

#include <types.h>

/* struct to hold info about builtins */
typedef struct {
    const char *name;
    PF_CP fp;			/* ptr to function that does the builtin */
    unsigned char min_args, max_args;
/* info for parser to check correct number of arguments */
} BI_REC;

/*---------------------------
   structures and types for arrays
 *--------------------------*/

#include <array.h>

extern ARRAY Argv;

/* for parsing  (i,j) in A  */
typedef struct {
    int start;			/* offset to code_base */
    int cnt;
} ARG2_REC;

/*------------------------
  user defined functions
  ------------------------*/

typedef struct fblock {
    const char *name;
    INST *code;
    size_t size;
    unsigned short nargs;
    char *typev;		/* array of size nargs holding types */
} FBLOCK;			/* function block */

extern void add_to_fdump_list(FBLOCK *);
extern void dump_funcs(void);
extern void dump_regex(void);

/*-------------------------
  elements of the symbol table
  -----------------------*/

typedef enum {
    ST_NONE
    ,ST_VAR
    ,ST_KEYWORD
    ,ST_BUILTIN			/* a pointer to a builtin record */
    ,ST_ARRAY			/* a void * ptr to a hash table */
    ,ST_FIELD			/* a cell ptr to a field */
    ,ST_FUNCT
    ,ST_NR			/*  NR is special */
    ,ST_ENV			/* and so is ENVIRON */
    ,ST_LENGTH			/* ditto and bozo */
    ,ST_LOCAL_NONE
    ,ST_LOCAL_VAR
    ,ST_LOCAL_ARRAY
} SYMTAB_TYPES;

#define  is_array(stp)   ((stp)->type == ST_ARRAY || (stp)->type == ST_LOCAL_ARRAY)
#define  is_local(stp)   ((stp)->type >= ST_LOCAL_NONE)

typedef struct {
    const char *name;
    char type;
    unsigned char offset;	/* offset in stack frame for local vars */
#ifdef NO_LEAKS
    char free_name;
#endif
    union {
	CELL *cp;
	int kw;
	PF_CP fp;
	const BI_REC *bip;
	ARRAY array;
	FBLOCK *fbp;
    } stval;
} SYMTAB;

/*****************************
 structures for type checking function calls
 ******************************/

typedef struct ca_rec {
    struct ca_rec *link;
    short type;
    short arg_num;		/* position in callee's stack */
/*---------  this data only set if we'll  need to patch -------*/
/* happens if argument is an ID or type ST_NONE or ST_LOCAL_NONE */

    int call_offset;
/* where the type is stored */
    SYMTAB *sym_p;		/* if type is ST_NONE  */
    char *type_p;		/* if type  is ST_LOCAL_NONE */
} CA_REC;			/* call argument record */

/* type field of CA_REC matches with ST_ types */
#define   CA_EXPR       ST_LOCAL_VAR
#define   CA_ARRAY      ST_LOCAL_ARRAY

typedef struct fcall {
    struct fcall *link;
    FBLOCK *callee;
    short call_scope;
    short move_level;
    FBLOCK *call;		/* only used if call_scope == SCOPE_FUNCT  */
    INST *call_start;		/* computed later as code may be moved */
    CA_REC *arg_list;
    short arg_cnt_checked;
} FCALL_REC;

extern FCALL_REC *resolve_list;

extern void resolve_fcalls(void);
extern void check_fcall(FBLOCK * callee, int call_scope, int move_level,
			FBLOCK * call, CA_REC * arg_list);
extern void relocate_resolve_list(int, int, FBLOCK *, int, unsigned, int);

/* hash.c */
extern unsigned hash(const char *);
extern unsigned hash2(const char *, size_t);
extern SYMTAB *insert(const char *);
extern SYMTAB *find(const char *);
extern const char *reverse_find(int, PTR);
extern SYMTAB *save_id(const char *);
extern void restore_ids(void);

/* error.c */
extern const char *type_to_str(int);
extern void type_error(SYMTAB *);

#ifdef NO_LEAKS
extern void no_leaks_array(ARRAY);
#else
#define no_leaks_array(a)	/* nothing */
#endif

#endif /* SYMTYPE_H */
