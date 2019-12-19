#include "filesys/cache.h"
#include "devices/block.h"
#include "filesys/filesys.h"
#include "lib/string.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"

#define CACHE_SECTOR_NUMBER 64

/* buffer cache entry's structure */
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
    buffer_cache[i].isempty = true;
    buffer_cache[i].isdirty = false;
    buffer_cache[i].clock = false;
    buffer_cache[i].cache_addr = caddr;
    buffer_cache[i].sector = 0;
  }
}

/* Destory buffer cache. Scan cache_entries, if entry is dirty, flush it 
	 to Disk. Free all the memory spaces allocated for buffer cache. */
void
bc_exit(void)
{
  bc_flush_all();

  int i;
  for(i=0; i < CACHE_SECTOR_NUMBER; i++){
    free(buffer_cache[i].cache_addr); 
  }
}

/* Get next empty buffer cache. Return index of empty buffer cache. If cache
   full. return -1. */
static int
get_cache_entry(void)
{
  int i;
  for (i=0; i < CACHE_SECTOR_NUMBER; i++) {
    if (buffer_cache[i].isempty == true)
      return i;
  }
  return -1;
}

/* Look up buffer cache and find cache_entry that has sector. If no matching 
	 cache entry exists, return -1. */
int
bc_lookup(block_sector_t sector)
{
  int i;
  for(i=0; i < CACHE_SECTOR_NUMBER; i++) {
    if (!buffer_cache[i].isempty && (buffer_cache[i].sector == sector)) {
      return i;
    }
  }
  return -1;
}

/* Select victim entry to evict when cache is full. Return index of victim 
   cache entry. Based on Clock algorithm. If buffer cache entry's clock bit
   is true, change clock bit to false and pass. If clock bit is false, check
   if that entry is dirty, flush it to disk and return. */
int
bc_select_victim (void)
{
  int i;
  for (i=0; i < CACHE_SECTOR_NUMBER; i++) {
    if (!buffer_cache[i].clock) {
      if (buffer_cache[i].isdirty == true) {
        bc_flush_entry(i);
      }
      return i;
    }
    else {
      buffer_cache[i].clock = false;
    }
  }
  if (buffer_cache[0].isdirty)
    bc_flush_entry(0);
  return 0;
}

/* Flush buffer cache index 'index' to disk. Use block_write function. */
void
bc_flush_entry(int index)
{
  block_sector_t sector = buffer_cache[index].sector;
  block_write(fs_device, sector, buffer_cache[index].cache_addr);
  buffer_cache[index].isdirty = false;
  buffer_cache[index].isempty = true;
}

/* Flush all dirty sectors in buffer cache to disk. */
void
bc_flush_all(void)
{
  int i;
  for(i=0; i < CACHE_SECTOR_NUMBER; i++){
    if (buffer_cache[i].isdirty == true && buffer_cache[i].isempty == false) {
      bc_flush_entry(i);
    }
  }
}

/* Read sector to buffer in buffer cache. If no such sector exist in buffer 
   cache, allocate new buffer cache and read sector from disk to buffer 
   cache. */
void
bc_read(block_sector_t sector, void *buffer, int chunk_size, int sector_ofs)
{
  int index = bc_lookup(sector);
  if (index == -1) {
    index = get_cache_entry();
    if (index == -1) {
      index = bc_select_victim(); 
    }
    block_read(fs_device, sector, buffer_cache[index].cache_addr);
    buffer_cache[index].sector = sector;
    buffer_cache[index].isempty = false;
  }
  uint8_t *c_addr = buffer_cache[index].cache_addr;
  memcpy(buffer, c_addr + sector_ofs, chunk_size);
  buffer_cache[index].clock = true;
}

/* Write buffer to sector in buffer cache. If no such sector exist in buffer
   cache, allocate new buffer cache and read sector from disk to buffer cache.
   Then, write buffer to buffer cache who have 'sector', and mark dirty bit to
   true. */
void
bc_write(block_sector_t sector, const void *buffer, int chunk_size,
                                                    int sector_ofs)
{
  int index = bc_lookup(sector);
  if (index == -1) {
    index = get_cache_entry();
    if (index == -1) {
      index = bc_select_victim();
    }
    block_read(fs_device, sector, buffer_cache[index].cache_addr);
    buffer_cache[index].sector = sector;
    buffer_cache[index].isempty = false;
  }
  uint8_t *c_addr = buffer_cache[index].cache_addr;
  memcpy(c_addr + sector_ofs, buffer, chunk_size);
  buffer_cache[index].isdirty = true;
  buffer_cache[index].clock = true;
}
