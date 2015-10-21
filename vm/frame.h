
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame{
	void* page;
	tid_t tid;
	struct list_elem elem;
	void* uaddr;
	bool done;
        bool pinned;
	int count;	// checks which frame is the oldest for eviction policy
};

struct list frames_list;

void* frame_allocate(enum palloc_flags flags, void* uaddr);
void frame_init();
static bool add_frame(void* frame_addr, void* uaddr);
void* evict_frame(void* new_frame_uaddr);
struct frame* choose_evict();
void age_frames(int64_t timer_ticks);
void frame_set_done(void *kpage, bool value);
void frame_free (void *frame);
