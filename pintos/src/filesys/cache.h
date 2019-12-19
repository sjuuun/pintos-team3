#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/inode.h"
#include "lib/stdbool.h"

/* Cache entry structure. */
struct cache_entry
{
  bool isdirty;                             /* Dirty flag */
  bool isempty;                             /* Empty flag */
  bool clock;                               /* Clock bit (for eviction) */

  void *cache_addr;                         /* Memory space for cache entry */
  block_sector_t sector;                    /* Disk's sector */
};

void bc_init (void);
void bc_exit (void);

int bc_lookup (block_sector_t);
int bc_select_victim (void);
void bc_flush_entry (int);
void bc_flush_all (void);

void bc_read (block_sector_t, void *, int, int);
void bc_write (block_sector_t, const void *, int, int);

#endif /* filesys/cache.h */
