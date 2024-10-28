# xv-6
## PBS Scheduling
### Implementation:
Create new entities in struct proc such as:<br>
static_priority,
dynamic_priority,
no_schedule,
RBI,
runtime,
waittime,
sleeptime.<br>

Initialize entities in allocproc():<br>
p->static_priority = 50<br>
p->RBI = 25<br>
p->no_schedule = 0<br>
p->runtime = 0<br>
p->waittime = 0<br>
p->sleeptime = 0<br>

In scheduler() function<br>
First update the dynamic priority for each process using formula given to us.
```c
for(p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if(p->state == RUNNABLE)
      {
        int numerator = (3*(p->runtime)-(p->sleeptime)-(p->waittime))*50;
        int denominator = (p->runtime) + (p->waittime) + (p->sleeptime) + 1;
        int temp = (numerator/denominator);

        if(temp > 0)
        {
          p->RBI = temp;
        }
        else
        {
          p->RBI = 0;
        }

        if(p->static_priority + p->RBI > 100)
        {
          p->dynamic_priority = 100;
        }
        else
        {
          p->dynamic_priority = p->static_priority + p->RBI;
        }
      }
      release(&p->lock);
    }
```

then find the process with min dynamic priority. If dynamic priority is same then comapre no_sheculed. If no_scheduled is also same then select the process basesd on ctime.
```c
struct proc* new = 0;
    // new->dynamic_priority = 150;
    for(p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if(p->state == RUNNABLE)
      {
        if(new != 0)
        {
          if(new->dynamic_priority > p->dynamic_priority)
          {
            if(new)
            {
              release(&new->lock);
            }
            new = p;
          }
          else if(new->dynamic_priority == p->dynamic_priority)
          {
            if(new->no_schedule > p->no_schedule)
            {
              if(new)
              {
                release(&new->lock);
              }
              new = p;
            }
            else if(new->no_schedule == p->no_schedule)
            {
              if(new->ctime > p->ctime)
              {
                if(new)
                {
                  release(&new->lock);
                }
                new = p;
              }
              else
              {
                release(&p->lock);
              }
            }
            else
            {
              release(&p->lock);
            }
            
          }
          else
          {
            release(&p->lock);
          }
        }
        else
        {
          new = p;
        }
      }
      else
      {
        release(&p->lock);
      }
    }
```

then update the Selected process state to RUNNING,runtime=0,waittime=0,sleeptime=0 and increase number of times scheduled.
```c
if(new != 0)
{
    // acquire(&new->lock);
    new->state = RUNNING;
    new->runtime = 0;
    new->waittime = 0;
    new->sleeptime = 0;
    new->no_schedule = new->no_schedule + 1;
    c->proc = new;
    swtch(&c->context, &new->context);
    c->proc = 0;
    release(&new->lock);
}
```
To change static priority we use set_priority() function :<br>
The system call returns the old Static Priority of the process. In case the priority of the process increases(the value is lower than before), then rescheduling should be done. Note that calling this system call will also reset the Recent Behaviour Index (RBI) of the process to 25 as well.

```c
    int
set_priority(int pid,int new_priority)
{
  int old_priority = -1;
  struct proc* p;

  for(p = proc; p < &proc[NPROC] ; p++)
  {
    acquire(&p->lock);
    if(p->pid == pid)
    {
      old_priority = p->static_priority;
      p->static_priority = new_priority;
      p->RBI = 25;
      release(&p->lock);
      if(old_priority > new_priority)
      {
        yield();
      }
      break;
    }
    else
    {
      release(&p->lock);
    }
  }

  return old_priority;
}
```

## COW(Copy-On-Write)
### Implementation:
Idea : <br>
In '__fork()__' function we call '__uvmcopy__' function which allocates new memory to pages.so, we have to modify this.<br>
In '__uvmcopy()__' function remove memory allocation part, just map the page table entries and change permission to readonly.<br>
```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  // char* mem;

  for(i = 0; i < sz; i += PGSIZE)
  {
    if((pte = walk(old, i, 0)) == 0)
    {
      panic("uvmcopy: pte should exist");
    }
    if((*pte & PTE_V) == 0)
    {
      panic("uvmcopy: page not present");
    }
    flags = PTE_FLAGS(*pte);
    pa = PTE2PA(*pte);

    if(flags & PTE_W)
    {
      flags = (PTE_FLAGS(*pte) | PTE_COW) & (~PTE_W);
      *pte = (*pte & (~PTE_W)) | PTE_COW;
    }
    // if((mem = kalloc()) == 0)
    //   goto err;
    // memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, pa, flags) != 0)
    {
      // kfree(mem);
      goto err;
    }

    incrref_count((void*)pa);
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```
Then if we got interrupt in uertrap then call the function('__newalloc__') which then copies the pages.<br>
```c
else if(r_scause() == 15)
  {
    uint64 address = r_stval(); // to get addr of page where fault occurs

    if((address >= (p->trapframe->sp - PGSIZE) && address < p->trapframe->sp) || address >= MAXVA)
    {
      p->killed = 1;
    }

    int check = newalloc(p->pagetable, PGROUNDDOWN(address));
    if(check<0)
    {
      p->killed = 1;
    }
  }
```
Also during '__copyout()__'(exchanging pages from kernel to user) call '__newalloc()__' function. <br>
```c
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  int check;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);

    check = newalloc(pagetable, va0);
    if(check < 0)
    {
      return -1;
    }

    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}
```
In '__newalloc()__' we will create a copy of pages with read/write permissions.
```c
int newalloc(pagetable_t pagetable, uint64 vaddr)
{
  pte_t *pte;
  uint64 paddr;

  if((vaddr % PGSIZE) != 0 || vaddr >= MAXVA)
  {
    return -1;
  }

  if((pte = walk(pagetable, vaddr, 0)) == 0)
  {
    return -1;
  }
  if((paddr = PTE2PA(*pte)) == 0)
  {
    return -1;
  }

  // check whether the written process is child or parent
  if(*pte & PTE_COW)
  {
    uint flags = PTE_FLAGS(*pte);
    flags = (flags & (~PTE_COW)) | PTE_W;

    char* newpaddr;
    if((newpaddr = kalloc()) == 0)
    {
      return -1;
    }

    memmove(newpaddr, (char*)paddr, PGSIZE);
    uvmunmap(pagetable, PGROUNDUP(vaddr), 1, 1);
    mappages(pagetable, vaddr, PGSIZE, (uint64)newpaddr, flags);
  }

  return 0;
}
```

If child process is done and then we have to remove assigned pages for child then we do '__kfree()__' which then remove pages. If we dont allocate a new page then it will remove page table shared by parent and child. <br>
So, to avoid this we maintain counter for each page.<br>
Initially put your all counters to one.During '__kalloc()__' put counter to one.<br>
```c
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
```

If '__kfree()__' is done then we decrease counter and if counter=0 then remove page memory.<br>
In uvmcopy also increase the counter.
```c
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
```

### Analysis
when the staticpriority=80 is put in then:(with 1CPU)
```bash
Process 5 finished
Process 7 finished
Process 8 finished
Process 9 finished
Process 6 finished
Process 0 finished
Process 1 finished
Process 2 finished
Process 3 finished
Process 4 finished
Average rtime 15,  wtime 162
```
<br>

when the staticpriority=50(default) is put in then:(with 1CPU)
```bash
Process 5 finished
Process 7 finished
Process 8 finished
Process 6 finished
Process 9 finished
Process 0 finished
Process 1 finished
Process 2 finished
Process 3 finished
Process 4 finished
Average rtime 15,  wtime 162
```
<br>
if we set the priority for all process then produce the same result.



# Concurrency

## I.Cafe sim
### Implementation:

We define three structs for Barista,Coffee and Costumer.<br>
1. After taking input we define '__num_costumers__' of threads.so,that each costumer get single thread.
2. Intialize semaphore to '__num_baristas__' because at a given time only '__num_baristas__' threads has to work.<br>

Now actual process begins:
1. In a for loop we will send constumers to create threads. Each costumer have different arrival time so we will put in a sleep between arrival time. If costumers arrive at same time then i will print '__Costumer %d arrives at %d second(s)__' for all costumers.
2. Before assingning thread we will do sem_wait.If it loops again then parent process goes into sleep.After completion of any threads then parent wakesup and sends next costumer.
3. we will caluclate the time stamps before sem_wait and after sem_wait then difference between them gives starttime of next costumer.<br>

```c
int main()
{
    int num_baristas;
    int num_coffees;
    int num_costumers;

    scanf("%d %d %d",&num_baristas,&num_coffees,&num_costumers);
    Coffee coffees[num_coffees];
    for(int i=0;i<num_coffees;i++)
    {
        scanf("%s %d",coffees[i].coffee,&coffees[i].preptime);
    }

    Costumer costumer[num_costumers];
    for(int i=0;i<num_costumers;i++)
    {
        scanf("%d %s %d %d",&costumer[i].costumerindex,costumer[i].order,&costumer[i].arrivetime,&costumer[i].tolerance);
    }

    for(int i=0;i<num_costumers;i++)
    {
        for(int k=0;k<num_coffees;k++)
        {
            if(strcmp(costumer[i].order,coffees[k].coffee)==0)
            {
                costumer[i].preptime = coffees[k].preptime;
            }
        }
    }

    int current_time = 0;
    int j = 0;
    pthread_mutex_init(&lock, NULL);
    sem_init(&s,0,num_baristas);
    pthread_t costumers[num_costumers];

    for(int i=0;i<num_costumers;i++)
    {
        sleep(costumer[i].arrivetime - current_time);
        current_time = costumer[i].arrivetime;
        if(j<=i)
        {
            for(j=i;j<num_costumers;j++)
            {
                if(costumer[i].arrivetime != costumer[j].arrivetime)
                {
                    break;
                }
                else
                {
                    printf("Costumer %d arrives at %d second(s)""\n",costumer[j].costumerindex,costumer[j].arrivetime);
                    printf(ANSI_COLOR_YELLOW "Customer %d orders an %s" ANSI_COLOR_RESET "\n",costumer[j].costumerindex,costumer[j].order);
                }
            }
        }
        time_t start,end;
        start = time(NULL);
        sem_wait(&s);
        end = time(NULL);
        costumer[i].starttime = costumer[i].arrivetime + (end-start) + 1;

        pthread_mutex_lock(&lock);
        if(end-start>0)
        {
            waittime = waittime + (end-start) + 1;
        }
        else
        {
            waittime = waittime + (end-start);
        }
        for(int k=1;k<=num_baristas;k++)
        {
            if(array[k]==0)
            {
                costumer[i].baristaindex = k;
                array[k] = 1;
                break;
            }
        }
        pthread_mutex_unlock(&lock);
        pthread_create(&costumers[i],NULL,threads,&costumer[i]);
    }

    for(int i = 0; i < num_costumers; i++)
    {
        pthread_join(costumers[i], NULL);
    }

    printf("\n");
    printf("Waiting Time : %d second(s)\n",waittime);
    printf("%d coffee wasted\n",waste);

    pthread_mutex_destroy(&lock);
    sem_destroy(&s);
    return 0;

}
```

After entering the thread:
1. Intially put it in sleep for 1 second because after assigning costumer it start working after 1 second.
2. check whether the coffee completes before tolerance time or not<br>
if '__going to complete__' then put that in sleep for prep time then coffee is prepared.<br>
if '__not goinn to complete__' then put it in sleep upto tolerane time then print costumer leaves. After that put it in sleep for reamining preperation time of coffee then print coffee is completed.
3. Now Put the that this barista is free such that we can assign new costumers. This step we will do it with global array.
4. Now do '__sem_post__' so that parent will wake up and assign new costumers.

```c
void* threads(void *arg)
{
    Costumer *args = (Costumer*) arg;

    sleep(1);

    printf(ANSI_COLOR_CYAN "Barista %d begins preparing the order of customer %d at %d second(s)" ANSI_COLOR_RESET "\n",args->baristaindex,args->costumerindex,args->starttime);
    
    int duration = args->preptime;
    if((args->starttime)+(args->preptime)-(args->arrivetime) > args->tolerance)
    {
        duration = (args->arrivetime + args->tolerance) - args->starttime;
        sleep(duration);
        printf(ANSI_COLOR_RED "Customer %d leaves without their order at %d second(s)" ANSI_COLOR_RESET "\n",args->costumerindex,args->arrivetime+args->tolerance + 1);
        sleep(args->preptime - duration);
        printf(ANSI_COLOR_BLUE "Barista %d successfully completes the order of customer %d at %d seconds(s)" ANSI_COLOR_RESET "\n",args->baristaindex,args->costumerindex,args->starttime+args->preptime);
        pthread_mutex_lock(&lock);
        waste = waste + 1;
        pthread_mutex_unlock(&lock);
    }
    else
    {
        sleep(duration);
        printf(ANSI_COLOR_BLUE "Barista %d successfully completes the order of customer %d at %d seconds(s)" ANSI_COLOR_RESET "\n",args->baristaindex,args->costumerindex,args->starttime+args->preptime);
        printf(ANSI_COLOR_GREEN "Customer %d leaves with their order at %d second(s)" ANSI_COLOR_RESET "\n",args->costumerindex,args->starttime+args->preptime);
    }

    pthread_mutex_lock(&lock);
    array[args->baristaindex] = 0;
    pthread_mutex_unlock(&lock);

    sem_post(&s);
    
}
```

### Questions
Q1 . Waiting time<br>
we put an global variable '__waittime__'. The only waiting time is 'time before assigning the thread to a costumer' we can caluclate using time stamp differece before and after '__sem_wait__' in parent.<br>

Q2 . Coffee Wasting<br>
we will put a global variable '__waste__'. The coffee is wasted when tolerance is higher than preperation time then we will increase the waste value.

## II.Ice-cream parlour sim 
### Questions:

__Minimising Incomplete Orders:__<br>
- The progaram can be modified such that the availability of topplings are checked before assigning the machine.If topplings are not available costumer order is cancelled.
- Prioritize the allocation of machines to orders that can be fullfilled entirely.
- Avoid allocating machine to an order that may exceed the machines working hours, leading to an incomplete order.


__Ingredient Replenishment__<br>
- If the toppings in shortage, keep the order of the customer at wait and verify if the requirements are availble at the nearest store.
- Implement the continuous monitoring way of checking the ingridients


__Unserved Orders__<br>
- Implement based on sorting algorithm such that we can process the orders and can ensure that unserved orders can be minimum.
- Avoid allocating impossible orders which no machine can fulfill in their lifetime.
