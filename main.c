/* =============================================================================
 * main.c — Test Driver for the Fixed-Size Memory Pool Allocator
 *
 * Compile:
 *   gcc -Wall -Wextra -std=c11 -o mempool_demo main.c mempool.c
 *
 * Run:
 *   ./mempool_demo
 *
 * This file simulates a realistic embedded use-case: a pool of fixed-size
 * "Packet" structures (like network receive buffers) being allocated and
 * freed in various patterns to exercise every code path in the allocator.
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "mempool.h"

/* ---------------------------------------------------------------------------
 * A realistic payload type — must fit in POOL_BLOCK_SIZE (64) bytes.
 * ---------------------------------------------------------------------------
 */
typedef struct {
    uint32_t id;          /* packet sequence number                          */
    uint16_t length;      /* payload length in bytes                         */
    uint8_t  data[58];    /* inline payload bytes  (4 + 2 + 58 = 64 bytes)  */
} Packet;

/* Sanity check at compile time — Packet must fit in one pool block.        */
_Static_assert(sizeof(Packet) <= POOL_BLOCK_SIZE,
               "Packet is too large for one pool block — increase POOL_BLOCK_SIZE.");

/* ---------------------------------------------------------------------------
 * Helper: pretty-print a Packet pointer (or NULL) with its pool slot offset.
 * ---------------------------------------------------------------------------
 */
static void print_packet(const MemPool *pool, const Packet *pkt, const char *label)
{
    if (pkt == NULL) {
        printf("  %-20s → NULL\n", label);
        return;
    }
    /* Compute which slot number this address corresponds to.               */
    size_t offset = (uint8_t *)pkt - pool->storage;
    size_t slot   = offset / POOL_BLOCK_SIZE;
    printf("  %-20s → slot[%02zu]  id=%-4u  len=%u\n",
           label, slot, pkt->id, pkt->length);
}

/* ===========================================================================
 * main()
 * =========================================================================== */
int main(void)
{
    /* ── Declare and initialise the pool on the stack ────────────────────── */
    MemPool pool;
    pool_init(&pool);

    printf("=============================================================\n");
    printf("  Fixed-Size Memory Pool Allocator — Demo\n");
    printf("  Block size: %d bytes | Blocks: %d | Pool: %d bytes total\n",
           POOL_BLOCK_SIZE, POOL_BLOCK_COUNT,
           POOL_BLOCK_SIZE * POOL_BLOCK_COUNT);
    printf("=============================================================\n\n");

    /* ── Stage 1: Allocate three Packets and populate them ───────────────── */
    printf("── Stage 1: Allocate 3 Packets ──────────────────────────────\n");

    Packet *pkt_a = (Packet *)pool_alloc(&pool);
    Packet *pkt_b = (Packet *)pool_alloc(&pool);
    Packet *pkt_c = (Packet *)pool_alloc(&pool);

    if (pkt_a) { pkt_a->id = 101; pkt_a->length = 10; memset(pkt_a->data, 0xAA, 10); }
    if (pkt_b) { pkt_b->id = 202; pkt_b->length = 20; memset(pkt_b->data, 0xBB, 20); }
    if (pkt_c) { pkt_c->id = 303; pkt_c->length = 30; memset(pkt_c->data, 0xCC, 30); }

    print_packet(&pool, pkt_a, "pkt_a (id=101)");
    print_packet(&pool, pkt_b, "pkt_b (id=202)");
    print_packet(&pool, pkt_c, "pkt_c (id=303)");
    printf("\n");
    pool_print_stats(&pool);

    /* ── Stage 2: Free the middle packet and reallocate ──────────────────── */
    printf("\n── Stage 2: Free pkt_b, then allocate pkt_d ────────────────\n");

    pool_free(&pool, pkt_b);
    pkt_b = NULL;   /* Nullify so we cannot accidentally use it again.      */

    printf("  pkt_b returned to pool.\n");

    /* The pool recycles the just-freed slot — pkt_d will occupy slot[1].   */
    Packet *pkt_d = (Packet *)pool_alloc(&pool);
    if (pkt_d) { pkt_d->id = 404; pkt_d->length = 5; memset(pkt_d->data, 0xDD, 5); }

    print_packet(&pool, pkt_d, "pkt_d (id=404)");
    printf("\n");
    pool_print_stats(&pool);

    /* ── Stage 3: Exhaust the pool ───────────────────────────────────────── */
    printf("\n── Stage 3: Fill all remaining slots ────────────────────────\n");

    /* We have 3 blocks in use (pkt_a, pkt_c, pkt_d).
     * Allocate the remaining POOL_BLOCK_COUNT - 3 blocks.                  */
    Packet *extras[POOL_BLOCK_COUNT];
    int extra_count = 0;

    for (int i = 0; i < POOL_BLOCK_COUNT; i++) {
        extras[i] = (Packet *)pool_alloc(&pool);
        if (extras[i] == NULL) break;          /* pool exhausted            */
        extras[i]->id     = (uint32_t)(1000 + i);
        extras[i]->length = (uint16_t)(i + 1);
        extra_count++;
    }
    printf("  Allocated %d additional blocks.\n", extra_count);
    printf("\n");
    pool_print_stats(&pool);

    /* ── Stage 4: Attempt an over-allocation (expect NULL + error msg) ───── */
    printf("\n── Stage 4: Over-allocate (pool is full) ────────────────────\n");
    Packet *overflow = (Packet *)pool_alloc(&pool);
    print_packet(&pool, overflow, "overflow attempt");

    /* ── Stage 5: Free all blocks and confirm the pool is fully restored ─── */
    printf("\n── Stage 5: Free everything and verify full recovery ─────────\n");

    pool_free(&pool, pkt_a);  pkt_a = NULL;
    pool_free(&pool, pkt_c);  pkt_c = NULL;
    pool_free(&pool, pkt_d);  pkt_d = NULL;

    for (int i = 0; i < extra_count; i++) {
        pool_free(&pool, extras[i]);
        extras[i] = NULL;
    }

    printf("  All blocks returned.\n\n");
    pool_print_stats(&pool);

    /* ── Stage 6: NULL free guard ────────────────────────────────────────── */
    printf("\n── Stage 6: pool_free(NULL) should be a no-op ───────────────\n");
    pool_free(&pool, NULL);

    /* ── Stage 7: Final alloc proves the pool is fully reusable ─────────── */
    printf("\n── Stage 7: Fresh alloc after full reset ─────────────────────\n");
    Packet *fresh = (Packet *)pool_alloc(&pool);
    if (fresh) { fresh->id = 999; fresh->length = 1; }
    print_packet(&pool, fresh, "fresh (id=999)");
    pool_free(&pool, fresh);

    printf("\n=============================================================\n");
    printf("  All stages complete. Pool allocator is working correctly.\n");
    printf("=============================================================\n");

    return 0;
}
