#include "devices/block.h"
#include "vm/page.h"
#include <bitmap.h>

struct block* swap_block;
static struct bitmap* swap_free;

// Constant that is the number of blocks needed to save a page
static const size_t NUM_SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

// Initialize the swapping mechanism
void swap_init() {

	// Initialize swap block with type BLOCK_SWAP
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block == NULL) {
		PANIC("Cannot get swap block");
	}

	// Initialize free swap 
	swap_free = bitmap_create(block_size(swap_block) / NUM_SECTORS_PER_PAGE);
	bitmap_set_all(swap_free, true);
}

void swap_read(int swap_page, const void* uaddr) {
	int i;

	// Read through enough blocks to read a full page into swap_block
	for(i = 0; i < NUM_SECTORS_PER_PAGE; i++) {
		block_read(swap_block, (swap_page * NUM_SECTORS_PER_PAGE) + i, uaddr + (i * BLOCK_SECTOR_SIZE);
	}

	bitmap_set(swap_free, swap_page, true);
}

void swap_write(const void* uaddr) {
	int swap_page = bitmap_scan(swap_free, 0, 1, true);
	int i;

	// Write enough blocks for a full page into swap_block
	for(i = 0; i < NUM_SECTORS_PER_PAGE; i++) {
		block_write(swap_block, (swap_page * NUM_SECTORS_PER_PAGE) + i, uaddr + (i * BLOCK_SECTOR_SIZE);
	}

	bitmap_set(swap_free, swap_page, false);
	return swap_page;
}

void swap_remove(int swap_page) { //Put me in thread exit!
	bitmap_set(swap_free. swap_page, true);
}
