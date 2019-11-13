#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "lib/kernel/list.h"
#include "vm/page.h"

/* List of page in physical memory */
struct list lru_list;

void lru_init (void);

void get_page (struct page *);
void free_page (struct page *);

#endif
