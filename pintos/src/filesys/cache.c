#include "filesys/cache.h"
#include "device/block.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"

#define CACHE_SECTOR_NUMBER 64

struct cache_entry buffer_cache[CACHE_SECTOR_NUMBER];

/* Initialize buffer cache. Allocate 32KB cache memory and match it with 
	 each entry. If fail to allocate, exit(-1). Called in threads/init.c */
void
bc_init(void)
{
  int i;
  for (i=0; i < CACHE_SECTOR_NUMBER; i++){
    void *caddr = malloc(BLOCK_SECTOR_SIZE);
    if (caddr == NULL) {
      exit(-1);
  }
  buffer_cache[i].cache_addr = caddr;
  buffer_cache[i].isempty = true;
  buffer_cache[i].isdirty = false;
  buffer_cache[i].inode = NULL;
  }
}

/* Destory buffer cache. Scan cache_entries, if entry is dirty, flush it 
	 to Disk. */
void
bc_exit(void)
{
  int i;
  for(i=0; i < CACHE_SECTOR_NUMBER; i++){
    if (buffer_cache[i].isdirty == true) {
      bc_flush_entry(i);
    }
    free(buffer_cache[i].cache_addr); 
  }
}

bool
bc_read()
{

}

bool
bc_write()
{

}


/* Look up buffer cache and find cache_entry that has sector. If no matching 
	 cache entry exists, return -1. */
int
bc_lookup(block_sector_t sector)
{
  int i;
  for(i=0; i < CACHE_SECTOR_NUMBER; i++) {
    if (buffer_cache[i].sector == sector) {
      return i;
    }
  }
  return -1;
}

/* */
int
bc_select_victin (void)
{
  

}

/* */
void
bc_flush_entry(int index)
{
  struct block *block = block_get_role(BLOCK_FILESYS);
  block_sector_t sector = buffer_cache[index].sector;
  block_write(block, sector, buffer_cache[index].cache_addr);
  buffer_cache[index].isdirty = false;
}


