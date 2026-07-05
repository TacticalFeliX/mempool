/* =============================================================================
 * mempool.c — Fixed-Size Memory Pool Allocator — Implementation
 *
 * The allocator manages a single flat byte array (pool->storage[]).
 * During pool_init() that array is divided into POOL_BLOCK_COUNT fixed-size
 * slots of POOL_BLOCK_SIZE bytes each.  The first sizeof(FreeBlock) bytes of
 * every free slot are reused to store a FreeBlock node that points to the
 * next free slot — forming a singly-linked "intrusive free list".
 *
 * The key insight:
 *   ┌──────────────────────────────────────────────────────────────────────┐
 *   │ Free slot: │ FreeBlock.next ──► next free slot ... ─► NULL          │
 *   │ Used slot: │ [caller data — pool never reads/writes these bytes]    │
 *   └──────────────────────────────────────────────────────────────────────┘
 * =============================================================================
 */

#include <stdio.h>       /* printf()                  */
#include <string.h>      /* memset()                  */
#include "mempool.h"

/* ---------------------------------------------------------------------------
 * pool_init()
 *
 * Steps:
 *   1. Zero out all storage (clean slate; optional but good practice).
 *   2. For each slot i, cast its starting address to a FreeBlock pointer.
 *   3. Point that FreeBlock's `next` at slot i+1 (or NULL for the last slot).
 *   4. Set free_head to point at slot 0.
 *
 * After init the free list looks like:
 *
 *   free_head
 *       │
 *       ▼
 *   [slot 0]──next──►[slot 1]──next──►[slot 2]── ... ──►[slot N-1]──next──►NULL
 * ---------------------------------------------------------------------------
 */
void pool_init(MemPool *pool)
{
    /* ── 1. Clear all storage ─────────────────────────────────────────────── */
    memset(pool->storage, 0, sizeof(pool->storage));

    /* ── 2 & 3. Walk every slot and wire up the free list ────────────────── */
    for (int i = 0; i < POOL_BLOCK_COUNT; i++) {

        /* Compute the byte offset of slot i inside storage[].              */
        uint8_t *slot_address = pool->storage + (i * POOL_BLOCK_SIZE);

        /* Treat the raw bytes at slot_address as a FreeBlock node.
         * This is safe: POOL_BLOCK_SIZE >= sizeof(FreeBlock) is required
         * (enforced by the static_assert below).                           */
        FreeBlock *current_node = (FreeBlock *)slot_address;

        /* Is there a next slot?  If yes, point `next` there; if no (last
         * slot), terminate the list with NULL.                             */
        if (i + 1 < POOL_BLOCK_COUNT) {
            uint8_t *next_slot_address = pool->storage + ((i + 1) * POOL_BLOCK_SIZE);
            current_node->next = (FreeBlock *)next_slot_address;
        } else {
            current_node->next = NULL;   /* end of the free list            */
        }
    }

    /* ── 4. Point free_head at the very first slot ───────────────────────── */
    pool->free_head = (FreeBlock *)pool->storage;

    /* ── Reset diagnostic counters ──────────────────────────────────────── */
    pool->blocks_in_use = 0;
    pool->total_allocs  = 0;
    pool->total_frees   = 0;
}

/* Compile-time safety net: each slot must be large enough to hold the
 * FreeBlock pointer we embed in it while it is free.                       */
_Static_assert(POOL_BLOCK_SIZE >= sizeof(FreeBlock),
               "POOL_BLOCK_SIZE is too small to hold a FreeBlock pointer.");

/* ---------------------------------------------------------------------------
 * pool_alloc()
 *
 * "Pop" the head node off the free list and return it to the caller.
 *
 *   Before:
 *     free_head ──► [slot A] ──next──► [slot B] ──► ...
 *
 *   After:
 *     free_head ──► [slot B] ──► ...
 *     returned ──► [slot A]   (caller may write anything here)
 * ---------------------------------------------------------------------------
 */
void *pool_alloc(MemPool *pool)
{
    /* If free_head is NULL there are no slots left — report failure.       */
    if (pool->free_head == NULL) {
        printf("[mempool] ERROR: pool exhausted — all %d blocks in use.\n",
               POOL_BLOCK_COUNT);
        return NULL;
    }

    /* ── 1. Remember the block we are about to hand out ─────────────────── */
    FreeBlock *allocated_block = pool->free_head;

    /* ── 2. Advance the free list head to the next free slot ────────────── */
    pool->free_head = allocated_block->next;

    /* ── 3. (Optional but polite) Wipe the FreeBlock metadata the caller
     *       is about to overwrite so they start with zeroed memory.        */
    memset(allocated_block, 0, POOL_BLOCK_SIZE);

    /* ── 4. Update diagnostics ───────────────────────────────────────────── */
    pool->blocks_in_use++;
    pool->total_allocs++;

    return (void *)allocated_block;
}

/* ---------------------------------------------------------------------------
 * pool_free()
 *
 * "Push" the returned block back onto the front of the free list.
 *
 *   Before:
 *     free_head ──► [slot B] ──► ...
 *     ptr ──► [slot A]   (caller is done with it)
 *
 *   After:
 *     free_head ──► [slot A] ──next──► [slot B] ──► ...
 * ---------------------------------------------------------------------------
 */
void pool_free(MemPool *pool, void *ptr)
{
    /* Guard against accidental double-free of NULL.                        */
    if (ptr == NULL) {
        printf("[mempool] WARNING: pool_free() called with NULL — ignored.\n");
        return;
    }

    /* ── 1. Reinterpret the returned memory as a FreeBlock node ─────────── */
    FreeBlock *returned_block = (FreeBlock *)ptr;

    /* ── 2. Link it in front of the current free list ───────────────────── */
    returned_block->next = pool->free_head;

    /* ── 3. Make it the new head ─────────────────────────────────────────── */
    pool->free_head = returned_block;

    /* ── 4. Update diagnostics ───────────────────────────────────────────── */
    pool->blocks_in_use--;
    pool->total_frees++;
}

/* ---------------------------------------------------------------------------
 * pool_print_stats()
 *
 * Walk the free list to count how many blocks are genuinely free right now,
 * then print a readable status report.
 * ---------------------------------------------------------------------------
 */
void pool_print_stats(const MemPool *pool)
{
    /* Count free blocks by walking the list (O(n) — diagnostic only).     */
    size_t free_count = 0;
    FreeBlock *node = pool->free_head;
    while (node != NULL) {
        free_count++;
        node = node->next;
    }

    printf("┌─────────────────────────────────────────┐\n");
    printf("│          MemPool Status Report           │\n");
    printf("├─────────────────────────────────────────┤\n");
    printf("│  Block size   : %4d bytes               │\n", POOL_BLOCK_SIZE);
    printf("│  Total blocks : %4d                     │\n", POOL_BLOCK_COUNT);
    printf("│  In use       : %4zu                     │\n", pool->blocks_in_use);
    printf("│  Free         : %4zu                     │\n", free_count);
    printf("│  Total allocs : %4zu (lifetime)          │\n", pool->total_allocs);
    printf("│  Total frees  : %4zu (lifetime)          │\n", pool->total_frees);
    printf("└─────────────────────────────────────────┘\n");
}
