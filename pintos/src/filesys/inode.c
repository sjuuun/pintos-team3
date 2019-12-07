#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <stdbool.h>
#include <string.h>
#include "devices/block.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Block number in inode_disk */
#define DIRECT_BLOCK_ENTRIES 123
#define INDIRECT_BLOCK_ENTRIES 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //uint32_t unused[125];               /* Not used. */
    block_sector_t direct_block[DIRECT_BLOCK_ENTRIES];
    block_sector_t indirect_block;
    block_sector_t double_indirect_block;

    uint32_t isfile;			/* Indicating if it is file. */
  };

/* Structure of indirect block. */
struct inode_indirect_block
  {
    block_sector_t table[INDIRECT_BLOCK_ENTRIES];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Function prototypes */
static bool inode_extend_file(struct inode_disk *, off_t);
static void free_inode_sectors (struct inode_disk *);

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct lock extend_lock;		/* Use for extending file length. */
    struct inode_disk data;             /* Inode content. */
  };

/* Get related inode_disk from inode. Store inode_disk in input. */
/*
static void
get_disk_inode (const struct inode *inode, struct inode_disk *inode_disk)
{
  bc_read(inode->sector, inode_disk, BLOCK_SECTOR_SIZE, 0);
}
*/

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode_disk *inode_disk, off_t pos)
{
  ASSERT (inode_disk != NULL);
  ASSERT (pos <= inode_disk->length);
  /*
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
  */

  off_t pos_sector = pos / BLOCK_SECTOR_SIZE;
  block_sector_t result_sec;
  /* Direct Access */
  if (pos_sector < DIRECT_BLOCK_ENTRIES)
  {
    result_sec = inode_disk->direct_block[pos_sector];
  }
  /* Indirect Access */
  else if (pos_sector < (off_t)(DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES))
  {
    struct inode_indirect_block *indirect = malloc(sizeof *indirect);
    off_t index1, remain;
    if (indirect == NULL)
      return -1;

    index1 = inode_disk->indirect_block;
    remain = pos_sector - DIRECT_BLOCK_ENTRIES;
    bc_read(index1, indirect, BLOCK_SECTOR_SIZE, 0);
    result_sec = indirect->table[remain];
    free(indirect);
  }
  /* Double Indirect Access */
  else if (pos_sector < (off_t)(DIRECT_BLOCK_ENTRIES +
			INDIRECT_BLOCK_ENTRIES * (INDIRECT_BLOCK_ENTRIES + 1)))
  {
    struct inode_indirect_block *indirect = malloc(sizeof *indirect);
    off_t index1, index2, remain;
    if (indirect == NULL)
      return -1;

    index1 = inode_disk->double_indirect_block;
    remain = pos_sector - (DIRECT_BLOCK_ENTRIES + INDIRECT_BLOCK_ENTRIES);
    bc_read(index1, indirect, BLOCK_SECTOR_SIZE, 0);

    index2 = indirect->table[remain / INDIRECT_BLOCK_ENTRIES];
    remain %= INDIRECT_BLOCK_ENTRIES;
    bc_read(index2, indirect, BLOCK_SECTOR_SIZE, 0);
    result_sec = indirect->table[remain];
    free(indirect);
  }
  else
  {
    result_sec = -1;
  }

  return result_sec;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_file)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      //size_t sectors = bytes_to_sectors (length);
      //disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->indirect_block = 0;
      disk_inode->double_indirect_block = 0;
      if (is_file)
        disk_inode->isfile = REGULAR_FILE;
      else
        disk_inode->isfile = DIRECTORY;

      if (length > 0) {
        inode_extend_file(disk_inode, length);
      }
      /*
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        }
      */
      bc_write(sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
      free (disk_inode);
      success = true;
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->extend_lock);
  //block_read (fs_device, inode->sector, &inode->data);
  bc_read (inode->sector, &inode->data, BLOCK_SECTOR_SIZE, 0);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          /*
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.start,
                            bytes_to_sectors (inode->data.length)); 
          */
          free_inode_sectors(&inode->data);
          free_map_release(inode->sector, 1);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  //uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (&inode->data, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      /*
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
         
        }
      */
      bc_read(sector_idx, buffer + bytes_read, chunk_size, sector_ofs);
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  //uint8_t *bounce = NULL;
  struct inode_disk *disk_inode = &inode->data;

  if (inode->deny_write_cnt)
    return 0;

  /* Extend file if needed */
  lock_acquire(&inode->extend_lock);
  off_t old_length = disk_inode->length;
  off_t write_end = offset + size;

  if (write_end > old_length) {
    inode_extend_file(disk_inode, write_end);
  }
  bc_write(inode->sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
  lock_release(&inode->extend_lock);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (disk_inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      //if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //  {
      //    /* Write full sector directly to disk. */
      //    block_write (fs_device, sector_idx, buffer + bytes_written);
      //  }
      //else 
      //  {
      //    /* We need a bounce buffer. */
      //    if (bounce == NULL) 
      //      {
      //        bounce = malloc (BLOCK_SECTOR_SIZE);
      //        if (bounce == NULL)
      //          break;
      //      }

      //    /* If the sector contains data before or after the chunk
      //       we're writing, then we need to read in the sector
      //       first.  Otherwise we start with a sector of all zeros. */
      //    if (sector_ofs > 0 || chunk_size < sector_left) 
      //      block_read (fs_device, sector_idx, bounce);
      //    else
      //      memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //    memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //    block_write (fs_device, sector_idx, bounce);
      //  }

      bc_write(sector_idx, buffer + bytes_written, chunk_size, sector_ofs);

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  //bc_write(inode->sector, disk_inode, BLOCK_SECTOR_SIZE, 0);
  //free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  /*
  struct inode_disk *inode_disk = malloc(sizeof *inode_disk);
  get_disk_inode(inode, inode_disk);
  off_t result = inode_disk->length;
  free(inode_disk);
  return result;
  */
  return inode->data.length;
}

/* Update new sector number in disk_inode. */
static bool
register_sector (struct inode_disk *disk_inode, block_sector_t new_sector)
{
  block_sector_t *update = NULL;
  block_sector_t ind1, ind2;
  struct inode_indirect_block *indirect = NULL;
  struct inode_indirect_block *double_indirect = NULL;
  size_t i, j;

  /* Iterate to find empty table. */
  /* Direct access. */
  for (i = 0; i < DIRECT_BLOCK_ENTRIES; i++) {
    if (disk_inode->direct_block[i] == 0) {
      update = &disk_inode->direct_block[i];
      goto update;
    }
  }
  /* Indirect access. */
  indirect = malloc(sizeof *indirect);
  if (indirect == NULL) return false;

  ind1 = disk_inode->indirect_block;
  if (ind1 == 0) {		// First indirect access
    block_sector_t new_indirect;
    if (free_map_allocate(1, &new_indirect)) {
      static char zeros[BLOCK_SECTOR_SIZE];
      bc_write (new_indirect, zeros, BLOCK_SECTOR_SIZE, 0);
      disk_inode->indirect_block = new_indirect;
      ind1 = new_indirect;
    }
    else {
      free(indirect);
      return false;
    }
  }

  bc_read(ind1, indirect, BLOCK_SECTOR_SIZE, 0);
  for (i = 0; i < INDIRECT_BLOCK_ENTRIES; i++) {
    if (indirect->table[i] == 0) {
      update = &indirect->table[i];
      goto update;
    }
  }

  /* Double indirect access. */
  double_indirect = malloc(sizeof *indirect);
  if (double_indirect == NULL) {
    free(indirect);
    return false;
  }

  ind1 = disk_inode->double_indirect_block;
  if (ind1 == 0) {		// First indirect access
    block_sector_t new_indirect;
    if (free_map_allocate(1, &new_indirect)) {
      static char zeros[BLOCK_SECTOR_SIZE];
      bc_write (new_indirect, zeros, BLOCK_SECTOR_SIZE, 0);
      disk_inode->double_indirect_block = new_indirect;
      ind1 = new_indirect;
    }
    else {
      free(indirect);
      return false;
    }
  }

  bc_read(ind1, indirect, BLOCK_SECTOR_SIZE, 0);
  for (i = 0; i < INDIRECT_BLOCK_ENTRIES; i++) {
    ind2 = indirect->table[i];
    if (ind2 == 0) {		// First indirect access
      block_sector_t new_indirect;
      if (free_map_allocate(1, &new_indirect)) {
        static char zeros[BLOCK_SECTOR_SIZE];
        bc_write (new_indirect, zeros, BLOCK_SECTOR_SIZE, 0);
        indirect->table[i] = new_indirect;
        ind2 = new_indirect;
      }
      else {
        free(indirect);
        return false;
      }
    }

    bc_read(ind2, double_indirect, BLOCK_SECTOR_SIZE, 0);
    for (j = 0; j < INDIRECT_BLOCK_ENTRIES; j++) {
      if (double_indirect->table[j] == 0) {
        update = &double_indirect->table[j];
        goto update;
      }
    }
  }

  update:
    ASSERT (update != NULL);
    *update = new_sector;
    if (indirect != NULL) {
      bc_write(ind1, indirect, BLOCK_SECTOR_SIZE, 0);
      free(indirect);
    }
    if (double_indirect != NULL) {
      bc_write(ind2, double_indirect, BLOCK_SECTOR_SIZE, 0);
      free(double_indirect);
    }
    return true;
}

/* inode extend file length */
static bool
inode_extend_file (struct inode_disk *disk_inode, off_t pos)
{
  ASSERT (pos > disk_inode->length);
  off_t start = (off_t) bytes_to_sectors(disk_inode->length);
  off_t end = (off_t) bytes_to_sectors(pos);
  block_sector_t new_sector;

  while (start < end) {
    if (free_map_allocate (1, &new_sector))
      {
        static char zeros[BLOCK_SECTOR_SIZE];
        bc_write (new_sector, zeros, BLOCK_SECTOR_SIZE, 0);

        /* Register new sector */
        if (!register_sector(disk_inode, new_sector)) {
          return false;
        }
      }
    else
      {
        return false;
      }
    start++;
  }
  disk_inode->length = pos;
  return true;
}

static void
free_inode_sectors (struct inode_disk *disk_inode)
{
  /* Free double indirect blocks if exist */ 
  if(disk_inode->double_indirect_block > 0) {
    int i = 0;
    struct inode_indirect_block *ind_block1 = malloc(sizeof *ind_block1);
    bc_read(disk_inode->double_indirect_block, ind_block1, BLOCK_SECTOR_SIZE, 0);
    while (ind_block1->table[i] > 0) {
      int j = 0;
      struct inode_indirect_block *ind_block2 = malloc(sizeof *ind_block2);
      bc_read(ind_block1->table[i], ind_block2, BLOCK_SECTOR_SIZE, 0);
      while (ind_block2->table[j] > 0) {
        free_map_release(ind_block2->table[j], 1);
        j++;
      }
      free(ind_block2);
      free_map_release(ind_block1->table[i], 1);
      i++;
    }
    free(ind_block1);
    free_map_release(disk_inode->double_indirect_block, 1);
  }
  /* Free indirect blocks if exist */
  if(disk_inode->indirect_block > 0) {
    int i = 0;
    struct inode_indirect_block *ind_block1 = malloc(sizeof *ind_block1);
    bc_read(disk_inode->double_indirect_block, ind_block1, BLOCK_SECTOR_SIZE, 0);
    while (ind_block1->table[i] > 0) {
      free_map_release(ind_block1->table[i], 1);
      i++;
    }
    free(ind_block1);
    free_map_release(disk_inode->indirect_block, 1);
  }
  /* Free direct blocks */
  int i = 0;
  while(disk_inode->direct_block[i] > 0) {
    free_map_release(disk_inode->direct_block[i],1);
    i++;
  }
}
