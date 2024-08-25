/********************************************
fin.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1995,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: fin.c,v 1.56 2024/08/25 17:04:35 tom Exp $
 */

#define Visible_CELL
#define Visible_FIN
#define Visible_SEPARATOR
#define Visible_STRING
#define Visible_SYMTAB

#include <mawk.h>
#include <fin.h>
#include <memory.h>
#include <bi_vars.h>
#include <field.h>
#include <symtype.h>
#include <scan.h>

#ifdef	  HAVE_FCNTL_H
#include <fcntl.h>
#endif

/* This file handles input files.  Opening, closing,
   buffering and (most important) splitting files into
   records, FINgets().
*/

/*
 * An input buffer can grow much larger than the memory pool, and the number
 * of open files is fairly constrained.  We allow for that in zmalloc(), by
 * bypassing the memory pool.
 */
#ifdef MSDOS
#define JUMPSZ BUFFSZ
#else
#define JUMPSZ (BUFFSZ * 64)
#endif

static FIN *next_main(int);
static char *enlarge_fin_buffer(FIN *);
int is_cmdline_assign(char *);	/* also used by init */

/* this is how we mark EOF on main_fin  */
static char dead_buff = 0;
static FIN dead_main =
{0, (FILE *) 0, &dead_buff, &dead_buff, &dead_buff,
 1, EOF_FLAG};

static void
free_fin_data(FIN * fin)
{
    if (fin != &dead_main) {
	zfree(fin->buff, fin->buff_size);
	ZFREE(fin);
    }
}

/* convert file-descriptor to FIN*.
   It's the main stream if main_flag is set
*/
FIN *
FINdopen(int fd, int main_flag)
{
    FIN *fin = ZMALLOC(FIN);

    fin->fd = fd;
    fin->flags = main_flag ? (MAIN_FLAG | START_FLAG) : START_FLAG;
    fin->buff_size = JUMPSZ;
    fin->buffp = fin->buff = (char *) zmalloc(fin->buff_size);
    fin->limit = fin->buffp;
    fin->buff[0] = 0;

    if ((isatty(fd) && rs_shadow.type == SEP_CHAR && rs_shadow.c == '\n')
	|| interactive_flag) {
	/* interactive, i.e., line buffer this file */
	if (fd == 0) {
	    fin->fp = stdin;
	} else if (!(fin->fp = fdopen(fd, "r"))) {
	    errmsg(errno, "fdopen failed");
	    free_fin_data(fin);
	    mawk_exit(2);
	}
    } else {
	fin->fp = (FILE *) 0;
    }

    return fin;
}

/* open a FIN* by filename.
   It's the main stream if main_flag is set.
   Recognizes "-" as stdin.
*/

FIN *
FINopen(char *filename, int main_flag)
{
    FIN *result = 0;
    int fd;
    int oflag = O_RDONLY;

#if USE_BINMODE
    int bm = binmode() & 1;
    if (bm)
	oflag |= O_BINARY;
#endif

    TRACE(("FINopen(%s)\n", filename));
    if ((filename[0] == '-' && filename[1] == 0) ||
	(filename[0] == '/' && !strcmp(filename, "/dev/stdin"))) {
#if USE_BINMODE
	if (bm)
	    setmode(0, O_BINARY);
#endif
	result = FINdopen(0, main_flag);
    } else if ((fd = open(filename, oflag, 0)) != -1) {
	result = FINdopen(fd, main_flag);
    }
    return result;
}

/* frees the buffer and fd, but leaves FIN structure until
   the user calls close() */

void
FINsemi_close(FIN * fin)
{
    static char dead = 0;

    if (fin->buff != &dead) {
	zfree(fin->buff, fin->buff_size);

	if (fin->fd) {
	    if (fin->fp)
		fclose(fin->fp);
	    else
		close(fin->fd);
	}

	fin->flags |= EOF_FLAG;
	fin->limit =
	    fin->buff =
	    fin->buffp = &dead;	/* marks it semi_closed */
    }
    /* else was already semi_closed */
}

/* user called close() on input file */
void
FINclose(FIN * fin)
{
    FINsemi_close(fin);
    ZFREE(fin);
}

/* return one input record as determined by RS,
   from input file (FIN)  fin
*/

char *
FINgets(FIN * fin, size_t *len_p)
{
    char *p;
    char *q = 0;
    size_t match_len;
    size_t r;

  restart:

    if ((p = fin->buffp) >= fin->limit) {	/* need a refill */
	if (fin->flags & EOF_FLAG) {
	    if (fin->flags & MAIN_FLAG) {
		fin = next_main(0);
		goto restart;
	    } else {
		*len_p = 0;
		return (char *) 0;
	    }
	}

	if (fin->fp) {
	    int have_nl = 0;
	    int got_any = 0;
	    char *my_buff = fin->buff;

	    do {
		/* line buffering */
		if (!fgets(my_buff, BUFFSZ + 1, fin->fp)) {
		    if (got_any) {
			/* no newline, but we have data -- okay */
			break;
		    }
		    fin->flags |= EOF_FLAG;
		    fin->buff[0] = 0;
		    fin->buffp = fin->buff;
		    fin->limit = fin->buffp;
		    goto restart;	/* might be main_fin */
		} else {	/* return this line */
		    /*
		     * Using fgets, we cannot detect embedded nulls in the
		     * input.  Assume that a null is the one added by fgets
		     * after reading data.  If we have a newline, that is
		     * better, since fgets has the complete line.
		     */
		    p = my_buff;
		    while (*p != '\n' && *p != 0)
			p++;

		    if (*p == '\n') {
			have_nl = 1;
			*p = 0;
		    } else {
			/*
			 * Increase the buffer size to allow reading more data,
			 * and point 'my_buff' to the beginning of the extra
			 * space.  Doing it this way assumes very-long lines
			 * are rare.
			 */
			size_t my_size = (size_t) (p - fin->buff);

			enlarge_fin_buffer(fin);
			p = my_buff = my_size + fin->buff;
			got_any = 1;
		    }
		}
	    } while (!have_nl);

	    /*
	     * At this point, 'p' points to the terminating null for the
	     * input line.  Fill in the FIN structure details.
	     */
	    *len_p = (size_t) (p - fin->buff);
	    fin->buffp = p;
	    fin->limit = fin->buffp + strlen(fin->buffp);
	    return fin->buff;
	} else {
	    /* block buffering */
	    r = fillbuff(fin->fd, fin->buff, fin->buff_size);
	    if (r == 0) {
		fin->flags |= EOF_FLAG;
		fin->buffp = fin->buff;
		fin->limit = fin->buffp;
		goto restart;	/* might be main */
	    } else if (r < fin->buff_size) {
		fin->flags |= EOF_FLAG;
	    }

	    fin->limit = fin->buff + r;
	    p = fin->buffp = fin->buff;

	    if (fin->flags & START_FLAG) {
		fin->flags &= ~START_FLAG;
		if (rs_shadow.type == SEP_MLR) {
		    /* trim blank lines from front of file */
		    while (*p == '\n')
			p++;
		    fin->buffp = p;
		    if (p >= fin->limit)
			goto restart;
		}
	    }
	}
    }

  retry:

    switch (rs_shadow.type) {
    case SEP_CHAR:
	q = memchr(p, rs_shadow.c, (size_t) (fin->limit - p));
	match_len = 1;
	break;

    case SEP_STR:
	q = str_str(p,
		    (size_t) (fin->limit - p),
		    ((STRING *) rs_shadow.ptr)->str,
		    match_len = ((STRING *) rs_shadow.ptr)->len);
	break;

    case SEP_MLR:
    case SEP_RE:
	q = re_pos_match(p, (size_t) (fin->limit - p), rs_shadow.ptr,
			 &match_len,
			 (p != fin->buff) ||
			 (fin->flags & FIN_FLAG));
	/* if the match is at the end, there might still be
	   more to match in the file */
	if (q && q[match_len] == 0 && !(fin->flags & EOF_FLAG)) {
	    TRACE(("re_pos_match cancelled\n"));
	    q = (char *) 0;
	}
	break;

    default:
	bozo("type of rs_shadow");
    }

    if (q) {
	/* the easy and normal case */
	*q = 0;
	*len_p = (unsigned) (q - p);
	fin->buffp = q + match_len;
	return p;
    }

    if (fin->flags & EOF_FLAG) {
	/* last line without a record terminator */
	*len_p = r = (unsigned) (fin->limit - p);
	fin->buffp = p + r;

	if (rs_shadow.type == SEP_MLR && fin->buffp[-1] == '\n'
	    && r != 0) {
	    (*len_p)--;
	    *--fin->buffp = 0;
	    fin->limit--;
	}
	return p;
    }

    if (p == fin->buff) {
	/* current record is too big for the input buffer, grow buffer */
	p = enlarge_fin_buffer(fin);
    } else {
	/* move a partial line to front of buffer and try again */
	size_t rr;
	size_t amount = (size_t) (fin->limit - p);

	fin->flags |= FIN_FLAG;
	r = amount;
	if (fin->buff_size < r) {
	    fin->flags |= EOF_FLAG;
	    return 0;
	}

	p = (char *) memmove(fin->buff, p, r);
	q = p + r;
	rr = fin->buff_size - r;

	if ((r = fillbuff(fin->fd, q, rr)) < rr) {
	    fin->flags |= EOF_FLAG;
	    fin->limit = fin->buff + amount + r;
	}
    }
    goto retry;
}

static char *
enlarge_fin_buffer(FIN * fin)
{
    size_t r;
    size_t oldsize = fin->buff_size;
    size_t newsize = ((oldsize < JUMPSZ)
		      ? (oldsize * 2)
		      : (oldsize + JUMPSZ));
    size_t limit = (size_t) (fin->limit - fin->buff);
    size_t extra = (newsize - oldsize);

#ifdef  MSDOS
    /* I'm not sure this can really happen:
       avoid "16bit wrap" */
    if (fin->buff_size >= MAX_BUFFS) {
	errmsg(0, "out of input buffer space");
	mawk_exit(2);
    }
#endif

    fin->buff_size = newsize;
    fin->buffp =
	fin->buff = (char *) zrealloc(fin->buff, oldsize, newsize);

    if (fin->fp == 0) {
	r = fillbuff(fin->fd, fin->buff + oldsize, extra);
	if (r < extra)
	    fin->flags |= EOF_FLAG;
	fin->limit = fin->buff + limit + r;
    }
    return fin->buff;
}

/* fill the target with at most the number of bytes requested */
size_t
fillbuff(int fd, char *target, size_t size)
{
    register int r;
    size_t entry_size = size;

    while (size)
	switch (r = (int) read(fd, target, size)) {
	case -1:
	    errmsg(errno, "read error");
	    mawk_exit(2);

	case 0:
	    goto out;

	default:
	    target += r;
	    size -= (unsigned) r;
	    break;
	}

  out:
    return (size_t) (entry_size - size);
}

/* main_fin is a handle to the main input stream
   == 0	 never been opened   */

FIN *main_fin;
ARRAY Argv;			/* to the user this is ARGV  */
static double argi = 1.0;	/* index of next ARGV[argi] to try to open */

static void
set_main_to_stdin(void)
{
    cell_destroy(FILENAME);
    FILENAME->type = C_STRING;
    FILENAME->ptr = (PTR) new_STRING("-");
    cell_destroy(FNR);
    FNR->type = C_DOUBLE;
    FNR->dval = 0.0;
    rt_fnr = 0;
    main_fin = FINdopen(0, 1);
}

/* this gets called once to get the input stream going.
   It is called after the execution of the BEGIN block
   unless there is a getline inside BEGIN {}
*/
void
open_main(void)
{
    CELL argc;

#if USE_BINMODE
    int k = binmode();

    if (k & 1)
	setmode(0, O_BINARY);
    if (k & 2) {
	setmode(1, O_BINARY);
	setmode(2, O_BINARY);
    }
#endif

    cellcpy(&argc, ARGC);
    if (argc.type != C_DOUBLE)
	cast1_to_d(&argc);

    if (argc.dval == 1.0)
	set_main_to_stdin();
    else
	next_main(1);
}

/* get the next command line file open */
static FIN *
next_main(int open_flag)	/* called by open_main() if on */
{
    CELL argc;			/* copy of ARGC */
    CELL c_argi;		/* cell copy of argi */
    CELL argval;		/* copy of ARGV[c_argi] */
    int failed = 1;

    argval.type = C_NOINIT;
    c_argi.type = C_DOUBLE;

    if (main_fin) {
	FINclose(main_fin);
	main_fin = 0;
    }
    /* FILENAME and FNR don't change unless we really open
       a new file */

    /* make a copy of ARGC to avoid side effect */
    if (cellcpy(&argc, ARGC)->type != C_DOUBLE)
	cast1_to_d(&argc);

    while (argi < argc.dval) {
	register CELL *cp;

	c_argi.dval = argi;
	argi += 1.0;

	if (!(cp = array_find(Argv, &c_argi, NO_CREATE)))
	    continue;		/* its deleted */

	/* make a copy so we can cast w/o side effect */
	cell_destroy(&argval);
	cp = cellcpy(&argval, cp);
	if (cp->type < C_STRING)
	    cast1_to_s(cp);
	if (string(cp)->len == 0) {
	    /* file argument is "" */
	    cell_destroy(cp);
	    continue;
	}

	/* it might be a command line assignment */
	if (is_cmdline_assign(string(cp)->str)) {
	    continue;
	}

	/* try to open it -- we used to continue on failure,
	   but posix says we should quit */
	if (!(main_fin = FINopen(string(cp)->str, 1))) {
	    errmsg(errno, "cannot open \"%s\"", string(cp)->str);
	    mawk_exit(2);
	}

	/* success -- set FILENAME and FNR */
	cell_destroy(FILENAME);
	cellcpy(FILENAME, cp);
	cell_destroy(cp);
	cell_destroy(FNR);
	FNR->type = C_DOUBLE;
	FNR->dval = 0.0;
	rt_fnr = 0;

	failed = 0;
	break;
    }

    if (failed) {
	cell_destroy(&argval);

	if (open_flag) {
	    /* all arguments were null or assignment */
	    set_main_to_stdin();
	} else {
	    main_fin = &dead_main;
	    /* since MAIN_FLAG is not set, FINgets won't call next_main() */
	}
    }

    return main_fin;
}

int
is_cmdline_assign(char *s)
{
    static CELL empty_cell;

    register char *p;

    int c;
    SYMTAB *stp;
    CELL *cp = 0;
    size_t len;
    CELL cell = empty_cell;	/* used if command line assign to pseudo field */
    CELL *fp = NULL;		/* ditto */
    size_t length;

    if (scan_code[*(unsigned char *) s] != SC_IDCHAR)
	return 0;

    p = s + 1;
    while ((c = scan_code[*(unsigned char *) p]) == SC_IDCHAR
	   || c == SC_DIGIT)
	p++;

    if (*p != '=')
	return 0;

    *p = 0;
    stp = find(s);

    switch (stp->type) {
    case ST_NONE:
	stp->type = ST_VAR;
	stp->stval.cp = cp = ZMALLOC(CELL);
	break;

    case ST_VAR:
    case ST_NR:		/* !! no one will do this */
	cp = stp->stval.cp;
	cell_destroy(cp);
	break;

    case ST_FIELD:
	/* must be pseudo field */
	fp = stp->stval.cp;
	cp = &cell;
	break;

    default:
	rt_error(
		    "cannot command line assign to %s\n\ttype clash or keyword"
		    ,s);
    }

    /* we need to keep ARGV[i] intact */
    *p++ = '=';
    len = strlen(p) + 1;
    /* posix says escape sequences are on from command line */
    p = rm_escape(strcpy((char *) zmalloc(len), p), &length);
    cp->ptr = (PTR) new_STRING1(p, length);
    zfree(p, len);
    check_strnum(cp);		/* sets cp->type */
    if (fp)			/* move it from cell to pfield[] */
    {
	field_assign(fp, cp);
	free_STRING(string(cp));
    }
    return 1;
}

#ifdef NO_LEAKS
void
fin_leaks(void)
{
    TRACE(("fin_leaks\n"));
    if (main_fin) {
	free_fin_data(main_fin);
	main_fin = 0;
    }
}
#endif
