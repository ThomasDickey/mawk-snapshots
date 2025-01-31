/********************************************
rexpdb.c
copyright 2008-2024,2025, Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexpdb.c,v 1.33 2025/01/20 20:16:06 tom Exp $
 */

#include <rexp.h>

#include <ctype.h>

/*  print a machine for debugging  */

static const char xlat[][12] =
{
    "M_STR",
    "M_CLASS",
    "M_ANY",
    "M_START",
    "M_END",
    "M_U",
    "M_1J",
    "M_2JA",
    "M_2JB",
    "M_SAVE_POS",
    "M_2JC",
#ifndef NO_INTERVAL_EXPR
    "M_ENTER",
    "M_LOOP",
#endif
    "M_ACCEPT"
};

static const char *
REs_type(STATE * p)
{
    return (xlat[((UChar) (p->s_type)) % U_ON]);
}

void
REmprint(STATE * m, FILE *f)
{
    STATE *base = m;
    STATE *p = base;
    const char *end_on_string;

    while (1) {
	int line = (int) (p - base);
	fprintf(f, "%03d ", line);
	fprintf(f, ".\t");
	if (p->s_type >= END_ON) {
	    p->s_type = (SType) (p->s_type - END_ON);
	    end_on_string = "$";
	} else
	    end_on_string = "";

	if (p->s_type < 0 || p->s_type >= END_ON) {
	    fprintf(f, "unknown STATE type %d\n", (int) (p->s_type));
	    return;
	}

	fprintf(f, "%s", REs_type(p));
	switch (p->s_type) {
	case M_STR:
	    fprintf(f, "\t");
	    da_string2(f, p->s_data.str, (size_t) p->s_len, '"');
	    break;

	case M_1J:
	case M_2JA:
	case M_2JB:
	case M_2JC:
	    fprintf(f, "\t%03d", line + p->s_data.jump);
	    break;
#ifndef NO_INTERVAL_EXPR
	case M_ENTER:
	    fprintf(f, "\t%03d\t# level %d",
		    line + p->s_data.jump,
		    (int) p->it_cnt);
	    break;
	case M_LOOP:
	    fprintf(f, "\t%03d", line + p->s_data.jump);
	    fprintf(f, " %c", L_CURL);
	    if (p->it_min != 0)
		fprintf(f, INT_FMT, p->it_min);
	    if (p->it_max != p->it_min) {
		fprintf(f, ",");
		if (p->it_max != MAX__INT)
		    fprintf(f, INT_FMT, p->it_max);
	    }
	    fprintf(f, "%c", R_CURL);
	    fprintf(f, "\t# level %d", (int) (p + p->s_enter)->it_cnt);
	    break;
#endif
	case M_CLASS:
	    {
		UChar *q = (UChar *) p->s_data.bvp;
		UChar *r = q + sizeof(BV);
		unsigned bitnum = 0;
		fprintf(f, "\t[");
		while (q < r) {
		    unsigned bits = *q++;
		    if (bits != 0) {
			unsigned b;
			for (b = 0; b < 8; ++b) {
			    if (bits & (unsigned) (1 << b)) {
				int ch = (int) (bitnum + b);
				if (ch < 32 || ch >= 128) {
				    fprintf(f, "\\%03o", ch);
				} else {
				    if (strchr("\\[]", ch))
					fprintf(f, "\\");
				    fprintf(f, "%c", ch);
				}
			    }
			}
		    }
		    bitnum += 8;
		}
		fprintf(f, "]");
	    }
	    break;
	}
	if (end_on_string[0])
	    fprintf(f, "\t%s", end_on_string);
	fprintf(f, "\n");
	if (end_on_string[0])
	    p->s_type = (SType) (p->s_type + END_ON);
	if (p->s_type == M_ACCEPT)
	    return;
	p++;
    }
}
