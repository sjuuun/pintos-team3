#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "lib/string.h"


/* function prototypes */
static void syscall_handler (struct intr_frame *);

/* Check if input is user address */
bool
is_user_address (void *addr)
{
  return ((uint32_t) addr >= 0x8048000) && ((uint32_t)addr <= 0xbffffffb);
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
  if (!is_user_address((void *)cmd_line))
    exit (-1);

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
  if (!is_user_address((void *)file))
    exit(-1);

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
  if (!is_user_address((void *)file))
    exit(-1);
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

  if (!is_user_address(buffer))
    exit(-1);
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
  if (!is_user_address((void *)buffer))
    exit(-1);
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
  struct thread *cur = thread_current();
  if (cur->fdt[fd] != NULL) {
    file_close(cur->fdt[fd]);
    cur->fdt[fd] = NULL;
    if (fd < cur->next_fd)
      cur->next_fd = fd;
  }
}

/* Actual System call hander call System call */
static void
syscall_handler (struct intr_frame *f)
{
  void *esp = f->esp;
  int number = *(int *)esp;
  if (!is_user_address(esp))
    exit(-1);

  /* System Call */
  switch (number) {
    /* Process related system calls */
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      exit(*((int *)esp + 1));
      break;

    case SYS_EXEC:
      f->eax = exec(*((char **)esp + 1));
      break;

    case SYS_WAIT:
      f->eax = wait(*((int *)esp + 1));
      break;

    /* File related system calls */
    case SYS_CREATE:
      f->eax = create(*((char **)esp + 1), *((int *)esp + 2));
      break;

    case SYS_REMOVE:
      f->eax = remove(*((char **)esp + 1));
      break;

    case SYS_OPEN:
      f->eax = open(*((char **)esp + 1));
      break;

    case SYS_FILESIZE:
      f->eax = filesize(*((int *)esp +1));
      break;

    case SYS_READ:
      f->eax = read(*((int *)esp + 1), *((void **)esp + 2), *((int *)esp +3));
      lock_release(&filesys_lock);
      break;

    case SYS_WRITE:
      f->eax = write(*((int *)esp+1), *((char **)esp+2), *((int *)esp+3));
      lock_release(&filesys_lock);
      break;

    case SYS_SEEK:
      seek(*((int *)esp +1), *((int *)esp + 2));
      break;

    case SYS_TELL:
      f->eax = tell(*((int *)esp +1));
      break;

    case SYS_CLOSE:
      close(*((int *)esp + 1));
      break;

    default:
      break;
  }
}
