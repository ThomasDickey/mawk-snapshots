/********************************************
files.c
copyright 2008-2023,2024, Thomas E. Dickey
copyright 1991-1994,1996, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: files.c,v 1.40 2024/12/14 01:35:10 tom Exp $
 */

#define Visible_STRING

#include <mawk.h>
#include <files.h>
#include <memory.h>
#include <fin.h>
#include <init.h>

#include <sys/types.h>

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef	V7
#include  <sgtty.h>		/* defines FIOCLEX */
#endif

#ifdef	 HAVE_FCNTL_H

#include <fcntl.h>
#define	 CLOSE_ON_EXEC(fd)    fcntl(fd, F_SETFD, 1)

#else
#define	 CLOSE_ON_EXEC(fd) ioctl(fd, FIOCLEX, (PTR) 0)
#endif

#ifdef	  HAVE_FSTAT
#include <sys/types.h>
#include <sys/stat.h>
#endif

/* We store dynamically created files on a linked linear
   list with move to the front (big surprise)  */

typedef struct file {
    struct file *link;
    STRING *name;
    short type;
    int pid;			/* we need to wait() when we close a pipe */
    /* holds temp file index under MSDOS */

#ifdef  HAVE_FAKE_PIPES
    int inpipe_exit;
#endif

    PTR ptr;			/* FIN*   or  FILE*   */
} FILE_NODE;

static FILE_NODE *file_list;

/* Prototypes for local functions */

static FILE *tfopen(const char *, const char *);
static int efflush(FILE *);
static GCC_NORETURN void close_error(FILE_NODE * p);

static FILE_NODE *
alloc_filenode(void)
{
    FILE_NODE *result;

    result = ZMALLOC(FILE_NODE);

#ifdef NO_LEAKS
    result->name = 0;
#endif

    result->ptr = 0;
    return result;
}

static void
free_filenode(FILE_NODE * p)
{
#ifdef NO_LEAKS
    if (p->name != 0) {
	free_STRING(p->name);
    }
#endif
    zfree(p, sizeof(FILE_NODE));
}

static GCC_NORETURN void
output_failed(const char *name)
{
    errmsg(errno, "cannot open \"%s\" for output", name);
    mawk_exit(2);
    /* NOTREACHED */
}

/*
 * Most open-failures are recoverable, or affect only certain files.
 * However, running out of file-descriptors indicates a problem with
 * the script, and it is unlikely that recovery is possible.
 */
#ifdef EMFILE			/* (POSIX.1-2001) */
#define input_failed(name, p) \
 	do { \
	    if (errno == EMFILE) { \
		errmsg(errno, "cannot open \"%s\" for input", name); \
		mawk_exit(2); \
	    } \
	    free_filenode(p); \
	    return (PTR) 0; \
	} while (0)
#else
#define input_failed(name, p) \
	do { \
	    free_filenode(p); \
	    return (PTR) 0; \
	} while (0)
#endif

#if USE_BINMODE
#define BinMode2(t,f) ((binmode() & 2) ? (t) : (f))
#else
#define BinMode2(t,f) (f)
#endif

/*
 * Find a file on file_list.  Outputs return a FILE*, while inputs return FIN*.
 */
PTR
file_find(STRING * sval, int type)
{
    FILE_NODE *p = file_list;
    FILE_NODE *q = (FILE_NODE *) 0;
    char *name = sval->str;

    TRACE(("file_find(%s, %d)\n", name, type));
    while (1) {
	if (!p) {
	    /* open a new one */
	    p = alloc_filenode();

	    switch (p->type = (short) type) {
	    case F_TRUNC:
		if (!(p->ptr = (PTR) tfopen(name, BinMode2("wb", "w"))))
		    output_failed(name);
		break;

	    case F_APPEND:
		if (!(p->ptr = (PTR) tfopen(name, BinMode2("ab", "a"))))
		    output_failed(name);
		break;

	    case F_IN:
		if (!(p->ptr = (PTR) FINopen(name, 0)))
		    input_failed(name, p);
		break;

	    case PIPE_OUT:
	    case PIPE_IN:

#if    defined(HAVE_REAL_PIPES) || defined(HAVE_FAKE_PIPES)

		if (!(p->ptr = get_pipe(name, type, &p->pid))) {
		    if (type == PIPE_OUT)
			output_failed(name);
		    else
			input_failed(name, p);
		}
#else
		rt_error("pipes not supported");
#endif
		break;

#ifdef	DEBUG
	    default:
		bozo("bad file type");
#endif
	    }
	    /* successful open */
	    p->name = sval;
	    sval->ref_cnt++;
	    break;		/* while loop */
	}

	/* search is by name and type */
	if (strcmp(name, p->name->str) == 0 &&
	    (p->ptr != 0) &&
	    (p->type == type ||
	/* no distinction between F_APPEND and F_TRUNC here */
	     (p->type >= F_APPEND && type >= F_APPEND))) {
	    /* found */
	    if (!q)		/*at front of list */
		return p->ptr;
	    /* delete from list for move to front */
	    q->link = p->link;
	    break;		/* while loop */
	}

	q = p;
	p = p->link;
    }				/* end while loop */

    /* put p at the front of the list */
    p->link = file_list;
    return (PTR) (file_list = p)->ptr;
}

/* Close a file and delete its node from the file_list.
   Walk the whole list, in case a name has two nodes,
   e.g. < "/dev/tty" and > "/dev/tty"
*/

int
file_close(STRING * sval)
{
    FILE_NODE *p;
    FILE_NODE *q = 0;		/* trails p */
    FILE_NODE *hold;
    char *name = sval->str;
    int retval = -1;

    TRACE(("file_close(%s)\n", name));
    p = file_list;
    while (p) {
	if (strcmp(name, p->name->str) == 0) {
	    /* found */

	    /* Remove it from the list first because we might be called
	       again if an error occurs leading to an infinite loop.

	       Note that we don't have to consider the list corruption
	       caused by a recursive call because it will never return. */

	    if (q == 0)
		file_list = p->link;
	    else
		q->link = p->link;

	    switch (p->type) {
	    case F_TRUNC:
	    case F_APPEND:
		if (fclose((FILE *) p->ptr) != 0) {
		    close_error(p);
		}
		retval = 0;
		break;

	    case PIPE_OUT:
		if (fclose((FILE *) p->ptr) != 0) {
		    close_error(p);
		}
#ifdef  HAVE_REAL_PIPES
		retval = wait_for(p->pid);
#endif
#ifdef  HAVE_FAKE_PIPES
		retval = close_fake_outpipe(p->name->str, p->pid);
#endif
		break;

	    case F_IN:
		FINclose((FIN *) p->ptr);
		retval = 0;
		break;

	    case PIPE_IN:
		FINclose((FIN *) p->ptr);

#ifdef  HAVE_REAL_PIPES
		retval = wait_for(p->pid);
#endif
#ifdef  HAVE_FAKE_PIPES
		{
		    char xbuff[100];
		    unlink(tmp_file_name(p->pid, xbuff));
		    retval = p->inpipe_exit;
		}
#endif
		break;
	    }

	    hold = p;
	    p = p->link;
	    free_filenode(hold);
	} else {
	    q = p;
	    p = p->link;
	}
    }

    return retval;
}

/*
find an output file with name == sval and fflush it
*/

int
file_flush(STRING * sval)
{
    int ret = 0;
    FILE_NODE *p = file_list;
    size_t len = sval->len;
    char *str = sval->str;

    if (len == 0) {
	/* for consistency with gawk */
	ret = flush_all_output();
    } else {
	int found = 0;
	while (p) {
	    if (IS_OUTPUT(p->type) &&
		len == p->name->len &&
		strcmp(str, p->name->str) == 0) {
		found = 1;
		if (efflush((FILE *) p->ptr) != 0)
		    ret = -1;
		/* it's possible for a command and a file to have the same
		   name -- so keep looking */
	    }
	    p = p->link;
	}
	if (!found)
	    ret = -1;
    }
    return ret;
}

int
flush_all_output(void)
{
    int ret = 0;
    FILE_NODE *p;

    for (p = file_list; p; p = p->link) {
	if (IS_OUTPUT(p->type)) {
	    if (efflush((FILE *) p->ptr) != 0)
		ret = -1;
	}
    }
    return ret;
}

static int
efflush(FILE *fp)
{
    int ret;

    if ((ret = fflush(fp)) < 0) {
	errmsg(errno, "unexpected write error");
    }
    return ret;
}

/* When we exit, we need to close and wait for all output pipes */

#ifdef   HAVE_REAL_PIPES

/* work around for bug in AIX 4.1 -- If there are exactly 16 or
   32 or 48 ..., open files then the last one doesn't get flushed on
   exit.  So the following is now a misnomer as we'll really close
   all output.
*/

void
close_out_pipes(void)
{
    FILE_NODE *p = file_list;
    FILE_NODE *q = 0;

    while (p) {

	if (IS_OUTPUT(p->type)) {
	    if (fclose((FILE *) p->ptr) != 0) {
		/* if another error occurs we do not want to be called
		   for the same file again */

		if (q != 0)
		    q->link = p->link;
		else
		    file_list = p->link;
		close_error(p);
	    } else if (p->type == PIPE_OUT) {
		wait_for(p->pid);
	    }
	}

	q = p;
	p = p->link;
    }
}

#else
#ifdef  HAVE_FAKE_PIPES		/* pipes are faked with temp files */

void
close_fake_pipes(void)
{
    FILE_NODE *p = file_list;
    char xbuff[100];

    /* close input pipes first to free descriptors for children */
    while (p) {
	if (p->type == PIPE_IN) {
	    FINclose((FIN *) p->ptr);
	    unlink(tmp_file_name(p->pid, xbuff));
	}
	p = p->link;
    }
    /* doit again */
    p = file_list;
    while (p) {
	if (p->type == PIPE_OUT) {
	    if (fclose(p->ptr) != 0) {
		close_error(p);
	    }
	    close_fake_outpipe(p->name->str, p->pid);
	}
	p = p->link;
    }
}
#endif /* HAVE_FAKE_PIPES */
#endif /* ! HAVE_REAL_PIPES */

/* hardwire to /bin/sh for portability of programs */
const char *shell = "/bin/sh";

#ifdef  HAVE_REAL_PIPES

PTR
get_pipe(char *name, int type, int *pid_ptr)
{
    int the_pipe[2], local_fd, remote_fd;

    if (pipe(the_pipe) == -1)
	return (PTR) 0;

    /*
     * If this is an input-pipe then local_fd is reader, remote_fd is writer
     * If this is an output-pipe then local_fd is writer, remote_fd is reader
     */
    local_fd = the_pipe[type == PIPE_OUT];
    remote_fd = the_pipe[type == PIPE_IN];

    /* to keep output ordered correctly */
    fflush(stdout);
    fflush(stderr);

    switch (*pid_ptr = fork()) {
    case -1:
	close(local_fd);
	close(remote_fd);
	return (PTR) 0;

    case 0:
	/*
	 * This is the child process.  Close the unused end of the pipe, i.e,
	 * (local_fd) and then close the input/output file descriptor that
	 * corresponds to the direction we want to read/write.
	 *
	 * The dup() call uses the first unused file descriptor, which happens
	 * to be the one that we just closed, e.g., 0 or 1 for stdin and stdout
	 * respectively.
	 */
	close(local_fd);
	close(type == PIPE_IN);
	IGNORE_RC(dup(remote_fd));
	close(remote_fd);
	execl(shell, shell, "-c", name, (char *) 0);
	errmsg(errno, "failed to exec %s -c \"%s\"", shell, name);
	fflush(stderr);
	_exit(128);

    default:
	close(remote_fd);
	/* we could deadlock if future child inherit the local fd ,
	   set close on exec flag */
	(void) CLOSE_ON_EXEC(local_fd);
	break;
    }

    return ((type == PIPE_IN)
	    ? (PTR) FINdopen(local_fd, 0)
	    : (PTR) fdopen(local_fd, "w"));
}

/*------------ children ------------------*/

/* we need to wait for children at the end of output pipes to
   complete so we know any files they have created are complete */

/* dead children are kept on this list */

static struct child {
    int pid;
    int exit_status;
    struct child *link;
} *child_list;

static void
add_to_child_list(int pid, int exit_status)
{
    struct child *p = ZMALLOC(struct child);

    p->pid = pid;
    p->exit_status = exit_status;
    p->link = child_list;
    child_list = p;
}

static struct child *
remove_from_child_list(int pid)
{
    struct child dummy;
    struct child *p;
    struct child *q = &dummy;

    dummy.link = p = child_list;
    while (p) {
	if (p->pid == pid) {
	    q->link = p->link;
	    break;
	} else {
	    q = p;
	    p = p->link;
	}
    }

    child_list = dummy.link;
    return p;
    /* null return if not in the list */
}

/* wait for a specific child to complete and return its
   exit status

   If pid is zero, wait for any single child and
   put it on the dead children list
*/

int
wait_for(int pid)
{
    int exit_status = 0;
    struct child *p;
    int id;

    TRACE(("wait_for %d\n", pid));
    if (pid == 0) {
	id = wait(&exit_status);
	add_to_child_list(id, exit_status);
    }
    /* see if an earlier wait() caught our child */
    else if ((p = remove_from_child_list(pid))) {
	exit_status = p->exit_status;
	ZFREE(p);
    } else {
	/* need to really wait */
	while ((id = wait(&exit_status)) != pid) {
	    if (id == -1)	/* can't happen */
		bozo("wait_for");
	    else {
		/* we got the exit status of another child
		   put it on the child list and try again */
		add_to_child_list(id, exit_status);
	    }
	}
    }

    return wait_status(exit_status);
}

/*
 * POSIX system() call returns a value from wait(), which should be tested
 * using the POSIX macros.
 */
int
wait_status(int status)
{
    if (status != -1) {
	if (WIFEXITED(status)) {
	    status = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
	    status = 256 + WTERMSIG(status);
	}
    }

    return status;
}

#endif /* HAVE_REAL_PIPES */

/*
 * Create names by which the standard streams can be referenced in a script.
 */
void
set_stdio(void)
{
    FILE_NODE *p, *q, *r;

    /* We insert stderr first to get it at the end of the list. This is
       needed because we want to output errors encountered on closing
       stdout. */

    q = alloc_filenode();
    q->link = (FILE_NODE *) 0;
    q->type = F_TRUNC;
    q->name = new_STRING("/dev/stderr");
    q->ptr = (PTR) stderr;

    p = alloc_filenode();
    p->link = q;
    p->type = F_TRUNC;
    p->name = new_STRING("/dev/stdout");
    p->ptr = (PTR) stdout;

    r = alloc_filenode();
    r->link = p;
    r->type = F_IN;
    r->name = new_STRING("/dev/stdin");
    /* Note: ptr is set in file_find() */

    file_list = r;
}

/* fopen() but no buffering to ttys */
static FILE *
tfopen(const char *name, const char *mode)
{
    FILE *retval = fopen(name, mode);

    if (retval) {
#ifdef HAVE_FSTAT
	struct stat sb;
	int fd = fileno(retval);
	if (fstat(fd, &sb) != -1 && (sb.st_mode & S_IFMT) == S_IFDIR) {
	    fclose(retval);
	    retval = NULL;
	    errno = EISDIR;
	} else
#endif /* HAVE_FSTAT */
	if (isatty(fileno(retval)))
	    setbuf(retval, (char *) 0);
	else {
#ifdef MSDOS
	    enlarge_output_buffer(retval);
#endif
	}
    }
    return retval;
}

#ifdef MSDOS
void
enlarge_output_buffer(FILE *fp)
{
    if (setvbuf(fp, (char *) 0, _IOFBF, BUFFSZ) < 0) {
	errmsg(errno, "setvbuf failed on fileno %d", fileno(fp));
	mawk_exit(2);
    }
}
#endif

#if USE_BINMODE
void
stdout_init(void)
{
#ifdef MSDOS
    if (!isatty(1))
	enlarge_output_buffer(stdout);
#endif
    if (binmode() & 2) {
	setmode(1, O_BINARY);
	setmode(2, O_BINARY);
    }
}
#endif /* USE_BINMODE */

/* An error occurred closing the file referred to by P. We tell the
   user and terminate the program. */

static void
close_error(FILE_NODE * p)
{
    TRACE(("close_error(%s)\n", p->name->str));
    errmsg(errno, "close failed on file \"%s\"", p->name->str);
#ifdef NO_LEAKS
    free_filenode(p);
#endif
    mawk_exit(2);
}

#ifdef NO_LEAKS
void
files_leaks(void)
{
    TRACE(("files_leaks\n"));
    while (file_list != 0) {
	FILE_NODE *p = file_list;
	file_list = p->link;
	free_filenode(p);
    }
}
#endif
