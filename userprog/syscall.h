#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

void syscall_init (void);

typedef int pid_t;

#define CLOSE_ALL -1
#define ERROR -1

#define NOT_LOADED 0
#define LOAD_SUCCESS 1
#define LOAD_FAIL 2

struct child_process {
  int pid;
  int load;
  bool wait;
  bool exit;
  int status;
  struct lock wait_lock;
  struct semaphore sem_die;
  struct semaphore sem_read;
  struct list_elem elem;
};

void process_close_file (int fd);

#endif /* userprog/syscall.h */
