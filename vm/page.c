#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"


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

  return a->addr < b->addr;
}

bool init_sup_pte(void* uaddr){
	sup_pte *spte = (sup_pte*) malloc(sizeof(sup_pte));
	if(sup_pte == NULL)
		return false;
	spte->uaddr = uaddr;
	return (hash_insert(&(current_thread()->sup_pagedir), &(spte->elem)) == NULL);	
}
/*
      map kaddr to uaddr
*/
bool set_kaddr(void* uaddr, void* kaddr){
	sup_pte *pte = get_pte(uaddr);
	(pte != NULL) ? pte->kaddr = kaddr : return false;
	return true;
}
/*
	get pointer to a supplimental page table entry
*/
sup_pte* get_pte(void* uaddr){
	sup_pte pte;
	pte.uaddr = uaddr;
	struct hash_elem *e;
	e = hash_find(&(current_thread()->sup_pagedir), &(pte.elem));
	if(e == NULL)
		return e;
	else
		return hash_entry(e, sup_pte, elem);
}
/*
	remove pte
*/

void delete_spte(struct hash_elem *elem, void *aux UNUSED) {
  sup_pte *e = hash_entry(elem, sup_pte, elem);
  free(e);
}
/*
	free supplimental page table
*/
void delete_sup_pt(){
	hash_destroy(&(current_thread()->sup_pagedir), *delete_spte);
}

/*
	free spte
*/
void free_spte(void* uaddr){
	sup_pte spte* = get_pte(uaddr);
	hash_delete(&(current_thread()->sup_pagedir), &(spte->elem));
	free(spte);
}


