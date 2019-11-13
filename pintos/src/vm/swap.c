#include "vm/swap.h"
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"

void
lru_init (void)
{
  list_init(&lru_list);
}

void
get_page (enum palloc_flags flag, struct vm_entry *vme)
{
  void *addr = palloc_get_page(flag);
  /* Allocation failed - swap out */
  if (addr == NULL) {
    swap_out();
  }

  struct page *page = malloc(sizeof(struct page));

  page->pfn = pg_no(addr);
  page->thread = thread_current();
  page->vme = vme;
  list_push_back(&lru_list, &page->elem);
}


swap_init ()
{
  /* Initialize swap table - bitmap. */

}

swap_in ()
{
  /* Load from swap area. */

}

swap_out ()
{
  /* Choose victim and say goodbye */

}
