#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "device/block.h"
#include "filesys/off_t.h"
#include "filesys/inode.h"
#include "lib/stdbool.h"

struct cache_entry
{
  struct inode* inode;
  void *cache_addr;
  bool isdirty;
  bool isempty;
  block_sector_t sector;  

  /* lock, clock bit */
  bool clock;
};

void bc_init(void);
void bc_exit(void);

bool bc_read(block_sector_t, void *, off_t, int, int);
bool bc_write(block_sector_t, void *, off_t, int, int);
int bc_lookup (block_sector_t);
int bc_select_victim (void);
void bc_flush_entry(int);

#endif /* filesys/cache.h */
