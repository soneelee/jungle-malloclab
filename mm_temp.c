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
    "team6",
    /* First member's full name */
    "Songhee Lee",
    /* First member's email address */
    "shine.p715@gmail.com"
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define ALIGNMENT 8

#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7) // 가장 가까운 8의 배수로 올림

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE       4
#define DSIZE       8
#define MINIMUM     16          // Initial Prologue block size, header, footer, PREC, SUCC
#define CHUNKSIZE   (1<<12)
#define LISTLIMIT   

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)     (*(unsigned int*)(p) = (val))

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/*
header, footer 포인터

다음 블럭 포인터 (주소 뒤)
이전 블럭 포인터 (주소 앞)

이전 가용 블럭의 bp를 가리키는 포인터
다음 가용 블럭의 bp를 가리키는 포인터 
*/

#define HDRP(bp)    ((char*)(bp) - WSIZE)                               // char* => 한 칸 씩 이동
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // (char*)(bp) - GET_SIZE(이전 블록의 풋터값)
#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // (char*)(bp) + GET_SIZE(현재 블록의 헤더값)

/* Free List 상에서의 이전, 이후 블록의 포인터를 리턴한다. */
#define PREC_FREEP(bp)  (*(void**)(bp))             // 이전 블록의 bp
#define SUCC_FREEP(bp)  (*(void**)(bp + WSIZE))     // 이후 블록의 bp : bp에 들어있는 값 
// **bp = pointer *bp bp를 따라가면 있는 값이 주소다. 주소를 값으로 갖는 것 = pointer

/* define searching method for find suitable free blocks to allocate*/
// #define NEXT_FIT  // define하면 next_fit, 안 하면 first_fit으로 탐색

/* global variable & functions */
static char* heap_listp = NULL; // 항상 prologue block을 가리키는 정적 전역 변수
static char* free_listp = NULL; // free list의 맨 첫 블록을 가리키는 포인터
static char* seg_listp = NULL;

static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t newsize);

int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

///////////////////////////////// Block information /////////////////////////////////////////////////////////
/*
 
A   : Allocated? (1: true, 0:false)
RA  : Reallocation tag (1: true, 0:false)
 
 < Allocated Block >
 
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |  | A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                                                                                               |
            |                                                                                               |
            .                              Payload and padding                                              .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
 < Free block >
 
             31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Header :   |                              size of the block                                       |  |RA| A|
    bp ---> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its predecessor in Segregated list                          |
bp+WSIZE--> +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |                        pointer to its successor in Segregated list                            |
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            .                                                                                               .
            .                                                                                               .
            .                                                                                               .
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 Footer :   |                              size of the block                                       |     | A|
            +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 
 
*/
///////////////////////////////// End of Block information /////////////////////////////////////////////////////////



//////////////////////////////////////// Helper functions //////////////////////////////////////////////////////////
static void* extend_heap(size_t words) {
    char* bp;
    size_t size;
    
    /* 더블 워드 정렬에 따라 메모리를 mem_sbrk 함수를 이용해 할당받는다. */
    size = (words % 2) ? (words + 1) * WSIZE : (words) * WSIZE; // size를 짝수 word && byte 형태로 만든다.
    if ((long)(bp = mem_sbrk(size)) == -1) // 새 메모리의 첫 부분을 bp로 둔다. 주소값은 int로는 못 받아서 long으로 casting한 듯.
        return NULL;
    
    /* 새 가용 블록의 header와 footer를 정해주고 epilogue block을 가용 블록 맨 끝으로 옮긴다. */
    PUT(HDRP(bp), PACK(size, 0));  // 헤더. 할당 안 해줬으므로 0으로.
    PUT(FTRP(bp), PACK(size, 0));  // 풋터.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));  // 새 에필로그 헤더

    /* 만약 이전 블록이 가용 블록이라면 연결시킨다. */
    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     // 이전 블럭의 alloc은 footer에서
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     // 다음 블럭의 alloc은 header에서
    size_t size = GET_SIZE(HDRP(bp));                       // 현재 블록의 header에서 size 정보 가져옴

   // # Case 1 : 직전, 직후 블록 모두 할당 => 현재 블록만 free list에 넣어준다.

   // # Case 2 : 직전 - 할당, 직후 - free
    if (prev_alloc && !next_alloc){
        remove_freed_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));           // HDRP에 바뀐 size 정보 저장 -> FTRP에서 사용  
        PUT(FTRP(bp), PACK(size, 0));           // next block의 footer
    }

    // # Case 3 : 직전 - free, 직후 - alloc
    else if (!prev_alloc && next_alloc){
        remove_freed_block(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));           // 현재 block의 header의 size 정보 이용하므로 순서 바뀌어도 상관 없을 것 같긴 한데 확인 해볼 것
    }

    // # Case 4 : 직전 - free, 직후 - free
    else if (!prev_alloc && !next_alloc) {
        remove_freed_block(PREV_BLKP(bp));
        remove_freed_block(NEXT_BLKP(bp));
        // size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
        // GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp); 
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    insert_freed_block(bp);

    return bp;
}

void insert_freed_block(void *bp) {
    SUCC_FREEP(bp) = free_listp;    // 새로운 freed block의 succ = 현재 ptr
    PREC_FREEP(bp) = NULL;    // 새로운 freed block의 prec = NULL (가장 처음 삽입)
    PREC_FREEP(free_listp) = bp;    // 현재 freed block의 prec = 새로운 bp
    free_listp = bp;                // free_listp = 새로운 bp
}

void remove_freed_block(void *bp) {
    // free list의 첫번째 블록을 없앨 때
    if (bp == free_listp) {
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
    else {
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);
    }
}

static void *find_fit(size_t asize)
{
    void *bp;

    // asize의 범위 찾기 seg_listp

    // free_listp = seg_listp + WSIZE * N
    
    // 해당 포인터 free_listp 탐색
    for (bp=free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCC_FREEP(bp)) {
        if(asize <= GET_SIZE(HDRP(bp))) {
            return bp;
        }
    }

    return NULL;
}

static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    remove_freed_block(bp);

    if ((csize - asize) >= (2*DSIZE)) {          // header와 footer의 공간을 확보해야하므로, 8바이트 이상
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));

        insert_freed_block(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

}

//////////////////////////////////////// End of Helper functions ////////////////////////////////////////

int mm_init(void)
{   
    if ((heap_listp = mem_sbrk(20*WSIZE)) == (void*)-1)
        return -1;

    PUT(heap_listp, 0);
    
    // seg_free_list
    for(int i=1; i<15; i++) {
      PUT(heap_listp + (i*WSIZE), NULL); // free_list : 2^0
    }

    PUT(heap_listp + (15*WSIZE), PACK(MINIMUM, 1)); // header
    PUT(heap_listp + (16*WSIZE), NULL); // prev
    PUT(heap_listp + (17*WSIZE), NULL); // succ
    PUT(heap_listp + (18*WSIZE), PACK(MINIMUM, 1)); // footer
    PUT(heap_listp + (19*WSIZE), PACK(0, 1)); // ep 

    // ---------- 1 ----------
    
    seg_listp = heap_listp + WSIZE;

    if(extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    // ---------- 2 ----------
    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    asize = ALIGN(size + SIZE_T_SIZE);

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 할당될 수 없다면 크기 확장 후, 할당
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;    
}
