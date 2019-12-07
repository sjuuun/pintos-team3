#include "filesys/filesys.h"
#include <debug.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  bc_init();

  if (format) 
    do_format ();

  free_map_open ();

  thread_current()->directory = dir_open_root();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();

  bc_exit();
}


struct dir *
parse_path(const char *name, char *filename)
{
  name : sadasd/gd/file
  name1 : asdasd/file
  /* Copy name to name_cp for tokenizing */
  char *name_cp = malloc(strlen(name));
  strlcpy(name_cp, name, strlen(name));
  /* If name contains root dir */
  struct dir *dir;
  char *token, *save_ptr;
  if (name_cp[0] == '/') {
    dir = dir_open_root();
    name_cp++; 
  }
  else {
    dir = dir_reopen(thread_current()->directory);
  }

  int count = 0, i = 0;
  char *iter = name_cp;
  while(*iter != '\0') {
    if (*iter == '/')
      count++;
    iter++;
  }
  count++;

  char *parse[count];
  for (token = strtok_r (name_cp, "/", &save_ptr); token != NULL; 
        token = strtok_r (NULL, "/", &save_ptr)) {
    parse[i] = token;
    i++;
  }

  struct inode *inode;
  for (i = 0; i < count - 1 ; i++) {
    if (!dir_lookup(dir, parse[i], &inode)) {
      // free name_cp
      dir_close(dir);
      return NULL;
    }
    dir_close(dir);
    dir = dir_open(inode);
  }
  strlcpy(filename, parse[i], strlen(parse[i]));
  return dir;
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();
  char *filename = malloc(strlen(name));
  struct dir *dir = parse_path(name, filename);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, true)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free(filename);
  return success;
}

/* Create a directory named NAME. Returns true if successful,
   false otherwise. Fails if a directory named NAME already
   exists, or if internal memory allocation fails. */
bool
filesys_create_dir (const char *name)
{
  block_sector_t inode_sector = 0;
  char *dirname[NAME_MAX + 1];
  struct dir *dir = parse_path(name, dirname);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, false)
                  && dir_add (dir, dirname, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
