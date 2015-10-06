
#include "threads/thread.h"
#include "threads/palloc.h"

struct frame{
	void* page;
	tid_t tid;
	struct list_elem elem;
	void* uaddr;
	bool done;
	int count;
};

struct list frames_list;

