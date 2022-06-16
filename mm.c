/*
 * mm.c
 *
 * Name: Matthew Sites
 * SID: mjs7938
 * Date: 06/15/2022
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"
#include "stree.h"
#include "config.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? 8 bytes for x86 16 bytes for x86-64 */
#define ALIGNMENT 16


/* Global Variables: Only allowed 128 bytes, pointers are 8 bytes each */
void *free_root  = NULL;
void *curr_pos = NULL;


/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x){
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void){

    // Allocate a page (1 GiB)
    if(!allocate_page()){
        print("Initial page allocation failed");
        return false;
    }

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size){

    // add size + header + footer and allign (in bytes)
    int block_size = align(size) + 32;

    // Error check; sbreak failing is checked lower
    if(size == 0){ // size is 0
        print("Malloc Returning NULL: size == 0\n");
        return NULL;
    }

    // Search free list for a block that will fit size

    /* 
    * If you find a block, put it there and add 
    * the remaining bits to the free list.
    */

    /////////////////////////////
    // When there is no blocks 
    // in the free list suitable
    // to fit the block size,
    // add it to the top of heap
    /////////////////////////////

    /* 
    * Increment curr_pos by block_size bytes;
    * Allocate pages if needed;
    * cast curr_pos to char to work in bytes
    */
    void *tmp_pos = (char*)curr_pos + block_size;

    // allocate page if tmp_pos exceeds the current heap size
    while(tmp_pos > mem_heap_hi()){
        if(!allocate_page()){
            return NULL;
        }
    }

    // Set the payload (curr_pos + 16 bytes) to 0
    mem_memset( (char*)curr_pos + 16, 0, size);

    // set the size and valid bit in the header and footer

    /*
    * set the newly allocated bits that arent
    * used in the malloc to the free list
    * OR
    * Maybe dont. If you dont, you can just
    * understand that everything after curr_pos
    * until mm_heap_hi() is free. This might 
    * be a better option.
    */

    curr_pos = tmp_pos;
    return NULL;
}

/*
 * free
 */
void free(void* ptr)
{
    /* IMPLEMENT THIS */
    return;
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
#endif /* DEBUG */
    return true;
}

/*
* Allocates a page 
*/
bool allocate_page(){

    // Allocate a page (1 GiB)
    int mem_brk = mem_sbrk(2e30);

    // Initial allocation failed
    if(mem_brk == NULL || *(int*)mem_brk == -1){
        print("Page allocation failed: heap size %ld/%ld bytes", mem_heap_size() + 2e30, MAX_HEAP_SIZE);
        return false;
    }

    print("Page allocated: heap size %ld/%ld bytes", mem_heap_size(), MAX_HEAP_SIZE);
    return true;
}
