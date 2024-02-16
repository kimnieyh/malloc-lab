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
#define CHUNKSIZE (1<<12) //4096

/*매크로*/
#define MAX(x,y)            ((x) > (y) ? (x):(y))
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

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t a_size);
static void place(void *bp, size_t a_size);

//힙 생성
int mm_init(void)
{
    //초기 힙 생성
    char *heap_listp;
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    //패딩값
    PUT(heap_listp,0);
    //프롤로그 헤더
    PUT(heap_listp + (1*WSIZE),PACK(DSIZE,1));
    //프롤로그 푸터
    PUT(heap_listp + (2*WSIZE),PACK(DSIZE,1));
    //에필로그 헤더
    PUT(heap_listp + (3*WSIZE),PACK(0,1));
    heap_listp += (2*WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
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
    //사이즈 조정 8보다 작으면 최소 블록 크기인 16바이트 할당
    if(size <= DSIZE){
        asize = 2*DSIZE;
    }else{
        //8의 배수로 올림처리
        asize = DSIZE * ((size + DSIZE + DSIZE - 1)/DSIZE);
    }
    //가용 블록 검색
    if((bp = find_fit(asize))!= NULL){
        //할당
        place(bp,asize);
        return bp;
    }
    // 적합한 블록이 없는 경우 힙 확장
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE))==NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
}
//가용 블록 검색 first fit 
static void *find_fit(size_t asize){
    // 첫번째 블록의 주소
    void *bp = mem_heap_lo() + 2 * WSIZE;
    while (GET_SIZE(HDRP(bp))>0)
    {   
        //가용 상태이며, 사이즈가 적합하다면 ? 
        if(!GET_ALLOC(HDRP(bp))&&(asize <= GET_SIZE(HDRP(bp))))
            return bp;
        bp = NEXT_BLKP(bp);
    }
    return NULL;
}

//최소 블록 크기는 16바이트 (헤더,푸터 4바이트 씩이고, 주소는 8의 배수여야 되기 때문)
//할당 
static void place(void *bp, size_t asize){

    size_t csize = GET_SIZE(HDRP(bp));
    //printf("내 주소~ %p\n",bp);
    if ((csize-asize) >= (2*DSIZE)){
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        void *next_bp = NEXT_BLKP(bp);
        //printf("내 다음 주소 ~ 잘리는 곳%p\n",bp);
        PUT(HDRP(next_bp),PACK(csize-asize,0));
        PUT(FTRP(next_bp),PACK(csize-asize,0));
    }else{
        PUT(HDRP(bp),PACK(csize,1));
        PUT(FTRP(bp),PACK(csize,1));
    }
}

//힙 확장
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    // 새 빈 블록 헤더 초기화
    PUT(HDRP(bp),PACK(size,0));
    // 새 빈 블록 푸터 초기화
    PUT(FTRP(bp),PACK(size,0));
    // 에필로그 블록 헤더 초기화
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

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
    // 이전 블록의 할당 상태
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    // 다음 블록의 할당 상태
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 블록 사이즈 
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) { // 모두 할당 된 경우
        return bp;
    }else if(prev_alloc && !next_alloc){ //다음 블록만 빈 경우
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }else if(!prev_alloc && next_alloc){ // 이전 빈 경우
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
        bp = PREV_BLKP(bp);
    }else{ // 이전, 다음 모두 빈 경우
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;

}

// 재 할당하기
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL) //포인터가 null이면 할당만 수행
        return mm_malloc(size);
    
    if(size<= 0) //사이즈가 0이면 메모리 반환만 수행
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














