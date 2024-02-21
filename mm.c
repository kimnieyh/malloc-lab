/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/*explicit code*/

#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (1<<12)

#define MAX(x,y)        (x > y ? x : y)
#define GET(p)          (*(unsigned int *) (p))
#define PUT(p,value)    (*(unsigned int *) (p) = (value))
#define PACK(size,alloc)(size | alloc)
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)    
#define NEXT_FREEP(p)   (*(void **) (p))
#define PREV_FREEP(p)   (*(void **) (p + WSIZE))
#define HDRP(p)         ((char *) (p - WSIZE))
#define FTRP(p)         ((char *) p + GET_SIZE(HDRP(p)) - DSIZE)
#define NEXT_BLKP(p)    ((char *) p + GET_SIZE(HDRP(p)))
#define PREV_BLKP(p)    ((char *) p - GET_SIZE((char *) p - DSIZE))

static char *heap_listp;
static char *free_listp;
static void *extend_heap(size_t words);
static void *coalesce(void *ptr);
static void remove_in_freelist(void *bp);
static void put_front_of_freelist(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/*  mm_init - initialize the malloc package  */
int mm_init(void)
{ /* Create the initial empty heap */
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *) -1)
    {
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK(DSIZE*2,1));
    PUT(heap_listp+(2*WSIZE),NULL);
    PUT(heap_listp+(3*WSIZE),NULL);
    PUT(heap_listp+(4*WSIZE),PACK(DSIZE*2,1));
    PUT(heap_listp+(5*WSIZE),PACK(0,1));
    
    heap_listp += DSIZE;
    free_listp = heap_listp;
    if(extend_heap(CHUNKSIZE/DSIZE)== NULL)
    {
        return -1;
    }
    return 0;
}

static void remove_in_freelist(void *bp)
{   
    if(bp == free_listp)
    {
        PREV_FREEP(NEXT_FREEP(bp)) = NULL;
        free_listp = NEXT_FREEP(bp);
    }
    else
    {
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
        PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
    }
    
}

static void insert_in_freelist(void *bp)
{
    NEXT_FREEP(bp) = free_listp;
    PREV_FREEP(bp) = NULL;
    PREV_FREEP(free_listp) = bp;
    free_listp = bp;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
   size_t asize;
   size_t extendsize;
   char *bp;

   if (size == 0)
   {
        return NULL;
   }

   if (size <= DSIZE)
   {
        asize = DSIZE *2;
   }
   else 
   {
        asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE );
   }

   if ((bp = find_fit(asize)) != NULL) {
        place(bp,asize);
        return bp;
   }

    extendsize = MAX(CHUNKSIZE,asize);

    if ((bp = extend_heap(extendsize/DSIZE)) == NULL)
    {
        return NULL;
    }

    place(bp,asize);
    return bp;
}
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    size =  words * DSIZE;
    
    if ((bp = mem_sbrk(size) ) == (void*) -1)
    {
        return NULL;
    }
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    return coalesce(bp);
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    return coalesce(ptr);
}
static void *coalesce(void *ptr)
{
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (next_alloc&& prev_alloc){

    }
    else if (!next_alloc && prev_alloc)
    {
        remove_in_freelist(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr),PACK(size,0));
        PUT(FTRP(ptr),PACK(size,0));

    }else if (next_alloc && !prev_alloc)
    {
        remove_in_freelist(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        ptr = PREV_BLKP(ptr);
        PUT(HDRP(ptr),PACK(size,0));
        PUT(FTRP(ptr),PACK(size,0));
    }else
    {
        remove_in_freelist(PREV_BLKP(ptr));
        remove_in_freelist(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);
    }
    insert_in_freelist(ptr);

    return ptr;
}

static void *find_fit(size_t asize)
{
   char *bp;
   bp = free_listp;

   for (bp; GET_ALLOC(HDRP(bp)) != 1; bp = NEXT_FREEP(bp))
   {    
        if (GET_SIZE(HDRP(bp)) >= asize)
        {
            return bp;
        }    
   }
   return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_in_freelist(bp);  
    if ((csize-asize) >= (DSIZE*2))
    {
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK((csize-asize),0));
        PUT(FTRP(bp),PACK((csize-asize),0));
        insert_in_freelist(bp);
    }else
    {
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
         
    }
}

void *mm_realloc(void *ptr, size_t size)
{
    if(size <= 0)
    {
        mm_free(ptr);
        return 0;
    }

    if(ptr == NULL)
    {
        return mm_malloc(size);
    }

    void *newp = mm_malloc(size);

    if(newp ==NULL)
    {
        return 0;
    }
    size_t oldsize = GET_SIZE(HDRP(ptr));

    if(size < oldsize)
    {
        oldsize = size;
    }

    memcpy(newp,ptr,oldsize);
    mm_free(ptr);

    return newp;
}