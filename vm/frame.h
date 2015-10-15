
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame{
	void* page;
	tid_t tid;
	struct list_elem elem;
	void* uaddr;
	bool done;
	int count;	// checks which frame is the oldest for eviction policy
};

struct list frames_list;

