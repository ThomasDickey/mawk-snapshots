/********************************************
repl.h
copyright 2009-2020,2023, Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: repl.h,v 1.11 2023/07/20 00:32:26 tom Exp $
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
STRING *re_uncompile(PTR);

CELL *repl_compile(STRING *);
const STRING *repl_uncompile(CELL *);
void re_destroy(PTR);
void repl_destroy(CELL *);
CELL *replv_cpy(CELL *, CELL *);
CELL *replv_to_repl(CELL *, STRING *);

#endif
