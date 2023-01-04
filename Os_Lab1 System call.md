# Lab1 System calls

## system call trace

### 实验简介

#### 实验目的

- 创建一个系统调用跟踪的功能，该函数具有一个整数参数，掩码（`mask`），用于实现对跟踪的系统调用。
- 如果掩码中设置了系统调用的编号，则必须修改` xv6` 内核以在每个系统调用即将返回时打印出一行。
- 该行应包含 进程 ID 、 系统调用名称 和 返回值 ；您不需要打印系统调用参数。 `trace` 系统调用应该为调用它的进程和它随后派生的任何子进程启用跟踪，但不应影响其他进程。

### 实验内容

#### 实验提示

1. 将 `$U/_trace` 添加到 `Makefile` 的 `UPROGS` 中
2. 运行 `make qemu` ， 你将看到编译器无法编译 `user/trace.c` ，因为系统调用的用户空间存根还不存在：将系统调用的原型添加到 `user/user.h` ，将存根添加到 `user/usys.pl` ，以及将系统调用号添加到 `kernel/syscall.h` 中。 `Makefile` 调用` perl` 脚本 `user/usys.pl` ，它生成 `user/usys.S` ，实际的系统调用存根，它使用 RISC-V `ecall` 指令转换到内核。修复编译问题后，运行 `trace 32 grep hello README` ；它会失败，因为你还没有在内核中实现系统调用。
3. 在 `kernel/sysproc.c` 中添加一个 `sys_trace()` 函数，该函数通过在 `proc` 结构中的新变量中记住其参数来实现新系统调用(请参阅 `kernel/proc.h` )。从用户空间检索系统调用参数的函数位于 `kernel/syscall.c` 中，你可以在 `kernel/sysproc.c` 中查看它们的使用示例。
4. 修改 `fork()` (参见 `kernel/proc.c` )以将跟踪的掩码从父进程复制到子进程。
5. 修改 `kernel/syscall.c` 中的 `syscall()` 函数以打印跟踪输出。你将需要添加要索引的系统调用名称数组

#### 实验内容

首先根据提示1添加将 `$U/_trace` 添加到 `Makefile` 的 `UPROGS` 中

根据提示2发现应该`kernel/syscall.h`添加系统调用号,模仿前面的内容，在`sys_close`后添加宏定义

```c
#define SYS_trace  22
```

其次应该添加系统调用，发现在注释`system call`下有系统调用函数的函数声明，在`trace.c`中发现其参数为`atoi`,故函数参数类型应该为`int`型,仿照前面的添加声明,。

```c
int trace(int);
```

然后应该在`user/usys.pl`中 添加存根，进入之后仿照发现需要添加`entry("trace");`,通过查资料以及阅读`xv6`手册得知，这里的`perl`语言会自动生成汇编语言`usys.S`,是用户态系统调用的接口，实际的系统调用存根，它使用 RISC-V `ecall` 指令转换到内核。

```c
entry("trace");
```

之后应该去具体实现系统调用功能。

根据提示3，我们应该在`kernel/sysproc.c` 中添加一个 `sys_trace()` 函数，该函数通过在 `proc` 结构中的新变量中记住其参数来实现新系统调用。因为已知我们需要传入一个mask参数，所以我们应该做的应该就是通过这函数来对`proc`结构体中添加这个参数。但打开`proc`结构体发现没有mask这个参数，因此在结构体中添加这个参数。然后编写`sys_trace()`函数实现对赋值。

```c
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  //添加的mask参数
  int mask;
};
```

```c
uint64//仿照前面的函数，发现应该为uint64
sys_trace(void)
{
    int mask;
    if(argint(0,&mask)<0)//因为RISCV中把返回值放在寄存器a0中，而且mask的值应该是一个非负整数
    {
      return -1;
    }
    struct proc *my_proc =myproc();
    my_proc->mask=mask;
    return 0;
}
```

根据提示4，我们应该修改`fork()`函数来对子进程复制父进程的值，阅读代码后发现，`np`应该为子进程的`proc`,`fork()`函数的作用就是将父进程p的信息复制给子进程。

```c
int
fork(void)
{
  ....
  //将父进程的mask赋给子进程
  np->mask=p->mask;
  ....
}
```

最后根据提示5，我们需要修改 `kernel/syscall.c` 中的 `syscall()` 函数以打印跟踪输出。从事例看出输出的格式为`<进程号>: syscall <系统调用名> -> <系统调用返回值>`

首先发现需要`extern`函数声明

```c
extern uint64 sys_trace(void);
```

然后阅读代码发现有`syscalls[num]`,从阅读手册可知，`num`为从系统调用寄存器`a7`中读取的系统调用号，因此数组中应该添加进`trace`相关的内容。

```c
static uint64 (*syscalls[])(void) = {
    //通过转到定义，发现数组的信息，仿照其他的加入sys_trace即可
    [SYS_close] sys_close,
    [SYS_trace] sys_trace,
};
```

最后还需要实现一个打印信息的功能,应为需要打印系统调用功能的名字，因此设置一个数组来存储名字。

```c
static char *syscall_names[] = {
     "fork", "exit", "wait", "pipe",
    "read", "kill", "exec", "fstat", "chdir",
    "dup", "getpid", "sbrk", "sleep", "uptime",
    "open", "write", "mknod", "unlink", "link",
    "mkdir", "close", "trace"};//系统调用数组的名称

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;//系统调用号
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    int mask=p->mask;
    int return_num=p->trapframe->a0;
    if((1<<num)&mask)//右移与mask与运算，不为说明需要追踪
    {
      printf("%d:syscall %s ->%d\n",p->pid,syscall_names[num-1],return_num);
      //因为数组起始为0，对应调用号起始为1，所以需要-1
    }
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

### 实验结果

```c
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 966
3: syscall read -> 70
3: syscall read -> 0
$ trace 2147483647 grep hello README
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
4: syscall read -> 966
4: syscall read -> 70
4: syscall read -> 0
4: syscall close -> 0
$ 
$ grep hello README
$ 
$ trace 2 usertests forkforkfork
usertests starting
8: syscall fork -> 9
test forkforkfork: 8: syscall fork -> 10
10: syscall fork -> 11
11: syscall fork -> 12
11: syscall fork -> 13
11: syscall fork -> 14
11: syscall fork -> 15
12: syscall fork -> 16
12: syscall fork -> 17
13: syscall fork -> 18
11: syscall fork -> 19
11: syscall fork -> 20
11: syscall fork -> 21
11: syscall fork -> 22
12: syscall fork -> 23
11: syscall fork -> 24
12: syscall fork -> 25
11: syscall fork -> 26
12: syscall fork -> 27
11: syscall fork -> 28
12: syscall fork -> 29
11: syscall fork -> 30
11: syscall fork -> 31
12: syscall fork -> 32
11: syscall fork -> 33
12: syscall fork -> 34
13: syscall fork -> 35
11: syscall fork -> 36
11: syscall fork -> 37
12: syscall fork -> 38
11: syscall fork -> 39
12: syscall fork -> 40
11: syscall fork -> 41
12: syscall fork -> 42
11: syscall fork -> 43
12: syscall fork -> 44
11: syscall fork -> 45
12: syscall fork -> 46
11: syscall fork -> 47
12: syscall fork -> 48
13: syscall fork -> 49
14: syscall fork -> 50
11: syscall fork -> 51
12: syscall fork -> 52
11: syscall fork -> 53
12: syscall fork -> 54
12: syscall fork -> 55
11: syscall fork -> 56
25: syscall fork -> 57
11: syscall fork -> 58
12: syscall fork -> 59
11: syscall fork -> 60
13: syscall fork -> 61
11: syscall fork -> 62
12: syscall fork -> 63
11: syscall fork -> 64
53: syscall fork -> 65
11: syscall fork -> 66
12: syscall fork -> 67
11: syscall fork -> 68
12: syscall fork -> 69
11: syscall fork -> 70
12: syscall fork -> -1
OK
8: syscall fork -> 71
ALL TESTS PASSED

```



## Sysinfo

### 实验简介

#### 实验目的

在本实验中，您将添加一个系统调用 `sysinfo` ，它收集有关正在运行的系统信息。系统调用接受一个参数：一个指向 `struct sysinfo` 的指针(参见 `kernel/sysinfo.h` )。内核应该填写这个结构体的字段： `freemem` 字段应该设置为空闲内存的字节数， `nproc` 字段应该设置为状态不是 UNUSED 的进程数。我们提供了一个测试程序 `sysinfotest` ；如果它打印 `“sysinfotest：OK”` ，则实验结果通过测试。

### 实验内容

#### 实验提示

1. 将 `$U/_sysinfotest` 添加到 `Makefile` 的 `UPROGS` 中。
2. 运行 `make qemu` ， 你将看到编译器无法编译 `user/sysinfotest.c` 。添加系统调用 `sysinfo` ，按照与之前实验相同的步骤。要在` user/user.h` 中声明 `sysinfo()` 的原型，您需要预先声明 `struct sysinfo` ：

```c
struct sysinfo;
int sysinfo(struct sysinfo *);
```

- 修复编译问题后，运行 `sysinfotest` 会失败，因为你还没有在内核中实现系统调用。
- `sysinfo` 需要复制一个 `struct sysinfo` 返回用户空间；有关如何使用 `copyout()` 执行此操作的示例，请参阅 `sys_fstat()` (` kernel/sysfile.c` ) 和 `filestat()` ( `kernel/file.c` )。
- 要收集空闲内存量，请在 `kernel/kalloc.c` 中添加一个函数。
- 要收集进程数，请在 `kernel/proc.c` 中添加一个函数。

#### 实验内容

首先根据提示1，将将 `$U/_sysinfotest` 添加到 `Makefile` 的 `UPROGS` 中。

根据提示二，仿照前面的添加系统调用，首先添加宏定义。

```c
#define SYS_sysinfo  23
```

然后在 `user/usys.pl` 文件加入下面的语句：

```pl
entry("sysinfo");
```

然后根据提示在`user.h`中添加声明 `sysinfo()` 的原型，您需要预先声明 `struct sysinfo`

```c
struct stat;
struct rtcdate;
struct sysinfo;//添加结构体声明

//声明sysfinfo()
int sysinfo(struct sysinfo *);
```

根据提示3去实现系统调用，仿照上面的实验去实现系统调用。

在 `kernel/syscall.c` 中新增 `sys_sysinfo` 函数的定义：

```c
extern uint64 sys_sysinfo(void);
```

在 `kernel/syscall.c` 中函数指针数组新增 `sys_trace` ：

```c
[SYS_sysinfo]   sys_sysinfo,
```

根据提示3，以及上面实验的启示需要在`kern\sysproc.c`中添加一个`sys_sysinfo()`函数

```c
//在编写的时候发现vscode提示错误，没有引入sysinfo的声明，故引入头文件
#include "sysinfo.h"
//添加acquire_freemen()和acquire_nproc()的声明
uint64          acquire_freemem(void);
uint64          acquire_nproc(void);


uint64
sys_sysinfo(void)
{
  struct proc *my_proc=myproc();
  struct sysinfo info;
  uint64 addr;//设置一个地址

  if(argaddr(0,&addr)<0)
  {
    return -1;
  }
  info.freemem=acquire_freemen();//此时acquire_freemen()函数还没有创建，应根据提示4去实现得到空闲大小的数量
  info.nproc=acquire_nproc();//根据提示五去实现该函数，得到空闲进程的数量。
  //仿照sys_fstat()来讲proc的内容copy给addr
  if(copyout(my_proc->pagetable,addr,(char*)&info,sizeof(info))<0)
  {
    return -1;
  }
  return 0;
}
```

根据提示4要收集空闲内存量，请在 `kernel/kalloc.c` 中添加一个函数。

```c
uint64
acquire_freemem(void)
{
  struct run *r;
  // 空闲页表的数量
  uint64 num = 0;
  // 需要锁来进行同步互斥操作
  acquire(&kmem.lock);
  // 仿照前面kalloc的内容，定义r指向空闲列表的指针。
  r = kmem.freelist;
  
  while (r)
  {
    
    num++;//用来追踪空闲列表的个数
    r = r->next;
  }
  // 释放锁
  release(&kmem.lock);
  //因为要返回的是空间的大小，所以需要用空闲的页表数去乘以页表块的大小。
  return num * PGSIZE;
}
```



最后去写获取进程数量的函数，根据提示5，需要在 `kernel/proc.c` 中添加一个函数。

```c
uint64
acquire_nproc(void)
{
  struct proc *my_proc;
  // 记录进程数量
  uint64 num = 0;
  // 遍历进程
  for (my_proc = proc; my_proc < &proc[NPROC]; my_proc++)
  {
    //获取锁
    acquire(&my_proc->lock);
    if (my_proc->state != UNUSED)
    {
      //如果不是未使用的，都计数
      num++;
    }
    // 释放锁
    release(&my_proc->lock);
  }
  return num;
}

```



### 实验结果

```c
xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ sysinfo
free space: 133386240
used process: 3
$ sysinfotest
sysinfotest: start
sysinfotest: OK

```



## 实验感想

第一次做实验感觉难以入手，很多东西不知道去哪儿找，但是通过多读提示，以及xv6实验的指导书，便能慢慢理解为什么这么做。实验做了很长时间，也收获了很多东西。本次实验，主要让我了解到了，如何实现系统调用相关的知识。总结如下：

###  如何在XV6中添加新的系统调用（以书上setrlimit为例）

在Linux系统中，setrlimit系统调用的作用是设置资源使用限制。我们以setrlimit为例，要在XV6系统中添加一个新的系统调用，**首先在syscall.h中添加一个新的系统调用的定义**

```
#define SYS_setrlimit  22
```

**然后，在syscall.c中增加新的系统调用的函数指针**

```
static int (*syscalls[])(void) = {
        ...
    [SYS_setrlimit] sys_setrlimit,
};
```

**当然现在sys_setrlimit这个符号还不存在，因此在sysproc.c中声明并实现这个函数**

```
int sys_setrlimit(int resource, const struct rlimit *rlim) {
    // set max memory for this process, etc
}
```

**最后，在user.h中声明setrlimit()这个函数系统调用函数的接口，并在usys.S中添加有关的用户系统调用接口**。

```
SYSCALL(setrlimit)

int setrlimit(int resource, const struct rlimit *rlim);
```



