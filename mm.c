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

#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))
static char *last_bp;
static char *heap_listp;
static char *free_listp;
static void *coalesce_with_LIFO(void *bp);
void remove_in_freelist(void *bp);
void put_front_of_freelist(void *bp);
static void *extend_heap(size_t words);
// void *mm_malloc(size_t size);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

/* for explicit*/
#define PREV_FREEP(bp) (*(void **)(bp))
#define NEXT_FREEP(bp) (*(void **)(bp + WSIZE))

/*  mm_init - initialize the malloc package  */
int mm_init(void)
{ /* Create the initial empty heap */

    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void *)-1) // 할당을 못해주면, null 대신 (voi *) -1
        return -1;
    PUT(heap_listp, 0);                                /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE * 2, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), NULL);               /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), NULL);               /* PREV */
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE * 2, 1)); /* NEXT */
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));         /* Epilogue header */
    heap_listp += (2 * WSIZE);
    free_listp = heap_listp;
    last_bp = heap_listp;
    // printf("innniiit \n");

    if (extend_heap(4) == NULL)
        return -1;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) // 헤더 푸터 쓰고나면 데이터 들어갈 곳이 0칸이므로.... 4칸으로 늘려줌
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); // 헤더 푸터 넣어주어야하니까 데이터 넣을 수 있는거 + 2칸 더 넣어주어야 함
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize); // find로 우리가 찾았음... 그 다음에, 찾은 녀석이 size보다 작을거 아님? 그러면
        last_bp = bp;
        return bp; // 찾은 free영역에서 내가 원하는 크기만큼 place시켜줌
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);                 // find 했을 때, 적당한 애를 못찾았을 때, 힙을 확장해줌
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) // 8바이트씩
        return NULL;
    place(bp, asize);
    last_bp = bp;
    return bp;
}
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    // size = words * DSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce_with_LIFO(bp);
}

void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0)); // HDRP(ptr)에 있는 값을 PACK(size,0)으로 바꾸어줌
    PUT(FTRP(ptr), PACK(size, 0)); // FTRP(ptr)에 있는 값을 PACK(size,0)으로 바꾸어줌
    coalesce_with_LIFO(ptr);
}
static void *coalesce_with_LIFO(void *ptr)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
    size_t size = GET_SIZE(HDRP(ptr));

    if (prev_alloc && next_alloc)
    { /* Case 1: 앞뒤로 꽉~ 막혀있을 때 */
        // 1.루트가 가리키고 있는 블럭A 찾기
        ////1.1 만약 free-list 비어있을 때,
        // 2.현재블록의 next는 블록A를 가리킴
        // 3.블럭A의 prev는 현재블럭을 가리킴
        // 4.루트는 현재블럭을 가리킴
        last_bp = ptr;
    }
    else if (prev_alloc && !next_alloc)
    { /* Case 2: 뒤는 막혀있고, 앞(다음)은 비어있을 때 */
        // 00. 직전블럭의 next가, 현재블럭의 next를 가리킴 +(TBD)예외상황 처리해 주어야 할듯
        // 00. 다음블럭의 previous가, 현재블럭이 가리키던 previous를 가리킴
        // coalescing
        remove_in_freelist(NEXT_BLKP(ptr));
        size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));
        // 1. 루트가 가리키고 있는 블럭 A찾아줌
        ////1.1 만약 free-list 비어있을 때,
        // 2. 블럭A의 previous가 coalescing된 블럭을 가리킴
        // 3. coalescing된 블럭의 next가 블럭A를 가리킴
        // 4. coalescing된 블럭의 previous는 어떤 값도 가지지 않음, (0)
        // 5. 루트가 coalescing된 블럭을 가리킴
    }
    else if (!prev_alloc && next_alloc)
    { /* Case 3: 뒤는 비어있고, 앞(다음)은 막혀있을 때 */
        remove_in_freelist(PREV_BLKP(ptr));
        size += GET_SIZE(HDRP(PREV_BLKP(ptr)));
        ptr = PREV_BLKP(ptr);
        PUT(HDRP(ptr), PACK(size, 0));
        PUT(FTRP(ptr), PACK(size, 0));

        // 00. 직전블럭의 next가, 현재블럭의 next를 가리킴
        // 00. 다음블럭의 previous가, 현재블럭이 가리키던 previous를 가리킴
        //
        // 1. 루트가 가리키고 있는 블럭 A찾아줌
        // 2. 블럭A의 previous가 coalescing된 블럭을 가리킴
        // 3. coalescing된 블럭의 next가 블럭A를 가리킴
        // 4. coalescing된 블럭의 previous는 어떤 값도 가지지 않음, (0)
        // 5. 루트가 coalescing된 블럭을 가리킴
    }
    else
    { /* Case 4: 앞뒤 모두 비어있을 때*/
        remove_in_freelist(PREV_BLKP(ptr));
        remove_in_freelist(NEXT_BLKP(ptr));

        size += GET_SIZE(HDRP(PREV_BLKP(ptr))) +
                GET_SIZE(FTRP(NEXT_BLKP(ptr)));
        PUT(HDRP(PREV_BLKP(ptr)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(ptr)), PACK(size, 0));
        ptr = PREV_BLKP(ptr);

        // 00.현재블럭과 맞붙어있는 직전블럭
        //// 00. 직전블럭의 next가, 현재블럭의 next를 가리킴
        //// 00. 다음블럭의 previous가, 현재블럭이 가리키던 previous를 가리킴

        // 00.현재블럭과 맞붙어있는 다음블럭
        //// 00. 직전블럭의 next가, 현재블럭의 next를 가리킴
        //// 00. 다음블럭의 previous가, 현재블럭이 가리키던 previous를 가리킴

        // 1. 루트가 가리키고 있는 블럭 A찾아줌
        // 2. 블럭A의 previous가 coalescing된 블럭을 가리킴
        // 3. coalescing된 블럭의 next가 블럭A를 가리킴
        // 4. coalescing된 블럭의 previous는 어떤 값도 가지지 않음, (0)
        // 5. 루트가 coalescing된 블럭을 가리킴
    }
    last_bp = ptr;
    put_front_of_freelist(ptr);
    return ptr;
}

void remove_in_freelist(void *bp)
{
    // Free list의 첫번째 블록 없앨 때, 당연히 prev도 없을듯
    if (bp == free_listp)
    {
        PREV_FREEP(NEXT_FREEP(bp)) = NULL;
        free_listp = NEXT_FREEP(bp);
    }
    // free list 안에서 없앨 때
    else
    {
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
        PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
    }
}

void put_front_of_freelist(void *bp)
{
    NEXT_FREEP(bp) = free_listp;
    PREV_FREEP(bp) = NULL;
    PREV_FREEP(free_listp) = bp;
    free_listp = bp; // bp가 첫번째 블럭이므로
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
//  */

// 내꺼
static void *find_fit(size_t asize) //
{
    char *bp = last_bp;

    for (bp = NEXT_BLKP(bp); GET_SIZE(HDRP(bp)) != 0; bp = NEXT_BLKP(bp))
    {
        if (GET_ALLOC(HDRP(bp)) == 0 && GET_SIZE(HDRP(bp)) >= asize)
        {
            last_bp = bp;
            return bp;
        }
    }

    bp = heap_listp;
    while (bp < last_bp)
    {
        bp = NEXT_BLKP(bp);

        if (GET_ALLOC(HDRP(bp)) == 0 && GET_SIZE(HDRP(bp)) >= asize)
        {
            last_bp = bp;
            return bp;
        }
    }
    return NULL;
}

// 내꺼
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    remove_in_freelist(bp);
    if ((csize - asize) >= (2 * DSIZE)) // 사이즈 적당히 있어?
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        put_front_of_freelist(bp);
    }
    else // 사이즈 할당하고도 2*DSIZE이하로 남아? 그럼 아무것도 못넣잖아... 그냥 너가 다 먹어라
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

void *mm_realloc(void *ptr, size_t size)
{
    // realloc 을 할 수 없는 경우 밑에 if문 2가지 케이스
    if (size <= 0)
    {
        // 사이즈를 0으로 변경한 것은 free 한것과 동일함. 따라서 free 시켜준다.
        mm_free(ptr);
        return 0;
    }
    if (ptr == NULL)
    {
        // 애초에 크기를 조정할 블록이 존재하지 않았다. 그냥 할당한다 ( malloc )
        return mm_malloc(size);
    }
    // malloc 함수의 리턴값이 newp가 가리키는 주소임.

    void *newp = mm_malloc(size);
    // newp가 가리키는 주소가 NULL(할당되었다고 생각했지만 실패한 경우)
    if (newp == NULL)
    {
        return 0;
    }
    // 진짜 realloc 하는 코드임.
    size_t oldsize = GET_SIZE(HDRP(ptr));
    // ex int a(oldsize) = 10(GET_SIZE(HDRP(ptr));)
    // 그러므로 일단 여기까진 a = 10
    // 재할당이라는 것은 get_size 한 10값을 a(기존데이터주소)가 아닌 b(다른데이터주소)
    // 에 넣는다는 것이다.
    /*
    새로 할당하고자 하는 크기가 oldsize 보다 작으면
    그런데 oldsize가 가진 데이터의 크기가 size의 데이터 크기보다 크기때문에
    커서 전부 다 못들어간다. 그러면은 일단 size 만큼의 데이터만 size에 재할당하고
    나머지 부분(데이터)는 버린다. (가비지데이터)
    */
    if (size < oldsize)
    {
        oldsize = size; // oldsize의 데이터크기를 size 크기만큼 줄이겠다는 뜻.
    }
    // oldsize 데이터를  ptr(기존 주소) 에서 newp(새로 할당될 주소) 로 옮겨준다.
    memcpy(newp, ptr, oldsize);
    mm_free(ptr); // 원래 기존 주소에 있던 데이터를 free 시킨다.
    // 새로 할당된 데이터의 주소를 리턴한다.
    return newp;
}