#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

typedef int pid_t;

struct lock filesys_lock;

bool is_user_address(void *);
void halt(void);
void exit(int);
pid_t exec(const char *);
int wait(pid_t);
bool create(const char *, unsigned);
bool remove(const char *);
int open(const char *);
int filesize(int);
int read(int, void *, unsigned);
int write(int, const void *, unsigned);
void seek(int, unsigned);
unsigned tell(int);
void close(int);


#endif /* userprog/syscall.h */
