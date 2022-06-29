#include <stdio.h>
#include <stdbool.h>

#ifdef DRIVER

/* declare functions for driver tests */
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);
extern void *mm_calloc (size_t nmemb, size_t size);

#else
//trigger update
/* declare functions for interpositioning */
extern void *malloc (size_t size);
extern void free (void *ptr);
extern void *realloc(void *ptr, size_t size);
extern void *calloc (size_t nmemb, size_t size);

#endif

extern bool mm_init(void);

/* This is for debugging.  Returns false if error encountered */
extern bool mm_checkheap(int lineno);

// Helper Functions
extern bool allocate_page(void);
extern size_t pack(size_t size, int alloc);
extern void *GHA(void *payload_pointer);
extern void *GFA(void *payload_pointer);
extern size_t get(void *addr);
extern size_t get_size(void *addr);
extern int get_alloc(void *addr);
extern void put(void* adddr, size_t val);
extern char *prev_blk(void* payload_pointer);
extern char *next_blk(void* payload_pointer);
extern void* coalesce(void *payload_pointer);
extern size_t place(void* payload_pointer, size_t block_size);
extern void* find_fit(size_t block_size);
