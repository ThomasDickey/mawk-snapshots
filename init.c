/********************************************
init.c
copyright 2008-2024,2026, Thomas E. Dickey
copyright 1991-1994,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: init.c,v 1.87 2026/01/30 00:38:58 tom Exp $
 */

#define Visible_ARRAY
#define Visible_BI_REC
#define Visible_CELL
#define Visible_PFILE
#define Visible_STRING
#define Visible_SYMTAB

#include <mawk.h>
#include <code.h>
#include <memory.h>
#include <symtype.h>
#include <init.h>
#include <bi_funct.h>
#include <bi_vars.h>
#include <files.h>
#include <field.h>

#include <ctype.h>

typedef enum {
    W_UNKNOWN = 0,
    W_VERSION,
#if USE_BINMODE
    W_BINMODE,
#endif
    W_DUMP,
    W_EXEC,
    W_HELP,
    W_INTERACTIVE,
    W_POSIX,
    W_RANDOM,
    W_RE_INTERVAL,
    W_SPRINTF,
    W_TRADITIONAL,
    W_USAGE,
    W__IGNORE
} W_OPTIONS;

#ifdef  MSDOS
#if  HAVE_REARGV
extern void reargv(int *, char ***);
#endif
#endif

const char *progname;
short interactive_flag = 0;

#ifndef	 SET_PROGNAME
#define	 SET_PROGNAME() \
   {char *p = strrchr(argv[0],'/') ;\
    progname = p ? p+1 : argv[0] ; }
#endif

int dump_code_flag = 0;		/* if on dump internal code */
short posix_space_flag = 0;	/* -Wposix or --posix */
short traditional_flag = 0;	/* -Wtraditional or --traditional */

#ifndef NO_INTERVAL_EXPR
#define enable_repetitions(flag) repetitions_flag = flag
short repetitions_flag = 1;
#else
#define enable_repetitions(flag)	/* nothing */
#endif

#ifdef	 DEBUG
int dump_RE = 1;		/* if on dump compiled REs  */
#endif
/* *INDENT-OFF* */
static const struct {
    W_OPTIONS code;
    int mode;			/* 0=mawk, 1=both, 2=gawk */
    int args;			/* nonzero if argument */
    const char name[20];
} w_options[] = {
    { W_VERSION,     1, 0, "version" },
#if USE_BINMODE
    { W_BINMODE,     0, 0, "binmode" },
#endif
    { W_DUMP,        0, 0, "dump" },
    { W_EXEC,        1, 1, "exec" },
    { W_HELP,        1, 0, "help" },
    { W_INTERACTIVE, 0, 0, "interactive" },
    { W_POSIX,       1, 0, "posix" },
    { W_RANDOM,      0, 1, "random" },
    { W_RE_INTERVAL, 2, 0, "re-interval" },
    { W_SPRINTF,     0, 1, "sprintf" },
    { W_TRADITIONAL, 1, 0, "traditional" },
    { W_USAGE,       0, 0, "usage" },
    { W__IGNORE,     2, 0, "lint" },
    { W__IGNORE,     2, 0, "lint-old" },
    { W__IGNORE,     2, 0, "non-decimal-data" },
};
/* *INDENT-ON* */

static GCC_NORETURN void
no_program(void)
{
    mawk_exit(0);
}

static GCC_NORETURN void
usage(FILE *fp)
{
    static const char msg[][80] =
    {
	"Usage: mawk [Options] [Program] [file ...]",
	"",
	"Program:",
	"    The -f option value is the name of a file containing program text.",
	"    If no -f option is given, a \"--\" ends option processing; the following",
	"    parameters are the program text.",
	"",
	"Options:",
	"    -f program-file  Program  text is read from file instead of from the",
	"                     command-line.  Multiple -f options are accepted.",
	"    -F value         sets the field separator, FS, to value.",
	"    -v var=value     assigns value to program variable var.",
	"    --               unambiguous end of options.",
	"",
	"    Implementation-specific options are prefixed with \"-W\".  They can be",
	"    abbreviated:",
	"",
	"    -W version       show version information and exit.",
#if USE_BINMODE
	"    -W binmode",
#endif
	"    -W dump          show assembler-like listing of program and exit.",
	"    -W help          show this message and exit.",
	"    -W interactive   set unbuffered output, line-buffered input.",
	"    -W exec file     use file as program as well as last option.",
	"    -W posix         stricter POSIX checking.",
	"    -W random=number set initial random seed.",
	"    -W sprintf=number adjust size of sprintf buffer.",
	"    -W traditional   pre-POSIX 2001.",
	"    -W usage         show this message and exit.",
    };
    size_t n;
    for (n = 0; n < TABLESIZE(msg); ++n) {
	fprintf(fp, "%s\n", msg[n]);
    }
    mawk_exit(0);
}

/*
 * Compare ignoring case, but warn about mismatches.
 */
static int
ok_abbrev(const char *fullName, const char *partName, int partLen)
{
    int result = 1;
    int n;

    for (n = 0; n < partLen; ++n) {
	UChar ch = (UChar) partName[n];
	if (isalpha(ch))
	    ch = (UChar) toupper(ch);
	if (ch != (UChar) toupper((UChar) fullName[n])) {
	    result = 0;
	    break;
	}
    }
    return result;
}

static char *
skipValue(char *value)
{
    while (*value != '\0' && *value != ',') {
	++value;
    }
    return value;
}

static int
haveValue(char *value)
{
    int result = 0;

    if (*value++ == '=') {
	if (*value != '\0' && strchr("=,", *value) == NULL)
	    result = 1;
    }
    return result;
}

static W_OPTIONS
parse_w_opt(char *source, char **next, int *args)
{
    W_OPTIONS result = W_UNKNOWN;
    int match = -1;
    const char *first;

    *args = 0;

    /* forgive and ignore empty options */
    while (*source == ',') {
	++source;
    }

    first = source;
    if (*source != '\0') {
	int n;
	char mark;
	while (*source != '\0' && *source != ',' && *source != '=') {
	    ++source;
	}
	mark = *source;
	*source = '\0';
	for (n = 0; n < (int) TABLESIZE(w_options); ++n) {
	    if (w_options[n].mode > 1)
		continue;
	    if (!strcmp(w_options[n].name, first)) {
		match = n;
		break;
	    }
	    if (ok_abbrev(w_options[n].name, first, (int) (source - first))) {
		if (match >= 0) {
		    errmsg(0, "ambiguous -W value: \"%.*s\" (%s vs %s)",
			   (int) (source - first), first,
			   w_options[match].name,
			   w_options[n].name);
		} else {
		    match = n;
		}
	    }
	}
	if (match < 0 && ok_abbrev("POSIX_SPACE", first, (int) (source - first))) {
	    errmsg(0, "deprecated option, use -W posix");
	    for (n = 0; n < (int) TABLESIZE(w_options); ++n) {
		if (w_options[n].code == W_POSIX) {
		    match = n;
		    break;
		}
	    }
	}
	*source = mark;
    }
    *next = source;

    if (match >= 0) {
	result = w_options[match].code;
	*args = w_options[match].args;
    }

    return result;
}

static W_OPTIONS
parse_long_opt(char *source, char **next, int *args)
{
    int n;
    int match = -1;
    W_OPTIONS result = W_UNKNOWN;
    const char *first = source;
    char mark;

    *args = 0;

    while (*source != '\0' && *source != '=') {
	++source;
    }
    mark = *source;
    *source = '\0';
    for (n = 0; n < (int) TABLESIZE(w_options); ++n) {
	if (!strcmp(w_options[n].name, first)) {
	    match = n;
	    break;
	}
	if (ok_abbrev(w_options[n].name, first, (int) (source - first))) {
	    if (match >= 0) {
		errmsg(0, "ambiguous long option: \"--%.*s\" (--%s vs --%s)",
		       (int) (source - first), first,
		       w_options[match].name,
		       w_options[n].name);
	    } else {
		match = n;
	    }
	}
    }
    if (match >= 0) {
	result = w_options[match].code;
	*args = w_options[match].args;
    }
    *source = mark;
    *next = source;
    return result;
}

static long
numeric_option(const char *source)
{
    char *next = NULL;
    long result = strtol(source, &next, 0);
    if (next == source || next == NULL || (*next != '\0' && *next != ',')) {
	errmsg(0, "invalid numeric option: \"%s\"", source);
	mawk_exit(2);
    }
    return result;
}

/*
 * mawk allows the -W option to have multiple parts, separated by commas.  It
 * does that, to allow multiple -W options in a "sharpbang" line.
 *
 * Regarding "sharpbang:
 * While that is also referred to as a "shebang" or "hashbang" line, those
 * terms appear to have taken hold after Larry Wall referred to it as
 * "sharpbang" for changes to rn in 1985.  mawk's manual page refers to "magic
 * number", which is still older, but "sharpbang" is more descriptive.  Both
 * "sharpbang" and "magic number" were used in 4.3BSD, which of course predates
 * mawk.
 *
 * Within each comma-separated chunk, we can have an option value.
 * For instance:
 *	-Wname1
 *	-Wname1=value1
 *	-Wname1=value1,name2
 *	-Wname1=value1,name2=value2
 *
 * The corresponding long-options are blank-separated, but the "=" mark can
 * be used:
 *	--name1
 *	--name1 value1
 *	--name1=value1
 *	--name1=value1 --name2
 *	--name1=value1 --name2=value2
 *	--name1=value1 --name2 value2
 *
 * The caller has to allow for these cases, by checking the updated "value"
 * after each call.
 */
static int
handle_w_opt(W_OPTIONS code, int glue, char *option, char **value)
{
    char *optNext = *value;
    int result = 1;
    int wantArg = 0;

    switch (code) {
    case W_VERSION:
	print_version(stdout);
	/* NOTREACHED */

#if USE_BINMODE
    case W_BINMODE:
	wantArg = 1;
	if (optNext != 0) {
	    set_binmode(numeric_option(optNext));
	    wantArg = 2;
	}
	break;
#endif
    case W_TRADITIONAL:
	traditional_flag = 1;
	enable_repetitions(0);
	posix_space_flag = 0;
	break;

    case W_DUMP:
	dump_code_flag = 1;
	break;

    case W_EXEC:
	if (pfile_name) {
	    errmsg(0, "-W exec is incompatible with -f");
	    mawk_exit(2);
	}
	wantArg = 1;
	if (optNext != NULL) {
	    pfile_name = optNext;
	    wantArg = 2;
	} else {
	    no_program();
	}
	result = 0;		/* no_more_opts */
	break;

    case W_INTERACTIVE:
	interactive_flag = 1;
	setbuf(stdout, (char *) 0);
	break;

    case W_POSIX:
	posix_space_flag = 1;
	break;

    case W_RANDOM:
	wantArg = 1;
	if (optNext != NULL) {
	    long x = numeric_option(optNext);
	    CELL c[2];

	    memset(c, 0, sizeof(c));
	    c[1].type = C_DOUBLE;
	    c[1].dval = (double) x;
	    /* c[1] is input, c[0] is output */
	    bi_srand(c + 1);
	    wantArg = 2;
	}
	break;

#ifndef NO_INTERVAL_EXPR
    case W_RE_INTERVAL:
	enable_repetitions(1);
	break;
#endif

    case W_SPRINTF:
	wantArg = 1;
	if (optNext != NULL) {
	    long x = numeric_option(optNext);

	    if (x > (long) sizeof(string_buff)) {
		if (sprintf_buff != string_buff &&
		    sprintf_buff != NULL) {
		    zfree(sprintf_buff,
			  (size_t) (sprintf_limit - sprintf_buff));
		}
		sprintf_buff = (char *) zmalloc((size_t) x);
		sprintf_limit = sprintf_buff + x;
	    }
	    wantArg = 2;
	}
	break;

    case W_HELP:
	/* FALLTHRU */
    case W_USAGE:
	usage(stdout);
	/* NOTREACHED */

    case W_UNKNOWN:
	errmsg(0, "vacuous option: -W \"%s\"", option);
	break;
    case W__IGNORE:
	break;
    }
    if (wantArg) {
	if (wantArg == 1) {
	    int length = (int) (skipValue(option) - option);
	    errmsg(0, "missing value for -W \"%.*s\"", length, option);
	    mawk_exit(2);
	}
	optNext = skipValue(optNext);
    } else if (glue) {
	while (glue) {
	    errmsg(0, "unexpected option value \"%s\"", option);
	    optNext = skipValue(optNext);
	    glue = haveValue(optNext);
	}
    } else if (*value == NULL) {
	optNext = skipValue(option);
	if (*optNext == ',')
	    ++optNext;
    }
    *value = optNext;
    return result;
}

static GCC_NORETURN void
bad_option(const char *s)
{
    errmsg(0, "not an option: %s", s);
    mawk_exit(2);
}

static int
allow_long_options(char *arg, W_OPTIONS seen)
{
    char *env = getenv("MAWK_LONG_OPTIONS");
    int result = 0;

    if (env != NULL) {
	switch (*env) {
	default:
	case 'e':		/* error */
	    bad_option(arg);
	    /* NOTREACHED */
	case 'w':		/* warn */
	    errmsg(0, "ignored option: %s", arg);
	    break;
	case 'i':		/* ignore */
	    result = (seen != W_UNKNOWN);
	    break;
	case 'a':		/* allow */
	    result = 1;
	    break;
	}
    } else {
	result = (seen != W_UNKNOWN);
    }
    return result;
}

   /* argv[i] = ARGV[i] */
static void
set_ARGV(int argc, char **argv, int i)
{
    SYMTAB *st_p;
    CELL argi;
    register CELL *cp;

    st_p = insert("ARGV");
    st_p->type = ST_ARRAY;
    Argv = st_p->stval.array = new_ARRAY();
    no_leaks_array(Argv);
    argi.type = C_DOUBLE;
    argi.dval = 0.0;
    cp = array_find(st_p->stval.array, &argi, CREATE);
    cp->type = C_STRING;
    cp->ptr = (PTR) new_STRING(progname);

    /* ARGV[0] is set, do the rest
       The type of ARGV[1] ... should be C_MBSTRN
       because the user might enter numbers from the command line */

    for (argi.dval = 1.0; i < argc; i++, argi.dval += 1.0) {
	cp = array_find(st_p->stval.array, &argi, CREATE);
	cp->type = C_MBSTRN;
	cp->ptr = (PTR) new_STRING(argv[i]);
    }
    ARGC->type = C_DOUBLE;
    ARGC->dval = argi.dval;
}

static void
process_cmdline(int argc, char **argv)
{
    int i, j, nextarg;
    char *curArg = NULL;
    char *optArg;
    char *optNext;
    PFILE dummy;		/* starts linked list of filenames */
    PFILE *tail = &dummy;
    size_t length;

    if (argc <= 1)
	usage(stderr);

    for (i = 1; i < argc && *(curArg = argv[i]) == '-'; i = nextarg) {
	if (curArg[1] == 0) {
	    /* "-"  alone */
	    if (!pfile_name)
		no_program();
	    break;		/* the for loop */
	}
	/* safe to look at argv[i][2] */

	/*
	 * Check for "long" options and decide how to handle them.
	 */
	if (strlen(curArg) > 2 && !strncmp(curArg, "--", (size_t) 2)) {
	    char *name = curArg + 2;
	    int args;
	    W_OPTIONS code = parse_long_opt(name, &optNext, &args);
	    nextarg = i + 1;
	    if (allow_long_options(curArg, code)) {
		int glue = haveValue(optNext);
		char *optValue = (glue
				  ? (optNext + 1)
				  : (args
				     ? argv[nextarg++]
				     : NULL));
		if (!handle_w_opt(code, glue, name, &optValue)) {
		    goto no_more_opts;
		}
		optNext = optValue;
	    } else {
		bad_option(curArg);
		/* NOTREACHED */
	    }
	    continue;
	}

	if (curArg[2] == 0) {
	    if (i == argc - 1 && curArg[1] != '-') {
		if (strchr("WFvf", curArg[1])) {
		    errmsg(0, "option %s lacks argument", curArg);
		    mawk_exit(2);
		}
		bad_option(curArg);
		/* NOTREACHED */
	    }

	    optArg = argv[i + 1];
	    nextarg = i + 2;
	} else {		/* argument glued to option */
	    optArg = &curArg[2];
	    nextarg = i + 1;
	}

	switch (curArg[1]) {

	case 'W':
	    for (j = 0;
		 j < (int) strlen(optArg);
		 j = (optNext
		      ? (int) (optNext - optArg)
		      : (int) strlen(optArg))) {
		char *name = optArg + j;
		int args = 0;
		W_OPTIONS code = parse_w_opt(name, &optNext, &args);
		int glue = haveValue(optNext);
		char *optValue = (glue
				  ? (optNext + 1)
				  : ((strchr(optNext, ',') == NULL)
				     ? (args && argv[nextarg]
					? argv[nextarg++]
					: NULL)
				     : NULL));
		int done = !handle_w_opt(code, glue, name, &optValue);
		i = nextarg;
		if (done)
		    goto no_more_opts;
		optNext = optValue;
	    }
	    break;

#ifndef NO_INTERVAL_EXPR
	case 'r':
	    enable_repetitions(1);
	    break;
#endif

	case 'v':
	    if (!is_cmdline_assign(optArg)) {
		errmsg(0, "improper assignment: -v %s", optArg);
		mawk_exit(2);
	    }
	    break;

	case 'F':

	    rm_escape(optArg, &length);		/* recognize escape sequences */
	    cell_destroy(FS);
	    FS->type = C_STRING;
	    FS->ptr = (PTR) new_STRING1(optArg, length);
	    cast_for_split(cellcpy(&fs_shadow, FS));
	    break;

	case '-':
	    if (curArg[2] != 0) {
		bad_option(curArg);
		/* NOTREACHED */
	    }
	    curArg = argv[++i];
	    goto no_more_opts;

	case 'f':
	    /* first file goes in pfile_name ; any more go
	       on a list */
	    if (!pfile_name)
		pfile_name = optArg;
	    else {
		tail = tail->link = ZMALLOC(PFILE);
		tail->fname = optArg;
	    }
	    break;

	default:
	    bad_option(curArg);
	    /* NOTREACHED */
	}
    }

  no_more_opts:

    if (!interactive_flag && isatty(fileno(stdout))) {
	interactive_flag = 1;
	setbuf(stdout, (char *) 0);
    }

    tail->link = (PFILE *) 0;
    pfile_list = dummy.link;

    if (pfile_name) {
	set_ARGV(argc, argv, i);
	scan_init((char *) 0);
    } else {			/* program on command line */
	if (i == argc)
	    no_program();
	set_ARGV(argc, argv, i + 1);

#if  defined(MSDOS) && ! HAVE_REARGV	/* reversed quotes */
	{
	    char *p;

	    for (p = curArg; *p; p++)
		if (*p == '\'')
		    *p = '\"';
	}
#endif
	scan_init(curArg);
/* #endif  */
    }
}

/*----- ENVIRON ----------*/

#ifdef DECL_ENVIRON
#ifndef	 MSDOS_MSC		/* MSC declares it near */
#ifndef environ
extern char **environ;
#endif
#endif
#endif

void
load_environ(ARRAY ENV)
{
    CELL c;
    register char **p = environ;	/* walks environ */

    c.type = C_STRING;

    while (*p) {
	char *s;		/* looks for the '=' */

	if ((s = strchr(*p, '='))) {	/* shouldn't fail */
	    CELL *cp;		/* pts at ENV[&c] */
	    size_t len = (size_t) (s - *p);

	    c.ptr = (PTR) new_STRING0(len);
	    memcpy(string(&c)->str, *p, len);
	    s++;

	    cp = array_find(ENV, &c, CREATE);
	    cp->type = C_MBSTRN;
	    cp->ptr = (PTR) new_STRING(s);

	    free_STRING(string(&c));
	}
	p++;
    }
}

void
initialize(int argc, char **argv)
{

    SET_PROGNAME();

    bi_vars_init();		/* load the builtin variables */
    bi_funct_init();		/* load the builtin functions */
    kw_init();			/* load the keywords */
    field_init();

#if USE_BINMODE
    {
	char *p = getenv("MAWKBINMODE");

	if (p)
	    set_binmode(numeric_option(p));
    }
#endif

    process_cmdline(argc, argv);

    code_init();
    fpe_init();
    set_stdio();

#if USE_BINMODE
    stdout_init();
#endif
}

#ifdef NO_LEAKS
typedef struct _all_arrays {
    struct _all_arrays *next;
    ARRAY a;
} ALL_ARRAYS;

static ALL_ARRAYS *all_arrays;

void
array_leaks(void)
{
    while (all_arrays != NULL) {
	ALL_ARRAYS *next = all_arrays->next;
	array_clear(all_arrays->a);
	ZFREE(all_arrays->a);
	free(all_arrays);
	all_arrays = next;
    }
}

/* use this to identify array leaks */
void
no_leaks_array(ARRAY a)
{
    ALL_ARRAYS *p = calloc(1, sizeof(ALL_ARRAYS));
    p->next = all_arrays;
    p->a = a;
    all_arrays = p;
}
#endif
