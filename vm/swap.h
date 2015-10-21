#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init();
void swap_read(int swap_page, const void* uaddr);
int swap_write(const void* uaddr);
void swap_remove(int swap_page);

#endif
