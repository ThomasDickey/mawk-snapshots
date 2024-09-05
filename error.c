/********************************************
error.c
copyright 2008-2023,2024 Thomas E. Dickey
copyright 1991-1994,1995 Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: error.c,v 1.28 2024/08/29 00:19:40 tom Exp $
 */

#define Visible_CELL
#define Visible_STRING
#define Visible_SYMTAB

#include <mawk.h>
#include <scan.h>
#include <bi_vars.h>

#ifdef DEBUG
#define FLUSH() fflush(stdout)
#else
#define FLUSH()			/* nothing */
#endif

/* for run time error messages only */
unsigned rt_nr, rt_fnr;
/* *INDENT-OFF* */
static const struct token_str {
    short token;
    const char str[12];
} token_str[] = {
    { EOF,                "end of file" },
    { NL,                 "end of line" },
    { SEMI_COLON,         ";" },
    { LBRACE,             "{" },
    { RBRACE,             "}" },
    { SC_FAKE_SEMI_COLON, "}" },
    { LPAREN,             "(" },
    { RPAREN,             ")" },
    { LBOX,               "[" },
    { RBOX,               "]" },
    { QMARK,              "?" },
    { COLON,              ":" },
    { OR,                 "||" },
    { AND,                "&&" },
    { ASSIGN,             "=" },
    { ADD_ASG,            "+=" },
    { SUB_ASG,            "-=" },
    { MUL_ASG,            "*=" },
    { DIV_ASG,            "/=" },
    { MOD_ASG,            "%=" },
    { POW_ASG,            "^=" },
    { EQ,                 "==" },
    { NEQ,                "!=" },
    { LT,                 "<" },
    { LTE,                "<=" },
    { GT,                 ">" },
    { GTE,                ">=" },
    { MATCH,              "" },	/* string_buff */
    { PLUS,               "+" },
    { MINUS,              "-" },
    { MUL,                "*" },
    { DIV,                "/" },
    { MOD,                "%" },
    { POW,                "^" },
    { NOT,                "!" },
    { COMMA,              "," },
    { INC_or_DEC,         "" },	/* string_buff */
    { DOUBLE,             "" },	/* string_buff */
    { STRING_,            "" },	/* string_buff */
    { ID,                 "" },	/* string_buff */
    { FUNCT_ID,           "" },	/* string_buff */
    { BUILTIN,            "" },	/* string_buff */
    { IO_OUT,             "" },	/* string_buff */
    { IO_IN,              "<" },
    { PIPE,               "|" },
    { DOLLAR,             "$" },
    { FIELD,              "$" },
    { 0,                  "" }
};
/* *INDENT-ON* */

/* if paren_cnt >0 and we see one of these, we are missing a ')' */
static const int missing_rparen[] =
{
    EOF, NL, SEMI_COLON, SC_FAKE_SEMI_COLON, RBRACE, 0
};

/* ditto for '}' */
static const int missing_rbrace[] =
{
    EOF, BEGIN, END, 0
};

static void
missing(int c, const char *n, unsigned ln)
{
    const char *s0, *s1;

    if (pfile_name) {
	s0 = pfile_name;
	s1 = ": ";
    } else
	s0 = s1 = "";

    errmsg(0, "%s%sline %u: missing %c near %s", s0, s1, ln, c, n);

    if (++compile_error_count >= MAX_COMPILE_ERRORS)
	mawk_exit(2);
}

void
yyerror(const char *s GCC_UNUSED)
{
    const char *ss = 0;
    const struct token_str *p;
    const int *ip;

    for (p = token_str; p->token; p++)
	if (current_token == p->token) {
	    ss = p->str[0] ? p->str : string_buff;
	    break;
	}

    if (!ss)			/* search the keywords */
	ss = find_kw_str(current_token);

    if (ss) {
	if (paren_cnt)
	    for (ip = missing_rparen; *ip; ip++)
		if (*ip == current_token) {
		    missing(')', ss, token_lineno);
		    paren_cnt = 0;
		    return;
		}

	if (brace_cnt)
	    for (ip = missing_rbrace; *ip; ip++)
		if (*ip == current_token) {
		    missing('}', ss, token_lineno);
		    brace_cnt = 0;
		    return;
		}

	compile_error("syntax error at or near %s", ss);

    } else {			/* special cases */
	switch (current_token) {
	case UNEXPECTED:
	    unexpected_char();
	    break;

	case BAD_DECIMAL:
	    compile_error(
			     "syntax error in decimal constant %s",
			     string_buff);
	    break;

	case RE:
	    compile_error(
			     "syntax error at or near /%s/",
			     string_buff);
	    break;

	default:
	    compile_error("syntax error");
	    break;
	}
    }
}

/* generic error message with a hook into the system error
   messages if errnum > 0 */

void
errmsg(int errnum, const char *format, ...)
{
    va_list args;

    FLUSH();
    fprintf(stderr, "%s: ", progname);

#if OPT_TRACE > 0
    va_start(args, format);
    Trace("\n?? errmsg \n");
    TraceVA(format, args);
    Trace("\n");
    va_end(args);
#endif

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (errnum > 0)
	fprintf(stderr, " (%s)", strerror(errnum));

    fprintf(stderr, "\n");
}

void
compile_error(const char *format, ...)
{
    va_list args;
    const char *s0, *s1;

    /* with multiple program files put program name in
       error message */
    if (pfile_name) {
	s0 = pfile_name;
	s1 = ": ";
    } else {
	s0 = s1 = "";
    }

    FLUSH();
    fprintf(stderr, "%s: %s%sline %u: ", progname, s0, s1, token_lineno);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    if (++compile_error_count >= MAX_COMPILE_ERRORS)
	mawk_exit(2);
}

void
bozo(const char *s)
{
    errmsg(0, "bozo: %s", s);
    mawk_exit(3);
}

void
overflow(const char *s, unsigned size)
{
    errmsg(0, "program limit exceeded: %s size=%u", s, size);
    mawk_exit(2);
}

/* print as much as we know about where a rt error occurred */

static void
rt_where(void)
{
    if (FILENAME->type != C_STRING)
	cast1_to_s(FILENAME);

    fprintf(stderr, "\tFILENAME=\"%s\" FNR=%u NR=%u\n",
	    string(FILENAME)->str, rt_fnr, rt_nr);
}

void
rt_error(const char *format, ...)
{
    va_list args;

    FLUSH();
    fprintf(stderr, "%s: run time error: ", progname);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    putc('\n', stderr);
    rt_where();
    mawk_exit(2);
}

/* run time */
void
rt_overflow(const char *s, unsigned size)
{
    errmsg(0, "program limit exceeded: %s size=%u", s, size);
    rt_where();
    mawk_exit(2);
}

void
unexpected_char(void)
{
    int c = yylval.ival;

    FLUSH();
    fprintf(stderr, "%s: %u: ", progname, token_lineno);
    if (c > ' ' && c < 127)
	fprintf(stderr, "unexpected character '%c'\n", c);
    else
	fprintf(stderr, "unexpected character 0x%02x\n", c);
    if (++compile_error_count >= MAX_COMPILE_ERRORS)
	mawk_exit(2);
}

const char *
type_to_str(int type)
{
    const char *retval = 0;

    switch (type) {
    case ST_NONE:
	retval = "untyped variable";
	break;
    case ST_VAR:
	retval = "variable";
	break;
    case ST_KEYWORD:
	retval = "keyword";
	break;
    case ST_BUILTIN:
	retval = "builtin";
	break;
    case ST_ARRAY:
	retval = "array";
	break;
    case ST_FIELD:
	retval = "field";
	break;
    case ST_NR:
	retval = "NR";
	break;
    case ST_ENV:
	retval = "ENVIRON";
	break;
    case ST_FUNCT:
	retval = "function";
	break;
    case ST_LOCAL_VAR:
	retval = "local variable";
	break;
    case ST_LOCAL_NONE:
	retval = "local untyped variable";
	break;
    case ST_LOCAL_ARRAY:
	retval = "local array";
	break;
    default:
	bozo("type_to_str");
    }
    return retval;
}

/* emit an error message about a type clash */
void
type_error(SYMTAB * p)
{
    compile_error("illegal reference to %s %s",
		  type_to_str(p->type), p->name);
}
