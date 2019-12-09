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
  struct dir *rootdir = dir_open_root();
  dir_add_basic (rootdir, rootdir);
  thread_current()->directory = rootdir;
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
  /* Copy name to name_cp for tokenizing */
  char *name_cp = calloc(1, strlen(name)+1);
  strlcpy(name_cp, name, strlen(name)+1);
  /* If name contains root dir or not */
  struct dir *dir;
  char *token, *save_ptr;
  //struct inode *tmp_inode;
  if (name_cp[0] == '/') {
    dir = dir_open_root();
    name_cp++; 
  }
  else {
    if (thread_current()->directory == NULL)
      return NULL;
    dir = dir_reopen(thread_current()->directory);
    //tmp_inode = dir_get_inode(dir);
    //if (tmp_inode->removed == true)
    //  return NULL;
  }
  
  /* If name contains just root dir sign (/) */ 
  if (strlen(name_cp) == 0) {
    return dir;
  }
   
  /* Iterate to find how many / in name */
  int count = 0, i = 0;
  char *iter = name_cp;
  while(*iter != '\0') {
    if (*iter == '/') {
      count++;
    }
    iter++;
  }
  count++;

  /* Parse and tokenize name */
  char *parse[count];
  for (token = strtok_r (name_cp, "/", &save_ptr); token != NULL; 
        token = strtok_r (NULL, "/", &save_ptr)) {
    parse[i] = token;
    i++;
  }

  /* Get directory */
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

  /* Copy filename into filename */
  if (filename != NULL)
    strlcpy(filename, parse[i], strlen(parse[i])+1);


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
  char *filename = calloc(1, strlen(name) + 1);
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
  char dirname[NAME_MAX + 1];
  struct dir *dir = parse_path(name, dirname);
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, 16)
                  && dir_add (dir, dirname, inode_sector));

  if (!success && inode_sector != 0) {
    free_map_release (inode_sector, 1);
    return success;
  }

  struct inode *new_inode;
  dir_lookup(dir, dirname, &new_inode);
  struct dir *new_dir = dir_open(new_inode);
  if (new_dir == NULL) return false;

  success = dir_add_basic(dir, new_dir);
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
  if (strlen(name) == 0)
    return NULL;
  char *filename = calloc(1, strlen(name) + 1);
  struct dir *dir = parse_path(name, filename);
  struct inode *inode = NULL;
  
  /* If dir doesn't exist */
  if (dir == NULL) {
    free(filename);
    return NULL;
  }

  /* If dir exist, but filename is empty */
  if (strlen(filename) == 0) {
    free(filename);
    return file_open(dir_get_inode(dir));
  }

  /* Normal case */
  else if (dir != NULL)
    dir_lookup (dir, filename, &inode);
  dir_close (dir);
  free(filename);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //struct dir *dir = dir_open_root ();
  bool success = false;
  if (!strcmp(name, ".") || !strcmp(name, ".."))
    return success;
  char *filename = calloc(1, strlen(name) + 1);
  struct dir *dir = parse_path(name, filename);
  success = dir != NULL && dir_remove (dir, filename);
  dir_close (dir); 
  free(filename);

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
