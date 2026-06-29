#include <stdio.h>
#include <string.h>
#include "mempool.h"

void pool_init(MemPool *pool)
{
    memset(pool->storage,0,sizeof(pool->storage));

    for(int i = 0; i < POOL_BLOCK_COUNT; i++){

        uint8_t *slot_address = pool->storage + ( i * POOL_BLOCK_SIZE);

        FreeBlock *current_node = (FreeBlock *)slot_address;

        if(i + 1 < POOL_BLOCK_COUNT){
            uint8_t *next_slot_address = pool->storage + ((i + 1) * POOL_BLOCK_SIZE);
            current_node->next = (FreeBlock *)next_slot_address;
        }
        else{
            current_node->next = NULL;
        }
    }
    
    pool->free_head = (FreeBlock *)pool->storage;

    pool->blocks_in_use = 0;
    pool->total_allocs = 0;
    pool->total_frees = 0;
}

_Static_assert(POOL_BLOCK_SIZE >=sizeof(FreeBlock), "POOL_BLOCK_SIZE is too small to hold a FreeBlock Pointer");

void *pool_alloc(MemPool *pool)
{
    if(pool->free_head == NULL){
        printf("[mempool] ERROR: pool exhausted, all %d blocks in use.\n", POOL_BLOCK_COUNT);
        return NULL;
    }

    FreeBlock *allocated_block = pool->free_head;
    pool->free_head = allocated_block->next;

    memset(allocated_block, 0, POOL_BLOCK_SIZE);

    pool->blocks_in_use++;
    pool->total_allocs++;

    return (void *)allocated_block;
}

void pool_free(MemPool *pool, void *ptr)
{
    if (ptr==NULL){
        printf("[mempool] WARNING: pool_free() called with NULL- ignored.\n");
        return;
    }

    FreeBlock *returned_block = (FreeBlock *)ptr;

    returned_block->next = pool->free_head;
    pool->free_head = returned_block;

    pool->blocks_in_use++;
    pool->total_frees++;
}

void pool_print_stats(const MemPool *pool)
{
    size_t free_count = 0;
    FreeBlock  *node = pool->free_head;
    while (node != NULL){
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
