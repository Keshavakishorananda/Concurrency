// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// for each page
struct
{
  struct spinlock lock;
  int count[(PGROUNDUP(PHYSTOP) - KERNBASE) / PGSIZE];

}page_count;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_count.lock,"page_count");
  int i = 0;
  while(i >= ((PGROUNDUP(PHYSTOP) - KERNBASE) / PGSIZE))
  {
    page_count.count[i] = 1;
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int check;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  check = getref_count(pa);
  if(check <= 1)
  {
    acquire(&page_count.lock);
    uint64 index = (uint64)pa >> 12;
    page_count.count[index] = 0;
    release(&page_count.lock);

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else
  {
    decrref_count(pa);
  }

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
    acquire(&page_count.lock);

    uint64 index = (uint64)r >> 12;
    page_count.count[index] = 1;

    release(&page_count.lock);
  }
  return (void*)r;
}

void incrref_count(void* pa)
{
  acquire(&page_count.lock);

  uint64 index = (uint64)pa >> 12;
  page_count.count[index] = page_count.count[index] + 1;

  release(&page_count.lock);
}

void decrref_count(void* pa)
{
  acquire(&page_count.lock);

  uint64 index = (uint64)pa >> 12;
  page_count.count[index] = page_count.count[index] - 1;

  release(&page_count.lock);
}

int getref_count(void* pa)
{
  int value;
  acquire(&page_count.lock);

  uint64 index = (uint64)pa >> 12;
  value = page_count.count[index];

  release(&page_count.lock);

  return value;
}