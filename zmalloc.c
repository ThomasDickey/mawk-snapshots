/********************************************
zmalloc.c
copyright 2008-2019,2023, Thomas E. Dickey
copyright 1991-1993,1995, Michael D. Brennan

This is a source file for mawk, an implementation of
the AWK programming language.

Mawk is distributed without warranty under the terms of
the GNU General Public License, version 2, 1991.
********************************************/

/*
 * $MawkId: zmalloc.c,v 1.33 2023/08/04 08:22:23 tom Exp $
 */

/*  zmalloc.c  */
#include  "mawk.h"
#include  "zmalloc.h"

#if defined(NO_LEAKS) && defined(HAVE_TSEARCH)
#define USE_TSEARCH 1
#endif

#ifdef USE_TSEARCH
#include <search.h>
#endif

#define ZSHIFT      3
#define ZBLOCKSZ    BlocksToBytes(1)

#define BytesToBlocks(size) ((((unsigned)size) + ZBLOCKSZ - 1) >> ZSHIFT)
#define BlocksToBytes(size) ((size) << ZSHIFT)

/*
  zmalloc() gets mem from malloc() in CHUNKS of 2048 bytes
  and cuts these blocks into smaller pieces that are multiples
  of eight bytes.  When a piece is returned via zfree(), it goes
  on a linked linear list indexed by its size.	The lists are
  an array, pool[].

  E.g., if you ask for 22 bytes with p = zmalloc(22), you actually get
  a piece of size 24.  When you free it with zfree(p,22) , it is added
  to the list at pool[2].
*/

#define POOLSZ	    16

#define	 CHUNK		256
 /* number of blocks to get from malloc */

/*****************************************************************************/

/*
 * zmalloc() implements a scheme which makes it hard to use memory-checking
 * tools such as valgrind to audit the program to find where there are leaks.
 * That is due to its maintaining (for speed...) pools of memory chunks.
 *
 * Define DEBUG_ZMALLOC to build mawk with a simpler interface to the system
 * malloc, which verifies whether the size-parameter passed to zfree() is
 * consistent with the zmalloc() parameter.
 *
 * The NO_LEAKS code relies upon this simpler interface.
 */

#if !defined(DEBUG_ZMALLOC) && defined(NO_LEAKS)
#define DEBUG_ZMALLOC 1
#endif

#ifdef DEBUG_ZMALLOC
#define IsPoolable(blocks)  0
#define Malloc(n) calloc(1,n)
#else
#define IsPoolable(blocks)  ((blocks) <= POOLSZ)
#define Malloc(n) malloc(n)
#endif

#ifdef USE_TSEARCH

typedef struct {
    PTR ptr;
    size_t size;
} PTR_DATA;

static void *ptr_data;

#if 0
static void
show_tsearch(const void *nodep, const VISIT which, const int depth)
{
    const PTR_DATA *p = *(PTR_DATA * const *) nodep;
    if (which == postorder || which == leaf) {
	TRACE(("show_data %d:%p ->%p %lu\n", depth, p, p->ptr, p->size));
    }
}

static void
show_ptr_data(void)
{
    twalk(ptr_data, show_tsearch);
}

#define ShowPtrData() show_ptr_data()
#else
#define ShowPtrData()		/* nothing */
#endif

static void
free_ptr_data(void *a)
{
    TRACE(("free_ptr_data %p -> %p\n", a, ((PTR_DATA *) (a))->ptr));
    free(a);
}

static int
compare_ptr_data(const void *a, const void *b)
{
    const PTR_DATA *p = (const PTR_DATA *) a;
    const PTR_DATA *q = (const PTR_DATA *) b;
    int result;

    TRACE2(("compare %p->%p %p->%p\n", p, p->ptr, q, q->ptr));
    if (p->ptr > q->ptr) {
	result = 1;
    } else if (p->ptr < q->ptr) {
	result = -1;
    } else {
	result = 0;
    }
    return result;
}

static void
record_ptr(PTR ptr, size_t size)
{
    PTR_DATA *item;
    PTR_DATA **result;

    item = malloc(sizeof(PTR_DATA));
    item->ptr = ptr;
    item->size = size;

    TRACE(("record_ptr %p -> %p %lu\n", (void *) item, ptr, (unsigned long) size));
    result = tsearch(item, &ptr_data, compare_ptr_data);
    assert(result != 0);
    assert(*result != 0);

    TRACE2(("->%p (%p %lu)\n",
	    (*result), (*result)->ptr,
	    (unsigned long) (*result)->size));
    ShowPtrData();
}

static void
finish_ptr(PTR ptr, size_t size)
{
    PTR_DATA dummy;
    PTR_DATA *later;
    PTR_DATA **item;
    void *check;

    dummy.ptr = ptr;
    dummy.size = size;

    TRACE2(("finish_ptr (%p) -> %p %lu\n", &dummy, ptr, (unsigned long) size));
    item = tfind(&dummy, &ptr_data, compare_ptr_data);

    assert(item != 0);
    assert(*item != 0);

    TRACE(("finish_ptr %p -> %p %lu\n",
	   (void *) (*item),
	   (*item)->ptr,
	   (unsigned long) (*item)->size));

    later = *item;
    check = tdelete(&dummy, &ptr_data, compare_ptr_data);
    if (check) {
	free(later);
    }

    ShowPtrData();
}

#define FinishPtr(ptr,size) finish_ptr(ptr,size)
#define RecordPtr(ptr,size) record_ptr(ptr,size)
#else
#define FinishPtr(ptr,size)	/* nothing */
#define RecordPtr(ptr,size)	/* nothing */
#endif

/*****************************************************************************/

static void
out_of_mem(void)
{
    static char out[] = "out of memory";

    if (mawk_state == EXECUTION) {
	rt_error("%s", out);
    } else {
	/* I don't think this will ever happen */
	compile_error("%s", out);
	mawk_exit(2);
    }
}

typedef union zblock {
    char dummy[ZBLOCKSZ];
    union zblock *link;
} ZBLOCK;

/* ZBLOCKS of sizes 1, 2, ... 16
   which is bytes of sizes 8, 16, ... , 128
   are stored on the linked linear lists in
   pool[0], pool[1], ... , pool[15]
*/

static ZBLOCK *pool[POOLSZ];

PTR
zmalloc(size_t size)
{
    unsigned blocks = BytesToBlocks(size);
    size_t bytes = (size_t) BlocksToBytes(blocks);
    register ZBLOCK *p;
    static unsigned amt_avail;
    static ZBLOCK *avail;

    if (!IsPoolable(blocks)) {
	p = (ZBLOCK *) Malloc(bytes);
	if (!p)
	    out_of_mem();
	RecordPtr(p, size);
    } else {

	if ((p = pool[blocks - 1]) != 0) {
	    pool[blocks - 1] = p->link;
	} else {

	    if (blocks > amt_avail) {
		if (amt_avail != 0)	/* free avail */
		{
		    avail->link = pool[--amt_avail];
		    pool[amt_avail] = avail;
		}

		if (!(avail = (ZBLOCK *) Malloc((size_t) (CHUNK * ZBLOCKSZ)))) {
		    /* if we get here, almost out of memory */
		    amt_avail = 0;
		    p = (ZBLOCK *) Malloc(bytes);
		    if (!p)
			out_of_mem();
		    RecordPtr(p, bytes);
		    return (PTR) p;
		} else {
		    RecordPtr(avail, CHUNK * ZBLOCKSZ);
		    amt_avail = CHUNK;
		}
	    }

	    /* get p from the avail pile */
	    p = avail;
	    avail += blocks;
	    amt_avail -= blocks;
	}
    }
    return (PTR) p;
}

void
zfree(PTR p, size_t size)
{
    unsigned blocks = BytesToBlocks(size);

    if (!IsPoolable(blocks)) {
	FinishPtr(p, size);
	free(p);
    } else {
	((ZBLOCK *) p)->link = pool[--blocks];
	pool[blocks] = (ZBLOCK *) p;
    }
}

PTR
zrealloc(PTR p, size_t old_size, size_t new_size)
{
    register PTR q;

    TRACE(("zrealloc %p %lu ->%lu\n",
	   p,
	   (unsigned long) old_size,
	   (unsigned long) new_size));

    if (new_size > (BlocksToBytes(POOLSZ)) &&
	old_size > (BlocksToBytes(POOLSZ))) {
	if (!(q = realloc(p, new_size))) {
	    out_of_mem();
	}
	FinishPtr(p, old_size);
	RecordPtr(q, new_size);
#ifdef DEBUG_ZMALLOC
	if (new_size > old_size) {
	    memset((char *) q + old_size, 0, new_size - old_size);
	}
#endif
    } else {
	q = zmalloc(new_size);
	memcpy(q, p, old_size < new_size ? old_size : new_size);
	zfree(p, old_size);
    }
    return q;
}

#ifdef NO_LEAKS
void
zmalloc_leaks(void)
{
#ifdef USE_TSEARCH
    TRACE(("zmalloc_leaks\n"));
    while (ptr_data != 0) {
	PTR_DATA *data = *(PTR_DATA **) ptr_data;
	tdelete(data, &ptr_data, compare_ptr_data);
	free_ptr_data(data);
    }
#endif
}
#endif
