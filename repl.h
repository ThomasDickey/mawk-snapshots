/********************************************
repl.h
copyright 2009-2014,2020, Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: repl.h,v 1.9 2020/07/17 23:16:52 tom Exp $
 */

/* repl.h */

#ifndef  REPL_H
#define  REPL_H

#include "types.h"

typedef struct re_data {
    PTR compiled;		/* must be first... */
    int anchored;		/* use to limit recursion in gsub */
    int is_empty;		/* check if pattern is empty */
} RE_DATA;

/*
 * re_compile returns a RE_DATA*, but mawk handles it as a PTR thereafter.
 */
#define isAnchored(ptr) (((RE_DATA *)(ptr))->anchored)
#define isEmpty_RE(ptr) (((RE_DATA *)(ptr))->is_empty)
#define cast_to_re(ptr) (((RE_DATA *)(ptr))->compiled)
#define refRE_DATA(re)  ((PTR) &(re))

PTR re_compile(STRING *);
char *re_uncompile(PTR);

CELL *repl_compile(STRING *);
char *repl_uncompile(CELL *);
void re_destroy(PTR);
void repl_destroy(CELL *);
CELL *replv_cpy(CELL *, CELL *);
CELL *replv_to_repl(CELL *, STRING *);

#endif
