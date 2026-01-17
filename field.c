/********************************************
field.c
copyright 2008-2024,2026 Thomas E. Dickey
copyright 1991-1995,2014 Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: field.c,v 1.52 2026/01/17 00:29:49 tom Exp $
 */

#define Visible_CELL
#define Visible_RE_DATA
#define Visible_SEPARATOR
#define Visible_STRING
#define Visible_SYMTAB

#include <mawk.h>
#include <split.h>
#include <field.h>
#include <init.h>
#include <memory.h>
#include <scan.h>
#include <bi_vars.h>
#include <regexp.h>

/* initial fields and pseudo fields,
    most programs only need these */
CELL field[FBANK_SZ + NUM_PFIELDS];
/* hold banks of more fields if needed */
CELL **fbankv;

/* fbankv grows in chunks */
#define FBANKV_CHUNK_SIZE    1024
static size_t fbankv_num_chunks;

/* make fbankv big enough to hold field CELL $i
   is called with i==0 during initialization

   This does not create field CELL $i, it just
   makes fbankv big enough to hold the fbank that will hold $i
*/
static void
allocate_fbankv(int i)
{
    if (i == 0) {
	size_t sz = FBANKV_CHUNK_SIZE * sizeof(CELL *);
	fbankv_num_chunks = 1;
	fbankv = (CELL **) zmalloc(sz);
	memset(fbankv, 0, sz);
	fbankv[0] = field;
    } else {
	size_t u = (size_t) i + 1;
	size_t chunks = (u / (FBANK_SZ * FBANKV_CHUNK_SIZE)) + 1;
	if (chunks > fbankv_num_chunks) {
	    size_t old_size = fbankv_num_chunks * FBANKV_CHUNK_SIZE;
	    size_t new_size = chunks * FBANKV_CHUNK_SIZE;
	    fbankv = zrealloc(fbankv,
			      SizeTimes(old_size, sizeof(CELL *)),
			      SizeTimes(new_size, sizeof(CELL *)));

	    memset(&fbankv[old_size], 0, (new_size - old_size) * sizeof(CELL *));
	    fbankv_num_chunks = chunks;
	}
    }
}

/* max_field created i.e. $max_field exists
   as new fields are created max_field grows
*/
static int max_field = FBANK_SZ - 1;

/*  The fields $0, $1, ... $max_field are always valid, the
    value of nf (below) does not affect validity of the
    allocated fields.  When a new $0 is read, nf is set to -1
    to indicate $0 has not been split, field[1], field[2] ...
    field[3] (actually fbankv[i/1024][i%1024]) are unchanged.

    So any time a field is assigned or changed, it has to
    be cell_destroyed first and this is the only way a field gets
    cell_destroyed.
*/

static void build_field0(void);

/* a description of how to split based on RS.
   If RS is changed, so is rs_shadow */
SEPARATOR rs_shadow =
{
    SEP_CHAR, '\n',
    {NULL}
};
/* a splitting CELL version of FS */
CELL fs_shadow =
{
    C_SPACE, 0, NULL, 0.0
};
int nf;
 /* nf holds the true value of NF.  If nf < 0 , then
    NF has not been computed, i.e., $0 has not been split
  */

static void
set_rs_shadow(void)
{
    CELL c;
    STRING *sval;
    char *s;
    size_t len;

    if (posix_space_flag && mawk_state == EXECUTION)
	scan_code['\n'] = SC_UNEXPECTED;

    if (rs_shadow.type == SEP_STR) {
	free_STRING(rs_shadow.u.s_ptr);
    }

    cast_for_split(cellcpy(&c, RS));
    switch (c.type) {
    case C_RE:
	if ((s = is_string_split(c.ptr, &len))) {
	    if (len == 1) {
		rs_shadow.type = SEP_CHAR;
		rs_shadow.c = s[0];
	    } else {
		rs_shadow.type = SEP_STR;
		rs_shadow.u.s_ptr = new_STRING(s);
	    }
	} else {
	    rs_shadow.type = SEP_RE;
	    rs_shadow.u.r_ptr = (RE_NODE *) c.ptr;
	}
	break;

    case C_SPACE:
	rs_shadow.type = SEP_CHAR;
	rs_shadow.c = ' ';
	break;

    case C_SNULL:		/* RS becomes one or more blank lines */
	if (mawk_state == EXECUTION)
	    scan_code['\n'] = SC_SPACE;
	rs_shadow.type = SEP_MLR;
	sval = new_STRING("\n\n+");
	rs_shadow.u.r_ptr = re_compile(sval);
	free_STRING(sval);
	break;

    case C_STRING:
	/*
	 * Check for special case where we retained the cell as a string,
	 * bypassing regular-expression compiling.
	 */
	if (string(&c)->len == 1) {
	    rs_shadow.type = SEP_CHAR;
	    rs_shadow.c = string(&c)->str[0];
	    break;
	}
	/* FALLTHRU */
    default:
	bozo("bad cell in set_rs_shadow");
    }
}

static void
load_pfield(const char *name, CELL *cp)
{
    SYMTAB *stp;

    stp = insert(name);
    stp->type = ST_FIELD;
    stp->stval.cp = cp;
}

/* initialize $0 and the pseudo fields */
void
field_init(void)
{
    allocate_fbankv(0);

    field[0].type = C_STRING;
    field[0].ptr = (PTR) & null_str;
    null_str.ref_cnt++;

    load_pfield("NF", NF);
    NF->type = C_DOUBLE;
    NF->dval = 0.0;

    load_pfield("RS", RS);
    RS->type = C_STRING;
    RS->ptr = (PTR) new_STRING("\n");
    /* rs_shadow already set */

    load_pfield("FS", FS);
    FS->type = C_STRING;
    FS->ptr = (PTR) new_STRING(" ");
    /* fs_shadow is already set */

    load_pfield("OFMT", OFMT);
    OFMT->type = C_STRING;
    OFMT->ptr = (PTR) new_STRING("%.6g");

    load_pfield("CONVFMT", CONVFMT);
    CONVFMT->type = C_STRING;
    CONVFMT->ptr = OFMT->ptr;
    string(OFMT)->ref_cnt++;
}

void
set_field0(const char *s, size_t len)
{
    cell_destroy(&field[0]);
    nf = -1;

    if (len) {
	field[0].type = C_MBSTRN;
	field[0].ptr = (PTR) new_STRING0(len);
	memcpy(string(&field[0])->str, s, len);
    } else {
	field[0].type = C_STRING;
	field[0].ptr = (PTR) & null_str;
	null_str.ref_cnt++;
    }
}

/* split field[0] into $1, $2 ... and set NF
 *
 * Note the current values are valid CELLS and
 * have to be destroyed when the new values are loaded.
*/

void
split_field0(void)
{
    CELL *cp0;
    size_t cnt = 0;
    CELL hold0;			/* copy field[0] here if not string */

    if (field[0].type < C_STRING) {
	cast1_to_s(cellcpy(&hold0, field + 0));
	cp0 = &hold0;
    } else {
	cp0 = &field[0];
    }

    if (string(cp0)->len > 0) {
	switch (fs_shadow.type) {
	case C_SNULL:		/* FS == "" */
	    cnt = null_split(string(cp0)->str, string(cp0)->len);
	    break;

	case C_SPACE:
	    cnt = space_split(string(cp0)->str, string(cp0)->len);
	    break;

	default:
	    cnt = re_split(string(cp0)->str, string(cp0)->len, fs_shadow.ptr);
	    break;
	}

    }
    /* the above xxx_split() function put the fields in an anonyous
     * buffer that will be pulled into the fields with a transfer call */

    /* we are done with cp0 */
    if (cp0 == &hold0)
	free_STRING(string(cp0));

    nf = (int) cnt;

    cell_destroy(NF);
    NF->type = C_DOUBLE;
    NF->dval = (double) nf;

    if (nf > max_field)
	slow_field_ptr(nf);
    /* fields 1 .. nf are created and valid */

    /* retrieves the result of xxx_split() */
    if (cnt > 0) {
	transfer_to_fields(cnt);
    }
}

static GCC_NORETURN void
invalid_format(CELL *fp)
{
    const char *what = (fp == CONVFMT) ? "CONVFMT" : "OFMT";
    const char *format = string(fp)->str;
    rt_error("illegal format assigned to %s: %s", what, format);
}

/*
 * We expect only one field, using the same format choices as in do_printf().
 */
static int
valid_format(CELL *fp)
{
    int result = 1;
    char *format = string(fp)->str;
    char *q = format;
    int args = 0;

    while (*q != '\0') {
	if (*q++ == '%') {
	    int l_flag = 0;
	    int h_flag = 0;

	    if (*q == '%') {
		continue;	/* allow an escaped '%' */
	    } else if (++args > 1)
		invalid_format(fp);

	    while (*q == '-' || *q == '+' || *q == ' ' ||
		   *q == '#' || *q == '0' || *q == '\'') {
		q++;
	    }
	    if (*q == '*') {
		invalid_format(fp);
	    } else {
		while (scan_code[*(unsigned char *) q] == SC_DIGIT) {
		    q++;
		}
	    }
	    if (*q == '.') {	/* have precision */
		q++;
		if (*q == '*') {
		    invalid_format(fp);
		} else {
		    while (scan_code[*(unsigned char *) q] == SC_DIGIT) {
			q++;
		    }
		}
	    }
	    if (*q == 'l') {
		q++;
	    } else if (*q == 'h') {
		q++;
	    }
	    switch (*q++) {
	    case 's':
		if (l_flag + h_flag)
		    invalid_format(fp);
		break;
	    case 'c':
		if (l_flag + h_flag)
		    invalid_format(fp);
		break;
	    case 'd':
	    case 'i':
		break;
	    case 'o':
	    case 'x':
	    case 'X':
	    case 'u':
		break;
	    case 'e':
	    case 'g':
	    case 'f':
	    case 'E':
	    case 'G':
		if (h_flag + l_flag)
		    invalid_format(fp);
		break;
	    default:
		invalid_format(fp);
	    }
	}
    }
    return result;
}

/*
  assign CELL *cp to field or pseudo field
  and take care of all side effects
*/

void
field_assign(CELL *fp, CELL *cp)
{
    CELL c;
    int i, j;

    /* the most common case first */
    if (fp == field) {
	cell_destroy(field);
	cellcpy(fp, cp);
	nf = -1;
	return;
    }

    /* its not important to do any of this fast */

    if (nf < 0)
	split_field0();

    switch (i = (int) (fp - field)) {

    case NF_field:

	cell_destroy(NF);
	cellcpy(NF, cellcpy(&c, cp));
	if (c.type != C_DOUBLE)
	    cast1_to_d(&c);

	if ((j = d_to_i(c.dval)) < 0)
	    rt_error("negative value assigned to NF");

	if (j > nf)
	    for (i = nf + 1; i <= j; i++) {
		cp = field_ptr(i);
		cell_destroy(cp);
		cp->type = C_STRING;
		cp->ptr = (PTR) & null_str;
		null_str.ref_cnt++;
	    }

	nf = j;
	build_field0();
	break;

    case RS_field:
	cell_destroy(RS);
	cellcpy(RS, cp);
	set_rs_shadow();
	break;

    case FS_field:
	cell_destroy(FS);
	cast_for_split(cellcpy(&fs_shadow, cellcpy(FS, cp)));
	break;

    case OFMT_field:
	OFMT_type = -1;
	/* FALLTHRU */
    case CONVFMT_field:
	/* If the user does something stupid with OFMT or CONVFMT,
	   we could crash.
	   We'll make an attempt to protect ourselves here.  This is
	   why OFMT and CONVFMT are pseudo fields.

	   The ptrs of OFMT and CONVFMT always have a valid STRING,
	   even if assigned a DOUBLE or NOINIT
	 */

	free_STRING(string(fp));
	cellcpy(fp, cp);
	if (fp->type < C_STRING) {	/* !! */
	    fp->ptr = (PTR) new_STRING("%.6g");
	} else if (valid_format(fp)) {
	    if (fp == CONVFMT) {
		/* It's a string, but if it's really goofy and CONVFMT,
		   it could still damage us. Test it .
		 */
		char xbuff[512];

		xbuff[256] = 0;
		sprintf(xbuff, string(fp)->str, 3.1459);
		if (xbuff[256])
		    rt_error("CONVFMT assigned unusable value");
	    }
	}
	break;

#ifdef MSDOS
      lm_dos_label:
#endif

    default:			/* $1 or $2 or ... */

	cell_destroy(fp);
	cellcpy(fp, cp);

	if (i < 0 || i >= FBANK_SZ) {
	    /* field assigned to was not in field[0..FBANK_SZ-1]
	     * or a pseudo field, so compute actual field index
	     */
	    i = field_addr_to_index(fp);
	}

	if (i > nf) {
	    for (j = nf + 1; j < i; j++) {
		cp = field_ptr(j);
		cell_destroy(cp);
		cp->type = C_STRING;
		cp->ptr = (PTR) & null_str;
		null_str.ref_cnt++;
	    }
	    nf = i;
	    cell_destroy(NF);
	    NF->type = C_DOUBLE;
	    NF->dval = (double) i;
	}

	build_field0();

    }
}

/* construct field[0] from the other fields */

static void
build_field0(void)
{

#ifdef DEBUG
    if (nf < 0)
	bozo("nf <0 in build_field0");
#endif

    cell_destroy(field + 0);

    if (nf == 0) {
	field[0].type = C_STRING;
	field[0].ptr = (PTR) & null_str;
	null_str.ref_cnt++;
    } else if (nf == 1) {
	cellcpy(field, field + 1);
    } else {
	CELL c;
	STRING *ofs, *tail;
	size_t len;
	register CELL *cp;
	int cnt;
	register char *p;
	CELL **fbp, *cp_limit;

	cast1_to_s(cellcpy(&c, OFS));
	ofs = (STRING *) c.ptr;
	cast1_to_s(cellcpy(&c, field_ptr(nf)));
	tail = (STRING *) c.ptr;
	cnt = nf - 1;

	len = ((size_t) cnt) * ofs->len + tail->len;

	fbp = fbankv;
	cp_limit = field + FBANK_SZ;
	cp = field + 1;

	while (cnt-- > 0) {
	    if (cp->type < C_STRING) {	/* use the string field temporarily */
		if (cp->type == C_NOINIT) {
		    cp->ptr = (PTR) & null_str;
		    null_str.ref_cnt++;
		} else {	/* its a double */
		    Long ival;
		    char xbuff[260];

		    ival = d_to_L(cp->dval);
		    if (ival == cp->dval)
			sprintf(xbuff, LONG_FMT, ival);
		    else
			sprintf(xbuff, string(CONVFMT)->str, cp->dval);

		    cp->ptr = (PTR) new_STRING(xbuff);
		}
	    }

	    len += string(cp)->len;

	    if (++cp == cp_limit) {
		cp = *++fbp;
		cp_limit = cp + FBANK_SZ;
	    }

	}

	field[0].type = C_STRING;
	field[0].ptr = (PTR) new_STRING0(len);

	p = string(field)->str;

	/* walk it again , putting things together */
	cnt = nf - 1;
	fbp = fbankv;
	cp = field + 1;
	cp_limit = field + FBANK_SZ;

	while (cnt-- > 0) {
	    register char *q;

	    memcpy(p, string(cp)->str, string(cp)->len);
	    p += string(cp)->len;
	    /* if not really string, free temp use of ptr */
	    if (cp->type < C_STRING) {
		free_STRING(string(cp));
	    }
	    if (++cp == cp_limit) {
		cp = *++fbp;
		cp_limit = cp + FBANK_SZ;
	    }
	    /* add the separator */
	    q = ofs->str;
	    while (*q)
		*p++ = *q++;
	}
	/* tack tail on the end */
	memcpy(p, tail->str, tail->len);

	/* cleanup */
	if (tail == ofs) {
	    free_STRING(tail);
	} else {
	    free_STRING(tail);
	    free_STRING(ofs);
	}
    }
}

/* We are assigning to a CELL and we aren't sure if its
   a field
*/
void
slow_cell_assign(CELL *target, CELL *source)
{
    if (field <= target && target <= LAST_PFIELD) {
	field_assign(target, source);
    } else {
	size_t i;
	for (i = 1; i < fbankv_num_chunks * FBANKV_CHUNK_SIZE; i++) {
	    CELL *bank_start = fbankv[i];
	    CELL *bank_end;

	    if (bank_start == NULL)
		break;

	    bank_end = bank_start + FBANK_SZ;
	    if (bank_start <= target && target < bank_end) {
		/* it is a field */
		field_assign(target, source);
		return;
	    }
	}
	/* its not a field */
	cell_destroy(target);
	cellcpy(target, source);
    }
}

int
field_addr_to_index(const CELL *cp)
{
    CELL **p = fbankv;

    while (cp < *p || cp >= *p + FBANK_SZ)
	p++;

    return (int) (((p - fbankv) << FB_SHIFT) + (cp - *p));
}

/*------- more than 1 fbank needed  ------------*/

/*
  compute the address of a field $i

  if CELL $i doesn't exist, because it is bigger than max_field,
  then it gets created and max_field grows.
*/

CELL *
slow_field_ptr(int i)
{

    if (i > max_field) {	/* need to allocate more field memory */
	int j;
	allocate_fbankv(i);

	j = (max_field >> FB_SHIFT) + 1;

	assert(j > 0 && fbankv[j - 1] != NULL && fbankv[j] == NULL);

	do {
	    fbankv[j] = (CELL *) zmalloc(sizeof(CELL) * FBANK_SZ);
	    memset(fbankv[j], 0, sizeof(CELL) * FBANK_SZ);
	    j++;
	    max_field += FBANK_SZ;
	}
	while (i > max_field);
    }

    return &fbankv[i >> FB_SHIFT][i & (FBANK_SZ - 1)];
}

#if USE_BINMODE

/* read current value of BINMODE */
int
binmode(void)
{
    CELL c;

    cast1_to_d(cellcpy(&c, BINMODE));
    return d_to_i(c.dval);
}

/* set BINMODE and RS and ORS
   from environment or -W binmode=   */

void
set_binmode(long x)
{
    CELL c;
    int change = ((x & 4) == 0);

    /* set RS */
    c.type = C_STRING;
    c.ptr = (PTR) new_STRING((change && (x & 1)) ? "\r\n" : "\n");
    field_assign(RS, &c);
    free_STRING(string(&c));

    /* set ORS */
    cell_destroy(ORS);
    ORS->type = C_STRING;
    ORS->ptr = (PTR) new_STRING((change && (x & 2)) ? "\r\n" : "\n");

    cell_destroy(BINMODE);
    BINMODE->type = C_DOUBLE;
    BINMODE->dval = (double) x;
}

#endif /* USE_BINMODE */

#ifdef NO_LEAKS

static void
fbank_free(CELL *const fb)
{
    CELL *end = fb + FBANK_SZ;
    CELL *cp;
    for (cp = fb; cp < end; cp++) {
	cell_destroy(cp);
    }
    zfree(fb, FBANK_SZ * sizeof(CELL));
}

static void
fbankv_free(void)
{
    unsigned i = 1;
    const size_t cnt = FBANKV_CHUNK_SIZE * fbankv_num_chunks;
    while (i < cnt && fbankv[i] != NULL) {
	fbank_free(fbankv[i]);
	i++;
    }
    for (; i < cnt; i++) {
	if (fbankv[i] != NULL) {
	    bozo("unexpected pointer in fbankv[]");
	}
    }
    zfree(fbankv, cnt * sizeof(CELL *));
}

void
field_leaks(void)
{
    int n;

    /* everything in field[] */
    for (n = 0; n < FBANK_SZ + NUM_PFIELDS; n++) {
	cell_destroy(&field[n]);
    }
    /* fbankv[0] == field
       this call does all the rest of the fields
     */
    fbankv_free();

    switch (fs_shadow.type) {
    case C_RE:
	re_destroy(fs_shadow.ptr);
	break;
    case C_STRING:
    case C_STRNUM:
    case C_MBSTRN:
	cell_destroy(&fs_shadow);
	break;
    default:
	break;
    }

    switch (rs_shadow.type) {
    case SEP_STR:
	free_STRING(rs_shadow.u.s_ptr);
	break;
    case SEP_RE:
	re_destroy(rs_shadow.u.r_ptr);
	break;
    }
}
#endif
