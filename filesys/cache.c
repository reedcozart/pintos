#include "filesys/cache.h"
#include <list.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "devices/block.h"
#include "filesys/filesys.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct list cache_block_list;
static struct lock cache_lock;

/* Checks if the block is already in the cache. */
struct cache_block * block_in_cache(struct inode *inode, block_sector_t sector_idx);

/* Initialize all necessary structures. */
void buffer_cache_init(void) {
    list_init(&cache_block_list);
    lock_init(&cache_lock);
    //fs_buffer_cache_is_inited = true;
}

/* Returns the block if the desired block is currently in the
 * cache and null otherwise. */
struct cache_block * block_in_cache(struct inode *inode, block_sector_t sector_idx) {
    struct list_elem *e;
    struct cache_block *c;

    lock_acquire(&cache_list_lock);
    e = list_begin(&cache_block_list);

    // Iterate through the list and get the proper block
    while (e != list_end(&cache_block_list) && e != NULL) {
        c = list_entry(e, struct cache_block, elem);
        if (c->inode == inode && c->sector_idx == sector_idx) {
            lock_release(&cache_list_lock);
            return c;
        }
        e = list_next(e);
    }
    lock_release(&cache_list_lock);
    return NULL;
}

/* Reads a block from the cache. If it's not currently there, it 
 * finds the correct block from memory and puts it in the cache, 
 * evicting another block if necessary.
 * Returns a pointer to a buffer that can be read from. */
uint8_t * cache_read(struct inode *inode, block_sector_t sector_idx) {
    struct cache_block *c;
    uint8_t *buffer = NULL;

    /* Check if the block is in the cache. */
    c = block_in_cache(inode, sector_idx);

    /* If it isn't, find the corresponding data in the filesystem
     * and allocate a new block for it and put it into the buffer, 
     * evicting an old block if necessary. */
    if (c == NULL) {
        buffer = malloc(BLOCK_SECTOR_SIZE);
        if(buffer == NULL) { return NULL; }

        // Read the block from disk
        block_read(fs_device, sector_idx, buffer);

        // Place the block to the cache
        c = malloc(sizeof(struct cache_block));
        if(c == NULL) { return NULL; }
        c->inode = inode;
        c->sector_idx = sector_idx;
        c->block = buffer;
        c->count = 0;
        c->accessed = true;
        c->dirty = false;
        while (list_size(&cache_block_list) >= CACHE_SIZE) {
            evict_block();
        }
        lock_acquire(&cache_list_lock);
        list_push_back(&cache_block_list, &c->elem);
        lock_release(&cache_list_lock);
    }
    else {
        c->accessed = true;
    }
    return c->block;
}
