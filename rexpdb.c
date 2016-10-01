/********************************************
rexpdb.c
copyright 2008-2014,2016, Thomas E. Dickey
copyright 1991,1993, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: rexpdb.c,v 1.16 2016/09/30 21:27:07 tom Exp $
 */

#include "rexp.h"
#include <ctype.h>

/*  print a machine for debugging  */

static const char *xlat[] =
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
    "M_ACCEPT"
};

const char *
REs_type(PTR p)
{
    return (xlat[((UChar) (((STATE *) p)->s_type)) % U_ON]);
}

void
REmprint(PTR m, FILE *f)
{
    STATE *p = (STATE *) m;
    const char *end_on_string;

    while (1) {
	fprintf(f, "%03d ", (int) (p - (STATE *) m));
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

	fprintf(f, "%s", xlat[((UChar) (p->s_type)) % U_ON]);
	switch (p->s_type) {
	case M_STR:
	    fprintf(f, "\t");
	    da_string(f, p->s_data.str, (size_t) p->s_len);
	    break;

	case M_1J:
	case M_2JA:
	case M_2JB:
	case M_2JC:
	    fprintf(f, "\t%d", p->s_data.jump);
	    break;
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
	fprintf(f, "\t%s\n", end_on_string);
	if (end_on_string[0])
	    p->s_type = (SType) (p->s_type + END_ON);
	if (p->s_type == M_ACCEPT)
	    return;
	p++;
    }
}
