#include "mm.h"      // prototypes of functions implemented in this file

#include "memlib.h"  // mem_sbrk -- to extend the heap
#include <string.h>  // memcpy -- to copy regions of memory

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) > (y) ? (y) : (x))

/**
 * A block header uses 4 bytes for:
 * - a block size, multiple of 8 (so, the last 3 bits are always 0's)
 * - an allocated bit (stored as LSB, since the last 3 bits are needed)
 *
 * A block footer has the same format.
 * Check Figure 9.48(a) in the textbook.
 */
typedef int BlockHeader;

/**
 * Read the size field from a block header (or footer).
 *
 * @param bp address of the block header (or footer)
 * @return size in bytes
 */
static int get_size(BlockHeader *bp) {
    return (*bp) & ~7;  // discard last 3 bits
}

/**
 * Read the allocated bit from a block header (or footer).
 *
 * @param bp address of the block header (or footer)
 * @return allocated bit (either 0 or 1)
 */
static int get_allocated(BlockHeader *bp) {
    return (*bp) & 1;   // get last bit
}

/**
 * Write the size and allocated bit of a given block inside its header.
 *
 * @param bp address of the block header
 * @param size size in bytes (must be a multiple of 8)
 * @param allocated either 0 or 1
 */
static void set_header(BlockHeader *bp, int size, int allocated) {
    *bp = size | allocated;
}

/**
 * Write the size and allocated bit of a given block inside its footer.
 *
 * @param bp address of the block header
 * @param size size in bytes (must be a multiple of 8)
 * @param allocated either 0 or 1
 */
static void set_footer(BlockHeader *bp, int size, int allocated) {
    char *footer_addr = (char *)bp + get_size(bp) - 4;
    // the footer has the same format as the header
    set_header((BlockHeader *)footer_addr, size, allocated);
}

/**
 * Find the payload starting address given the address of a block header.
 *
 * The block header is 4 bytes, so the payload starts after 4 bytes.
 *
 * @param bp address of the block header
 * @return address of the payload for this block
 */
static char *get_payload_addr(BlockHeader *bp) {
    return (char *)(bp + 1);
}

/**
 * Find the header address of the previous block on the heap.
 *
 * @param bp address of a block header
 * @return address of the header of the previous block
 */
static BlockHeader *get_prev(BlockHeader *bp) {
    // move back by 4 bytes to find the footer of the previous block
    BlockHeader *previous_footer = bp - 1;
    int previous_size = get_size(previous_footer);
    char *previous_addr = (char *)bp - previous_size;
    return (BlockHeader *)previous_addr;
}

/**
 * Find the header address of the next block on the heap.
 *
 * @param bp address of a block header
 * @return address of the header of the next block
 */
static BlockHeader *get_next(BlockHeader *bp) {
    int this_size = get_size(bp);
    char *next_addr = (char *)bp + this_size;  // TODO: to implement, look at get_prev
    return (BlockHeader *)next_addr;
}

/**
 * In addition to the block header with size/allocated bit, a free block has
 * pointers to the headers of the previous and next blocks on the free list.
 *
 * Pointers use 4 bytes because this project is compiled with -m32.
 * Check Figure 9.48(b) in the textbook.
 */
typedef struct {
    BlockHeader header;
    BlockHeader *prev_free;
    BlockHeader *next_free;
} FreeBlockHeader;

/**
 * Find the header address of the previous **free** block on the **free list**.
 *
 * @param bp address of a block header (it must be a free block)
 * @return address of the header of the previous free block on the list
 */
static BlockHeader *get_prev_free(BlockHeader *bp) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    return fp->prev_free;
}

/**
 * Find the header address of the next **free** block on the **free list**.
 *
 * @param bp address of a block header (it must be a free block)
 * @return address of the header of the next free block on the list
 */
static BlockHeader *get_next_free(BlockHeader *bp) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    return fp->next_free;
}

/**
 * Set the pointer to the previous **free** block.
 *
 * @param bp address of a free block header
 * @param prev address of the header of the previous free block (to be set)
 */
static void set_prev_free(BlockHeader *bp, BlockHeader *prev) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    fp->prev_free = prev;
}

/**
 * Set the pointer to the next **free** block.
 *
 * @param bp address of a free block header
 * @param next address of the header of the next free block (to be set)
 */
static void set_next_free(BlockHeader *bp, BlockHeader *next) {
    FreeBlockHeader *fp = (FreeBlockHeader *)bp;
    fp->next_free = next;
}

/* Pointer to the header of the first block on the heap */
static BlockHeader *heap_blocks;

/* Pointers to the headers of the first and last blocks on the free list */
static BlockHeader *free_headp;
static BlockHeader *free_tailp;

/**
 * Add a block at the beginning of the free list.
 *
 * @param bp address of the header of the block to add
 */
static void free_list_prepend(BlockHeader *bp) {
    // TODO: implement
    // Case when the free list is empty
    if(free_headp == NULL && free_tailp == NULL) {
        free_headp = bp;
        free_tailp = bp;
        set_prev_free(bp, NULL);
        set_next_free(bp, NULL);
    }
    else {
        set_prev_free(free_headp, bp);
        set_prev_free(bp, NULL);
        set_next_free(bp, free_headp);
        // Update the head
        free_headp = bp;
    }
}

/**
 * Add a block at the end of the free list.
 *
 * @param bp address of the header of the block to add
 */
static void free_list_append(BlockHeader *bp) {
    // TODO: implement
    // Case when the free list is empty
    if(free_headp == NULL && free_tailp == NULL) {
        free_headp = bp;
        free_tailp = bp;
        set_prev_free(bp, NULL);
        set_next_free(bp, NULL);
    }
    else {
        set_next_free(free_tailp, bp);
        set_next_free(bp, NULL);
        set_prev_free(bp, free_tailp);
        // Update the tail
        free_tailp = bp;
    }
}

/**
 * Remove a block from the free list.
 *
 * @param bp address of the header of the block to remove
 */
static void free_list_remove(BlockHeader *bp) {
    // TODO: implement
    // It is the only block in the free list
    if(get_prev_free(bp) == NULL && get_next_free(bp) == NULL) {
        free_headp = NULL;
        free_tailp = NULL;
    }
    // It is the head of the free list
    else if(get_prev_free(bp) == NULL) {
        BlockHeader* np = get_next_free(bp);
        set_prev_free(np, NULL);
        free_headp = np;
    }
    // It is the tail of the free list
    else if(get_next_free(bp) == NULL) {
        BlockHeader* pp = get_prev_free(bp);
        set_next_free(pp, NULL);
        free_tailp = pp;
    }
    // It is in the middle of the free list
    else {
        BlockHeader* pp = get_prev_free(bp);
        BlockHeader* np = get_next_free(bp);
        set_next_free(pp, np);
        set_prev_free(np, pp);
    }
}

int init_size = 1 << 10; // Init size for heap
int divide = 1 << 7; // Size to decide prepend or append
int order = 100; // Size to decide allocate head or tail
/**
 * Mark a block as free, coalesce with contiguous free blocks on the heap, add
 * the coalesced block to the free list.
 *
 * @param bp address of the block to mark as free
 * @return the address of the coalesced block
 */
static BlockHeader *free_coalesce(BlockHeader *bp) {

    // mark block as free
    int size = get_size(bp);
    set_header(bp, size, 0);
    set_footer(bp, size, 0);

    // check whether contiguous blocks are allocated
    int prev_alloc = get_allocated(get_prev(bp));
    int next_alloc = get_allocated(get_next(bp));

    if (prev_alloc && next_alloc) {
        // TODO: add bp to free list
        if(size >= divide) free_list_append(bp);
        else free_list_prepend(bp);
        return bp;
    } else if (prev_alloc && !next_alloc) {
        // TODO: remove next block from free list
        free_list_remove(get_next(bp));
        // TODO: add bp to free list
        int nsize = get_size(get_next(bp));
        if(size + nsize >= divide) free_list_append(bp);
        else free_list_prepend(bp);
        // TODO: coalesce with next block
        set_header(bp, size + nsize, 0);
        set_footer(bp, size + nsize, 0);
        return bp;
    } else if (!prev_alloc && next_alloc) {
        // TODO: coalesce with previous block
        BlockHeader *prev = get_prev(bp);
        int psize = get_size(prev);
        set_header(prev, size + psize, 0);
        set_footer(prev, size + psize, 0);
        return prev;
    } else {
        // TODO: remove next block from free list
        BlockHeader *prev = get_prev(bp);
        BlockHeader *next = get_next(bp);
        free_list_remove(next);
        // TODO: coalesce with previous and next block
        int psize = get_size(prev);
        int nsize = get_size(next);
        set_header(prev, size + psize + nsize, 0);
        set_footer(prev, size + psize + nsize, 0);
        return prev;
    }
}

/**
 * Extend the heap with a free block of `size` bytes (multiple of 8).
 *
 * @param size number of bytes to allocate (a multiple of 8)
 * @return pointer to the header of the new free block
 */
static BlockHeader *extend_heap(int size) {

    // bp points to the beginning of the new block
    char *bp = mem_sbrk(size);
    if ((long)bp == -1)
        return NULL;

    // write header over old epilogue, then the footer
    BlockHeader *old_epilogue = (BlockHeader *)bp - 1;
    set_header(old_epilogue, size, 0);
    set_footer(old_epilogue, size, 0);

    // write new epilogue
    set_header(get_next(old_epilogue), 0, 1);

    // merge new block with previous one if possible
    return free_coalesce(old_epilogue);
}

int mm_init(void) {
    // init list of free blocks
    free_headp = NULL;
    free_tailp = NULL;

    // create empty heap of 4 x 4-byte words
    char *new_region = mem_sbrk(16);
    if ((long)new_region == -1)
        return -1;

    heap_blocks = (BlockHeader *)new_region;
    set_header(heap_blocks, 0, 0);      // skip 4 bytes for alignment
    set_header(heap_blocks + 1, 8, 1);  // allocate a block of 8 bytes as prologue
    set_footer(heap_blocks + 1, 8, 1);
    set_header(heap_blocks + 3, 0, 1);  // epilogue
    heap_blocks += 1;                   // point to the prologue header

    // TODO: extend heap with an initial heap size
    extend_heap(init_size);

    return 0;
}

void mm_free(void *bp) {
    // TODO: move back 4 bytes to find the block header, then free block
    BlockHeader *header = (BlockHeader *)bp - 1;
    free_coalesce(header);
}

/**
 * Find a free block with size greater or equal to `size`.
 *
 * @param size minimum size of the free block
 * @return pointer to the header of a free block or `NULL` if free blocks are
 *         all smaller than `size`.
 */
static BlockHeader *find_fit(int size) {
    // TODO: implement
    // Loop over all the free blocks and see whether there is a fit
    if(size < divide)
    {
        BlockHeader *bp = free_headp;
        while(bp != NULL) {
            if(get_size(bp) >= size) {
                return bp;
            }
            bp = get_next_free(bp);
        }
    }
    else
    {
        BlockHeader *bp = free_tailp;
        while(bp != NULL) {
            if(get_size(bp) >= size) {
                return bp;
            }
            bp = get_prev_free(bp);
        }
    }
    return NULL;
}

/**
 * Allocate a block of `size` bytes inside the given free block `bp`.
 *
 * @param bp pointer to the header of a free block of at least `size` bytes
 * @param size bytes to assign as an allocated block (multiple of 8)
 * @return pointer to the header of the allocated block
 */
static BlockHeader *place(BlockHeader *bp, int size) {
    // TODO: if current size is greater, use part and add rest to free list
    free_list_remove(bp);
    int rsize = get_size(bp) - size;
    // Case when the remain size is enough for a free block
    if(rsize >= 16)
    {
        if(size <= order) // Case allocate the block to the head
        {
            set_header(bp, size, 1);
            set_footer(bp, size, 1);
            BlockHeader *next = get_next(bp);
            set_header(next, rsize, 0);
            set_footer(next, rsize, 0);
            free_coalesce(next);
        }
        else // Case allocate the block to the tail
        {
            set_header(bp, rsize, 0);
            set_footer(bp, rsize, 0);
            BlockHeader *next = get_next(bp);
            set_header(next, size, 1);
            set_footer(next, size, 1);
            free_coalesce(bp);
            return next;
        }
    } 
    else { // Case when the remain size is not enough for a free block
        set_header(bp, get_size(bp), 1);
        set_footer(bp, get_size(bp), 1);
    }
    // TODO: return pointer to header of allocated block
    return bp;
}

/**
 * Compute the required block size (including space for header/footer) from the
 * requested payload size.
 *
 * @param payload_size requested payload size
 * @return a block size including header/footer that is a multiple of 8
 */
static int required_block_size(int payload_size) {
    payload_size += 8;                    // add 8 for for header/footer
    return ((payload_size + 7) / 8) * 8;  // round up to multiple of 8
}

void *mm_malloc(size_t size) {
    // ignore spurious requests
    if (size == 0)
        return NULL;

    int required_size = required_block_size(size);

    BlockHeader *addr = NULL;
    // TODO: find a free block or extend heap
    BlockHeader *bp = find_fit(required_size);
    // Case when find a free block
    if(bp != NULL) {
        addr = place(bp, required_size);
    }
    // Case when need to extend heap
    else
    {
        while(bp == NULL)
        {
            extend_heap(init_size);
            bp = find_fit(required_size);
        }
        addr = place(bp, required_size);
    }
    
    // TODO: allocate and return pointer to payload
    return get_payload_addr(addr);
}

void *mm_realloc(void *ptr, size_t size) {

    if (ptr == NULL) {
        // equivalent to malloc
        return mm_malloc(size);

    } else if (size == 0) {
        // equivalent to free
        mm_free(ptr);
        return NULL;

    } else {
        int required_size = required_block_size(size);
        BlockHeader *bp= (BlockHeader *)ptr - 1;
        // // TODO: reallocate, reusing current block if possible
        if(required_size <= get_size(bp))
        {
            int rsize = get_size(bp) - required_size;
            // Case when the remain size is enough for a free block
            if(rsize > 8)
            {
                set_header(bp, required_size, 1);
                set_footer(bp, required_size, 1);
                BlockHeader *next = get_next(bp);
                set_header(next, rsize, 0);
                set_footer(next, rsize, 0);
                free_coalesce(next);
            }
            return get_payload_addr(bp);
        }
        else
        {
            // Case when the curr block is the last block
            /*
            if(get_size(get_next(bp)) == 0)
            {
                int curr_size = get_size(bp);
                while(curr_size < required_size)
                {
                    extend_heap(init_size);
                    curr_size += init_size;
                }
                if(curr_size - required_size >= 16)
                {
                    set_header(bp, required_size, 1);
                    set_header(bp, required_size, 1);
                    BlockHeader *next = get_next(bp);
                    set_header(next, curr_size - required_size, 0);
                    set_footer(next, curr_size - required_size, 0);
                    free_coalesce(next);
                }
                else
                {
                    set_header(bp, curr_size, 1);
                    set_footer(bp, curr_size, 1);
                }
                return get_payload_addr(bp);
            }
            */

            int next_allocated = get_allocated(get_next(bp));
            // Check if the next block is free
            if(!next_allocated)
            {
                int collected_size = get_size(bp) + get_size(get_next(bp));
                // Handle the case when the size is larger than the required
                if(collected_size >= required_size)
                {
                    free_list_remove(get_next(bp));
                    // Case when the remain size is big enough
                    if(collected_size - required_size > 8)
                    {
                        set_header(bp, required_size, 1);
                        set_footer(bp, required_size, 1);
                        BlockHeader *next = get_next(bp);
                        set_header(next, collected_size - required_size, 0);
                        set_footer(next, collected_size - required_size, 0);
                        free_coalesce(next);
                    } else { // Case when the remain size is not enough
                        set_header(bp, collected_size, 1);
                        set_footer(bp, collected_size, 1);
                    }
                    return get_payload_addr(bp);
                }
            }
            
            // Handle the case we need to find new block
            void *ep = mm_malloc(size
            // TODO: copy data over new block with memcpy
            memcpy(ep, ptr, size);
            mm_free(ptr);
            // TODO: return pointer to payload of new block
            return ep;
        }
    }
}
