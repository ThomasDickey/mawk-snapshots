/********************************************
types.h
copyright 2009-2023,2024 Thomas E. Dickey
copyright 1991-1993,2014 Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: types.h,v 1.16 2024/08/25 19:36:05 tom Exp $
 */

/*  types.h  */

#ifndef  MAWK_TYPES_H
#define  MAWK_TYPES_H

#include  <nstd.h>
#include  <sizes.h>

/*  CELL  types  */

typedef enum {
    C_NOINIT
    ,C_DOUBLE
    ,C_STRING
    ,C_STRNUM
    ,C_MBSTRN			/*could be STRNUM, has not been checked */
    ,C_RE
    ,C_SPACE			/* split on space */
    ,C_SNULL			/* split on the empty string  */
    ,C_REPL			/* a replacement string   '\&' changed to &  */
    ,C_REPLV			/* a vector replacement -- broken on &  */
    ,NUM_CELL_TYPES
} MAWK_CELL_TYPES;

/* these defines are used to check types for two
   CELLs which are adjacent in memory */

#define  TWO_NOINITS  (2*(1<<C_NOINIT))
#define  TWO_DOUBLES  (2*(1<<C_DOUBLE))
#define  TWO_STRINGS  (2*(1<<C_STRING))
#define  TWO_STRNUMS  (2*(1<<C_STRNUM))
#define  TWO_MBSTRNS  (2*(1<<C_MBSTRN))
#define  NOINIT_AND_DOUBLE  ((1<<C_NOINIT)+(1<<C_DOUBLE))
#define  NOINIT_AND_STRING  ((1<<C_NOINIT)+(1<<C_STRING))
#define  NOINIT_AND_STRNUM  ((1<<C_NOINIT)+(1<<C_STRNUM))
#define  DOUBLE_AND_STRING  ((1<<C_DOUBLE)+(1<<C_STRING))
#define  DOUBLE_AND_STRNUM  ((1<<C_STRNUM)+(1<<C_DOUBLE))
#define  STRING_AND_STRNUM  ((1<<C_STRING)+(1<<C_STRNUM))
#define  NOINIT_AND_MBSTRN  ((1<<C_NOINIT)+(1<<C_MBSTRN))
#define  DOUBLE_AND_MBSTRN  ((1<<C_DOUBLE)+(1<<C_MBSTRN))
#define  STRING_AND_MBSTRN  ((1<<C_STRING)+(1<<C_MBSTRN))
#define  STRNUM_AND_MBSTRN  ((1<<C_STRNUM)+(1<<C_MBSTRN))

typedef unsigned char UChar;

typedef struct _string
#ifdef Visible_STRING
{
    size_t len;
    unsigned ref_cnt;
    char str[2];
}
#endif
STRING;

/* number of bytes more than the characters to store a
   string */
#define  STRING_OH   (sizeof(STRING)-1)

typedef unsigned short VCount;

typedef struct _cell
#ifdef Visible_CELL
{
    short type;
    VCount vcnt;		/* only used if type == C_REPLV   */
    PTR ptr;
    double dval;
}
#endif
CELL;

/* all builtins are passed the evaluation stack pointer and
   return its new value, here is the type */
typedef CELL *(*PF_CP) (CELL *);

/* an element of code (instruction) */
typedef union {
    int op;
    PTR ptr;
    PF_CP fnc;
} INST;

/* regex types */
typedef int SType;

#endif /* MAWK_TYPES_H */
