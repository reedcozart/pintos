#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux)
{
  const struct sup_pte *p = hash_entry (p_, struct sup_pte, elem);
  return hash_bytes (&p->uaddr, sizeof p->uaddr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux)
{
  const struct sup_pte *a = hash_entry (a_, struct sup_pte, elem);
  const struct sup_pte *b = hash_entry (b_, struct sup_pte, elem);

  return (a->uaddr < b->uaddr);
}

// Create a supplemental page table and also store executable details WITHIN FILESYSTEM
bool init_sup_pte(void* uaddr, struct file* f, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
	struct thread* t = thread_current();

	// Allocate the supplemental page table entry in memory
	struct sup_pte *spte = (struct sup_pte*) malloc(sizeof(struct sup_pte));

	// Cannot allocate memory, fail
	if(spte == NULL)
		return false;

	// Fill in the dirty details
	spte->file = f;
	spte->type = SPTE_FS;
	spte->offset = offset;
	spte->read_bytes = read_bytes;
	spte->zero_bytes = zero_bytes;
	spte->swapped = false;
	spte->loadded = false;
	spte->uaddr = uaddr;
	spte->writable = writable;
	return (hash_insert(&(t->sup_pagedir), &(spte->elem)) == NULL);	
}

bool zero_sup_pte(void *uaddr, bool writable) {
  struct thread *t = thread_current();

  struct sup_pte *spte;
  spte = malloc(sizeof(struct sup_pte));
  if(spte == NULL) { return false; }

  spte->uaddr = uaddr;
  spte->type = SPTE_ZERO;
  spte->writable = writable;

  return hash_insert(&t->sup_pagedir, &(spte->elem)) == NULL;
}



/*
      map kaddr to uaddr
*/
bool set_kaddr(void* uaddr, void* kaddr){
	struct sup_pte *pte = get_pte(uaddr);
	if(pte != NULL){
		pte->kaddr = kaddr;
	}else{
		return false;
	}
	//(pte != NULL) ? pte->kaddr = kaddr : return false;
	return true;
}
/*
	get pointer to a supplimental page table entry
*/
struct sup_pte* get_pte(void* uaddr){
	struct sup_pte pte;
	pte.uaddr = uaddr;
	struct hash_elem *e;
        struct thread* t = thread_current();
	e = hash_find(&(t->sup_pagedir), &(pte.elem));
	if(e == NULL)
		return e;
	else
		return hash_entry(e, struct sup_pte, elem);
}
/*
	remove pte
*/

void delete_spte(struct hash_elem *elem, void *aux UNUSED) {
  struct sup_pte *e = hash_entry(elem, struct sup_pte, elem);
  free(e);
}
/*
	free supplimental page table
*/
void delete_sup_pt(){
	struct thread* t = thread_current();
	hash_destroy(&(t->sup_pagedir), *delete_spte);
}

/*
	free spte
*/
void free_spte(void* uaddr){
	struct sup_pte *spte = get_pte(uaddr);
	struct thread *t = thread_current();
	hash_delete(&(t->sup_pagedir), &(spte->elem));
	free(spte);
}
/*
	load from file to a page i.e for starting programs
*/
bool load_page_file (struct sup_pte *spte)
{
  enum palloc_flags flags = PAL_USER;
  if (spte->read_bytes == 0){
      flags |= PAL_ZERO;
    }
  uint8_t *frame = frame_allocate(flags, spte);
  if (!frame){
      return false;
    }
  if (spte->read_bytes > 0){
      if (file_read_at(spte->file, frame, spte->read_bytes, spte->offset) != (int) spte->read_bytes){
	  frame_free(frame);
	  return false;
	}
     // memset(frame + spte->read_bytes, 0, spte->zero_bytes);
    }
    memset(frame + spte->read_bytes, 0, spte->zero_bytes);
  if (!pagedir_set_page((thread_current())->pagedir, spte->uaddr, frame, spte->writable)){
      frame_free(frame);
      return false;
    }
  frame_set_done(frame, true);
  pagedir_set_dirty((thread_current())->pagedir, spte->uaddr, false);

  spte->loadded = true;  
  return true;
}




