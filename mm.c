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
    "team6",
    "Songhee Lee",
    "shine.p715@gmail.com",
    "",
    ""
};

/* Basic constants and macros */

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

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
  
/* global variable & functions */
static char *heap_listp;    // static 전역 변수 : 항상 프롤로그 블록 가리킴 (힙 리스트 포인터)
static char *cur_bp;

/* Helper functions */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     // 이전 블럭의 alloc은 footer에서
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     // 다음 블럭의 alloc은 header에서
    size_t size = GET_SIZE(HDRP(bp));                       // 현재 블록의 header에서 size 정보 가져옴

    if (prev_alloc && next_alloc) {                         // #Case1 : 이전 할당, 이후 할당
        return bp;
    }

    else if (prev_alloc && !next_alloc){                    // #Case2 : 이전 할당, 이후 가용
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));                       // HDRP에 바뀐 size 정보 저장 -> FTRP에서 사용  
        PUT(FTRP(bp), PACK(size, 0));                       // next block의 footer
    }

    else if (!prev_alloc && next_alloc){                    // #Case3 : 이전 가용, 이후 할당
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);                                 
    }

    else {                                                  // #Case4 : 이전 가용, 이후 할당
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));  
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));        
        bp = PREV_BLKP(bp);                                 
    }
    cur_bp = bp;       
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; // size : bytes
    if ((long)(bp = mem_sbrk(size)) == -1)                  // 힙 확장 -> return : bp = old_brk
        return NULL;                                        // bp = -1 이면 NULL : brk가 뒤로 이동하거나, 힙 영역 벗어날 경우

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));                   // ep block hdr

    return coalesce(bp);                                    // 힙 확장 후, coalesce
}

static void *find_fit(size_t asize)
{
    void *bp;

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {  // prologue block은 8/1  
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {    // alloc = 0 (free) && asize <= free 크기
            return bp;
        }
    }

    return NULL;  
}

static void *find_next_fit(size_t asize)
{
    void *bp;

    for(bp = cur_bp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if ((asize <= GET_SIZE(HDRP(bp))) && !(GET_ALLOC(HDRP(bp)))) {
            cur_bp = bp;
            return bp;
        }
    }

    for(bp = heap_listp; (char *)bp < cur_bp; bp = NEXT_BLKP(bp)) {
        if ((asize <= GET_SIZE(HDRP(bp))) && !(GET_ALLOC(HDRP(bp)))) {
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

    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {             // 가용 공간 8바이트 확보 header와 footer
        PUT(HDRP(bp), PACK(asize, 1));           
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));          // 남은 가용 리스트가 8바이트 이상이 되지 않으면 header와 footer의 공간이 쓸모 없으므로
        PUT(FTRP(bp), PACK(csize, 1));          // 분할하지 않고 모두 할당
    }
} 
/* End of Helper functions */


/* functions */

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // Create the initial empty heap
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) // heap_listp -> 0
        return -1;
    PUT(heap_listp, 0);                             // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // Prologue header
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // Prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // Epilogue header
    heap_listp += (2*WSIZE);                        // heap_listp -> prologue_block 사이

    cur_bp = heap_listp;

    // Extend the empty heap with a free block of CHUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
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
        return NULL;
    
    if (size <= DSIZE)
        asize = 2*DSIZE;        // 8 = 4(header) + 4(footer)
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); // 8bytes * ((size + 8(header&footer) + 7)/8)


    // find_fit : 가장 처음 가용 리스트 탐색
    // find_next_fit : 이후 리스트에서 가용 리스트 탐색
    // find_best_fit : 최소 크기 가용 리스트 탐색
    if ((bp = find_next_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 필요하다면, asize와 CHUNKSIZE 중 더 큰 값만큼 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    // bp -> find 이후 NULL + extend_heap 이후 NULL이 아닐 때
    // find = NULL : 찾는 위치가 없을 때 + 힙 확장 성공 했을 때
    place(bp, asize);
    return bp;    
}

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
void *mm_realloc(void *oldptr, size_t size)
{
    size_t oldsize, newsize;
    void *newptr, *temp;

    // If size == 0 then this is just free, and we return NULL.
    if(size == 0) 
    {
        mm_free(oldptr);
        return 0;
    }

    // If oldptr is NULL, then this is just malloc.
    if(oldptr == NULL) 
    {
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(oldptr));
    newsize = ALIGN(size + DSIZE);

    // oldsize == newsize, return oldptr
    if (oldsize == newsize) 
    {
        return oldptr;
    } 
    // newsize < oldsize, do not need to alloc new block 
    else if (newsize < oldsize)
    {
        if (oldsize - newsize < 16) 
            return oldptr;
        // oldsize - newsize > 16 -> need to split and coalesce
        PUT(HDRP(oldptr), PACK(newsize, 1));
        PUT(FTRP(oldptr), PACK(newsize, 1));
        
        temp = NEXT_BLKP(oldptr);
        PUT(HDRP(temp), PACK(oldsize - newsize, 0));
        PUT(FTRP(temp), PACK(oldsize - newsize, 0));
        
        cur_bp = coalesce(temp);

        return oldptr;
    } 
    // newsize > oldsize
    else
    {
        temp = NEXT_BLKP(oldptr);
        if ((!GET_ALLOC(HDRP(temp))) && (oldsize + GET_SIZE(HDRP(temp)) >= newsize)) 
        {
            // next block is free and space is enough
            place(temp, newsize - oldsize);
            PUT(HDRP(oldptr), PACK(oldsize + GET_SIZE(HDRP(temp)), 1));
            PUT(FTRP(oldptr), PACK(GET_SIZE(HDRP(oldptr)), 1));
            cur_bp = oldptr;
            return oldptr;
        }
    }

    newptr = mm_malloc(size);

    // If realloc() fails the original block is left untouched
    if(!newptr) 
    {
        return 0;
    }

    // Copy the old data. 
    oldsize = GET_SIZE(HDRP(oldptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    // Free the old block. 
    mm_free(oldptr);

    return newptr;
}