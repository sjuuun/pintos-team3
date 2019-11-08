#ifndef VM_PAGE_H
#define VM_PAGE_H



#include "lib/kernel/hash.h"
#include "lib/stdbool.h"
#include "lib/stdint.h"
#include "lib/stddef.h"
#include "filesys/file.h"

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
  struct file* file;
  size_t read_bytes;
  size_t zero_bytes;
  size_t offset;

  //uint32_t d_size;
  // TODO: loaction in the swap area
  // TODO: In-memory flag - is it in memory? 
};

void vm_init (struct hash *);
void vm_destroy (struct hash *);
struct vm_entry *find_vme (void *);
bool insert_vme (struct hash *, struct vm_entry *);
bool delete_vme (struct hash *, struct vm_entry *);
bool load_file (void *, struct vm_entry *);
#endif
