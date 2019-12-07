#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "vm/page.h"
#include "vm/swap.h"

/* function prototypes */
static void syscall_handler (struct intr_frame *);

/* Check if input is user address. Return corresponding vm_entry */
static struct vm_entry *
is_user_address (void *addr)
{
  if ( !(((uint32_t) addr >= 0x8048000) && ((uint32_t)addr <= 0xbffffffb)) )
    exit(-1);
  
  return find_vme(addr);
}

/* Check validation whole buffer address in read system call.
   If it is valid address but no matching vm_entry exist,
   compare with esp and call grow_stack() or exit(-1). */
static void
is_valid_buffer (void *buffer, unsigned size, void *esp)
{
  unsigned i;
  unsigned tmp = pg_no(buffer);
  for (i = 0; i < size; ) {
    struct vm_entry *vme = is_user_address(buffer + i);
    if (vme == NULL) {
      if ((uint32_t)buffer + i >= (uint32_t)esp)
        grow_stack(buffer + i);
      else
        exit(-1);
    }
    tmp = pg_no(buffer+i);
    while(tmp == pg_no(buffer+i))
      i++;
  }
}

/* Check validation of char * in exec, create, open, write system call.
   Also, if it is valid address but no matching vm_entry exist,
   compare with esp and call grow_stack() or exit(-1). */
static void
is_valid_char (const char *str, void *esp)
{
  struct vm_entry *vme = is_user_address((void *)str);
  if (vme == NULL) {
    if ((uint32_t)str >= (uint32_t) esp)
      grow_stack((void *)str);
    else
      exit(-1);
  }
}

/* Initialize syscall_handler and filesys_lock */
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

/* System call handler functions - Process related */
void
halt (void)
{
  /* Shutdown Pintos. */
  shutdown_power_off();
}

void
exit (int status)
{
  struct thread *cur = thread_current();

  /* Save exit status at process descriptor */
  cur->exit_status = status;
  printf("%s: exit(%d)\n", cur->name, status);
  thread_exit();
}

pid_t
exec (const char *cmd_line)
{
  /* Create child process and execute program */
  tid_t tid = process_execute(cmd_line);

  struct thread *child = get_child_process(tid);
  if (child == NULL)
    return -1;
  sema_down(&child->load_sema);

  if (child->load_status == 0)
    return tid;
  else
    return -1;
}

int
wait (pid_t pid)
{
  /* Wait for termination of child process whose process id is pid */
  return process_wait(pid);
}

/* System call handler functions - File related */
bool
create (const char *file, unsigned initial_size)
{
  /* Create file which have size of initial_size
     Use bool filesys_create(const char *name, off_t initial_size) */
  return filesys_create(file, initial_size);
}

bool
remove (const char *file)
{
  /* Remove file whose name is file
     Use bool filesys_remove(const char *name) */
  return filesys_remove(file);
}

int
open (const char *file)
{
  /* Open the file corresponds to path in file
     Use struct file *filesys_open(const char *name) */
  lock_acquire(&filesys_lock);

  struct thread *cur = thread_current();
  if (cur->next_fd == 64)
    return -1;

  struct file *f = filesys_open(file);
  if (f == NULL)
    return -1;
  cur->fdt[cur->next_fd] = f;
  int fd = cur->next_fd;
  while (cur->fdt[cur->next_fd] != NULL) {
    cur->next_fd++;
  }
  return fd;
}

int
filesize(int fd)
{
  /* Return the size, in bytes, of the file open as fd */
  return (int) file_length(thread_current()->fdt[fd]);
}

int
read (int fd, void *buffer, unsigned size)
{
  lock_acquire(&filesys_lock);
  if (fd == 0)
    return input_getc();
  else
    return (int) file_read(thread_current()->fdt[fd], buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  lock_acquire(&filesys_lock); 
  if (fd == 1) {
    putbuf((char *)buffer, size);
    return size;
  }
  else
    return file_write(thread_current()->fdt[fd], (char *)buffer, size);
}

void
seek (int fd, unsigned position)
{
  /* Change the next byte to be read or written in open file fd to position */
  return file_seek(thread_current()->fdt[fd], position);
}

unsigned
tell (int fd)
{
  /* Return the position of next byte to be read or written in open file fd */
  return file_tell(thread_current()->fdt[fd]);
}

void
close (int fd)
{
  lock_acquire(&filesys_lock);
  struct thread *cur = thread_current();
  if (cur->fdt[fd] != NULL) {
    file_close(cur->fdt[fd]);
    cur->fdt[fd] = NULL;
    if (fd < cur->next_fd)
      cur->next_fd = fd;
  }
}

/* For Memory-mapped file system */

/* Memory map system call. Return allocated mapping id */
int
mmap (int fd, void *addr)
{
  /* Check validation of input */
  if (fd < 2 || fd > 63)
    return -1;
  if (!is_user_vaddr(addr) || addr == 0 || ((uint32_t)addr % PGSIZE) != 0)
    return -1;
  if (find_vme(addr) != NULL) return -1;

  struct file *m_file = file_reopen(process_get_file(fd));
  if(m_file == NULL || file_length(m_file) == 0)
    return -1;

  /* Allocate mmap_file structure and initialize it */
  struct mmap_file *mmf = malloc(sizeof (struct mmap_file));
  mmf->file = m_file;
  mmf->mapid = fd;
  list_init(&mmf->vme_list);
  
  /* Allocate and initialize vm_entry for mmap file */ 
  int i, iter = file_length(m_file) / PGSIZE;
  for(i = 0; i <= iter; i++) {
    struct vm_entry *vme = malloc(sizeof (struct vm_entry));
    vme->vaddr = addr + PGSIZE*i;
    vme->file = m_file;
    vme->vp_type = VP_FILE;
    vme->offset = PGSIZE*i;
    if(i == iter) {
      vme->read_bytes = file_length(m_file) % PGSIZE;
      vme->zero_bytes = PGSIZE - vme->read_bytes;
    }
    else {
      vme->read_bytes = PGSIZE;
      vme->zero_bytes = 0;
    }
    vme->writable = true;
    list_push_front(&mmf->vme_list, &vme->mmap_elem);
  }
  /* Insert mmap_file in thread's mmap_list */
  list_push_front(&thread_current()->mmap_list, &mmf->mf_elem);
  return mmf->mapid;
}

/* Remove and free input mmap_file's vm_entries. 
   if the page is dirty, write to file. */
void
do_munmap (struct mmap_file *m_file)
{
  while(!list_empty(&m_file->vme_list)) {
    struct list_elem *fr = list_front(&m_file->vme_list);
    struct vm_entry *vme = list_entry(fr, struct vm_entry, mmap_elem);
    void *addr = vme->vaddr;
    if (pagedir_is_dirty(thread_current()->pagedir, addr)) {
      lock_acquire(&filesys_lock);
      file_write_at(m_file->file, addr, vme->read_bytes, 
			vme->offset);
      lock_release(&filesys_lock);
    }
    list_remove(fr);
    if(pagedir_get_page(thread_current()->pagedir, addr) != NULL)
      free_page(pagedir_get_page(thread_current()->pagedir, addr));
    free(vme);
  }
}

/* Remove mmap_file and close the file, free the mmap_file structure. 
   if mapid is EXIT, do this for entire mmap_list's element in current
   running threads. */
void
munmap (mapid_t mapid)
{
  struct thread *cur = thread_current();
  struct list *m_list = &cur->mmap_list;
  struct list_elem *e, *e_next;
  if (!list_empty(m_list)) {
    e = list_front(m_list);
    while(!list_empty(m_list)) {
      e_next = list_next(e);
      struct mmap_file *m_file = list_entry(e, struct mmap_file, mf_elem);
      /* If matched, do munmap */
      if (m_file->mapid == mapid || mapid == EXIT) {
        do_munmap(m_file);
        list_remove(&m_file->mf_elem);
        file_close(m_file->file);
        free(m_file);
        if (mapid != EXIT) break;
      }
      e = e_next;
    }
  }
}

/* System call for project 4. */
/* Changes the current working directory of the process to input.
   Directory name might be relative or absolute.
   Return true if successful, false on failure. */
bool
chdir (const char *dir)
{
  struct dir *change;
  struct dir *new;
  struct inode *new_inode;
  char dirname[NAME_MAX + 1];
  change = parse_path(dir, dirname);

  if (change == NULL)
    return false;

  if (!dir_lookup(change, dirname, &new_inode))
    return false;

  new = dir_open(new_inode);
  if (new == NULL)
    return false;
  else {
    dir_close(thread_current()->directory);
    thread_current()->directory = new;
    return true;
  }
}

/* Creates the directory named input. Also, it can be relative or absolute.
   Return true if successful, false on failure.
   Fail if input already exists. */
bool
mkdir (const char *dir)
{
  if (filesys_create_dir(dir))
    return true;
  else
    return false;
}

/* Reads a directory entry from file descriptor fd, which must represent
   a directory. If successful, store the null-terminated file name in input,
   which must have room for READDIR_MAX_LEN + 1 bytes, and return true.
   If no entries are left in the directory, returns false. */
bool
readdir (int fd, char *name)
{
  struct file *file = thread_current()->fdt[fd];
  struct inode *inode = file_get_inode(file);
  if (is_inode_file (inode))
    return false;

  struct dir *dir = dir_open(inode);
  if (dir_readdir(dir, name)) {
    dir_close(dir);
    return true;
  }
  else {
    dir_close(dir);
    return false;
  }
}

/* Returns true if fd represents a directory, false if it represents
   an regular file. */
bool
isdir (int fd)
{
  struct file *file = thread_current()->fdt[fd];
  struct inode *inode = file_get_inode(file);
  return !(is_inode_file (inode));
}

/* Returns the inode number of the inode associated with fd, which may
   represent an ordinary file or a directory.
   Inode number is unique during th file's existence.
   In PintOS project, the sector number is used as an inode number. */
int
inumber (int fd)
{
  struct file *file = thread_current()->fdt[fd];
  struct inode *inode = file_get_inode(file);
  return (int)inode_get_inumber (inode);
}

/* Check valid address of esp, and store argument in arg. */
static void
get_argument (void *esp, int *arg, int count)
{
  int i;
  for (i = 0; i < count; i++) {
    is_user_address(esp);
    arg[i] = *(int *)esp;
    esp += sizeof(int);
  }
}

/* Actual System call hanlder that call System call */
static void
syscall_handler (struct intr_frame *f)
{
  void *esp = f->esp;
  int number = *(int *)esp;
  int arg[3];
  is_user_address(esp);
  esp += sizeof(int);

  /* System Call */
  switch (number) {
    /* Process related system calls */
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      get_argument(esp, arg, 1);
      exit((int)arg[0]);
      break;

    case SYS_EXEC:
      get_argument(esp, arg, 1);
      is_valid_char((const char *)arg[0], esp);
      set_page_pflags((void *)arg[0], PAGE_IN_USE);
      f->eax = exec((const char *)arg[0]);
      set_page_pflags((void *)arg[0], PAGE_NOT_IN_USE);
      break;

    case SYS_WAIT:
      get_argument(esp, arg, 1);
      f->eax = wait((int)arg[0]);
      break;

    /* File related system calls */
    case SYS_CREATE:
      get_argument(esp, arg, 2);
      is_valid_char((const char *)arg[0], esp);
      set_page_pflags((void *)arg[0], PAGE_IN_USE);
      f->eax = create((const char *)arg[0], (unsigned)arg[1]);
      set_page_pflags((void *)arg[0], PAGE_NOT_IN_USE);
      break;

    case SYS_REMOVE:
      get_argument(esp, arg, 1);
      is_valid_char((const char *)arg[0], esp);
      set_page_pflags((void *)arg[0], PAGE_IN_USE);
      f->eax = remove((const char *)arg[0]);
      set_page_pflags((void *)arg[0], PAGE_NOT_IN_USE);
      break;

    case SYS_OPEN:
      get_argument(esp, arg, 1);
      is_valid_char((const char *)arg[0], esp);
      set_page_pflags((void *)arg[0], PAGE_IN_USE);
      f->eax = open((const char *)arg[0]);
      set_page_pflags((void *)arg[0], PAGE_NOT_IN_USE);
      lock_release(&filesys_lock);
      break;

    case SYS_FILESIZE:
      get_argument(esp, arg, 1);
      f->eax = filesize((int)arg[0]);
      break;

    case SYS_READ:
      get_argument(esp, arg, 3);
      is_valid_buffer((void *)arg[1], (unsigned)arg[2], esp);
      set_page_pflags((void *)arg[0], PAGE_IN_USE);
      f->eax = read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
      set_page_pflags((void *)arg[0], PAGE_NOT_IN_USE);
      lock_release(&filesys_lock);
      break;

    case SYS_WRITE:
      get_argument(esp, arg, 3);
      is_valid_char((const char *)arg[1], esp);
      set_page_pflags((void *)arg[0], PAGE_IN_USE);
      f->eax = write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
      set_page_pflags((void *)arg[0], PAGE_NOT_IN_USE);
      lock_release(&filesys_lock);
      break;

    case SYS_SEEK:
      get_argument(esp, arg, 2);
      seek((int)arg[0], (unsigned)arg[1]);
      break;

    case SYS_TELL:
      get_argument(esp, arg, 1);
      f->eax = tell((int)arg[0]);
      break;

    case SYS_CLOSE:
      get_argument(esp, arg, 1);
      close((int)arg[0]);
      lock_release(&filesys_lock);
      break;

    case SYS_MMAP:
      get_argument(esp, arg, 2);
      f->eax = mmap((int)arg[0], (void *)arg[1]);
      break;
    
    case SYS_MUNMAP:
      get_argument(esp, arg, 1);
      munmap((mapid_t)arg[0]);
      break;

    case SYS_CHDIR:
      get_argument(esp, arg, 1);
      is_valid_char((const char *)arg[0], esp);
      f->eax = chdir((const char *)arg[0]);
      break;

    case SYS_MKDIR:
      get_argument(esp, arg, 1);
      is_valid_char((const char *)arg[0], esp);
      f->eax = mkdir((const char *)arg[0]);
      break;

    case SYS_READDIR:
      get_argument(esp, arg, 2);
      is_valid_char((const char *)arg[1], esp);
      f->eax = readdir((int)arg[0], (char *)arg[1]);
      break;

    case SYS_ISDIR:
      get_argument(esp, arg, 1);
      f->eax = isdir((int)arg[0]);
      break;

    case SYS_INUMBER:
      get_argument(esp, arg, 1);
      f->eax = inumber((int)arg[0]);
      break;

    default:
      break;

  }
}
