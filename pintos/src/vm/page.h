#ifndef VM_PAGE_H
#define VM_PAGE_H



#include "lib/kernel/hash.h"


struct vm_entry 
{
  /* vm_entry */

};

void vm_init (struct hash *vm);
void vm_destroy (struct hash *vm);
struct vm_entry *find_vme (void *vaddr);
bool insert_vme (struct hash *vm, struct vm_entry *vme);
bool delete_vme (struct hash *vm, struct vm_entry *vme);
static unsigned vm_hash_func (const struct hash_elem *e, void *aux UNUSED);
static bool vm_less_func (const struct hash_elem *a, 
	const struct hash_elem *b, void *aux UNUSED);
static void vm_destroy_func (struct hasn_elem *e, void *aux UNUSED);

#endif
