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
#include <stdint.h>

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

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//############### CODE ###############

static char *heap_listp;    // static 전역 변수 : 항상 프롤로그 블록 가리킴 (힙 리스트 포인터)
static char *cur_bp;
#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (1<<12) // Extend heap by this amount (bytes)

#define MAX(x, y)   ((x) > (y)? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp)    ((char *)(bp) - WSIZE)
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* 
 * mm_init - initialize the malloc package.
 */

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     // 이전 블럭의 alloc은 footer에서
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     // 다음 블럭의 alloc은 header에서
    size_t size = GET_SIZE(HDRP(bp));                       // 현재 블록의 header에서 size 정보 가져옴

    if (prev_alloc && next_alloc) {
        return bp;
    }

    else if (prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));           // HDRP에 바뀐 size 정보 저장 -> FTRP에서 사용  
        PUT(FTRP(bp), PACK(size, 0));           // next block의 footer
    }

    else if (!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));           // 현재 block의 header의 size 정보 이용하므로 순서 바뀌어도 상관 없을 것 같긴 한데 확인 해볼 것
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                     // bp 변경
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
            GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));        
        bp = PREV_BLKP(bp);                     // bp 변경
    }
    cur_bp = bp;       // 생각해보기 !!!!!
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; // size : bytes
    if ((long)(bp = mem_sbrk(size)) == -1)                  // 힙 확장 -> return : bp = old_brk
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));                   // size + alloc

    return coalesce(bp);                                    // 이전 블록이 free라면,
}

int mm_init(void)
{
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue headerxf
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // Epilogue header
    heap_listp += (2*WSIZE);

    cur_bp = heap_listp;

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

static void *find_fit(size_t asize)
{
    // char *bp;

    // bp = heap_listp + (2*WSIZE);   //h1부터 탐색

    // while((GET_SIZE(HDRP(bp)) > 0)) {   // size > 0 이라는 것은, ep는 아니라는 것
    //     if (!GET_ALLOC(HDRP(bp))) {  // 0이라면 (free 라면,)
    //         if (asize <= GET_SIZE(HDRP(bp))) {  // asize 범위 내,
    //             return bp;         
    //         }
    //     }
    //     bp = NEXT_BLKP(bp); // asize 범위 밖 -> 다음 헤더 탐색
    // }
    // return NULL;    // 사이즈가 0보다 크지 않다면 - ep block

    void *bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }

    return NULL;  
}

static void *find_next_fit(size_t asize)
{
    void *bp;

    for(bp = cur_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if ((asize <= GET_SIZE(HDRP(bp))) && !(GET_ALLOC(HDRP(bp)))){
            cur_bp = bp;
            return bp;
        }
        
    }

    for(bp = heap_listp; (char *)bp < cur_bp; bp = NEXT_BLKP(bp)) {
        if ((asize <= GET_SIZE(HDRP(bp))) && !(GET_ALLOC(HDRP(bp)))){
            cur_bp = bp;
            return bp;
        }
    }

    return NULL;
}

static void *find_best_fit(size_t asize)
{
    void *bp;
    void *min_bp = NULL;
    size_t min = SIZE_MAX;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if ((asize <= GET_SIZE(HDRP(bp))) && !(GET_ALLOC(HDRP(bp)))){
            
            if (asize == GET_SIZE(HDRP(bp))) {      // 시간 단축 
                return bp;
            }

            if (min > GET_SIZE(HDRP(bp))) {
                min = GET_SIZE(HDRP(bp));
                min_bp = bp;
            }
        }
    }

    return min_bp;
}

static void place(void *bp, size_t asize) {

    // asize : 새로 할당 받고 싶은 크기
    // csize : 현재 블록 크기

    // size_t csize = GET_SIZE(HDRP(bp));

    // PUT(HDRP(bp), PACK(asize, 1));
    // PUT(FTRP(bp), PACK(asize, 1));

    // bp = NEXT_BLKP(bp);

    // if ((csize - asize) >= (2*DSIZE)) {          // header와 footer의 공간을 확보해야하므로, 8바이트 이상
    //     PUT(HDRP(bp), PACK(csize - asize, 0));
    //     PUT(FTRP(bp), PACK(csize - asize, 0));
    // }

    size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2*DSIZE)) {          // header와 footer의 공간을 확보해야하므로, 8바이트 이상
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
} 

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;
    
    if (size <= DSIZE)
        asize = 2*DSIZE;        // 8 + 4(header) + 4(footer)
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if ((bp = find_best_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;    
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
// 	return NULL;
//     else {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) // ptr = bp
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    copySize = GET_SIZE(HDRP(oldptr)) - DSIZE;  

    // copySize = *(size_t *)((char *)oldptr - WSIZE) - (DSIZE + 1);

    if (size < copySize)
      copySize = size;

    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    
    return newptr;

}