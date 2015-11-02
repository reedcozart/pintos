#ifndef _page_h_
#define _page_h_ 1

#include "filesys/file.h"
#include "lib/kernel/hash.h"

#define PGSIZE 4096

// State of pages
enum spt_type {
	SPTE_FS = 001,		// Within file system
	SPTE_SWAP = 002,	// Within swap space
	SPTE_ZERO = 003,	// Zeroed
	SPTE_MMAP = 004		// Within MMap
};

/*Supplimental pte structure*/
struct sup_pte {
	void* uaddr;    
	void* kaddr;		// only if page is present in physical memory
	struct hash_elem elem;
	bool swapped;
	bool writable;
	enum spt_type type;
	bool loadded;
	// Details about the executable
	struct file* file;
	off_t offset;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	int swap;
};

void intialize_spte();
unsigned page_hash (const struct hash_elem *p_, void *aux);
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);
bool init_sup_pte(void* uaddr, struct file* f, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void delete_sup_pt();

void free_spte(void* uaddr);

bool set_kaddr(void* uaddr, void* kaddr);

struct sup_pte* get_pte(void* uaddr);

bool zero_sup_pte(void *uaddr, bool writable);

bool load_page_file (struct sup_pte *spte);

#endif //_Page_h_
