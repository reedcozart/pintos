#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"


/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED)
{
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->addr, sizeof p->addr);
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return (a->addr < b->addr);
}

bool init_sup_pte(void* uaddr){
	struct sup_pte *spte = (struct sup_pte*) malloc(sizeof(struct sup_pte));
	struct thread* t = thread_current();
	hash_init (&(t->sup_pagedir), page_hash, page_less, NULL); // initialize the hash supplimental page table
	spte->swapped = false;
	if(spte == NULL)
		return false;
	spte->uaddr = uaddr;
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


