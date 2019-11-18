#include "vm/swap.h"
#include "devices/block.h"
#include "lib/debug.h"
#include "lib/stdbool.h"
#include "lib/stddef.h"
#include "lib/kernel/bitmap.h"
#include "lib/kernel/list.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

/* # of sectors in a single page (or swap slot) */
#define PAGE_PER_SLOT (PGSIZE / BLOCK_SECTOR_SIZE)

/* Bitmap to manage swap area. */
static struct bitmap *swap_table;

/* Initialize LRU list to manage pages; 
   Called at threads/init.c , main()    */
void
lru_init (void)
{
  list_init(&lru_list);
}

/* Get page using palloc_get_page, if no available page to allocate, 
   Swap out current page and allocate new page and initialize page 
   structure. Return pointer to newly allocated page structure */
struct page *
get_page (enum palloc_flags flag)
{
  void *addr = palloc_get_page(flag);
  /* Allocation failed - swap out */
  while (addr == NULL) {
    swap_out();
    addr = palloc_get_page(flag);
  }

  struct page *page = malloc(sizeof(struct page));
  page->paddr = addr;
  page->thread = thread_current();
  page->pin = PAGE_NOT_IN_USE;
  list_push_back(&lru_list, &page->elem);
  return page;
}

/* Free page according to  input physical address. scan the LRU list, 
   find matching page to input address. And then, remove that element in 
   LRU list, and call palloc_free_page to free it. Also clear that page's
   page directory and free page structure. If no matching page, exit(-1) */
void
free_page (void *addr)
{
  struct list_elem *e;
  struct page *page = NULL;
  for (e = list_front(&lru_list); e != list_end(&lru_list); e = list_next(e)){
    page = list_entry (e, struct page, elem);
    if (page->paddr == addr) {
      goto done;
    }
  }
  /* Not reached */
  exit(-1);

  done:
    list_remove(&page->elem);
    palloc_free_page(page->paddr);
    pagedir_clear_page(page->thread->pagedir, page->vme->vaddr);
    free(page);
}

/* Return the victim page to be swapped out. scan the LRU list, check each 
   element's page directory is accessed. If accessed, set it's accessed bit 
   to 0 and move on to the next element. If not accessed, remove it from 
   LRU list and return it. */
static struct page*
get_victim (void)
{ 
  struct list_elem *e;
  struct page *victim;
  for(e = list_begin(&lru_list); e != list_end(&lru_list); e = list_next(e)){
    victim = list_entry(e, struct page, elem);
    uint32_t *pd = victim->thread->pagedir;
    void *vpage = victim->vme->vaddr;
    if (pagedir_is_accessed(pd, vpage)) {
      pagedir_set_accessed(pd, vpage, false);
    }
    else {
      if (victim->pin == PAGE_IN_USE) 
        continue;
      list_remove(e);
      return victim;
    }
  }
  return victim;
}

/* Initialize swap table - use bitmap. If failed, exit(-1) */
void
swap_init (void)
{
  struct block *block = block_get_role(BLOCK_SWAP);
  swap_table = bitmap_create((size_t) block_size(block));
  if (swap_table == NULL)
    exit(-1);
}

/* Write data in kaddr-page to swap area. Scan swap_table and find first-fit
   free slot to write to. Use block_write function. Store the slot of data 
   wrote, in vme structure. */
static void
swap_write (struct vm_entry *vme, void *kaddr) {
  struct block *block = block_get_role(BLOCK_SWAP);
  uint32_t swap_slot = bitmap_scan_and_flip(swap_table, 0, PAGE_PER_SLOT, 0);
  int i;
  for (i = 0; i < PAGE_PER_SLOT; i++) {
    block_write (block, swap_slot + i,
		(void *) (kaddr + BLOCK_SECTOR_SIZE * i));
  }
  vme->swap_slot = swap_slot;
}

/* Swap in the page from swap area to physical memory. Use block_read 
   function. Set swap_slot's bitmap to false */
void
swap_in (struct vm_entry *vme, void *kaddr)
{
  /* Get block of swap area. */
  struct block *block = block_get_role(BLOCK_SWAP);
  int i;
  for (i = 0; i < PAGE_PER_SLOT; i++) {
    block_read (block, vme->swap_slot + i,
		(void *) (kaddr + BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set_multiple (swap_table, vme->swap_slot, PAGE_PER_SLOT, false);
  vme->swap_slot = 0;
}

/* Swap out the page from memory to swap area. Behave different from victim
   page's vm_entry type. Finally, free the victim page. */
void
swap_out (void)
{
  /* Choose victim and say goodbye */
  ASSERT (!list_empty(&lru_list));
  struct page *victim = get_victim();
  struct vm_entry *vme = victim->vme;
  void *vaddr = vme->vaddr;

  switch(vme->vp_type) {
    case VP_ELF:
      if (pagedir_is_dirty(victim->thread->pagedir, vaddr)) {
        swap_write(vme, victim->paddr);
        vme->vp_type = VP_SWAP;
      }
      break;
    case VP_FILE:
      if (pagedir_is_dirty(victim->thread->pagedir, vaddr)) {
        lock_acquire(&filesys_lock);
        file_write_at(vme->file, vaddr, vme->read_bytes, vme->offset);
        lock_release(&filesys_lock);
      }
      break;
    case VP_SWAP:
      swap_write(vme, victim->paddr);
      break;
    default:
      exit(-1);
  }
  
  /* Free victim */
  palloc_free_page(victim->paddr);
  pagedir_clear_page(victim->thread->pagedir, vaddr);
  free(victim);
}

/* Set input virtual address's matching physical page's pin flags 
   to pin_flags (PAGE_IN_USE / PAGE_NOT_IN_USE). */
void
set_page_pflags(void *vaddr, enum pin_flags pin_flags)
{
  struct list_elem *e;
  struct page *page = NULL;
  void *addr = pagedir_get_page(thread_current()->pagedir, vaddr);
  for (e = list_front(&lru_list); e != list_end(&lru_list); e = list_next(e)){
    page = list_entry (e, struct page, elem);
    if (page->paddr == addr) {
      page->pin = pin_flags;
    }
  }
}
