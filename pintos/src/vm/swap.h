#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "lib/kernel/list.h"
#include "threads/palloc.h"
#include "vm/page.h"

/* List of page in physical memory */
struct list lru_list;

void lru_init (void);

struct page * get_page (enum palloc_flags);
void free_page (void *);

void swap_init (void);
void swap_in (struct vm_entry *, void *);
void swap_out (void);

#endif
