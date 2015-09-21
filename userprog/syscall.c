#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

struct lock file_sys_lock;

struct file_desc* get_fd(int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

void halt(void){

}

void exit(int status){

}

pid_t exec(const char* cmd_line){

}

int wait(pid_t pid){

}

bool create(conts char* file, unsigned initial_size){

}

bool remove(const char* file){

}

int open(cont char* file){

}

int filesize(int fd){

}

int read(int fd, void* buffer, unsigned size){

}

int write(int fd, const void* buffer, unsigned size){

}

void seek(int fd, unsigned position){
	lock_acquire(&file_sys_lock);
	struct file_desc* filed = get_fd(fd);
	if(filed && filed->file) {
		file_seek(filed->file, position);
	}
	lock_release(&file_sys_lock);
}

unsigned tell(int fd){
	lock_acquire(&file_sys_lock);
	int result = -1;
	struct file_desc* filed = get_fd(fd);
	if(filed && filed->file) {
		result = file_tell(filed);
	}
	lock_release(&file_sys_lock);
	return result;
}

void close(int fd){
	lock_acquire(&file_sys_lock);
	struct file_desc* filed = get_fd(fd);
	if(filed && filed->file) {
		file_close(filed->file);
		list_remove(&(filedd->elem));
		free(fd);
	}
	lock_release(&file_sys_lock);
}

struct file_desc* get_fd(int fd) {
	// Get thread list
	struct thread* t = thread_current();

	// Find the file descriptor 
	struct list_elem* e = list_begin(&t->file_descrips);
	while(e != list_end(&t->file_descrips) {
		struct file_desc* d = list_entry(e, struct file_desc, elem);
		if(d->id == fd) {
			return d;
		}
		e = list_next(e);
	}
	return NULL;
}


