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
	uint8_t *block;


	struct list_elem elem;

};

extern bool filesys_cache_initiated;


