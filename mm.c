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
    ""
};
/*기본 상수*/
#define WSIZE   4
#define DSIZE   8
#define MINIMUM 16
#define CHUNKSIZE (1<<12) //4096

/*매크로*/
#define MAX(x,y)            ((x) > (y) ? (x):(y))
#define ALIGNMENT 8
#define ALIGN(size)         (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE         (ALIGN(sizeof(size_t)))
#define PACK(size,alloc)    ((size) | (alloc)) // 사이즈와 할당 비트 결합
#define GET(p)              (*(unsigned int *)(p)) //p가 참조하는 워드 반환(포인터라서 역참조 불가능)-> 타입 캐스팅
#define PUT(p,val)          (*(unsigned int *)(p) = (val)) //p에  val 저장

#define GET_SIZE(p)         (GET(p) & ~0x7) //사이즈  뒤에 3개는 빼고! 0000111
#define GET_ALLOC(p)        (GET(p) & 0x1) // 할당 상태
#define HDRP(bp)            ((char *)(bp)-WSIZE) //헤더 포인터
#define FTRP(bp)            ((char *)(bp)+ GET_SIZE(HDRP(bp)) -DSIZE) //푸터 포인터 (헤더의 정보를 참조함, 헤더 정보 변경 시 footer도 변경됨)
//다음블록의 포인터
#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
//이전블록의 포인터
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))
#define GET_NEXT(bp)        (*(void **)((bp) + WSIZE)) // 다음 가용 블록의 주소
#define GET_PREV(bp)        (*(void **)(bp))                  // 이전 가용 블록의 주소

static char *heap_listp = NULL; // prologue 블록을 가리킴
static char *free_listp = NULL; //free list 첫 블록 가리킴
static void free_insert(void *bp);
static void free_delete(void *bp);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t a_size);
static void place(void *bp, size_t a_size);

/*
________________________________________
|     |      |      |      |      |     |
|  0  | 8/1  | NULL | NULL | 8/1  | 0/1 |
|     |header| prev | next |footer|     |
-----------------------------------------
*/

//힙 생성
int mm_init(void)
{
    //초기 힙 생성
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
        return -1;
    //패딩값 더블 워드 경계로 정렬된 미사용 패딩
    PUT(heap_listp, 0); //alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(MINIMUM, 1));
    PUT(heap_listp + (2*WSIZE), NULL); // prec
    PUT(heap_listp + (3*WSIZE), NULL); // succ
    PUT(heap_listp + (4*WSIZE), PACK(MINIMUM, 1));
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));

    free_listp = heap_listp + 2*WSIZE;

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}
 void *mm_malloc(size_t size)
{   
    //조정된 사이즈
    size_t asize;
    //확장할 사이즈
    size_t extendsize;
    char *bp;
    
    //잘못된 요청
    if(size == 0){
        return NULL;
    }
    
    asize = ALIGN(size + SIZE_T_SIZE);
    //가용 블록 검색
    if((bp = find_fit(asize))!= NULL){
        //할당
        place(bp,asize);
        return bp;
    }
    // 적합한 블록이 없는 경우 힙 확장, asize 별로 분기 처리 
    extendsize = MAX(asize,CHUNKSIZE);
    
    if ((bp = extend_heap(extendsize/WSIZE))==NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
}
//가용 블록 검색 first fit 
static void *find_fit(size_t asize){
    void *bp;
    for (bp = free_listp; GET_ALLOC(HDRP(bp))!= 1; bp = GET_NEXT(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    free_delete(bp);
    
    if((csize - asize) >= MINIMUM){ 
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize-asize,0));
        PUT(FTRP(bp),PACK(csize-asize,0));
        free_insert(bp);
    }else{
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
    }
}
static void free_insert(void *bp){
    GET_NEXT(bp) = free_listp;
    GET_PREV(bp) = NULL;
    GET_PREV(free_listp) = bp;
    free_listp = bp;
}

static void free_delete(void *bp){
    if(bp == free_listp) {
        GET_PREV(GET_NEXT(bp)) = NULL;
        free_listp = GET_NEXT(bp);
    }else {
        GET_NEXT(GET_PREV(bp)) = GET_NEXT(bp);
        GET_PREV(GET_NEXT(bp)) = GET_PREV(bp);
    }    
} 

static void* extend_heap(size_t words) {
    // 워드 단위로 받는다.
    char* bp;
    size_t size;

    /* 더블 워드 정렬에 따라 메모리를 mem_sbrk 함수를 이용해 할당받음. mem_sbrk는 힙 용량을 늘려줌.*/ 
    size = (words % 2) ? (words + 1) * WSIZE : (words) * WSIZE; // size를 짝수 word && byte 형태로 만든다.
    if ((long)(bp = mem_sbrk(size)) == -1) {// 새 메모리의 첫 부분을 bp로 둔다. 주소값은 int로는 못 받으니 long으로 type casting
    return NULL;
    }
    /* 새 가용 블록의 header와 footer를 정해주고 epliogue block을 가용 블록 맨 끝으로 옮긴다. */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새 에필로그 헤더

    /* 만약 이전 블록이 가용 블록이라면 연결 */
    return coalesce(bp);
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    coalesce(bp);
}

//병합 하기 
static void *coalesce(void *bp){
    //직전 블록의 footer, 직후 블록의 header를 보고 가용 여부를 확인.
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 직전 블록 가용 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 직후 블록 가용 여부 확인
    size_t size = GET_SIZE(HDRP(bp));


    // case 1: 직전, 직후 블록이 모두 할당 -> 해당 블록만 free list에 넣어준다.

    // case 2: 직전 블록 할당, 직후 블록 가용

    if (prev_alloc && !next_alloc) {
        free_delete(NEXT_BLKP(bp)); // free 상태였던 직후 블록을 free list에서 제거
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 3: 직전 블록 가용, 직후 블록 할당
    else if (!prev_alloc && next_alloc){ // alloc이 0이니까  !prev_alloc == True.
        free_delete(PREV_BLKP(bp)); // 직전 블록을 free list에서 제거
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 4: 직전, 직후 블록 모두 가용
    else if (!prev_alloc && !next_alloc) {
        free_delete(PREV_BLKP(bp));
        free_delete(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 앞뒤 free된 블록 전체 사이즈 계산
        bp = PREV_BLKP(bp); // 현재 bp를 가용인 앞쪽 블록으로 이동시켜 => 거기가 전체 free 블록의 앞쪽이 되어야 함.
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // 연결된 새 가용 블록을 free list에 추가
    free_insert(bp);

    return bp;
}


// 재 할당하기
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL) //포인터가 null이면 할당만 수행
        return mm_malloc(size);
    
    if(size == 0) //사이즈가 0이면 메모리 반환만 수행
    {
        mm_free(ptr);
        return 0;
    }
    // 새 블록에 할당
    void *newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    
    //데이터 복사 
    size_t copySize = GET_SIZE(HDRP(ptr)) - DSIZE; //payload만큼만 복사
    if (size < copySize)
        copySize = size;

    memcpy(newptr,ptr,copySize);

    mm_free(ptr);
    return newptr;
}