#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <stdlib.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define ARGS_MAX 3
#define USER_VADDR_BOTTOM ((void*) 0x08048000)

static void syscall_handler (struct intr_frame *);

struct lock file_sys_lock;

void check_valid_pointer(void* addr);
void halt(void);
void exit(int status);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
int filesize(int fdid);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
struct file_desc* get_fd(int fd);
int get_user(const uint8_t* uaddr);
int user_to_kernel_ptr(void* vaddr);
void get_args(struct intr_frame* f, int* arg, int n); 

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{	
	int args[ARGS_MAX];
	check_valid_pointer(f->esp);
	switch(*(int*) f->esp) {
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		get_args(f, &args[0], 1);
		exit(args[0]);
		break;
	case SYS_EXEC:
		get_args(f, &args[0], 1);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = exec((const char*)args[0]);
		break;
	case SYS_WAIT:
		get_args(f, &args[0], 1);
		f->eax = wait(args[0]);
		break;
	case SYS_CREATE:
		get_args(f, &args[0], 2);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = create((const char*) args[0], (unsigned) args[1]);
		break;
	case SYS_REMOVE:
		get_args(f, &args[0], 1);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = remove((const char*) args[0]);
		break;
	case SYS_OPEN:
		get_args(f, &args[0], 1);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = open((const char*)args[0]);
		break;
	case SYS_FILESIZE:
		get_args(f, &args[0], 1);
		f->eax = filesize(args[0]);
		break;
	case SYS_READ:
		get_args(f, &args[0], 3);
		args[1] = user_to_kernel_ptr((void*) args[1]);
		f->eax = read(args[0], (void*) args[1], (unsigned) args[2]);
		break;
	case SYS_WRITE:
		get_args(f, &args[0], 3);
		args[1] = user_to_kernel_ptr((void*) args[1]);
		f->eax = read(args[0], (void*) args[1], (unsigned) args[2]);
		break;
	case SYS_SEEK:
		get_args(f, &args[0], 2);
		seek(args[0], (unsigned) args[1]);
		break;
	case SYS_TELL:
		get_args(f, &args[0], 1);
		f->eax = tell(args[0]);
		break;
	case SYS_CLOSE:
		get_args(f, &args[0], 1);
		close(args[0]);
		break;
	default:
		printf("Unimplemented system call");
		thread_exit();
	}
}

void halt(void){
	shutdown_power_off();
}

void exit(int status){
	 struct thread* t = thread_current();

	 //start cleanup
	 struct file_desc* fd;		//place holder for file descriptors associated with this thread so we can close them
	 struct child_thread* child;	//place holder for children processes
	 struct list_elem* fdElem;	//place holder for beginning iterator for file descriptors
	 struct list_elem* childElem;//place holder for beginning iterator for child processes

	 //close file descriptors from the file descriptor list
	 while(!list_empty(&(t->file_descrips))){
	 	fdElem = list_begin(&(t->file_descrips));		//get first element from list of FD
  	fd = list_entry(fdElem, struct file_desc, elem);//get the fd from the list element
	 close(fd->id);									//close each fd this process has open
	}

	 // Update child processes
	 while(!list_empty(&(t->child_threads))){
	 	childElem = list_begin(&(t->child_threads));
	 	child = list_entry(childElem, struct child_thread, elem);
		if(!child->exited) {
			struct thread* orphan = get_thread_from_tid(child->tid);
			orphan->parent_pid = 1; // Orphan is now main process
		}
		list_remove(&(child->elem));
		palloc_free_page(child);
	}

	// Update parent process
	struct thread* parent = get_thread_from_tid(t->parent_pid);
	for(childElem = list_begin(&(parent->child_threads)); childElem != list_end(&(parent->child_threads)); childElem = list_next(childElem)) {
		struct child_thread* ct = list_entry(childElem, struct child_thread, elem); 
		if(ct->tid == t->tid) {
			ct->exited = true;
			ct->exit_status = status;
			if(ct->waiting)
				thread_unblock(parent);
		}
	}

	printf("NO PAGE FAULT\n");
}

pid_t exec(const char* cmd_line){
	tid_t child;

	if(cmd_line >=PHYS_BASE || get_user(cmd_line) == -1){	//REED, I need your max cmd Line char limit---- looked at process.c -> seems PHYS_BASE is the max
		exit(-1);
		return -1;
	}else{
		child = process_execute(cmd_line);
		return child;
	}
}

int wait(pid_t pid){
	return process_wait(pid);
}

bool create(const char* file, unsigned initial_size){
	if(file + initial_size > PHYS_BASE || get_user(file + initial_size -1) == -1){
		exit(-1);
		return -1;
	}else{
		return filesys_create(file, initial_size);
	}

}

bool remove(const char* file){
	if(file >= PHYS_BASE || get_user(file) == -1){
		exit(-1);
		return -1;
	}else{
		return filesys_remove(file);
	}
}

int open(const char* file){
	// Check validity of pointer
	if(file >= PHYS_BASE || get_user(file) == -1 ) {
		exit(-1);
		return -1;
	}
	lock_acquire(&file_sys_lock);
	struct file* f = filesys_open(file);
	if(!f) {
		free(f);
		lock_release(&file_sys_lock);
		return -1;
	}
	struct file_desc* fd = palloc_get_page(0);
	fd->file = file;
	if(list_empty(&(thread_current()->file_descrips))) {
		fd->id = 3;
	}
	else {
		fd->id = list_entry(list_back(&(thread_current()->file_descrips)), struct file_desc, elem)->id + 1;
	}
	list_push_back(&(thread_current()->file_descrips), &(fd->elem));
	lock_release(&file_sys_lock);
	return fd->id;
}

int filesize(int fdid){
	struct file_desc* fd = get_fd(fdid);

	if(fd && fd->file){
		return file_length(fd->file);
	}else{
		return -1;
	}

}

int read(int fd, void* buffer, unsigned size){
	int result = -1;
	unsigned offset;
	
	// Error check the buffer pointer
	if(buffer + size - 1 >= PHYS_BASE || get_user(buffer + size - 1) == -1 ) {
		exit(-1);
		return -1;
	}

	// If reading from stdin
	if(fd == STDIN_FILENO) {
		uint8_t* local_buffer = (uint8_t*) buffer;
		for(offset = 0; offset < size; offset++) {
			local_buffer[offset] = input_getc();
		}
		return size;
	}

	// If reading from a file
	lock_acquire(&file_sys_lock);
	struct file_desc* file_d = get_fd(fd);
	if(file_d && file_d->file) {
		result = file_read(file_d->file, buffer, size);
	}
	return result;
}

int write(int fd, const void* buffer, unsigned size){
	int result = -1;

	// Error check the buffer pointer
	if(buffer + size - 1 >= PHYS_BASE || get_user(buffer + size - 1) == -1 ) {
		exit(-1);
		return -1;
	}

	// If writing to console
	if(fd == STDOUT_FILENO) {
		size_t offset = 0;
		while(offset + 200 < size) {
			putbuf((char*)(buffer + offset), (size_t) 200);
			offset = offset + 200;
		}
		putbuf((char*)(buffer + offset), (size_t) (size - offset));
		return size;
	}

	// If writing to a file
	lock_acquire(&file_sys_lock);
	struct file_desc* file_d = get_fd(fd);
	if(file_d && file_d->file) {
		result = file_write(file_d->file, buffer, size);
	}
	lock_release(&file_sys_lock);
	return result;
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
		result = file_tell(filed->file);
	}
	lock_release(&file_sys_lock);
	return result;
}

void close(int fd){
	lock_acquire(&file_sys_lock);
	struct file_desc* filed = get_fd(fd);
	if(filed && filed->file) {
		file_close(filed->file);
		list_remove(&(filed->elem));
		free(filed);
	}
	lock_release(&file_sys_lock);
}

struct file_desc* get_fd(int fd) {
	// Get thread list
	struct thread* t = thread_current();

	// Find the file descriptor 
	struct list_elem* e = list_begin(&t->file_descrips);
	while(e != list_end(&t->file_descrips)) {
		struct file_desc* d = list_entry(e, struct file_desc, elem);
		if(d->id == fd) {
			return d;
		}
		e = list_next(e);
	}
	return NULL;
}

void check_valid_pointer(void* addr) {
	if(!is_user_vaddr(addr) || addr < USER_VADDR_BOTTOM) {
		exit(-1);
	}
}

int user_to_kernel_ptr(void* vaddr) {
	check_valid_pointer(vaddr);
	void* ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if(!ptr) {
		exit(-1);
	}
	return (int) ptr;
}

void get_args(struct intr_frame* f, int* arg, int n) {
	int i;
	int* ptr;
	for(i = 0; i < n; i++) {
		ptr = (int*) f->esp + i + 1;
		check_valid_pointer((void*)ptr);
		arg[i] = *ptr;
	}
}

int get_four_bytes_user(const void* addr) {
	// if the address goes past physical memory
	if(addr >= PHYS_BASE) {
		exit(-1);
	}
}

// Gets byte at user virtual address
int get_user(const uint8_t* uaddr) {
	int result;
	asm("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
	return result;
}


