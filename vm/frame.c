#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include "userprog/pagedir.h"

#include "vm/frame.h"
#include "filesys/file.h"
#include "threads/interrupt.h"

static struct lock lock;

void frame_init(){
	lock_init(&lock);
	list_init(&frames_list);
}

// Attempt to allocate a frame
void* frame_allocate(enum palloc_flags flags, void* uaddr){
	void* frame;

	// should  never be called on kernel 
	if(!(flags & PAL_USER))
		return NULL;
	
	frame = palloc_get_page(flags);

	if(frame != NULL) {
		// Successful adding of frame
		add_frame(frame);
	}
	// When frame table is full, need to evict
	else {
		frame = evict_frame(uaddr);
	}
	return frame;
}

// Add frame to frame table
static bool add_frame(void* frame_addr, void* uaddr) {
	struct frame* frame;

	// Frame structure resides in heap
	frame = malloc(sizeof(struct frame));

	// Unable to allocate memory
	if(frame == NULL) {
		return false;
	}

	// Initalizing frame
	frame->page = frame_addr;
	frame->tid = thread_current()->tid;
	frame->uaddr = uaddr;
	frame->done = false;
	frame->count = 0;

	// Synchronize adding to the frame list
	lock_acquire(lock);
	list_push_back(&frames_list, &frame->elem);
	lock_release(lock);

	return true;
}