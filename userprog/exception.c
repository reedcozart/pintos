#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "userprog/exception.h"
#include "userprog/process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

#define DEBUG 1


/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 
      /*REED: I commented the panic because we were failing tests from a page
      fault in a kernel context. To fix this if (write && !user) kill(f) 
      we need to catch this condition without panicing the kernel.
      */
      //thread_exit();

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  void* fault_addr_original;

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr_original));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
  
  //printf("I demand a page \n");
  //printf ("Page fault at %p: %s error %s page in %s context.\n",fault_addr_original,not_present ? "not present" : "rights violation",write ? "writing" : "reading",user ? "user" : "kernel");

  if(!user) //indicates a page fault in kernel context
    kill(f);
    //thread_exit();

   if(fault_addr_original == 0) {
    //printf("Fault address is zero\n");
    kill(f);
    return;
   }

  // If the page is not present in physical memory
  if(not_present){
    void* esp;
    void* upage;
    void* kpage;
    bool success;
    struct thread* t;
    struct sup_pte* spte;

    esp = f->esp;

    //printf("esp + 4 bytes: %p\n", esp + 1);
    //printf("esp + 32 bytes: %p\n", esp + 8);

    // Get the address of the actual page
    fault_addr = pg_round_down(fault_addr_original); //page aligning the fault_addr
    //printf("esp: %p fault_addr: %p\n", esp, fault_addr);
    t = thread_current();

    // Obtain the page table entry for the requested page
    spte = get_pte(fault_addr);

    if(spte == NULL) {
      //printf("Obtaining supplemental page table entry failed.\n");
      //printf("Fault address: %p\n", fault_addr);

      // Handle stack growth
      // Get stack pointer for user/kernel context
      if(user) {
        esp = f->esp;
      }
      else {
        esp = thread_current()->stack;
      }
      //printf("Difference in fault addr and stack: %d", ((uint32_t*) PHYS_BASE) - ((uint32_t*) fault_addr));
      // Check if the stack is too big 
      /*if(((uint32_t*) PHYS_BASE) - ((uint32_t*) fault_addr) >= MAX_STACK_SIZE) {
        //printf("Stack is too big\n");
        thread_exit();
      }*/

      // Check if the memory access is within a page of the stack pointer
      //printf("Fault addr - esp: %d\n", (((uint32_t) esp) - ((uint32_t)fault_addr_original)));
      if((int) (((uint32_t) esp) - ((uint32_t)fault_addr_original)) < PGSIZE) {
        upage = esp;
        kpage = frame_allocate(PAL_USER | PAL_ZERO, upage);
        if(kpage == NULL) {
          printf("Stack page allocation failed\n");
          kill(f);
        }
        if(!pagedir_set_page(thread_current()->pagedir, fault_addr, kpage, true)) {
          printf("Stack page allocation mapping failed\n");
          kill(f);
        }
        return;
      }

      // Invalid memory access
      else {
        //printf("Invalid stack memory access\n");
        thread_exit();
      }
    }

    // Allocate the frame for  the requested virtual address
    kpage = frame_allocate(PAL_USER, fault_addr);
    if(kpage == NULL) {
      printf("Frame allocation failed.\n");
      kill(f);
      return;
    }

    // Set the virtual page mapping to the new physical frame
    if(!pagedir_set_page(t->pagedir, fault_addr, kpage, spte->writable)) {
      printf("Page mapping failed");
      return;
    }

   // pagedir_clear_page (t->pagedir, spte->uaddr);

    switch(spte->type) {
      case SPTE_FS: //load_page_file(spte); break;
        if (spte->read_bytes > 0){
          if (file_read_at(spte->file, kpage, spte->read_bytes, spte->offset) != (int) spte->read_bytes){
            printf("Error loading file into memory");
            return;
          }
        }

        // Zero pad the rest of the page
        memset(kpage + spte->read_bytes, 0, spte->zero_bytes);
        break;
      case SPTE_MMAP: break;
      case SPTE_SWAP: break;
      case SPTE_ZERO: break;
        memset(fault_addr, 0, PGSIZE);
        break;
    }

    frame_set_done(kpage, true);
    pagedir_set_dirty(t->pagedir, fault_addr, false);
  }

  // Handle stack growth
  else /*if (stack_heuristic(f, fault_addr)) */{

   // printf("Hits else statement\n");
    void* esp;
    void* upage;
    void* kpage;

    // Get the user program's stack pointer
    if(user) {
      esp = f->esp;
    }
    else {
      esp = thread_current()->stack;
    }

    // If the stack pointer is 4 or 32 bytes below esp, we need to allocate a new page
    if(esp - 4 == fault_addr || esp - 32 == fault_addr || !user) {
      //printf("Stack is below esp, need to allocate a new page\n");
      // Check to make sure the memory a
      /*if(((uint32_t*) PHYS_BASE) - ((uint32_t*) fault_addr) > MAX_STACK_SIZE) {
        printf("Stack is too big\n");
        kill(f);
      }*/

      upage = esp;
      kpage = frame_allocate(PAL_USER | PAL_ZERO, upage);
      if(kpage == NULL) {
        printf("Frame allocation has failed\n");
        return;
      }
      pagedir_set_page(thread_current()->pagedir, upage, kpage, true);
      frame_set_done(kpage, true);
      return;
    }
    kill(f);
  }

  // Not stack growth, it is present, it is user.


		// stack growth
    //kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    //upage = esp;
    //kpage = frame_allocate(PAL_USER, fault_addr);
   // success = install_page(fault_addr, kpage, 1); //grow stack by 1 page


  /*
  Page fault in the kernel context.
  */
 
  //kill (f)
}
