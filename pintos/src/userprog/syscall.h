#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H


#include "vm/page.h"
#include "threads/synch.h"

/* Used for munmap in process_exit */
#define EXIT -1

typedef int pid_t;
typedef int mapid_t;

struct lock filesys_lock;

void syscall_init (void);

/* Project 2 */
void halt (void);
void exit (int);
pid_t exec (const char *);
int wait (pid_t);
bool create (const char *, unsigned);
bool remove (const char *);
int open (const char *);
int filesize (int);
int read (int, void *, unsigned);
int write (int, const void *, unsigned);
void seek (int, unsigned);
unsigned tell (int);
void close (int);

/* Project 3 */
int mmap (int, void*);
void do_munmap (struct mmap_file *);
void munmap (int);

/* Project 4 */
bool chdir (const char *);
bool mkdir (const char *);
bool readdir (int, char *);
bool isdir (int);
int inumber (int);

#endif /* userprog/syscall.h */
