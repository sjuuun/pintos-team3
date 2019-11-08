#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "vm/page.h"

/* function prototypes */
static void syscall_handler (struct intr_frame *);

/* Check if input is user address */
static struct vm_entry *
is_user_address (void *addr)
{
  if ( !(((uint32_t) addr >= 0x8048000) && ((uint32_t)addr <= 0xbffffffb)) )
    exit(-1);
  
  /* Find corresponding vm_entry*/
  return find_vme(addr);
}

/* Check validation of buffer in read system call. */
static void
is_valid_buffer (void *buffer, unsigned size, void *esp) //, bool to_write)
{
  //if (buffer < esp)
  //  exit(-1);

  unsigned i;
  int tmp = pg_no(buffer);
  for (i = 0; i < size; ) {
  //for (i = 0; i <= (size / PGSIZE + 1); i++) {
    // Check writable?
    struct vm_entry *vme = is_user_address(buffer + i);
    if (vme == NULL)
      exit(-1);
    tmp = pg_no(buffer+i);
    while(tmp == pg_no(buffer+i))
      i++;
  }
}

/* Check validation of char * in exec, create, open, write system call. */
static void
is_valid_char (const char *str, void *esp)
{
  //if ((void *)str < esp)
  //  exit(-1);

  struct vm_entry *vme = is_user_address((void *)str);
  if (vme == NULL)
    exit(-1);
}

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
  /* Return the size, in bytes, of the file open as fd
     Use off_t file_length(struct file *file) */
  return (int) file_length(thread_current()->fdt[fd]);
}

int
read (int fd, void *buffer, unsigned size)
{
  /* Use uint8_t input_getc(void) for fd = 0, otherwise
     use off_t file_read(struct file *file, void *buffer, off_t size) */
  lock_acquire(&filesys_lock);

  if (fd == 0)
    return input_getc();
  else
    return (int) file_read(thread_current()->fdt[fd], buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  /* Use void putbuf(const char *buffer, size_t n) for fd = 1, otherwise
     use off_t file_write(struct file *file, const void *buffer, off_t size) */
  lock_acquire(&filesys_lock);

  if (fd == 1) {
    putbuf((char *)buffer, size);
    return size;
  }
  else {
    return file_write(thread_current()->fdt[fd], (char *)buffer, size);
  }
}

void
seek (int fd, unsigned position)
{
  /* Changes the next byte to be read or written in open file fd to position
     Use void file_seek(struct file *file, off_t new_pos */
  return file_seek(thread_current()->fdt[fd], position);
}

unsigned
tell (int fd)
{
  /* Return the position of next byte to be read or written in open file fd
     Use off_t file_tell(struct file *file) */
  return file_tell(thread_current()->fdt[fd]);
}

void
close (int fd)
{
  /* Use void file_close(struct file *file) */
  lock_acquire(&filesys_lock);

  struct thread *cur = thread_current();
  if (cur->fdt[fd] != NULL) {
    file_close(cur->fdt[fd]);
    cur->fdt[fd] = NULL;
    if (fd < cur->next_fd)
      cur->next_fd = fd;
  }
}

/* Get argument from esp. */
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

/* Actual System call hander call System call */
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
      f->eax = exec((const char *)arg[0]);
      break;

    case SYS_WAIT:
      get_argument(esp, arg, 1);
      f->eax = wait((int)arg[0]);
      break;

    /* File related system calls */
    case SYS_CREATE:
      get_argument(esp, arg, 2);
      is_valid_char((const char *)arg[0], esp);
      f->eax = create((const char *)arg[0], (unsigned)arg[1]);
      break;

    case SYS_REMOVE:
      get_argument(esp, arg, 1);
      f->eax = remove((const char *)arg[0]);
      break;

    case SYS_OPEN:
      get_argument(esp, arg, 1);
      is_valid_char((const char *)arg[0], esp);
      f->eax = open((const char *)arg[0]);
      lock_release(&filesys_lock);
      break;

    case SYS_FILESIZE:
      get_argument(esp, arg, 1);
      f->eax = filesize((int)arg[0]);
      break;

    case SYS_READ:
      get_argument(esp, arg, 3);
      is_valid_buffer((void *)arg[1], (unsigned)arg[2], esp);
      f->eax = read((int)arg[0], (void *)arg[1], (unsigned)arg[2]);
      lock_release(&filesys_lock);
      break;

    case SYS_WRITE:
      get_argument(esp, arg, 3);
      is_valid_char((const char *)arg[1], esp);
      f->eax = write((int)arg[0], (const void *)arg[1], (unsigned)arg[2]);
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

    default:
      break;

  }
}
