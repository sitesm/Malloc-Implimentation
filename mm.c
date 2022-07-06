/*
 * mm.c
 *
 * Name: Matthew Sites
 * SID: mjs7938
 * Date: 06/15/2022
 *
*                                       MALLOC DESIGN DESCRIPTION
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 * In this first implimentation, I decided to create a LIFO explicit free list memory allocator.
 * I decided to go this route so that I could understand the basics of making an explicit free
 * list before I move forward and change to a segregated free list to increase memory utilazation.
 * 
 * In Malloc, the free list is always searched for a suitable block before adding the block to the top
 * of the heap. When searching, this allocator uses a first fit system.
 * 
 * I decided to coalesce the free blocks during calls to free. This way, all free blocks are at the 
 * largest size possible before any attempt is made to allocate to them.
 * 
 * Realloc uses the implimentation of malloc to first create a new memory region, then memcopy the memory 
 * from the old block to the new block, depending on its size.
 * 
 * When no free block is found and the allocator must aquire more memory from the OS, it does so in a page 
 * granularity of 1 MiB. In the final submission, this value will change based on maximal throughput/memory util
 * 
 * Placing blocks in a free block is done through the place function. This function allocatees a block at the given
 * address. In place, it check to see if the remainder is smaller than the minimum block size (32 bytes); if so, it 
 * allocates the whole free block, otherwise it splices the block and places the unused bytes at the begining of the 
 * free list.
 * =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

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

// Prototypes
static bool allocate_page(void);
static size_t pack(size_t size, int alloc);
static void *GHA(void *payload_pointer);
static void *GFA(void *payload_pointer);
static size_t get(void *addr);
static size_t get_size(void *addr);
static int get_alloc(void *addr);
static void put(void* adddr, size_t val);
static char *prev_blk(void* payload_pointer);
static char *next_blk(void* payload_pointer);
static void* coalesce(void *payload_pointer);
static size_t place(void* payload_pointer, size_t block_size);
static void* find_fit(size_t block_size);
static void put_pointer(void* addr, void* pointer);
static size_t PtI(void* pointer);
static void* ItP(size_t ptr_int);
static void* find_first(void* addr1, void* addr2);

/* Global Variables: Only allowed 128 bytes, pointers are 8 bytes each */
char *free_root = NULL; // The root of the the free list (points to payload pointer; INVARIANT: pred is always NULL)
static char *TOH = NULL; // next free payload pointer of the unallocated heap area

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x){
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void){

    // Please tell me this is why it was bugging
    // Reset free_root to null because traces are ran twicee
    free_root = NULL;
    TOH = NULL;

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

    dbg_printf("\nStepping into malloc:\n");
    dbg_printf("----- Before mallocing: ");
    mm_checkheap(__LINE__);

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

        dbg_printf("----- Aafter mallocing: ");
        mm_checkheap(__LINE__);
        dbg_printf("----- Payload pointer : %p\n", payload_pointer);

        return payload_pointer;
    }

    /***************************************************
    * When there is no blocks in the free list suitable
    * to fit the block size, add it to the top of heap
    ****************************************************/

    // tmp_pos = how far the block will extend; also next PP
    void *tmp_pos = TOH + block_size; 

    // allocate page if tmp_pos exceeds the current heap size (Minus the epilogue header) 
    while(tmp_pos > (void*)((char*)mem_heap_hi() - 8)){
        if(!allocate_page()){
            printf("Page allocation failed during malloc");
            return NULL;
        }
    }

    // place the block at the top of the heap
    allocated_size = place((void*)TOH, block_size);

    // update 
    TOH = (allocated_size == block_size) ? (char*)tmp_pos : (char*)tmp_pos - block_size + allocated_size;

    dbg_printf("----- Aafter mallocing:  ");
    mm_checkheap(__LINE__);

    // return payload location 
    return (TOH - allocated_size);
}

/*
 * free
 */
void free(void* payload_pointer)
{    
    dbg_printf("\nStepping into free:\n");
    dbg_printf("----- Freeing: %p\n", payload_pointer);
    mm_checkheap(__LINE__);

    // If PP != NULL && PP was allocated, free
    if(!(payload_pointer == NULL) && get_alloc(GHA(payload_pointer))){
        size_t size = get_size(GHA(payload_pointer));

        put(GHA(payload_pointer), pack(size, 0));
        put(GFA(payload_pointer), pack(size, 0));


        // Edge case: the block you are trying to free was just allocated at TOH
        if((char*)payload_pointer + size == TOH && !get_alloc(GHA(TOH))){
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
    // Pointer to new location
    void* newptr = NULL;

    // "malloc"
    if(oldptr == NULL){
        newptr = malloc(size);
    }

    // "free"
    if(size == 0){
        free(oldptr);
        // NULL will be returned ?? is that correct?
    }

    // Realloc and free
    if(get_alloc(GHA(oldptr))){
        size_t old_size = get_size(GHA(oldptr));

        if(old_size >= size){
            newptr = malloc(size);
            memcpy(newptr, oldptr, size);
        }else{
            newptr = malloc(size);
            memcpy(newptr, oldptr, old_size);
        }

        free(oldptr);
        
    }else{
        return NULL;
    }

    return newptr;
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
    dbg_printf("\nChecking Heap...\n");

    // Vars for checking free list 
    char* next_free = free_root;
    char* pred = NULL;

    // Vars for checking allocated blocks (first block)
    // char* next_allocated = (char*)0x7efff72e7020; 

    // Checks the free list
    while(next_free != NULL){

        // Check free list
        if(!in_heap(next_free)){
            dbg_printf("free root (%p) is not in heap at line %d\n", next_free, lineno);
        }else if(!in_heap(ItP(get(next_free + 8))) && ItP(get(next_free + 8)) != NULL ){
            dbg_printf("succ(free root) (OFR = %p, FR = %p) is not in heap at line %d\n", ItP(get(next_free + 8)), next_free, lineno);
        }

        // // Headers and footers match 
        // if(get(GHA(next_blk)) != get(GFA(next_blk))){
        //     dbg_printf("Header and footer of payload pointer %p do not match", next_blk);
        // }

        // // Contiguious memory escaped coalescing
        // if(!get_alloc(GHA(prev_blk(next_free)))){
        //     dbg_printf("Previous block at %p is free: Contigious memory", prev_blk(next_free));
        // }else if(!get_alloc(GHA(next_blk(next_free)))){
        //     dbg_printf("Next block at %p is free: Contigious memory", next_blk(next_free));
        // }

        // Check each free block is actualy freed
        if(get_alloc(GHA(next_free)) != 0){
            dbg_printf("Check heap: address %p is currently allocated and pointed to by %p\n", next_free, pred);
            return false;
        }

        // go to next free block
        pred = next_free;
        next_free = ItP(get(next_free + 8));   
    }

    // // Check allocated blocks (not needed right now)
    // while(next_allocated != mem_heap_hi() + 1){

    //     // Get the next block
    //     void* next_block = next_blk(next_allocated);

    //     if(get_alloc(GHA(next_block))){
    //         if((char*)next_block - 8 < (next_allocated + get_size(GHA(next_allocated)))){
    //             dbg_printf("Overlapping allocated bytes");
    //         }
    //     }

    //     next_allocated = next_block;
    // }

    dbg_printf("Heap is consistent at line %d\n", lineno);
#endif /* DEBUG */
    return true;
}

/*
* Allocates a page and coalesces to set TOH to the first unused payload pointer of allocated memory
*/
bool allocate_page(){

    // 1 MiB
    size_t page_size = (size_t)pow(2,20);
    // size_t page_size = 4096;

    // Allocate a page (page_size bytes);
    void *payload_pointer = mem_sbrk(page_size); // mem-brk returns a PP in this implimentation

    // Initial allocation failed
    if(payload_pointer == NULL || *(int*)payload_pointer == -1){
        printf("Page allocation failed: heap size %zu/%llu bytes\n", mem_heapsize() + page_size, MAX_HEAP_SIZE);
        return false;
    }

    // Set footer and header blocks for allocated block
    put(GHA(payload_pointer), pack(page_size,0)); // Overwrites old epilogue header
    put(GFA(payload_pointer), pack(page_size,0));

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

    dbg_printf("Stepping into coalesce:");

    size_t prev_block = get_alloc(GFA(prev_blk(payload_pointer)));
    size_t next_block = get_alloc(GHA(next_blk(payload_pointer)));
    size_t block_size = get_size(GHA(payload_pointer));

    // Save old information
    void* old_payload_succ;
    void* old_payload_pred;

    // prev and next, allocated 
    if(prev_block && next_block){

        dbg_printf("Coalesce Block 1\n");
        // Update free list
        if(free_root != NULL){ 
            // Its never possibly for a newly freed block to be the free root
            put(payload_pointer, PtI(NULL)); // pred
            put((char*)payload_pointer + 8, PtI(free_root)); // succ
            put(free_root, PtI(payload_pointer)); // pred       
        }else{ // i.e the first allocate_page()
            put(payload_pointer, PtI(NULL)); // pred
            put((char*)payload_pointer + 8, PtI(NULL)); // pred
        }
   
        // Update the free root
        free_root = payload_pointer;

        mm_checkheap(__LINE__);

        return(payload_pointer);
    }

    // prev allocated, next not allocated
    else if(prev_block && !next_block){

        dbg_printf("Coalesce Block 2\n");

        // Save next blocks payload pointer's old successor and predeseccor
        old_payload_succ = ItP(get(next_blk(payload_pointer) + 8)); // succ
        old_payload_pred = ItP(get(next_blk(payload_pointer))); // pred
        void* old_next_blk = next_blk(payload_pointer);

        // Update block information
        block_size += get_size(GHA(next_blk(payload_pointer)));
        put(GHA(payload_pointer), pack(block_size, 0));
        put(GFA(payload_pointer), pack(block_size, 0));

        // Update linked list
        if(old_next_blk != free_root){
            put(payload_pointer, PtI(NULL)); // pred
            put((char*)payload_pointer + 8, PtI(free_root)); // succ
            put(free_root, PtI(payload_pointer)); // pred
            put((char*)old_payload_pred + 8, PtI(old_payload_succ));
            if(old_payload_succ != NULL){ 
                put(old_payload_succ, PtI(old_payload_pred)); // pred
            }
        }else{
            put(payload_pointer, PtI(NULL)); // pred
            put((char*)payload_pointer + 8, PtI(old_payload_succ)); // succ
            if(old_payload_succ != NULL){ 
                put(old_payload_succ, PtI(payload_pointer)); // pred
            }
        }

        // Update the free root
        free_root = payload_pointer;

        mm_checkheap(__LINE__);
    }

    // prev not allocated, next allocated (allocate page)
    else if(!prev_block && next_block){  

        dbg_printf("Coalesce Block 3\n");

        // Update block information
        block_size += get_size(GHA(prev_blk(payload_pointer)));
        put(GFA(payload_pointer), pack(block_size,0));
        put(GHA(prev_blk(payload_pointer)), pack(block_size, 0));
        payload_pointer = prev_blk(payload_pointer);

        // Save payload pointers old successor and predeseccor
        old_payload_succ = ItP(get(((char*)payload_pointer + 8))); // succ
        old_payload_pred = ItP(get(payload_pointer)); // pred

        // Update linked list
        if(payload_pointer != free_root){
            put(payload_pointer, PtI(NULL)); // pred
            put((char*)payload_pointer + 8, PtI(free_root)); // succ
            put(free_root, PtI(payload_pointer)); // pred
            put((char*)old_payload_pred + 8, PtI(old_payload_succ));
            if(old_payload_succ != NULL){ 
                put(old_payload_succ, PtI(old_payload_pred)); // pred
            }
        }

        // Update the free root
        free_root = payload_pointer;

        mm_checkheap(__LINE__);
    }

    // prev and next, not allocated
    else{   

        dbg_printf("Coalesce Block 4\n");

        // Save old pred/succ of next_block
        void* old_payload_succ_right = ItP(get(next_blk(payload_pointer) + 8)); // succ
        void* old_payload_pred_right = ItP(get(next_blk(payload_pointer))); // pred
        void* right_free_block = next_blk(payload_pointer);

        // Update block information
        block_size += get_size(GHA(prev_blk(payload_pointer))) + get_size(GFA(next_blk(payload_pointer)));
        put(GHA(prev_blk(payload_pointer)), pack(block_size,0));
        put(GFA(next_blk(payload_pointer)), pack(block_size,0));
        payload_pointer = prev_blk(payload_pointer);

        // Save old pred/succ of prev_block
        old_payload_pred = ItP(*(size_t*)payload_pointer); // pred
        old_payload_succ = ItP(*(size_t*)((char*)payload_pointer + 8)); // succ
        
        if(payload_pointer != free_root && right_free_block != free_root) {

            // Update linked list after to preserve order for find_first
            put(payload_pointer, PtI(NULL)); // pred
            put((char*)payload_pointer + 8, PtI(free_root));// succ
            put(free_root, PtI(payload_pointer)); // pred

            // Check if the 2 freed blocks point to each other
            if(old_payload_succ == right_free_block){
                put((char*)old_payload_pred + 8, PtI(old_payload_succ_right));// succ
                if(old_payload_succ_right != NULL){
                    put(old_payload_succ_right, PtI(old_payload_pred));// succ
                } 
            }
            
            else if(old_payload_succ_right == payload_pointer){
                put((char*)old_payload_pred_right + 8, PtI(old_payload_succ));// succ
                if(old_payload_succ != NULL){
                    put(old_payload_succ, PtI(old_payload_pred_right));// succ
                } 
            }

            // Neither block points to the other
            else{ 

                // Update
                put((char*)old_payload_pred + 8, PtI(old_payload_succ));// succ
                if(old_payload_succ != NULL){
                    put(old_payload_succ, PtI(old_payload_pred));// pred
                }  

                put((char*)old_payload_pred_right + 8, PtI(old_payload_succ_right));// succ
                if(old_payload_succ_right != NULL){
                    put(old_payload_succ_right, PtI(old_payload_pred_right));// pred
                }
            }
        }

        else if(right_free_block == free_root){    

            // Check if the free root point to the other left-adjacent free block
            if(old_payload_succ_right == payload_pointer){
                // Update linked list
                put(payload_pointer, PtI(NULL)); // pred
            }
            
            else{ // succ(FR) != left-adjacent free block

                if(ItP(get((char*)old_payload_succ_right + 8)) == payload_pointer){  
                    // Update
                    put(payload_pointer, PtI(NULL)); // pred
                    put((char*)payload_pointer + 8, PtI(old_payload_succ_right));
                    put(old_payload_succ_right, PtI(payload_pointer));

                    put((char*)old_payload_succ_right + 8, PtI(old_payload_succ));
                    if(old_payload_succ != NULL){
                        put((char*)old_payload_succ, PtI(old_payload_succ_right));
                    }
                }else{
                    // Update
                    put(payload_pointer, PtI(NULL)); // pred
                    put((char*)payload_pointer + 8, PtI(old_payload_succ_right));
                    put(old_payload_succ_right, PtI(payload_pointer));

                    put((char*)old_payload_pred + 8, PtI(old_payload_succ));
                    if(old_payload_succ != NULL){
                        put((char*)old_payload_pred, PtI(old_payload_succ));
                    }
                }

                // OLD
                // if(ItP(get((char*)old_payload_succ_right + 8)) == payload_pointer){ 
                //     // Update
                //     put(payload_pointer, PtI(NULL)); // pred
                //     put((char*)payload_pointer + 8, PtI(old_payload_succ_right));
                //     put(old_payload_succ_right, PtI(payload_pointer));
                //     put((char*)old_payload_succ_right + 8, PtI(old_payload_succ));
                //     if(old_payload_succ != NULL){
                //         put((char*)old_payload_succ + 8, PtI(old_payload_succ_right));
                //     }
                // }else{
                //     // Update
                //     put(payload_pointer, PtI(NULL)); // pred
                //     put((char*)payload_pointer + 8, PtI(old_payload_succ_right));
                //     put(old_payload_succ_right, PtI(payload_pointer));
                //     put((char*)old_payload_pred + 8, PtI(old_payload_succ));
                //     if(old_payload_succ != NULL){
                //         put((char*)old_payload_pred + 8, PtI(old_payload_succ));
                //     }
                // }
            }
        }
        
        // Left-adjacent free block is the free root
        else if(payload_pointer == free_root){

            // Check if the free root point to the other right-adjacent free block
            if(old_payload_succ == right_free_block){
                // Update linked list
                put(payload_pointer, PtI(NULL)); // pred
                put((char*)payload_pointer + 8, PtI(old_payload_succ_right)); // pred
                if(old_payload_succ_right != NULL){
                    put(old_payload_succ_right, PtI(payload_pointer)); // pred
                }
            }
            
            else{ // succ(FR) != right-adjacent free block

                if(ItP(get((char*)old_payload_succ + 8)) == right_free_block){  
                    // Update
                    put(payload_pointer, PtI(NULL)); // pred
                    put((char*)payload_pointer + 8, PtI(old_payload_succ)); // succ
                    put(old_payload_succ, PtI(payload_pointer));

                    put((char*)old_payload_succ + 8, PtI(old_payload_succ_right));
                    if(old_payload_succ_right != NULL){
                        put((char*)old_payload_succ_right, PtI(old_payload_succ));
                    }
     
                }else{
                    // Update
                    put(payload_pointer, PtI(NULL)); // pred
                    put((char*)payload_pointer + 8, PtI(old_payload_succ));
                    put(old_payload_succ, PtI(payload_pointer));

                    put((char*)old_payload_pred_right + 8, PtI(old_payload_succ_right));

                    if(old_payload_succ_right != NULL){
                        put((char*)old_payload_succ_right, PtI(old_payload_pred_right));
                    }
                }
            }
        }

        // Update the free root
        free_root = payload_pointer;

        mm_checkheap(__LINE__);
    }

    return(payload_pointer);
}

/*
* place: places a block of block_size at payload_pointer most effectivley
*/
size_t place(void* payload_pointer, size_t block_size){

    dbg_printf("\nStepping into place:\n");

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
            put(old_payload_pred + 8, PtI(old_payload_succ)); // succ
            if(old_payload_succ != NULL){
                put(old_payload_succ, PtI(old_payload_pred)); // pred      
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
                put((char*)old_payload_pred + 8, PtI(old_payload_succ));
                if(old_payload_succ != NULL){ 
                    put(old_payload_succ, PtI(old_payload_pred)); // pred
                }
            }else{ // Payload pointer is the free root
                put(next_blk(payload_pointer), PtI(NULL)); // pred
                put(next_blk(payload_pointer) + 8, PtI(old_payload_succ)); // succ
                if(old_payload_succ != NULL){
                    put(old_payload_succ, PtI(next_blk(payload_pointer))); // pred
                }
            }
             
        }else{ // NOTE: 99% sure will never happen
            put(next_blk(payload_pointer), PtI(NULL)); // pred
            put(next_blk(payload_pointer) + 8, PtI(NULL)); // pred
        }

        // Update free root
        free_root = next_blk(payload_pointer);
        
        mm_checkheap(__LINE__);
        
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

/*
* find_first: finds which address passed in comes first in the free linked list
*/
void* find_first(void* addr1, void* addr2){

    char* succ = free_root;
    // char * pred = NULL;

    while(succ != NULL){

        if(succ == addr1){
            return addr1;
        }else if(succ == addr2){
            return addr2;
        }

        // if succ is not one of the addresses, continue
        succ = ItP(get(succ + 8));
    }

    return NULL;
}

