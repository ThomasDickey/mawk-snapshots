/********************************************
rexp.h
copyright 2008-2024,2025, Thomas E. Dickey
copyright 2010, Jonathan Nieder
copyright 1991,2014, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexp.h,v 1.48 2025/01/20 20:37:13 tom Exp $
 */

#ifndef  REXP_H
#define  REXP_H

#include <nstd.h>
#include <types.h>

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
    ,M_2JC			/* pop position, optional jump if advanced */
#ifndef NO_INTERVAL_EXPR
    ,M_ENTER			/* begin counted loop (reset counter) */
    ,M_LOOP			/* end counted loop (update/test counter) */
#endif
    ,M_ACCEPT			/* end of match */
    ,U_ON			/* ...distinct from the preceding */
} MAWK_REGEX;

#define  U_OFF     0
#define  END_OFF   0
#define  END_ON    (2*U_ON)	/* ...distinct from the preceding */

#define  L_CURL         '{'
#define  R_CURL         '}'

typedef UChar BV[32];		/* bit vector */

typedef struct _state
#ifdef Visible_STATE
{
    SType s_type;
#ifndef NO_INTERVAL_EXPR
    int s_enter;		/* M_LOOP offset to M_ENTER, runtime check */
#endif
    size_t s_len;		/* used for M_STR  */
    union {
	char *str;		/* string */
	BV *bvp;		/*  class  */
	int jump;
    } s_data;
#ifndef NO_INTERVAL_EXPR
    Int it_min;			/* used for s_type == M_LOOP */
    Int it_max;			/* used for s_type == M_LOOP */
    Int it_cnt;			/* M_ENTER level, M_LOOP working counter */
#endif
}
#endif
STATE;

#define  STATESZ  (sizeof(STATE))

typedef struct _machine
#ifdef Visible_MACHINE
{
    STATE *start, *stop;
}
#endif
MACHINE;

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
typedef struct _rt_state
#ifdef Visible_RT_STATE
{
    STATE *m;			/* save the machine ptr */
    int u;			/* save the u_flag */
    char *s;			/* save the active string ptr */
    int pos_index;		/* index into position stack */
    int top_index;		/* offset to top entry of position stack */
    char *ss;			/* save the match start -- only used by REmatch */
}
#endif
RT_STATE;			/* run time state */

/* entry for the position stack */
typedef struct _rt_pos_entry
#ifdef Visible_RT_POS_ENTRY
{
    /* if we have not advanced beyond this character,
     * do not bother trying another round.
     */
    const char *pos;

    /* run time stack frame responsible for removing this node */
    int owner;
    /* previous node is this - this->prev_offset.  See pos_pop() */
    int prev_offset;
}
#endif
RT_POS_ENTRY;

/*  error  trap   */
extern int REerrno;
void GCC_NORETURN RE_error_trap(int);

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

#if OPT_TRACE
#define if_TRACE(stmt) stmt
#else
#define if_TRACE(stmt)		/*nothing */
#endif

#define pos_push(pos_param, run_param, position) do { \
    pos_param->pos = s; \
    pos_param->owner = (int) (run_param - RE_run_stack_base); \
 \
    TRACE2(("[%s@%d] pos_push #%ld: \"%s\" owner %d\n", \
	    __FILE__, __LINE__, \
	    (pos_param - RE_pos_stack_base), \
	    NonNull(position), \
	    pos_param->owner)); \
 \
    if (++pos_param == RE_pos_stack_limit) { \
	pos_param = RE_new_pos_stack(); \
    } \
    if_TRACE(pos_param->pos = NULL); \
    if_TRACE(pos_param->owner = 0); \
    pos_param->prev_offset = 1; \
} while (0)

#define pos_pop(pos_param, run_param, popped_position) do { \
    RT_POS_ENTRY *prev2 = pos_param - pos_param->prev_offset; \
 \
    if (prev2->owner == run_param - RE_run_stack_base) { /* likely */ \
	/* no need to preserve intervening nodes */ \
	TRACE2(("[%s@%d] pos_pop #%ld -> #%ld \"%s\" owner %d\n", \
	        __FILE__, __LINE__, \
	        (pos_param - RE_pos_stack_base), \
	        (prev2 - RE_pos_stack_base), \
	        NonNull(prev2->pos), \
	        prev2->owner)); \
	pos_param = prev2; \
    } else if (pos_param == prev2) { \
	RE_panic("unbalanced M_SAVE_POS and M_2JC"); \
    } else { \
	TRACE2(("[%s@%d] pos_pop #%ld: \"%s\" offset %d -> %d\n", \
	        __FILE__, __LINE__, \
	        (pos_param - RE_pos_stack_base), \
	        NonNull(pos_param->pos), \
	        pos_param->prev_offset, \
	        pos_param->prev_offset + prev2->prev_offset)); \
	pos_param->prev_offset += prev2->prev_offset; \
    } \
    popped_position = prev2->pos; \
} while (0)

#if defined(LOCAL_REGEXP) && defined(REGEXP_INTERNALS)

#ifdef NO_LEAKS
extern void RE_copy_states(STATE *, const STATE *, size_t);
#else
#define RE_copy_states(d, s, n) memcpy((d), (s), (n) * STATESZ)
#endif

#endif /* LOCAL_REGEXP */

#endif /* REXP_H  */
