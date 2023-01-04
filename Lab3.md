# Lab 3 Multithreading

## Uthread: switching between threads

### 实验目的

**实验目的**：

> 本实验是在给定的代码基础上实现用户级线程切换。

**实验提示**：

>**提示：**
>
>- `thread_switch`只需要保存/还原被调用方保存的寄存器（`callee-save register`，参见`LEC5`使用的文档《Calling Convention》）。为什么？
>
>- 您可以在**user/uthread.asm**中看到`uthread`的汇编代码，这对于调试可能很方便。
>
>- 这可能对于测试你的代码很有用，使用`riscv64-linux-gnu-gdb`的单步调试通过你的`thread_switch`，你可以按这种方法开始：
>
>  - ```c
>    (gdb) file user/_uthread
>    Reading symbols from user/_uthread...
>    (gdb) b uthread.c:60
>    ```



### 实验步骤

因为是用户级线程，不需要设计用户栈和内核栈，用户页表和内核页表等等切换，所以本实验中只需要一个类似于`context`的结构，而不需要费尽心机的维护`trapframe`。所以首先增加一个用户线程的上下文结构体，用于存储上下文。由`Xv6`实验指导书可知`context`在`kernel/proc.h`中，照着把他复制下来，并命名为`tcontext`

```c
//定义线程的上下文结构体
struct tcontext
{
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
```

然后修改`thread`结构体，添加`context`字段

```c
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct tcontext context;      /*用户进程上下文*/

};
```

然后根据（提示您需要将代码添加到`user/uthread.c`中的`thread_create()`和`thread_schedule()`，以及`user/uthread_switch.S`中的`thread_switch`。），模仿`kernel/swtch.S`在`kernel/uthread_switch.S`中写入如下代码:

```c
.text

/*
* save the old thread's registers,
* restore the new thread's registers.
*/

.globl thread_switch
thread_switch:
    /* YOUR CODE HERE */
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)
    ret    /* return to ra */
```

然后修改`thread_scheduler`，添加线程切换语句设置相关的 context：

- `ra` 寄存器指向线程要运行的函数，`switch `结束后会返回到 `ra `处开始运行；
- `sp` 指向线程自己的栈。要注意：压栈是减小栈指针，所以一开始在最高处。

```c
// YOUR CODE HERE
t->context.ra = (uint64)func;                   // 设定函数返回地址
t->context.sp = (uint64)&t->stack[STACK_SIZE-1];  // 设定栈指针
```

### 实验结果

```c
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads

```

<img src="C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221109200213518.png" alt="image-20221109200213518" style="zoom:50%;" />



## Using threads

### 实验目的

**实验内容：**

>本实验主要为了实现利用锁的机制实现多线程并发编程

**实验提示：**

>每个散列桶加一个锁怎么样

### 实验内容

首先确定数据会发生丢失的原因：

>假设现在有两个线程`T1`和`T2`，两个线程都走到put函数，且假设两个线程中`key%NBUCKET`相等，即要插入同一个散列桶中。两个线程同时调用`insert(key, value, &table[i], table[i])`，`insert`是通过头插法实现的。如果先`insert`的线程还未返回另一个线程就开始`insert`，那么前面的数据会被覆盖

因此只需要对插入操作上锁即可

因为在相同的桶中插入才会发生这种事情，因此，我们需要设置与桶相同数量的锁。（如果只设置一个大锁也可以实现互斥，但是设置了当时实验时，发现这种不能通过`ph_fast`测试(如图)，因此为每一个桶设置一个锁，能减少线程之间互斥等待的时间，充分利用多核系统的优势）实验将五个锁放在一个数组中，并进行初始化。

![image-20221109201421457](C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221109201421457.png)

```c
pthread_mutex_t lock[NBUCKET] = { PTHREAD_MUTEX_INITIALIZER }; // 每个散列桶一把锁
```

然后在`put`函数中对`insert`上锁即可：

```c
//使用头插法，在5个桶中插入元素
static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?根据键值来查找
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.更新值
    e->value = value;
  } else {
    pthread_mutex_lock(&lock[i]);//上锁
    // the new is new.
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock[i]);//解锁
  }
}

```

### 实验结果

```
.ph 1
00000 puts, 15.005 seconds, 6664 puts/second
0: 0 keys missing
100000 gets, 13.614 seconds, 7345 gets/second

.ph 2
100000 puts, 7.385 seconds, 13541 puts/second
0: 0 keys missing
1: 0 keys missing
200000 gets, 13.136 seconds, 15225 gets/second

```

![image-20221109203023038](C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221109203023038.png)

![image-20221109203207652](C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221109203207652.png)



## Barrier

### 实验目的

**实验目的：**

> 这一个实验是要实现一个屏障点，使所有线程都到达这个点之后才能继续执行。

**实验提示：**

> - 我们已经为您提供了`barrier_init()`。您的工作是实现`barrier()`，这样panic就不会发生。我们为您定义了`struct barrier`；它的字段供您使用。
> - 你必须处理一系列的`barrier`调用，我们称每一连串的调用为一轮（round）。`bstate.round`记录当前轮数。每次当所有线程都到达屏障时，都应增加`bstate.round`。
> - 您必须处理这样的情况：一个线程在其他线程退出`barrier`之前进入了下一轮循环。特别是，您在前后两轮中重复使用`bstate.nthread`变量。确保在前一轮仍在使用`bstate.nthread`时，离开`barrier`并循环运行的线程不会增加`bstate.nthread`。

### 实验过程

根据提示将`barrier`补充全就可以了。

```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  // 申请持有锁
  pthread_mutex_lock(&bstate.barrier_mutex);

  bstate.nthread++;
  if (bstate.nthread == nthread)
  {
    // 所有线程已到达
    bstate.round++;//根据提示应该让round++，nthread归0
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  else
  {
    // 等待其他线程
    // 调用pthread_cond_wait时，mutex必须已经持有
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }
  // 释放锁
  pthread_mutex_unlock(&bstate.barrier_mutex);
}

```

### 实验结果

```
tarena@tedu:~/my-xv6-labs-2020$ ./barrier 2
OK; passed
```

![image-20221109203414913](C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221109203414913.png)



## 最终结果

```shell
tarena@tedu:~/my-xv6-labs-2020$ ./grade-lab-thread 
make: “kernel/kernel”已是最新。
== Test uthread == uthread: OK (6.3s) 
== Test answers-thread.txt == answers-thread.txt: OK 
== Test ph_safe == gcc -o ph -g -O2 notxv6/ph.c -pthread
ph_safe: OK (49.2s) 
== Test ph_fast == make: “ph”已是最新。
ph_fast: OK (91.4s) 
== Test barrier == make: “barrier”已是最新。
barrier: OK (13.6s) 
== Test time == 
time: OK 
Score: 60/60

```

![image-20221109202709422](C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221109202709422.png)



## Question

- Uthread: switching between threads:`thread_switch` needs to save/restore only the callee-save registers. Why?

  >调用者保存寄存器（caller saved registers）也叫**易失性寄存器**，在程序调用的过程中，这些寄存器中的值不需要被保存（即压入到栈中再从栈中取出），如果某一个程序需要保存这个寄存器的值，需要调用者自己压入栈；
  >
  >被调用者保存寄存器（callee saved registers）
  >
  >也叫**非易失性寄存器**，在程序调用过程中，这些寄存器中的值需要被保存，不能被覆盖；当某个程序调用这些寄存器，被调用寄存器会先保存这些值然后再进行调用，且在调用结束后恢复被调用之前的值；
  >
  >因为易失性寄存器的值可以 在从栈中取出，所以不需要保存。

- Using threads:Why are there missing keys with 2 threads, but not with 1 thread? Identify a sequence of events with 2 threads that can lead to a key being missing. S

  >因为一个线程执行的话，不会发生冲突。但多线程执行的话，当插入数据的时候，一个线程插入没有结束时，另一个线程接着插入，就会导致前一个线程的结果会被覆盖。例如：假设现在有两个线程`T1`和`T2`，两个线程都走到put函数，且假设两个线程中`key%NBUCKET`相等，即要插入同一个散列桶中。两个线程同时调用`insert(key, value, &table[i], table[i])`，`insert`是通过头插法实现的。如果先`insert`的线程还未返回另一个线程就开始`insert`，那么前面的数据会被覆盖。

## 实验感悟

通过这次实验，让我对`Xv6`多线程任务有了一个较深刻的了解，也亲自动手实现了部分代码。同时对`Unix`的线程库也增加了一定的了解，通过查文档来去了解线程库的响应内容也提升了自己阅读文档的能力，以及学会了阅读注释来快速阅读大框架代码的方法提升了代码的阅读能力。