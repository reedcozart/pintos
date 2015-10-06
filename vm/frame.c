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

void* frame_allocate(){
	
}