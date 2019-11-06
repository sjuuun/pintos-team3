#ifndef VM_PAGE_H
#define VM_PAGE_H



#include "lib/kernel/hash.h"
#include "lib/stdbool.h"
#include "lib/stdint.h"

enum vpage_type
  {
     VP_ELF,
     VP_FILE,
     VP_SWAP
  };


struct vm_entry 
{
  /* vm_entry */
  uint32_t vpn;					/* Virtual Page Number */
  bool writable;				/* Read/Write Permission */
  enum vpage_type vp_type;			/* Type of virtual page */
  struct hash_elem vm_elem;
  // TODO: reference to the file object and offset
  uint32_t d_size;
  // TODO: loaction in the swap area
  // TODO: In-memory flag - is it in memory? 
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
