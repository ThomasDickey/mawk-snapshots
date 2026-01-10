/* array.h */
/*
$MawkId: array.h,v 1.25 2026/01/09 09:19:23 tom Exp $

copyright 2009-2024,2026 Thomas E. Dickey
copyright 1991-1996,2014 Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
-------------------------------------------------------------------------------
array.c and array.h were originally generated with the commands

   notangle -R'"array.c"' array.w > array.c
   notangle -R'"array.h"' array.w > array.h

Notangle is part of Norman Ramsey's noweb literate programming package
available from CTAN(ftp.shsu.edu).
*/

#ifndef ARRAY_H
#define ARRAY_H 1

#include <nstd.h>
#include <types.h>

typedef struct _array
#ifdef Visible_ARRAY
{
    PTR ptr;			/* What this points to depends on the type */
    size_t size;		/* number of elts in the table */
    size_t limit;		/* Meaning depends on type */
    unsigned hmask;		/* bitwise and with hash value to get table index */
    short type;			/* values in AY_NULL .. AY_SPLIT */
}
#endif
 *ARRAY;

#define AY_NULL         0
#define AY_INT          1
#define AY_STR          2
#define AY_SPLIT        4

#define NO_CREATE  0
#define CREATE     1
#define new_ARRAY()  ((ARRAY)memset(ZMALLOC(struct _array),0,sizeof(struct _array)))
CELL *array_find(ARRAY, CELL *, int);
void array_delete(ARRAY, CELL *);
void array_load(ARRAY, size_t);
void array_clear(ARRAY);
STRING **array_loop_vector(ARRAY, size_t *);
CELL *array_cat(CELL *, int);

#endif /* ARRAY_H */
