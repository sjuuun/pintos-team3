#include "vm/page.h"
#include <debug.h>
#include <stdio.h>
#include <hash.h>
#include "lib/string.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"

/* Function prototypes */
static unsigned vm_hash_func (const struct hash_elem *, void *);
static bool vm_less_func (const struct hash_elem *,
                          const struct hash_elem *, void *);
static void vm_destroy_func (struct hash_elem *, void*);

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
  hash_destroy(vm, vm_destroy_func);
}

/* Search vm_entry corresponding to vaddr in the address space of 
						the current process. */
struct vm_entry *
find_vme (void *vaddr)
{
  uint32_t vpn;
  struct hash_iterator iter;

  vpn = pg_no(vaddr);
  hash_first (&iter, &thread_current()->vm);
  while (hash_next (&iter)) {
    struct vm_entry *vme = hash_entry(hash_cur(&iter),
                                      struct vm_entry, vm_elem);
    if (vme->vpn == vpn)
      return vme;
  }

  struct thread *cur = thread_current();
  if (!list_empty(&cur->mmap_list)) {
    struct list_elem *e; 
    for(e = list_begin(&cur->mmap_list); e != list_end(&cur->mmap_list);
                                e = list_next(e)) {
      struct mmap_file *mmf = list_entry(e, struct mmap_file, mf_elem);
      struct list_elem *v;
      for(v = list_begin(&mmf->vme_list); v != list_end(&mmf->vme_list);
                                v = list_next(v)){
        struct vm_entry *vme = list_entry(v, struct vm_entry, mmap_elem);
        if (vme->vpn == vpn)
          return vme;
      }
    }
  }
  return NULL;
}

/* Insert vm_entry to hash table. */
bool
insert_vme (struct hash *vm, struct vm_entry *vme)
{
  if (hash_insert(vm, &vme->vm_elem) == NULL)
    return true;
  return false;
}

/* Delete vm_entry from hash table. */
bool
delete_vme (struct hash *vm, struct vm_entry *vme)
{
  if (hash_delete(vm, &vme->vm_elem) == NULL)
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
  cur = thread_current();
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
vm_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  struct vm_entry *vme;
  vme = hash_entry(e, struct vm_entry, vm_elem);
  delete_vme(&thread_current()->vm, vme);
  free(vme);
}

/* load a page to kaddr by <file, offset> of vme */
bool
load_file (void *kaddr, struct vm_entry *vme)
{
  /* TODO: Use file_read_at()
	   Return file_read_at status
	   Pad 0 as much as zero bytes */
  int32_t load_bytes;
  load_bytes = file_read_at(vme->file, kaddr, vme->read_bytes, vme->offset);
  if (load_bytes != (int32_t) vme->read_bytes)
    return false;
  memset(kaddr + load_bytes, 0, vme->zero_bytes);
  vme->accessible = true;
  return true;
}
