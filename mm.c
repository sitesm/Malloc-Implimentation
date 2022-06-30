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
 #define DEBUG

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

/* What is the correct alignment?*/
#define ALIGNMENT 16

/* Global Variables: Only allowed 128 bytes, pointers are 8 bytes each */
char *free_root = NULL; // The root of the the free list (points to payload pointer; pred is always NULL)
static char *TOH = NULL; // next free payload pointer of the unallocated heap area

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x){
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void){

    // Initial allocate of 8 words
    char *mem_brk = mem_sbrk(32);

    // Initial allocation failed
    if(mem_brk == NULL || *(int*)mem_brk == -1){
        printf("Initial 16 byte allocation failed\n");
        return false;
    }

    // Set unused blocks
    put(mem_brk, 0);

    // Set prologue block 
    put(mem_brk + 8, pack(16, 1));
    put(mem_brk + 16, pack(16, 1));

    // Set initial epilogue block 
    put(mem_brk + 24 , pack(0, 1));

    // Allocate the first free block
    if(!allocate_page()){
        printf("Initial page allocation failed\n");
        return false;
    }

    return true;
}

/*
 * malloc
 */
void* malloc(size_t size){

    // Local payload_pointer
    char* payload_pointer;
    size_t allocated_size;

    // size + header + footer alligned (in bytes)
    size_t block_size = align(size+16);

    // size = 0 error
    if(size == 0){ 
        printf("Malloc Returning NULL: size == 0\n");
        return NULL;
    }

    // Search free list for a block that will fit size
    if((payload_pointer = find_fit(block_size)) != NULL){
        allocated_size = place(payload_pointer, block_size);
        if(payload_pointer == TOH){
            // update 
            TOH = (allocated_size == block_size) ? TOH + block_size : TOH + allocated_size;
        }

        return payload_pointer;
    }

    /***************************************************
    * When there is no blocks in the free list suitable
    * to fit the block size, add it to the top of heap
    ****************************************************/

    // tmp_pos = how far the block will extend; also next PP
    void *tmp_pos = TOH + block_size; 

    // allocate page if tmp_pos exceeds the current heap size 
    while(tmp_pos >= mem_heap_hi()){
        if(!allocate_page()){
            printf("Page allocation failed during malloc");
            return NULL;
        }
    }

    // place the block at the top of the heap
    allocated_size = place((void*)TOH, block_size);

    // update 
    TOH = (allocated_size == block_size) ? (char*)tmp_pos : (char*)tmp_pos - block_size + allocated_size;

    // return payload location 
    return (TOH - allocated_size);
}

/*
 * free
 */
void free(void* payload_pointer)
{   
    bool isNULL = payload_pointer == NULL;
    bool wasAlloc = get_alloc(GHA(payload_pointer));
    
    // If PP != NULL && PP was allocated, free
    if(!isNULL && wasAlloc){
        size_t size = get_size(GHA(payload_pointer));

        put(GHA(payload_pointer), pack(size, 0));
        put(GFA(payload_pointer), pack(size, 0));

        // Edge case: the block you are trying to free was just allocated at TOH
        if((char*)payload_pointer + size == TOH){
            TOH = coalesce(payload_pointer);
        }else{
            coalesce(payload_pointer); 
        }
    } 
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
* Allocates a page and coalesces to set TOH to the first unused payload pointer of allocated memory
*/
bool allocate_page(){

    // Allocate a page (4096 bytes);
    void *payload_pointer = mem_sbrk(4096); // mem-brk returns a PP in this implimentation

    // Initial allocation failed
    if(payload_pointer == NULL || *(int*)payload_pointer == -1){
        printf("Page allocation failed: heap size %zu/%llu bytes\n", mem_heapsize() + 4096, MAX_HEAP_SIZE);
        return false;
    }

    // Set footer and header blocks for allocated block
    put(GHA(payload_pointer), pack(4096,0)); // Overwrites old epilogue header
    put(GFA(payload_pointer), pack(4096,0));

    // Set new epilogue header
    put(GHA(next_blk(payload_pointer)), pack(0,1));
    
    // Update TOH 
    TOH = coalesce(payload_pointer);

    return true;
}

/*
* Pack: create a value for the header/footer
*/
size_t pack(size_t size, int alloc){
    // Bitwise or size and alloc into one 8 byte number
    return (size | alloc);
}

/*
* GHA: returns the addressof the header via a payload pointer
*/
void *GHA(void *payload_pointer){
    return((char*)payload_pointer - 8);
}

/*
* GFA: returns the address of the footer via a payload pointer
*/
void *GFA(void *payload_pointer){
    return((char*)payload_pointer + get_size(GHA(payload_pointer)) - 16);    
}

/*
* get: returns a word from addr as a size_t
*   - Used in conjuction with get_size & get_alloc
*/
size_t get(void *addr){
    return(*(size_t *)addr);
}

/*
* get_size: gets the size of a header/footer in bytes
*/
size_t get_size(void *addr){
    return(get(addr) & ~0xF); 
}

/*
* get_alloc: returns if the addr block is allocated
*/
int get_alloc(void *addr){
    return(get(addr) & 0x1);
}

/*
* put: puts a header/footer value at addr
*/ 
void put(void* addr, size_t val){
    *(size_t *)addr = val;
}

/*
* prev_fblk: gets the address of the previous free blocks payload pointer
*/
char *prev_blk(void* payload_pointer){
    return((char*)payload_pointer - get_size((char*)payload_pointer - 16));
}

/*
* next_fblk: gets the address of the next free blocks payload pointer
*/
char *next_blk(void* payload_pointer){
    return((char*)payload_pointer + get_size((char*)payload_pointer - 8));
}

/*
* coalesce: merges adjacent free blocks, returns the payload pointer
*/
void* coalesce(void *payload_pointer){
    size_t prev_block = get_alloc(GFA(prev_blk(payload_pointer)));
    size_t next_block = get_alloc(GHA(next_blk(payload_pointer)));
    size_t block_size = get_size(GHA(payload_pointer));

    // Save old information
    void* old_payload_succ;
    void* old_payload_pred;

    // prev and next, allocated 
    if(prev_block && next_block){

        // Update current blocks pred/succ
        put(payload_pointer, PtI(NULL)); // pred
        put((char*)payload_pointer + 8, PtI(free_root)); // succ

        // Update previous FR blocks pred (Edge Case: Initial allocate_page())
        if(free_root != NULL){put(free_root, PtI(payload_pointer));} // pred

        // Update the free root
        free_root = payload_pointer;

        return(payload_pointer);
    }
    // prev allocated, next not allocated
    else if(prev_block && !next_block){

        // Save next blocks payload pointer's old successor and predeseccor
        old_payload_succ = ItP(*(size_t*)(next_blk(payload_pointer) + 8)); // succ
        old_payload_pred = ItP(*(size_t*)next_blk(payload_pointer)); // pred

        block_size += get_size(GHA(next_blk(payload_pointer)));
        put(GHA(payload_pointer), pack(block_size,0));
        put(GFA(payload_pointer), pack(block_size, 0));

        // Update current blocks pred/succ
        put(payload_pointer, PtI(NULL)); // pred
        put((char*)payload_pointer + 8, PtI(free_root)); // succ
        put(free_root, PtI(payload_pointer)); // pred

        if(old_payload_succ == NULL){ 
            put(free_root + 8, PtI(NULL)); // pred       
        }else{
            put(free_root + 8, PtI(old_payload_succ));
            put(old_payload_succ, PtI(free_root));
        }

        // Update the free root
        free_root = payload_pointer;
    }

    // prev not allocated, next allocated (allocate page)
    else if(!prev_block && next_block){
        block_size += get_size(GHA(prev_blk(payload_pointer)));
        put(GFA(payload_pointer), pack(block_size,0));
        put(GHA(prev_blk(payload_pointer)), pack(block_size, 0));
        payload_pointer = prev_blk(payload_pointer);

        // Save payload pointers old successor and predeseccor
        old_payload_succ = ItP(*(size_t*)((char*)payload_pointer + 8)); // succ
        old_payload_pred = ItP(*(size_t*)payload_pointer); // pred

        // Update current blocks pred/succ
        put(payload_pointer, PtI(NULL)); // pred
        if(payload_pointer == free_root) { 
            put((char*)payload_pointer + 8, PtI(old_payload_succ)); // succ
            put(free_root, PtI(NULL)); // pred
        }else{
            put((char*)payload_pointer + 8, PtI(free_root)); // succ
            put(free_root, PtI(payload_pointer));// pred

            if(old_payload_pred != NULL) {put((char*)old_payload_pred + 8, PtI(old_payload_succ));} // succ
            if(old_payload_succ != NULL) {put(old_payload_succ, PtI(old_payload_pred));}
        } 

        // Update the free root
        free_root = payload_pointer;
    }

    // prev and next, not allocated
    else{      
        // Save old pred/succ of prev_block
        void* old_payload_succ_next = ItP(*(size_t*)(next_blk(payload_pointer) + 8)); // succ
        bool next_blk_FR = next_blk(payload_pointer) == free_root;
        void* next_free_block = next_blk(payload_pointer);

        block_size += get_size(GHA(prev_blk(payload_pointer))) + get_size(GFA(next_blk(payload_pointer)));
        put(GHA(prev_blk(payload_pointer)), pack(block_size,0));
        put(GFA(next_blk(payload_pointer)), pack(block_size,0));
        payload_pointer = prev_blk(payload_pointer);

        // Save old pred/succ of prev_block
        old_payload_pred = ItP(*(size_t*)payload_pointer); // pred
        old_payload_succ = ItP(*(size_t*)((char*)payload_pointer + 8)); // succ
        
        // Update curreent blocks pred/succ
        put(payload_pointer, PtI(NULL)); // pred

        if(payload_pointer != free_root && !next_blk_FR) {
            put((char*)payload_pointer + 8, PtI(free_root));// succ
            put(free_root, PtI(payload_pointer)); // pred     
        }else if(next_blk_FR){
            if(old_payload_succ_next == payload_pointer){
                put((char*)payload_pointer + 8, PtI(old_payload_succ));
                put(old_payload_succ, PtI(payload_pointer));
            }else{
                put((char*)payload_pointer + 8, PtI(old_payload_succ_next));// succ
                put(old_payload_succ_next, PtI(payload_pointer));// succ
            }
        }else if(payload_pointer == free_root){
            if(old_payload_succ == next_free_block){
                put((char*)payload_pointer + 8, PtI(old_payload_succ_next));
                if(old_payload_succ_next != NULL){
                    put(old_payload_succ_next, PtI(payload_pointer));
                }
            }
        }

        // if(old_payload_succ_next == payload_pointer) {
        //     put(free_root + 8, PtI(NULL)); // succ
        // }else{
        //     put(free_root + 8, PtI(old_payload_succ_next));
        //     put(old_payload_succ_next, PtI(free_root));
        // }  

        // Update the free root
        free_root = payload_pointer;
    }

    return(payload_pointer);
}

/*
* place: places a block of block_size at payload_pointer most effectivley
*/
size_t place(void* payload_pointer, size_t block_size){

    // Save old information
    size_t old_size = get_size(GHA(payload_pointer));
    size_t remainder = old_size - block_size;

    // Save next blocks payload pointer's old successor and predeseccor
    void* old_payload_succ = ItP(get((char*)payload_pointer + 8));
    void* old_payload_pred = ItP(get(payload_pointer));

    // If the remaining block is going to be smaller than the minimum block size
    if(remainder < 32){
        // set the header and footer of the whole allocated block
        put(GHA(payload_pointer), pack(old_size, 1));
        put(GFA(payload_pointer), pack(old_size, 1)); 

        if(payload_pointer != free_root){
            // Jump over fully allocated free block
            put(old_payload_pred + 8, PtI(old_payload_succ)); // pred
            if(old_payload_succ != NULL){
                put(old_payload_succ, PtI(old_payload_pred)); // succ      
            }
        }else{ // paylaod_pointer == free_root
            // Update free root
            put(old_payload_succ, PtI(NULL)); // pred
            free_root = old_payload_succ;
        }

        return old_size;
    }else{ // Only split if remainder >= 32

        // set the header and footer of the just the allocated block
        put(GHA(payload_pointer), pack(block_size, 1));
        put(GFA(payload_pointer), pack(block_size, 1)); 

        // Set header and footer for un-used bytes 
        put(GHA(next_blk(payload_pointer)), pack(remainder, 0)); 
        put(GFA(next_blk(payload_pointer)), pack(remainder, 0));     

        if(free_root != NULL){ 

            if(payload_pointer != free_root){
                put(next_blk(payload_pointer), PtI(NULL)); // pred
                put(next_blk(payload_pointer) + 8, PtI(free_root)); // succ
                put(free_root, PtI(next_blk(payload_pointer))); // pred
                put((char*)old_payload_pred + 8, old_payload_succ);
                if(old_payload_succ != NULL){ 
                    put(old_payload_succ, PtI(old_payload_pred); // pred
                }
            }else{ // Payload pointer is the free root
                put(next_blk(payload_pointer), PtI(NULL)); // pred
                put(next_blk(payload_pointer) + 8, PtI(old_payload_succ)); // succ
                if(old_payload_succ != NULL){
                    put(old_payload_succ, PtI(next_blk(payload_pointer))); // succ
                }
            }
             
        }else{ // No free root is set yet (99% sure will never happen)
            put(next_blk(payload_pointer), PtI(NULL)); // pred
            put(next_blk(payload_pointer) + 8, PtI(NULL)); // pred
        }

        // Update free root
        free_root = next_blk(payload_pointer);
        
        return block_size;
    }
} 

/*
* find_fit: finds the first fit starting from the free_root
*/
void* find_fit(size_t block_size){

    // Check to see if any blocks have been added to the free list yet
    if(free_root == NULL){
        return NULL;
    }

    // Set the initial successor pointer
    char* succ = free_root;

    while(succ != NULL){
        //get block size
        size_t size = get_size(GHA(succ));

        // check if its large enough
        if(size > block_size){
            return (void*)succ;
        }

        // if not big enough, go to next free block
        succ = ItP(get(succ + 8));
    }

    // no block found
    return NULL;
}

/*
* Pointer to Int (PtI): Converts a pointer value into its integer representation
*                       so they can be used with the "put" function.
*/
size_t PtI(void* pointer){
    return (size_t)pointer;
}

/*
* Int to Pointer: Converts a size_t integer to its address form
*/
void* ItP(size_t ptr_int){
    return (void*)(ptr_int);
}



