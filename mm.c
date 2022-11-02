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
    "team 9",
    /* First member's full name */
    "Kim Dokyung",
    /* First member's email address */
    "dkkim0122@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define ALIGNMENT 8

// 8의 배수 올림
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// header + footer 메모리 크기 = 8 byte
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// word, double-word 
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount : 4096bytes -> 4kib */

// #####
#define LISTLIMIT 20
#define REALLOC_BUFFER (1<<7)

#define MAX(x, y) ((x) > (y) ? (x) : (y))  // 최댓값 구하는 함수 매크로
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))   

// #####
#define GET(p)          (*(unsigned int*)(p))
#define PUT(p, val)       (*(unsigned int *)(p) = (val) | GET_TAG(p))
#define PUT_NOTAG(p, val) (*(unsigned int *)(p) = (val))

#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

// #####
#define SET_RATAG(p)   (GET(p) |= 0x2)
#define REMOVE_RATAG(p) (GET(p) &= ~0x2)


#define GET_SIZE(p)     (GET(p) & ~0x7) 
#define GET_ALLOC(p)    (GET(p) & 0x1)

// #####
#define GET_TAG(p)   (GET(p) & 0x2)


#define HDRP(bp)    ((char*)(bp) - WSIZE) 
#define FTRP(bp)    ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)


#define NEXT_BLKP(bp)   ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // (char*)(bp) + GET_SIZE(지금 블록의 헤더값)
#define PREV_BLKP(bp)   ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // (char*)(bp) - GET_SIZE(이전 블록의 풋터값)
    
/* Address of free block's predecessor and successor entries */
#define SUCC_PTR(ptr) ((char *)(ptr))
#define PRED_PTR(ptr) ((char *)(ptr) + WSIZE)

/* Address of free block's predecessor and successor on the segregated list */
#define SUCC(ptr) (*(char **)(ptr))
#define PRED(ptr) (*(char **)(PRED_PTR(ptr)))

/*
 * Global variables
 */
void *segregated_free_lists[LISTLIMIT];

/* define searching method for find suitable free blocks to allocate*/
// #define NEXT_FIT  // define하면 next_fit, 안 하면 first_fit으로 탐색

/* global variable & functions */
static void *extend_heap(size_t size);
static void *coalesce(void *ptr);
static void *place(void *ptr, size_t asize);
static void insert_node(void *ptr, size_t size);
static void delete_node(void *ptr);

