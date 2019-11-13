#ifndef VM_PAGE_H
#define VM_PAGE_H


#include "devices/block.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#include "lib/stdbool.h"
#include "lib/stdint.h"
#include "lib/stddef.h"
#include "filesys/file.h"

/* List of page in physical memory */
struct list lru_list;

enum vpage_type
  {
     VP_ELF,
     VP_FILE,
     VP_SWAP
  };


struct mmap_file
{
  int mapid;				/* mapping id */
  struct file *file;			/* mapped file */
  struct list_elem mf_elem;		/* list_elem between mmap_file */
  struct list vme_list;			/* mapped file's vm_entries */
};

struct vm_entry 
{
  /* Virtual page status */
  uint32_t vpn;				/* Virtual Page Number */
  enum vpage_type vp_type;		/* Type of virtual page */
  bool writable;			/* Read/Write Permission */

  /* Reference to the file object and offset */
  struct file* file;
  size_t read_bytes;
  size_t zero_bytes;
  size_t offset;

  /* list_elem */
  struct hash_elem vm_elem;		/* list_elem between vm_entry */
  struct list_elem mmap_elem;		/* list_elem in mmap_file */

  /* Swap */
  bool accessable;			/* Check page is in memory */
  struct block *swap;			/* Location in swap area */
};

/* Data structure representing each physical page. */
struct page
{
  uint32_t pfn;				/* Physical Frame number */
  struct thread *thread;		/* Thread own this page */
  struct vm_entry *vme;			/* Related vm_entry */
  struct list_elem elem;		/* list_elem in lru_list */
};

void vm_init (struct hash *);
void vm_destroy (struct hash *);
struct vm_entry *find_vme (void *);
bool insert_vme (struct hash *, struct vm_entry *);
bool delete_vme (struct hash *, struct vm_entry *);
bool load_file (void *, struct vm_entry *);
#endif
