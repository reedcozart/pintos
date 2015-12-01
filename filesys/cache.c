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
static struct lock cache_list_lock;
static struct list filesys_lock_list;
static struct lock filesys_lock; //TODO turn this into a R/W lock


/*o of 1 instead of using count aging, keep an ordered cache as an ordered list, 
every time something is accessed, put it at the front of the list
evict the tail of the list.

search the list, put it back on the front
always put things in on the front
evict always the back

*/

// Makes sure that the cache has been initialized
bool fs_buffer_cache_is_inited = false;

/* Checks if the block is already in the cache. */
struct cache_block * block_in_cache(struct inode *inode, block_sector_t sector_idx);

/* Initialize all necessary structures. */
void buffer_cache_init(void) {
    list_init(&cache_block_list);
    lock_init(&cache_lock);
    lock_init(&cache_list_lock);
    list_init(&filesys_lock_list);
    lock_init(&filesys_lock);
    fs_buffer_cache_is_inited = true;
}

/* Returns the block if the desired block is currently in the
 * cache and null otherwise. */
struct cache_block * block_in_cache(struct inode *inode, block_sector_t sector_idx) {
    struct list_elem *e;
    struct list_elem *block_elem;
    struct cache_block *c;

    lock_acquire(&cache_list_lock);
    e = list_begin(&cache_block_list);

    // Iterate through the list and get the proper block
    while (e != list_end(&cache_block_list) && e != NULL) {
        c = list_entry(e, struct cache_block, elem);
        if (c->inode == inode && c->sector_idx == sector_idx) {
        	list_remove(&c->elem);
        	list_push_front(&cache_block_list, &c->elem);
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
    struct list_elem *accessed_elem;
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
        c->accessed = true;
        c->dirty = false;
        while (list_size(&cache_block_list) >= CACHE_SIZE) {
            evict_block();
        }
        lock_acquire(&cache_list_lock);
        /*insert the element at the front (it is now least recently used)*/	
        list_push_front(&cache_block_list, &c->elem); 
        lock_release(&cache_list_lock);
    }
    else {
        c->accessed = true;
    }
    return c->block;
}

uint8_t * cache_write(struct inode *inode, block_sector_t sector_idx) {
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
        /* Reads the data from the disk to the buffer.
         * Writes happen later. */
        block_read(fs_device, sector_idx, buffer);
        c = malloc(sizeof(struct cache_block));
        if(c == NULL) { return NULL; }
        c->inode = inode;
        c->sector_idx = sector_idx;
        c->block = buffer;
        c->count = 0;
        c->accessed = true;
        c->dirty = true;
        while (list_size(&cache_block_list) >= CACHE_SIZE) {
            evict_block();
        }
        lock_acquire(&cache_list_lock);
        list_push_front(&cache_block_list, &c->elem);
        lock_release(&cache_list_lock);
    }
    else {
        c->accessed = true;
        c->dirty = true;
    }
    return c->block;
}

/* Writes the data in a buffer to disk. This is called on two 
 * occasions:
 * 1. When a block is evicted (in evict_block()).
 * 2. Periodically when all dirty blocks are written back to disk
 *    (in buffer_cache_tick()). */
void cache_write_to_disk(struct cache_block *c) {
    lock_acquire(&filesys_lock);
    block_write(fs_device, c->sector_idx, c->block);
    lock_release(&filesys_lock);
    c->dirty = false;
}

/* Uses an aging replacement policy to find the block to evict. */
void evict_block(void) {
    struct list_elem *e;
    struct list_elem *remove_elem;
    struct cache_block *c;
    struct cache_block *evict = NULL;
    int lowest_count;

    lock_acquire(&cache_list_lock);

    /* removes the last element of the list, as the last one 
    is the least recently used*/
    ASSERT(list_size(&cache_block_list) > 0);
    remove_elem = list_pop_back(&cache_block_list);
    evict = list_entry(remove_elem, struct cache_block, elem);
    ASSERT(evict != NULL);
    if(evict->dirty){
    	cache_write_to_disk(evict);
    }

    lock_release(&cache_list_lock);
    /* Free memory. */
    free(evict->block);
    free(evict); 
}


void cache_try_write_to_disk(struct cache_block* c){}