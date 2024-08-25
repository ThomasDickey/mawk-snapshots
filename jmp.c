/********************************************
jmp.c
copyright 2009-2021,2024, Thomas E. Dickey
copyright 1991-1993,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: jmp.c,v 1.9 2024/08/25 17:03:50 tom Exp $
 */

/* this module deals with back patching jumps, breaks and continues,
   and with save and restoring code when we move code.
   There are three stacks.  If we encounter a compile error, the
   stacks are frozen, i.e., we do not attempt error recovery
   on the stacks
*/

#define Visible_CELL
#define Visible_CODEBLOCK

#include <mawk.h>
#include <symtype.h>
#include <jmp.h>
#include <code.h>
#include <sizes.h>
#include <init.h>
#include <memory.h>

#define error_state  (compile_error_count>0)

/*---------- back patching jumps  ---------------*/

typedef struct jmp {
    struct jmp *link;
    int source_offset;
} JMP;

static JMP *jmp_top;

void
code_jmp(int jtype, const INST * target)
{
    if (error_state)
	return;

    /* WARNING: Don't emit any code before using target or
       relocation might make it invalid */

    if (target)
	code2op(jtype, (int) (target - (code_ptr + 1)));
    else {
	register JMP *p = ZMALLOC(JMP);

	/* stack for back patch */
	code2op(jtype, 0);
	p->source_offset = code_offset - 1;
	p->link = jmp_top;
	jmp_top = p;
    }
}

/* patch a jump on the jmp_stack */
void
patch_jmp(const INST * target)
{
    if (!error_state) {
	register JMP *p;
	register INST *source;	/* jmp starts here */

#ifdef	DEBUG
	if (!jmp_top)
	    bozo("jmp stack underflow");
#endif

	p = jmp_top;
	jmp_top = p->link;
	source = p->source_offset + code_base;
	source->op = (int) (target - source);

	ZFREE(p);
    }
}

/*-- break and continue -------*/

typedef struct bc {
    struct bc *link;		/* stack as linked list */
    int type;			/* 'B' or 'C' or mark start with 0 */
    int source_offset;		/* position of _JMP  */
} BC;

static BC *bc_top;

void
BC_new(void)			/* mark the start of a loop */
{
    BC_insert(0, (INST *) 0);
}

void
BC_insert(int type, const INST * address)
{
    register BC *p;

    if (error_state)
	return;

    if (type && !bc_top) {
	compile_error("%s statement outside of loop",
		      type == 'B' ? "break" : "continue");

	return;
    } else {
	p = ZMALLOC(BC);
	p->type = type;
	p->source_offset = (int) (address - code_base);
	p->link = bc_top;
	bc_top = p;
    }
}

/* patch all break and continues for one loop */
void
BC_clear(INST * B_address, INST * C_address)
{
    register BC *p;

    if (error_state)
	return;

    p = bc_top;
    /* pop down to the mark node */
    while (p->type) {
	INST *source;
	register BC *q;

	source = code_base + p->source_offset;
	source->op = (int) ((p->type == 'B' ? B_address : C_address)
			    - source);

	q = p;
	p = p->link;
	ZFREE(q);
    }
    /* remove the mark node */
    bc_top = p->link;
    ZFREE(p);
}

/*-----	 moving code --------------------------*/

/* a stack to hold some pieces of code while
   reorganizing loops .
*/

typedef struct mc {		/* mc -- move code */
    struct mc *link;
    INST *code;			/* the save code */
    unsigned len;		/* its length */
    int scope;			/* its scope */
    int move_level;		/* size of this stack when coded */
    FBLOCK *fbp;		/* if scope FUNCT */
    int offset;			/* distance from its code base */
} MC;

static MC *mc_top;
int code_move_level = 0;	/* see comment in jmp.h */

#define	 NO_SCOPE	-1
 /* means relocation of resolve list not needed */

void
code_push(INST * code, unsigned len, int scope, FBLOCK * fbp)
{
    register MC *p;

    if (!error_state) {
	p = ZMALLOC(MC);
	p->len = len;
	p->link = mc_top;
	mc_top = p;

	if (len) {
	    p->code = (INST *) zmalloc(sizeof(INST) * len);
	    memcpy(p->code, code, sizeof(INST) * len);
	}
	if (!resolve_list)
	    p->scope = NO_SCOPE;
	else {
	    p->scope = scope;
	    p->move_level = code_move_level;
	    p->fbp = fbp;
	    if (code)
		p->offset = (int) (code - code_base);
	    else
		p->offset = 0;
	}
    }
    code_move_level++;
}

/* copy the code at the top of the mc stack to target.
   return the number of INSTs moved */

unsigned
code_pop(INST * target)
{
    register MC *p;
    unsigned len;
    int target_offset;

    if (error_state)
	return 0;

#ifdef	DEBUG
    if (!mc_top)
	bozo("mc underflow");
#endif

    p = mc_top;
    mc_top = p->link;
    len = p->len;

    while (target + len >= code_warn) {
	target_offset = (int) (target - code_base);
	code_grow();
	target = code_base + target_offset;
    }

    if (len) {
	memcpy(target, p->code, len * sizeof(INST));
	zfree(p->code, len * sizeof(INST));
    }

    if (p->scope != NO_SCOPE) {
	target_offset = (int) (target - code_base);
	relocate_resolve_list(p->scope, p->move_level, p->fbp,
			      p->offset, len, target_offset - p->offset);
    }

    ZFREE(p);
    code_move_level--;
    return len;
}
