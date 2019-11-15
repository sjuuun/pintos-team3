#include "vm/swap.h"
#include "devices/block.h"
#include "lib/debug.h"
#include "lib/stdbool.h"
#include "lib/stddef.h"
#include "lib/kernel/bitmap.h"
#include "lib/kernel/list.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

#define PAGE_PER_SLOT (PGSIZE / BLOCK_SECTOR_SIZE)

/* Bitmap to manage swap area. */
static struct bitmap *swap_table;

void
lru_init (void)
{
  list_init(&lru_list);
}

struct page *
get_page (enum palloc_flags flag)
{
  void *addr = palloc_get_page(flag);
  /* Allocation failed - swap out */
  if (addr == NULL) {
    swap_out();
    addr = palloc_get_page(flag);
  }

  struct page *page = malloc(sizeof(struct page));

  page->paddr = addr;
  page->thread = thread_current();
  //page->vme = vme;
  list_push_back(&lru_list, &page->elem);
  return page;
}

void
free_page (void *addr)
{
  struct list_elem *e;
  struct page *page = NULL;
  for (e = list_front(&lru_list); e != list_end(&lru_list); e = list_next(e)) {
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
    pagedir_clear_page(page->thread->pagedir, (void *)(page->vme->vpn << PGBITS));
    free(page);
}

void
swap_init (void)
{
  /* Initialize swap table - bitmap. */
  struct block *block = block_get_role(BLOCK_SWAP);
  swap_table = bitmap_create((size_t) block_size(block));
  if (swap_table == NULL)
    exit(-1);
}

static void
swap_write (struct vm_entry *vme, void *kaddr) {
  struct block *block = block_get_role(BLOCK_SWAP);

  /* Scan bitmap to find free slot */
  uint32_t swap_slot = bitmap_scan_and_flip(swap_table, 0, PAGE_PER_SLOT, false);
  int i;
  for (i = 0; i < PAGE_PER_SLOT; i++) {
    block_write (block, swap_slot + i,
		(void *) (kaddr + BLOCK_SECTOR_SIZE * i));
  }
  vme->swap_slot = swap_slot;
}

void
swap_in (struct vm_entry *vme, void *kaddr)
{
  /* Load from swap area. */
  struct block *block = block_get_role(BLOCK_SWAP);
  int i;
  for (i = 0; i < PAGE_PER_SLOT; i++) {
    block_read (block, vme->swap_slot + i,
		(void *) (kaddr + BLOCK_SECTOR_SIZE * i));
  }
  bitmap_set_multiple (swap_table, vme->swap_slot, PAGE_PER_SLOT, false);
}

void
swap_out (void)
{
  /* Choose victim and say goodbye */
  ASSERT (!list_empty(&lru_list));
  struct page *victim = list_entry(list_pop_front(&lru_list),
						struct page, elem);
  struct vm_entry *vme = victim->vme;
  void *vaddr = (void *)(vme->vpn << PGBITS);

  switch(vme->vp_type) {
    case VP_ELF:
      if (pagedir_is_dirty(victim->thread->pagedir, vaddr)) {
        swap_write(vme, victim->paddr);
        vme->vp_type = VP_SWAP;
      }
      break;
    case VP_FILE:
      if (pagedir_is_dirty(victim->thread->pagedir, vaddr)) {
        file_write_at(vme->file, vaddr, vme->read_bytes, vme->offset);
      }
      break;
    case VP_SWAP:
      swap_write(vme, victim->paddr);
      break;
    default:
      exit(-1);
  }

  /* Free page and update page table */
  vme->accessible = false;

  pagedir_clear_page(victim->thread->pagedir, vaddr);
  /* Free victim */
  palloc_free_page(victim->paddr);
  free(victim);
}
