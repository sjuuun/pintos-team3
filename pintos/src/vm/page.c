#include "vm/page.h"
#include <stdio.h>
#include <hash.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


/* Hash table initialization. */
void
vm_init (struct hash *vm)
{
  // start_process call this function to initialize hash table.
  // TODO: 
  
}

/* Hash table delete. */
void
vm_destroy (struct hash *vm)
{
  // process_exit should call this function to remove vm_entries.
  // TODO: destroy hash with destroy_hash(h, delete_vme?) Update current thread's hash to NULL.

}

/* Search vm_entry corresponding to vaddr in the address space of the current process. */
struct vm_entry *
find_vme (void *vaddr)
{
  // Use hash_find, and hash_entry.
}

/* Insert vm_entry to hash table. */
bool
insert_vme (struct hash *vm, struct vm_entry *vme)
{
  // Use hash_insert.
}

/* Delete vm_entry from hash table. */
bool
delete_vme (struct hash *vm, struct vm_entry *vme)
{
  // Delete with hash_delete
}

/* Calculate where to put the vm_entry into the hash table. */
static unsigned
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
}

/* Compare address values of two entered hash_elem. */
static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
}

/* Remove memory of vm_entry. */
static void
vm_destroy_func (struct hasn_elem *e, void *aux UNUSED)
{
}
