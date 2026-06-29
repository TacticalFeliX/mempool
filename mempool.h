
#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stdint.h>
#include <stddef.h>

#define POOL_BLOCK_SIZE 32
#define POOL_BLOCK_COUNT 16

typedef struct FreeBlock {
    struct FreeBlock *next;
} FreeBlock;

typedef struct {

    uint8_t storage[POOL_BLOCK_SIZE * POOL_BLOCK_COUNT];
    FreeBlock *free_head;

    size_t blocks_in_use;
    size_t total_allocs;
    size_t total_frees;
} MemPool;

void pool_init (MemPool *pool);

void *pool_alloc(MemPool *pool);

void pool_free(MemPool *pool, void *ptr);

void pool_print_stats(const MemPool *pool);

#endif
