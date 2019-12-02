#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "filesys/inode.h"
#include "lib/stdbool.h"

struct cache_entry
{
  bool isdirty;
  bool isempty;
  bool clock;

  void *cache_addr;
  block_sector_t sector;  
  /* lock, clock bit */
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
