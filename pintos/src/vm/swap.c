#include "vm/swap.h"
#include "devices/block.h"
#include "lib/kernel/debug.h"
#include "lib/kernel/bitmap.h"
#include "lib/kernel/list.h"
#include "userprog/syscall.h"
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
get_page (enum palloc_flags flag, struct vm_entry *vme)
{
  void *addr = palloc_get_page(flag);
  /* Allocation failed - swap out */
  if (addr == NULL) {
    swap_out();
  }

  struct page *page = malloc(sizeof(struct page));

  page->paddr = addr;
  page->thread = thread_current();
  page->vme = vme;
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
      break;
    }
  }

  if (page == NULL) {
    exit(-1);
  }
  list_remove(page->elem);
  palloc_free_page(page->paddr);
  free(page);
}

void
swap_init (void)
{
  /* Initialize swap table - bitmap. */
  struct block *block = block_get_role(BLOCK_SWAP);
  
  /* Divide block->size by 8 - one bit means one swap slot */
  size_t bit_cnt = block->size / PAGE_PER_SLOT;
  swap_table = bitmap_create(bit_cnt);
  if (swap_table == NULL)
    exit(-1);
}

void
swap_in (struct vm_entry *vme)
{
  /* Load from swap area. */
  ASSERT (vme->vp_type == VP_SWAP);
  ASSERT (!vme->accessible);

  struct block *block = block_get_role(BLOCK_SWAP);
  int i;
  uint32_t addr = (uint32_t) (vme->vpn << PGBITS);
  for (i = 0; i < PAGE_PER_SLOT; i++) {
    block_read (block, vme->swap_slot + i,
		(void *) (addr + BLOCK_SECTOR_SIZE * i));
  }
  vme->type = VP_FILE;
  vme->accessible = true;
  pagedir_set_accessed (&thread_current()->pagedir, (const void *)addr, true);
}

uint32_t
swap_out (void)
{
  /* Choose victim and say goodbye */
  struct page *victim = list_entry(list_pop_front(&lru_list),
						struct page, elem);
  if (pagedir_is_dirty(&victim->thread->pagedir, victim->paddr)) {
    
  }  
}
