#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/cache.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"
#include "vm/swap.h"

/* Prototypes */
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
void argument_stack (char **, int, void **);

/* Set-up stack for starting user process. Push arguments,
   push argc and argv, and push the address of the next
   instruction. This function is used in start_process. */
void
argument_stack (char **argv, int argc, void **esp_)
{
  void *esp = *esp_;
  char *arg_addr[argc];
  /* Push arguments in argv */
  int i, j;
  for (i = argc-1; i >= 0; i--) {
    for (j = strlen(argv[i]); j >= 0; j--) {
      esp--;
      *(char *)esp = argv[i][j];
    }
    arg_addr[i] = esp;
  }
  /* Place padding to align esp by 4 Byte */
  while (((int)esp % 4) != 0) {
    esp--;
    *(char *)esp = 0;
  }
  /* Push start address of argv */
  for (i = argc; i >= 0; i--) {
    esp = esp - sizeof(char *);
    if (i == argc) {
      *(char **)esp = NULL;
    }
    else {
      *(char **)esp = arg_addr[i];
    }
  }

  /* Push argc and argv */
  esp = esp -sizeof(char **);
  *(char ***)esp = (char **)(esp + sizeof(char **));

  esp = esp - sizeof(int *);
  *(int *)esp = argc;

  /* Push the address of the next instruction */
  esp = esp - sizeof(void **);
  *(void **)esp = NULL;

  /* Update esp */
  *esp_ = esp;
}

/* Get child process of current running thread with tid.
   If not exists, return NULL */
struct thread *
get_child_process (tid_t tid)
{
  if (tid == TID_ERROR) {
    return NULL;
  }
  struct thread *child;
  struct list_elem *iter;
  if (list_empty(&thread_current()->child_list))
    return NULL;

  iter = list_front(&thread_current()->child_list);
  while (iter != NULL) {
    child = list_entry(iter, struct thread, c_elem);
    if (child->tid == tid) {
      break;
    }
    iter = list_next(iter);
  }

  /* If child_tid is not found. */
  if (iter == NULL) {
    return NULL;
  }
  return child;
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Parse the file_name.
     Deliver the first argument of it to thread_create below */
  char *save_ptr;
  char *cmd_line = palloc_get_page(0);
  strlcpy (cmd_line, file_name, PGSIZE);
  char *token = strtok_r(cmd_line, " ", &save_ptr);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (token, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  palloc_free_page (cmd_line);
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  
  /* Todo : initializing the hash table using the vm_init() */  
  vm_init(&thread_current()->vm);
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* Count argc */
  int count = 0;
  char *iter = (char *)file_name;
  while (*iter != '\0') {
    if (*iter == ' ') {
      count++;
      while (*(iter+1) == ' ')
        iter++;
    }
    iter++;
  }
  count++;

  /* Parse arguments */
  char *parse[count];
  char *token;
  char *save_ptr;
  int i = 0;
  for (token = strtok_r(file_name, " ", &save_ptr);
       token != NULL; token = strtok_r(NULL, " ", &save_ptr)) {
    parse[i] = token;
    i++;
  }
  count = i;
  success = load (parse[0], &if_.eip, &if_.esp);

  /* If load failed, quit. */
  if (!success) {
    thread_current()->load_status = -1;
    sema_up(&thread_current()->load_sema);
    palloc_free_page (file_name);
    exit(-1);
  }
  argument_stack(parse, count, &if_.esp);
  //hex_dump((uintptr_t) if_.esp, if_.esp, PHYS_BASE - if_.esp, true);
  palloc_free_page (file_name);
  thread_current()->load_status = 0;
  sema_up(&thread_current()->load_sema);

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  struct thread *child = get_child_process(child_tid);
  enum intr_level old_level;

  if (child == NULL)
    return -1;

  sema_down(&child->exit_sema);
  old_level = intr_disable ();
  list_remove(&child->c_elem);
  child->c_elem.prev = NULL;
  child->c_elem.next = NULL;
  thread_unblock(child);
  intr_set_level (old_level);
  return child->exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  int i;

  /* Delete vm_entry and unmap mapped files. */
  vm_destroy(&cur->vm);
  munmap(EXIT);

  /* Flush all buffer cache to Disk. */
  bc_flush_all();

  /* close all files opened by current process */
  for (i=2; i<64; i++){
    if (cur->fdt[i] != NULL) {
      file_close(cur->fdt[i]);
      cur->fdt[i] = NULL;
    }
  }
  file_close(cur->running_file);

  /* Find physical page element and remove. */
  struct list_elem *e;
  for(e = list_begin(&lru_list); e != list_end(&lru_list); e = list_next(e)) {
    struct page *page = list_entry(e, struct page, elem);
    if(thread_current() == page->thread) {
      list_remove(e);
    }
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* Return file pointer of current process's input file descriptor */
struct file *
process_get_file (int fd)
{
  if (fd > 63 || fd < 2) 
    return NULL;
  return thread_current()->fdt[fd];
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  lock_acquire(&filesys_lock);
  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      lock_release(&filesys_lock);
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }
  file_deny_write (file);
  thread_current()->running_file = file;
  lock_release(&filesys_lock);
  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (!success) {
    file_close (file);
    thread_current()->running_file = NULL;
  }
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      /*uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;
      */
      /* Load this page. */
      /*if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);
      */
      /* Add the page to the process's address space. */
      /*if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }
      */
 
      /* Allocate vm_entry and initialize (Virtual Memory) */
      struct vm_entry *vme = malloc(sizeof(struct vm_entry));
      vme->writable = writable;
      vme->vp_type = VP_ELF;
      vme->vaddr = upage;
      vme->file = file;
      vme->read_bytes = page_read_bytes;
      vme->zero_bytes = page_zero_bytes;
      vme->offset = ofs;

      insert_vme(&thread_current()->vm, vme);
      
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  struct page *kpage;
  bool success = false;
  
  /* Allocate page and install it */
  kpage = get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, 
				kpage->paddr, true);
      if (success) 
      {
        /* Initialize vm_entry for top of stack. */
        *esp = PHYS_BASE;
        struct vm_entry *vme = malloc(sizeof(struct vm_entry));
        vme->vaddr = PHYS_BASE - PGSIZE;
        vme->writable = true;
        vme->vp_type = VP_SWAP;
        vme->file = NULL;
        vme->read_bytes = 0;
        vme->zero_bytes = PGSIZE;
        vme->offset = 0;
        kpage->vme = vme;        

        success = insert_vme(&thread_current()->vm, vme);
        if (!success)
          free(vme);
      }
      if (!success)
        free_page (kpage->paddr);
    }

  return success;
}


/* Grow stack by mapping a zeroed page at the addr.
   It is called by page_fault(), is_valid_buffer(), or is_valid_str().
   For page_fault(), if finding vme at faulted address failed, check stack
   growth condition and call this function. And same in is_valid_buffer()
   and is_valid_str() in userprog/syscall.c */
bool
grow_stack (void *addr)
{
  struct page *kpage;
  bool success = false;

  /* Check esp limit. Maximum size of stack is 8MB. */
  uint32_t gaddr = (uint32_t)pg_no(addr) << PGBITS;
  if (gaddr < ((uint32_t)PHYS_BASE - (1 << 23)) )
    return success;

  kpage = get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page ((void *)gaddr, kpage->paddr, true);
      if (success) {
        /* Initialize vm_entry for growed stack */
        struct vm_entry *vme = malloc (sizeof(struct vm_entry));
        vme->vaddr = (void *) gaddr;
        vme->writable = true;
        vme->vp_type = VP_SWAP;
        vme->file = NULL;
        vme->read_bytes = 0;
        vme->zero_bytes = PGSIZE;
        vme->offset = 0;
        kpage->vme = vme;

        success = insert_vme(&thread_current()->vm, vme);
        if (!success)
          free(vme);
      }
      if (!success)
        free_page (kpage->paddr);
    }
  return success;
}

/* Page fault handler. It is called by page_fault(), when there exist
   vm_entry but PTE doesn't exist. So, this function allocate physical
   page for vm_entry and install that page.
   For ELF type vm_entry and FILE type vm_entry, call load_file() to load
   data from file. For SWAP type vm_entry, call swap_in() to load data
   from swapped page. */
bool
handle_mm_fault (struct vm_entry *vme)          
{
  struct page *kpage;
  bool success = false;
  bool have_lock = false;

  kpage = get_page (PAL_ZERO | PAL_USER);
  kpage->vme = vme;
  if (kpage == NULL)
    return success;

  /* Acquire filesys_lock for synch. Check if current process already have */
  if (!lock_held_by_current_thread(&filesys_lock))
    have_lock = lock_try_acquire(&filesys_lock);

  /* Check vm_entry type */
  switch(vme->vp_type) {
    case VP_ELF:
      if (!load_file(kpage->paddr, vme))
        goto done;
      break;

    case VP_FILE:
      if (!load_file(kpage->paddr, vme))
        goto done;
      break;

    case VP_SWAP:
      swap_in(vme, kpage->paddr);
      break;

    default:
      goto done;
  }

  /* Setup page table */
  if (!install_page(vme->vaddr, kpage->paddr, vme->writable))
    goto done;
  /* If VP_ELF is swapped, set vm_entry type to VP_ELF, and set page's dirty 
     bit to true because for VP_ELF, only dirty page is swapped in. */
  if ((vme->vp_type == VP_SWAP) && (vme->file != NULL)) {
    vme->vp_type = VP_ELF;
    pagedir_set_dirty (thread_current()->pagedir, vme->vaddr, true);
  }

  /* Release lock */
  if(have_lock)
    lock_release(&filesys_lock);   
  success = true;

  done:
    if(!success)
      free_page(kpage->paddr);
    return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
