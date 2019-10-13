#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* System call handler functions - Process related */
void
halt (void)
{
  /* Use void shutdown_power_off(void) */
  shutdown_power_off();
}

void
exit (int status)
{
  /* Use void thread_exit(void) */
  struct thread *cur = thread_current();
  /* Save exit status at process descriptor */
  printf("%s: exit (%d) \n", cur->name, status);
  thread_exit();
}

pid_t
exec (const char *cmd_line)
{
  /* Create child process and execute program */
  /* process_execute? */
}

int
wait (pid_t pid)
{
  /* Wait for termination of child process whose process id is pid */
  /* process_wait? */
}

/* System call handler functions - File related */
bool
create (const char *file, unsigned initial_size)
{
  /* Create file which have size of initial_size */
  /* Use bool filesys_create(const char *name, off_t initial_size) */
}

bool
remove (const char *file)
{
  /* Remove file whose name is file */
  /* Use bool filesys_remove(const char *name) */
}

int
open (const char *file)
{
  /* Open the file corresponds to path in file */
  /* Use struct file *filesys_open(const char *name) */
}

int
filesize(int fd)
{
  /* Return the size, in bytes, of the file open as fd */
  /* Use off_t file_length(struct file *file) */
}

int
read (int fd, void *buffer, unsigned size)
{
  /* Use uint8_t input_getc(void) for fd = 0, otherwise
     use off_t file_read(struct file *file, void *buffer, off_t size) */
}

int
write (int fd, const void *buffer, unsigned size)
{
  /* Use void putbuf(const char *burrer, size_t n) for fd = 1, otherwise
     use off_t file_write(struct file *file, const void *buffer, off_t size) */
}

void
seek (int fd, unsigned position)
{
  /* Changes the next byte to be read or written in open file fd to position */
  /* Use void file_seek(struct file *file, off_t new_pos */
}

unsigned
tell (int fd)
{
  /* Return the position of the next byte to be read or written in open file fd */
  /* Use off_t file_tell(struct file *file) */
}

void
close (int fd)
{
  /* Use void file_close(struct file *file) */
}

/* Actual System call hander call System call */
static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");
  //thread_exit ();

  void *esp = f->esp;
  int number = *(int *)esp;
  
  /* System Call */
  switch (number) {
    /* Process related system calls */
    case SYS_HALT:
      halt();
      break;

    case SYS_EXIT:
      //exit(int status);
      break;

    case SYS_EXEC:
      //exec(char *cmd_line);
      break;

    case SYS_WAIT:
      //wait(pid_t pid);
      break;

    /* File related system calls */
    case SYS_CREATE:
      //create(char *file, unsigned initial_size);
      break;

    case SYS_REMOVE:
      //remove(char *file);
      break;

    case SYS_OPEN:
      //open(char *file);
      break;

    case SYS_FILESIZE:
      //filesize(int fd);
      break;

    case SYS_READ:
      //read(int fd, void *buffer, unsigned size);
      break;

    case SYS_WRITE:
      //write(int fd, void *buffer, unsigned size);
      break;

    case SYS_SEEK:
      //seek(int fd, unsigned position);
      break;

    case SYS_TELL:
      //tell(int fd);
      break;

    case SYS_CLOSE:
      //close(int fd);
      break;
    default:
      break;
  }
}
