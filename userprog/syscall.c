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

#include <debug.h>

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
  lock_init(&file_sys_lock);
  //printf("LOCK INITIALIZED\n");
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{	
	int args[ARGS_MAX];
	check_valid_pointer(f->esp);
	switch(*(int*) f->esp) {
	case SYS_HALT:
	//printf("SYS_HALT\n");
		halt();
		break;
	case SYS_EXIT:
	//printf("SYS_EXIT\n");
		get_args(f, &args[0], 1);
		exit(args[0]);
		break;
	case SYS_EXEC:
	//printf("SYS_EXEC\n");
		get_args(f, &args[0], 1);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = exec((const char*)args[0]);
		break;
	case SYS_WAIT:
	//printf("SYS_WAIT\n");
		get_args(f, &args[0], 1);
		f->eax = wait(args[0]);
		break;
	case SYS_CREATE:
	//printf("SYS_CREATE\n");
		get_args(f, &args[0], 2);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = create((const char*) args[0], (unsigned) args[1]);
		break;
	case SYS_REMOVE:
	//printf("SYS_REMOVE\n");
		get_args(f, &args[0], 1);
		args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = remove((const char*) args[0]);
		break;
	case SYS_OPEN:
	//printf("SYS_OPEN\n");
		get_args(f, &args[0], 1);
		//args[0] = user_to_kernel_ptr((void*) args[0]);
		f->eax = open((const char*)args[0]);
		break;
	case SYS_FILESIZE:
	//printf("SYS_FILESIZE\n");
		get_args(f, &args[0], 1);
		f->eax = filesize(args[0]);
		break;
	case SYS_READ:
	//printf("SYS_READ\n");
		get_args(f, &args[0], 3);
		args[1] = user_to_kernel_ptr((void*) args[1]); //causing page fault
		f->eax = read(args[0], (void*) args[1], (unsigned) args[2]);
		break;
	case SYS_WRITE:
	///printf("SYS_WRITE\n");
		get_args(f, &args[0], 3);
		//args[1] = user_to_kernel_ptr((void*) args[1]);
		//f->eax = read(args[0], (void*) args[1], (unsigned) args[2]);
		f->eax = write(args[0], (void*) args[1], (unsigned) args[2]);
		break;
	case SYS_SEEK:
	//printf("SYS_SEEK\n");
		get_args(f, &args[0], 2);
		seek(args[0], (unsigned) args[1]);
		break;
	case SYS_TELL:
	//printf("SYS_TELL\n");
		get_args(f, &args[0], 1);
		f->eax = tell(args[0]);
		break;
	case SYS_CLOSE:
	//printf("SYS_CLOSE\n");
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
  struct thread *t = thread_current ();
  t->returnval = status;
  printf("%s: exit(%d)\n", t->name, status);
  thread_exit ();
  NOT_REACHED ();

	 //start cleanup
//	 struct file_desc* fd;		//place holder for file descriptors associated with this thread so we can close them
//	 struct child_thread* child;	//place holder for children processes
//	 struct list_elem* fdElem;	//place holder for beginning iterator for file descriptors
//	 struct list_elem* childElem;//place holder for beginning iterator for child processes
//
	 //close file descriptors from the file descriptor list

	//printf("NO PAGE FAULT\n");
}

pid_t exec(const char* cmd_line){
	tid_t child;

	if(cmd_line >=PHYS_BASE || get_user(cmd_line) == -1){	//REED, I need your max cmd Line char limit---- looked at process.c -> seems PHYS_BASE is the max
		exit(-1);
		return -1;
	}else{
		//printf("EXECUTING %s\n", cmd_line);
		child = process_execute(cmd_line);
		return child;
	}
}

int wait(pid_t pid){
	return process_wait(pid);
}

bool create(const char* file, unsigned initial_size){
	if(checkMemorySpace((void*) file, initial_size)) {
		//printf("creating filesys\n");
		return filesys_create(file, initial_size);
	}
	/*if(file + initial_size > PHYS_BASE || get_user(file + initial_size -1) == -1){
		exit(-1);
		return -1;
	}else{
		return filesys_create(file, initial_size);
	}*/
}

bool remove(const char* file){
	if(user_to_kernel_ptr((void*) file)) {
		return filesys_remove(file);
	}
	/*if(file >= PHYS_BASE || get_user(file) == -1){
		exit(-1);
		return -1;
	}else{
		return filesys_remove(file);
	}*/
}

int open(const char* file){
	//printf("OPEN STARTS\n");
	if(user_to_kernel_ptr((void*) file)) {
		//printf("OPEN DID NOT FAIL\n");
		//debug_backtrace();
		lock_acquire(&file_sys_lock);
		//printf("Lock acquired.\n");
		//printf("File name = %s\n", file);
		struct file* f = filesys_open(file);
		if(!f) {
			//printf("NULL FILE\n");
			free(f);
			lock_release(&file_sys_lock);
			return -1;
		}
		struct file_desc* fd = palloc_get_page(0);
		fd->file = f;
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
	//printf("OPEN FAILED\n");
	return -1;
		// Check validity of pointer
		/*if(file >= PHYS_BASE || get_user(file) == -1 ) {
			exit(-1);
			return -1;
		}*/
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
		palloc_free_page(filed);
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
		//printf("INVALID POINTER\n");
		exit(-1);
	}
}

int user_to_kernel_ptr(void* vaddr) {
	check_valid_pointer(vaddr);
	void* ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if(!ptr) {
		//printf("INVALID MEMORY ACCESS\n");
		exit(-1);
	}
	return (int) ptr;
}

void process_close_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->file_descrips);

  while (e != list_end (&t->file_descrips))
    {
      next = list_next(e);
      struct file_desc *pf = list_entry (e, struct file_desc, elem);
      if (fd == pf->id || fd == CLOSE_ALL)
	  {
	  file_close(pf->file);
	  list_remove(&pf->elem);
	  palloc_free_page(pf);
	  if (fd != CLOSE_ALL)
	    {
	      return;
	    }
	}
      e = next;
    }
}

struct child_process* add_child_process (int pid)
{
  struct child_process* cp = malloc(sizeof(struct child_process));
  cp->pid = pid;
  cp->load = NOT_LOADED;
  cp->wait = false;
  cp->exit = false;
  lock_init(&cp->wait_lock);
  list_push_back(&thread_current()->child_threads,
		 &cp->elem);
  return cp;
}

struct child_process* get_child_process (int pid)
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->child_threads); e != list_end (&t->child_threads);
       e = list_next (e))
        {
          struct child_process *cp = list_entry (e, struct child_process, elem);
          if (pid == cp->pid)
	    {
	      return cp;
	    }
        }
  return NULL;
}

void remove_child_process (struct child_process *cp)
{
  list_remove(&cp->elem);
  free(cp);
}

void remove_child_processes (void)
{
  struct thread *t = thread_current();
  struct list_elem *next, *e = list_begin(&t->child_threads);

  while (e != list_end (&t->child_threads))
    {
      next = list_next(e);
      struct child_process *cp = list_entry (e, struct child_process,
					     elem);
      list_remove(&cp->elem);
      free(cp);
      e = next;
    }
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

int checkMemorySpace(void* vaddr, int size) {
	int i;
	for(i = 0; i < size; i++) {
		user_to_kernel_ptr(vaddr);
	}
	return 1;
}

