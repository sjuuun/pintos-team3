#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "filesys/file.h"
#include "threads/thread.h"
#include "vm/page.h"

struct thread * get_child_process (tid_t);

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
struct file* process_get_file(int);
bool grow_stack (void *);
bool handle_mm_fault (struct vm_entry *);         

#endif /* userprog/process.h */
