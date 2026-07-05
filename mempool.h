/* =============================================================================
 * mempool.h — Fixed-Size Memory Pool Allocator
 * Public API: types, configuration constants, and function declarations.
 *
 * Design overview:
 *   A MemPool manages a single, contiguous block of raw memory that is
 *   carved at startup into POOL_BLOCK_COUNT fixed-size slots.  Every free
 *   slot stores a FreeBlock node at its very beginning; those nodes are
 *   chained into a singly-linked free list.  pool_alloc() pops the head
 *   of that list; pool_free() pushes back onto it — both are O(1).
 * =============================================================================
 */

#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stdint.h>   /* uint8_t, size_t              */
#include <stddef.h>   /* NULL, size_t                 */

/* ── Configuration ───────────────────────────────────────────────────────────
 * Tune these two constants to fit your target use-case.
 * POOL_BLOCK_SIZE  must be >= sizeof(void*) so a free-list pointer fits
 *                  inside every free slot.
 * POOL_BLOCK_COUNT is the maximum number of objects you can have alive at
 *                  once.
 * ─────────────────────────────────────────────────────────────────────────── */
#define POOL_BLOCK_SIZE   64    /* bytes per allocation slot                  */
#define POOL_BLOCK_COUNT  16    /* total number of slots in the pool          */

/* ── Internal free-list node ─────────────────────────────────────────────────
 * When a block is FREE  → its first bytes hold a FreeBlock that points to
 *                         the next free block (or NULL if it is the last).
 * When a block is IN USE → those same bytes are owned by the caller; the
 *                          pool does NOT touch them.
 *w
 * This is the classic "intrusive free list" trick: no separate bookkeeping
 * array is needed because the free slots themselves store the list.
 * ─────────────────────────────────────────────────────────────────────────── */
typedef struct FreeBlock {
    struct FreeBlock *next;   /* pointer to the next free slot, or NULL       */
} FreeBlock;

/* ── Pool descriptor ─────────────────────────────────────────────────────────
 * Everything the allocator needs is bundled here.  Callers treat this as an
 * opaque handle — access it only through the pool_* functions.
 * ─────────────────────────────────────────────────────────────────────────── */
typedef struct {
    /* The raw storage: a flat array of bytes, statically sized at compile
     * time.  No heap allocation required — safe for embedded targets.       */
    uint8_t  storage[POOL_BLOCK_SIZE * POOL_BLOCK_COUNT];

    /* Head of the intrusive free list.  NULL means the pool is exhausted.  */
    FreeBlock *free_head;

    /* Diagnostic counters — handy for debugging and unit tests.            */
    size_t   blocks_in_use;   /* number of currently allocated blocks       */
    size_t   total_allocs;    /* lifetime allocation count                  */
    size_t   total_frees;     /* lifetime free count                        */
} MemPool;

/* ── Public API ──────────────────────────────────────────────────────────────
 * All three functions operate in O(1) constant time.
 * ─────────────────────────────────────────────────────────────────────────── */

/*
 * pool_init()
 *   Walk through `storage`, overlay a FreeBlock node at the start of every
 *   slot, and link them into a chain.  Must be called once before any
 *   pool_alloc() / pool_free() calls.
 */
void  pool_init (MemPool *pool);

/*
 * pool_alloc()
 *   Pop the head of the free list and return it to the caller.
 *   Returns NULL if every slot is already in use.
 *   Time complexity: O(1).
 */
void *pool_alloc(MemPool *pool);

/*
 * pool_free()
 *   Push `ptr` back onto the head of the free list so it can be reused.
 *   `ptr` MUST be a pointer previously returned by pool_alloc() on this
 *   pool — passing anything else is undefined behaviour.
 *   Time complexity: O(1).
 */
void  pool_free (MemPool *pool, void *ptr);

/*
 * pool_print_stats()
 *   Print a human-readable summary of the pool's current state to stdout.
 *   Useful for debugging and for demonstrating the allocator in interviews.
 */
void  pool_print_stats(const MemPool *pool);

#endif /* MEMPOOL_H */
