#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include <list.h>

#define CACHE_TIMER_FREQ 100

#define CACHE_SIZE 64

struct cache_block{

	bool dirty;
	bool accessed;
	int count;
	struct inode *inode;

	/* The disk sector of the inode. */
    block_sector_t sector_idx;

    /* The block of memory allocated for the data. 
     * For some reason, this is defined as a uint8_t * in 
     * inode.c (see the bounce buffer), so I left it the 
     * same here. */
    uint8_t *block;

	struct list_elem elem;
};

extern bool filesys_cache_initiated;


