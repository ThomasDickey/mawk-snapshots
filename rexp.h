/********************************************
rexp.h
copyright 2008-2014,2016, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991,2014, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp.h,v 1.28 2016/09/30 19:02:09 tom Exp $
 */

#ifndef  REXP_H
#define  REXP_H

#include "nstd.h"
#include "types.h"
#include <stdio.h>
#include  <setjmp.h>

extern PTR RE_malloc(size_t);
extern PTR RE_realloc(void *, size_t);

#ifdef NO_LEAKS
extern void RE_free(void *);
#else
#define RE_free(p) free(p)
#endif

/*  finite machine  state types  */

#define  M_STR     	0	/* matching a literal string */
#define  M_CLASS   	1	/* character class */
#define  M_ANY     	2	/* arbitrary character (.) */
#define  M_START   	3	/* start of string (^) */
#define  M_END     	4	/* end of string ($) */
#define  M_U       	5	/* arbitrary string (.*) */
#define  M_1J      	6	/* mandatory jump */
#define  M_2JA     	7	/* optional (undesirable) jump */
#define  M_2JB     	8	/* optional (desirable) jump */
#define  M_SAVE_POS	9	/* push position onto stack */
#define  M_2JC     	10	/* pop pos'n, optional jump if advanced */
#define  M_ACCEPT  	11	/* end of match */
#define  U_ON      	12

#define  U_OFF     0
#define  END_OFF   0
#define  END_ON    (2*U_ON)

typedef UChar BV[32];		/* bit vector */

typedef struct {
    SType s_type;
    SLen s_len;			/* used for M_STR  */
    union {
	char *str;		/* string */
	BV *bvp;		/*  class  */
	int jump;
    } s_data;
} STATE;

#define  STATESZ  (sizeof(STATE))

typedef struct {
    STATE *start, *stop;
} MACHINE;

/*  tokens   */
#define  T_NONE   0		/* no token */
#define  T_OR     1		/* | */
#define  T_CAT    2		/* binary operator */
#define  T_STAR   3		/* * */
#define  T_PLUS   4		/* + */
#define  T_Q      5		/* ? */
#define  T_LP     6		/* ( */
#define  T_RP     7		/* ) */
#define  T_START  8		/* ^ */
#define  T_END    9		/* $ */
#define  T_ANY   10		/* . */
#define  T_CLASS 11		/* starts with [ */
#define  T_SLASH 12		/*  \  */
#define  T_CHAR  13		/* all the rest */
#define  T_STR   14		/* string built of other tokens */
#define  T_U     15

/*  precedences and error codes  */
#define  L   0
#define  EQ  1
#define  G   2
#define  E1  (-1)
#define  E2  (-2)
#define  E3  (-3)
#define  E4  (-4)
#define  E5  (-5)
#define  E6  (-6)
#define  E7  (-7)

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
extern void RE_close(MACHINE *);
extern void RE_poscl(MACHINE *);
extern void RE_01(MACHINE *);
extern void RE_panic(const char *) GCC_NORETURN;

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

#ifdef LOCAL_REGEXP
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
    RT_POS_ENTRY *prev = *head - (*head)->prev_offset;

    if (prev->owner == current - RE_run_stack_base) {	/* likely */
	/* no need to preserve intervening nodes */
	*head = prev;
    } else if (*head == prev) {
	RE_panic("unbalanced M_SAVE_POS and M_2JC");
    } else {
	(*head)->prev_offset += prev->prev_offset;
    }

    return prev->pos;
}
#endif /* LOCAL_REGEXP */

#endif /* REXP_H  */
