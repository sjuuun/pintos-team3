#include "vm/page.h"
#include <debug.h>
#include <stdio.h>
#include <hash.h>
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"


/* Hash table initialization. */
void
vm_init (struct hash *vm)
{
  hash_init(vm, vm_hash_func, vm_less_func, NULL); 
}

/* Hash table delete. */
void
vm_destroy (struct hash *vm)
{
  hash_destroy(vm, delete_vme);
}

/* Search vm_entry corresponding to vaddr in the address space of the current process. */
struct vm_entry *
find_vme (void *vaddr)
{
  uint32_t vpn;
  struct hash_iterator iter;

  vpn = pg_no(vaddr);
  hash_first (iter, &thread_current()->vm);
  while (hash_next (&iter)) {
    struct vm_entry *vme = hash_entry(hash_cur(&iter),
                                      struct vm_entry, vm_elem);
    if (vme->vpn == vpn)
      return vme;
  }
  return NULL;
}

/* Insert vm_entry to hash table. */
bool
insert_vme (struct hash *vm, struct vm_entry *vme)
{
  if (hash_insert(vm, vme->vm_elem) == NULL)
    return true;
  return false;
}

/* Delete vm_entry from hash table. */
bool
delete_vme (struct hash *vm, struct vm_entry *vme)
{
  if (hash_delete(vm, vme->vm_elem) == NULL)
    return false;
  return true;
}

/* Calculate where to put the vm_entry into the hash table. */
static unsigned
vm_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  struct vm_entry *vme;
  struct thread * cur;

  vme = hash_entry(e, struct vm_entry, vm_elem);
  cur = thread_current()
  return (unsigned) vme->vpn % (cur->vm.bucket_cnt);
}

/* Compare address values of two entered hash_elem.
   Return true if address of a is less then address of b.  */
static bool
vm_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct vm_entry *vma;
  struct vm_entry *vmb;

  vma = hash_entry(a, struct vm_entry, vm_elem);
  vmb = hash_entry(b, struct vm_entry, vm_elem);
  
  return vma->vpn < vmb->vpn;
}

/* Remove memory of vm_entry. */
static void
vm_destroy_func (struct hasn_elem *e, void *aux UNUSED)
{
  struct vm_entry *vme;
  vme = hash_entry(e, struct vm_emtry, vm_elem);
  free(vme);
}
