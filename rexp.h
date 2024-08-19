/********************************************
rexp.h
copyright 2008-2023,2024, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991,2014, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp.h,v 1.42 2024/07/26 00:31:50 tom Exp $
 */

#ifndef  REXP_H
#define  REXP_H

#include "nstd.h"
#include "types.h"
#include <stdio.h>
#include <setjmp.h>

extern PTR RE_malloc(size_t);
extern PTR RE_realloc(void *, size_t);

#ifdef NO_LEAKS
extern void RE_free(void *);
#else
#define RE_free(p) free(p)
#endif

/*  finite machine  state types  */

typedef enum {
    M_STR			/* matching a literal string */
    ,M_CLASS			/* character class */
    ,M_ANY			/* arbitrary character (.) */
    ,M_START			/* start of string (^) */
    ,M_END			/* end of string ($) */
    ,M_U			/* arbitrary string (.*) */
    ,M_1J			/* mandatory jump */
    ,M_2JA			/* optional (undesirable) jump */
    ,M_2JB			/* optional (desirable) jump */
    ,M_SAVE_POS			/* push position onto stack */
    ,M_2JC			/* pop pos'n, optional jump if advanced */
    ,M_ACCEPT			/* end of match */
    ,U_ON			/* ...distinct from the preceding */
} MAWK_REGEX;

#define  U_OFF     0
#define  END_OFF   0
#define  END_ON    (2*U_ON)	/* ...distinct from the preceding */

#define  L_CURL         '{'
#define  R_CURL         '}'

typedef UChar BV[32];		/* bit vector */

typedef struct {
    SType s_type;
    size_t s_len;		/* used for M_STR  */
    union {
	char *str;		/* string */
	BV *bvp;		/*  class  */
	int jump;
    } s_data;
#ifndef NO_INTERVAL_EXPR
    Int it_min;			/* used for s_type == M_2JC */
    Int it_max;			/* used for s_type == M_2JC */
    Int it_cnt;
#endif
} STATE;

#define  STATESZ  (sizeof(STATE))

typedef struct {
    STATE *start, *stop;
} MACHINE;

/*  tokens   */
typedef enum {
    T_NONE = 0			/* no token */
    ,T_OR			/* | */
    ,T_CAT			/* binary operator */
    ,T_STAR			/* * */
    ,T_PLUS			/* + */
    ,T_Q			/* ? */
    ,T_LP			/* ( */
    ,T_RP			/* ) */
#ifdef NO_INTERVAL_EXPR
#define T_LB	T_CHAR		/* { */
#define T_RB	T_CHAR		/* } */
#else
    ,T_LB			/* { */
    ,T_RB			/* } */
#endif
    ,T_START			/* ^ */
    ,T_END			/* $ */
    ,T_ANY			/* . */
    ,T_CLASS			/* starts with [ */
    ,T_SLASH			/*  \  */
    ,T_CHAR			/* all the rest */
    ,T_STR			/* string built of other tokens */
    ,T_U
} MAWK_TOKEN;

/*  precedences and error codes  */
#define  OP_L   0
#define  OP_EQ  1
#define  OP_G   2
#define  ERR_1  (-1)
#define  ERR_2  (-2)
#define  ERR_3  (-3)
#define  ERR_4  (-4)
#define  ERR_5  (-5)
#define  ERR_6  (-6)
#define  ERR_7  (-7)

#define  MEMORY_FAILURE      5

#define  ison(b,x)  ((b)[((UChar)(x)) >> 3] & (1 << ((x) & 7)))

/* struct for the run time stack */
typedef struct {
    STATE *m;			/* save the machine ptr */
    int u;			/* save the u_flag */
    char *s;			/* save the active string ptr */
    int sp;			/* size of position stack */
    int tp;			/* offset to top entry of position stack */
    char *ss;			/* save the match start -- only used by REmatch */
} RT_STATE;			/* run time state */

/* entry for the position stack */
typedef struct {
    /* if we have not advanced beyond this character,
     * do not bother trying another round.
     */
    const char *pos;

    /* run time stack frame responsible for removing this node */
    int owner;
    /* previous node is this - this->prev_offset.  See RE_pos_pop() */
    int prev_offset;
} RT_POS_ENTRY;

/*  error  trap   */
extern int REerrno;
void RE_error_trap(int);

#ifndef GCC_NORETURN
#define GCC_NORETURN		/* nothing */
#endif

extern MACHINE RE_u(void);
extern MACHINE RE_start(void);
extern MACHINE RE_end(void);
extern MACHINE RE_any(void);
extern MACHINE RE_str(char *, size_t);
extern MACHINE RE_class(BV *);
extern void RE_cat(MACHINE *, MACHINE *);
extern void RE_or(MACHINE *, MACHINE *);
extern STATE *RE_close(MACHINE *);
extern STATE *RE_poscl(MACHINE *);
extern void RE_01(MACHINE *);
extern GCC_NORETURN void RE_panic(const char *, ...) GCC_PRINTFLIKE(1,2);

#ifndef NO_INTERVAL_EXPR
extern void RE_close_limit(MACHINE *, Int, Int);
extern void RE_poscl_limit(MACHINE *, Int, Int);
extern void duplicate_m(MACHINE *, MACHINE *);
#endif

#ifndef MAWK_H
extern char *str_str(char *, size_t, char *, size_t);
#endif

extern void RE_lex_init(char *, size_t);
extern int RE_lex(MACHINE *);
extern void RE_run_stack_init(void);
extern void RE_pos_stack_init(void);
extern RT_STATE *RE_new_run_stack(void);
extern RT_POS_ENTRY *RE_new_pos_stack(void);

extern RT_STATE *RE_run_stack_base;
extern RT_STATE *RE_run_stack_limit;
extern RT_STATE *RE_run_stack_empty;

extern RT_POS_ENTRY *RE_pos_stack_base;
extern RT_POS_ENTRY *RE_pos_stack_limit;
extern RT_POS_ENTRY *RE_pos_stack_empty;

extern Int intrvalmin;
extern Int intrvalmax;
extern char *re_exp;

#if defined(LOCAL_REGEXP) && defined(REGEXP_INTERNALS)
static /* inline */ RT_POS_ENTRY *
RE_pos_push(RT_POS_ENTRY * head, const RT_STATE * owner, const char *s)
{
    head->pos = s;
    head->owner = (int) (owner - RE_run_stack_base);

    if (++head == RE_pos_stack_limit) {
	head = RE_new_pos_stack();
    }
    head->prev_offset = 1;
    return head;
}

static /* inline */ const char *
RE_pos_pop(RT_POS_ENTRY ** head, const RT_STATE * current)
{
    RT_POS_ENTRY *prev2 = *head - (*head)->prev_offset;

    if (prev2->owner == current - RE_run_stack_base) {	/* likely */
	/* no need to preserve intervening nodes */
	*head = prev2;
    } else if (*head == prev2) {
	RE_panic("unbalanced M_SAVE_POS and M_2JC");
    } else {
	(*head)->prev_offset += prev2->prev_offset;
    }

    return prev2->pos;
}

#ifndef NO_INTERVAL_EXPR
/* reset it_cnt to zero for the M_2JC state
 *  which is where loop count is checked
 */
static void
RE_init_it_cnt(STATE * s)
{
    STATE *p = s;
    while (p->s_type < M_ACCEPT) {
	if (p->s_type == M_2JC)
	    p->it_cnt = 0;
	p++;
    }
}
#else
#define RE_init_it_cnt(s)	/* nothing */
#endif

#ifndef NO_INTERVAL_EXPR
#undef NO_RI_LOOP_UNROLL	/* experimental 2020/10/22 -TD */
#ifdef NO_RI_LOOP_UNROLL
#else
static void
RE_set_limit(STATE * s, Int minlimit, Int maxlimit)
{
    STATE *p = s;
    while (p->s_type < M_ACCEPT) {
	if (p->s_type == M_2JC) {
	    p->it_min = minlimit;
	    p->it_max = maxlimit;
	}
	p++;
    }
}
#endif /* ! NO_RI_LOOP_UNROLL */
#endif /* ! NO_INTERVAL_EXPR */

#ifdef NO_LEAKS
extern void RE_copy_states(STATE *, const STATE *, size_t);
#else
#define RE_copy_states(d, s, n) memcpy((d), (s), (n) * STATESZ)
#endif

#endif /* LOCAL_REGEXP */

#endif /* REXP_H  */
