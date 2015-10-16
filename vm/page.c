#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"


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
	spte->uaddr = uaddr;
	spte->writable = writable;
	return (hash_insert(&(t->sup_pagedir), &(spte->elem)) == NULL);	
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


