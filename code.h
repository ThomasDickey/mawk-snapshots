/********************************************
code.h
copyright 2009-2019,2023, Thomas E. Dickey
copyright 1991-1994,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: code.h,v 1.14 2023/08/16 23:34:11 tom Exp $
 */

/*  code.h  */

#ifndef  MAWK_CODE_H
#define  MAWK_CODE_H

#include <memory.h>

#define  PAGESZ	512
	/* number of code instructions allocated at one time */
#define  CODEWARN        16

/* coding scope */
#define   SCOPE_MAIN    0
#define   SCOPE_BEGIN   1
#define   SCOPE_END     2
#define   SCOPE_FUNCT   3

typedef struct {
    INST *base, *limit, *warn, *ptr;
} CODEBLOCK;

extern CODEBLOCK active_code;
extern CODEBLOCK *main_code_p, *begin_code_p, *end_code_p;

extern INST *main_start, *begin_start, *end_start;
extern size_t main_size, begin_size;
extern INST *execution_start;
extern INST *next_label;	/* next statements jump to here */
extern int dump_code_flag;

#define CodeOffset(base) (int)(code_ptr - (base))

#define code_ptr  active_code.ptr
#define code_base active_code.base
#define code_warn active_code.warn
#define code_limit active_code.limit
#define code_offset CodeOffset(code_base)

#define INST_BYTES(x) (sizeof(INST)*(unsigned)(x))

extern CELL eval_stack[];
extern int exit_code;

#define  code1(x)  code_ptr++ -> op = (x)
/* shutup picky compilers */
#define  code2(x,p)  xcode2(x,(PTR)(p))

void xcode2(int, PTR);
void code2op(int, int);
INST *code_shrink(CODEBLOCK *, size_t *);
void code_grow(void);
void set_code(void);
void be_setup(int);
void dump_code(void);

/*  the machine opcodes  */
/* to avoid confusion with a ptr FE_PUSHA must have op code 0 */

typedef enum {
    FE_PUSHA = 0
    ,FE_PUSHI
    ,F_PUSHA
    ,F_PUSHI
    ,NF_PUSHI
    ,_HALT
    ,_STOP
    ,_PUSHC
    ,_PUSHD
    ,_PUSHS
    ,_PUSHINT
    ,_PUSHA
    ,_PUSHI
    ,L_PUSHA
    ,L_PUSHI
    ,AE_PUSHA
    ,AE_PUSHI
    ,A_PUSHA
    ,LAE_PUSHA
    ,LAE_PUSHI
    ,LA_PUSHA
    ,_POP
    ,_ADD
    ,_SUB
    ,_MUL
    ,_DIV
    ,_MOD
    ,_POW
    ,_NOT
    ,_TEST
    ,A_TEST
    ,_LENGTH
    ,A_LENGTH
    ,A_DEL
    ,ALOOP
    ,A_CAT
    ,_UMINUS
    ,_UPLUS
    ,_ASSIGN
    ,_ADD_ASG
    ,_SUB_ASG
    ,_MUL_ASG
    ,_DIV_ASG
    ,_MOD_ASG
    ,_POW_ASG
    ,F_ASSIGN
    ,F_ADD_ASG
    ,F_SUB_ASG
    ,F_MUL_ASG
    ,F_DIV_ASG
    ,F_MOD_ASG
    ,F_POW_ASG
    ,_CAT
    ,_BUILTIN
    ,_PRINT
    ,_POST_INC
    ,_POST_DEC
    ,_PRE_INC
    ,_PRE_DEC
    ,F_POST_INC
    ,F_POST_DEC
    ,F_PRE_INC
    ,F_PRE_DEC
    ,_JMP
    ,_JNZ
    ,_JZ
    ,_LJZ
    ,_LJNZ
    ,_EQ
    ,_NEQ
    ,_LT
    ,_LTE
    ,_GT
    ,_GTE
    ,_MATCH0
    ,_MATCH1
    ,_MATCH2
    ,_EXIT
    ,_EXIT0
    ,_NEXT
    ,_NEXTFILE
    ,_RANGE
    ,_CALL
    ,_CALLX
    ,_RET
    ,_RET0
    ,SET_ALOOP
    ,POP_AL
    ,OL_GL
    ,OL_GL_NR
    ,_OMAIN
    ,_JMAIN
    ,DEL_A
} MAWK_OPCODES;

#endif /* MAWK_CODE_H */
