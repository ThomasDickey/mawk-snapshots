/********************************************
jmp.h
copyright 2009-2010,2023 Thomas E. Dickey
copyright 1991, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: jmp.h,v 1.6 2023/07/22 22:28:22 tom Exp $
 */

#ifndef   MAWK_JMP_H
#define   MAWK_JMP_H

#include "types.h"
#include "symtype.h"

void BC_new(void);
void BC_insert(int, INST *);
void BC_clear(INST *, INST *);
void code_push(INST *, unsigned, int, FBLOCK *);
unsigned code_pop(INST *);
void code_jmp(int, INST *);
void patch_jmp(INST *);

extern int code_move_level;
   /* used to as one part of unique identification of context when
      moving code.  Global for communication with parser.
    */

#endif /* MAWK_JMP_H  */
