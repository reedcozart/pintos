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

void* evict_frame(void* new_frame_uaddr){
	struct frame evicted_frame;
	struct thread evicted_thread;
	void* evicted_page;
	void* evicted_uaddr;
	struct spte evicted_spte;
	bool evicted_is_dirty;

	intr_disable();

	/* time to evict a frame!*/
	lock_aquire(&lock);
	evicted_frame = choose_evict();
	lock_release(&lock);

	evicted_page = evicted_frame->uaddr;
	evicted_thread = get_thread_from_tid(evicted_frame->tid);
	evicted_spte = get_pte(page);
	pagedir_clear_page(evicted_thread->pagedir, evicted_page);
	evicted_is_dirty = pagedir_is_dirty(evicted_thread->pagedir, evicted_page);

	if(evicted_is_dirty){
		evicted_spte->swapped = true; //INDICATE HERE THAT WE SWAPPED IT!
		swap_write(evicted_page);
	}

	evicted_frame->uaddr = new_frame_uaddr;
	evicted_frame->tid = thread_current()->tid;
	evicted_frame->done = false;
	evicted_frame->count = 0;

	intr_enable();
	return evicted_frame->page;
}

struct frame* choose_evict(){
	int smallest;
	struct list_elem *e;
  	struct frame *f;
  	struct frame *frame;

  	e = list_head(&frames_list);
  	f = list_entry(e, struct frame, elem);

  	smallest = f->count;
  	while(e != list_tail((&frames_list))){
  		if(f->done && smallest < f->count){
  			smallest = f->count;
  			frame = f; //we've found a more least recently used frame!
  		}
  		e = list_next(e); //look at next element in the list!
  		f = list_entry(e, struct frame, elem);
  	}
  	return frame;
 }

 void age_frames(int64_t timer_ticks){
  	struct thread *t;
 	struct vm_frame *f;
 	struct list_elem *e;
  	uint32_t *pd;
  	const void *uaddr;
  	bool accessed;

  	if(timer_ticks % 100 == 0){ //don't want to age our frames all the time, too much overhead
  		e = list_head(&vm_frames_list);
	  	while(e != list_tail((&frames_list))){
	  		f = list_entry(e, struct frame, elem);
	  		t = get_thread_from_tid(f->tid);
	  		if(t!= NULL){
	  			pd = t->pagedir;
	  			uaddr = f->uaddr;
	  			accessed = pagedir_is_accessed(pd, uaddr);
	  			f->count++;
	  			pagedir_set_accessed(pd, uaddr);
	  		}
	  		e = list_next(e); //look at next element in the list!
	  	}
  	}
 }