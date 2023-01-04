# Lab2 Traps

## Backtrace

### 实验简介

#### 实验目的

- 要求我们在`kernel/print.c`添加`backtrace()`函数，用来打印内核调用栈的信息(`return address`)；

- 在`kernel/printf.c`中实现`backtrace()`函数。在`sys_sleep()`中插入对这个函数的调用，然后运行`bttest`，它调用`sys_sleep`。你的输出应该是：

- ```c
  backtrace: 
  0x0000000080002cda 
  0x0000000080002bb6 
  0x0000000080002898
  ```

-  在`bttest`之后退出`qemu`。在你的`terminal`中：地址可能是不同的，但如果你运行`addr2line -e kernel/kernel（riscv64-unknown-elf-addr2line -e kernel/kernel）`并且把上面地址复制粘贴如下所示： 

- ```c
  $ addr2line -e kernel/kernel 
  0x0000000080002de2 
  0x0000000080002f4a 
  0x0000000080002bfc
  ```

-  Ctrl-D 你应该可以看到像下面所示： 

- ```c
  kernel/sysproc.c:74 
  kernel/syscall.c:224 
  kernel/trap.c:85
  ```

  

### 实验内容

#### 实验提示

1. 添加`backtrace`原型到`kernel/defs.h`，以便于你可以在`sys_sleep`中调用。

2. `GCC`编译器存储当前执行函数的帧指针在寄存器S0，添加下面函数到`kernel/risc.h`：

3. ```c
   static inline uint64 r_fp(){   
   uint64 x;    
   asm volatile(“mv %0, s0” : “=r” (x) );
   }
   ```

4.  在`backtrace`中调用这个函数来读取当前帧指针。这个函数使用内联汇编来读取`s0`。

5. 这些讲义有一个栈帧布局的图片。注意返回地址位于一个固定偏移量（对栈帧的帧指针偏移-8），保存的帧指针位于固定偏移量（对帧指针偏移-16）

6. `xv6`为每个`kernel stack`分配一页于页对齐地址。你可以计算栈页的顶部和底部地址，通过使用`PGROUNDDOWN(fp)`和`PGROUNDUP(fp)`（看`kernel/riscv.h`，这些数字对`backtrace`终止循环是有帮助的）。

#### 实验过程

首先根据提示1，添加`backtrace`原型到`kernel/defs.h`。

```c
// printf.c
void            printf(char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printfinit(void);
void            backtrace();//添加声明
```

然后根据提示二，我们需要获取当前 `fp（frame pointer）`寄存器的方法，添加到`kernel/risc.h`中

```c
static inline uint64
r_fp()
{
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```

然后应该在printf.c中添加backtrace函数,用于读取当前帧指针

由提示三中链接给出的这张图片可以看出

<img src="C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221027194848665.png" alt="image-20221027194848665" style="zoom:50%;" />

fp 指向当前栈帧的开始地址，sp 指向当前栈帧的结束地址。 （栈从高地址往低地址生长，所以 fp 虽然是帧开始地址，但是地址比 sp 高）
 栈帧中从高到低第一个 8 字节 `fp-8` 是 return address，也就是当前调用层应该返回到的地址。
 栈帧中从高到低第二个 8 字节 `fp-16` 是 previous address，指向上一层栈帧的 fp 开始地址。所以栈也由高地址向低地址增长。

然后根据提示6可知需要通过使用`PGROUNDDOWN(fp)`和`PGROUNDUP(fp)`来结束打印地址的循环，因为栈向上增长，应次需要使用`PGROUNDUP(fp)`来对结果进行比较。

```c
void 
backtrace()
{
  uint64 fp=r_fp();//获取当前栈指针
  // 获取当前进程在kernel中stack的最大地址
  uint64 max = PGROUNDUP(fp);
  while(fp<max)//只要没到栈底
  {
    printf("%p\n",*(uint64* )(fp-8));//打印当前的地址
    fp=*((uint64*)(fp-16));//fp-16存放的是指向上一个栈帧的开始地址的指针，取值，为上一个栈帧的开始地址。
  }
}
```

因为还要实现对`sleep()`进行追踪，所以在在` sys_sleep `的开头调用一次 `backtrace()`

```c

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  backtrace(); // 实现追踪
  if(argint(0, &n) < 0)
    return -1;
  // ......
  return 0;
}
```

### 实验结果

```c
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh
$ bttest
0x0000000080002e08
0x0000000080002ce2
0x00000000800028ee

```



## Alarm

### 实验简介

#### 实验目的

- 该题的主要目的是让我们增加一个系统调用，该系统调用会在指定时间间隔后调用用户态的函数，因此需要我们对内核态的相关状态进行保存。

- 在这个练习中，你将为`xv6`添加一个特性： 在进程使用`cpu`时间时，定期发出警报。对于`compute-bound`进程（想限制它们使用多少`cpu`时间），或对于既想计算也想采取一些周期性动作的进程，这可能是有用的。 更普遍一些，你将能实现一个原始形式的用户级中断`/fault handlers`；你可以使用某些相似的东西来处理在应用中的`page faults`。 如果通过`alarmtest`和`usertests`，你的解决方案就是正确的。 你应该添加一个新的`sigalarm(interval, handler)system call`。如果一个应用调用`sigalarm(n, fn)`，那么在每`n ticks`个`cpu time`（程序花费）后，`kernel`应该导致应用函数fn被调用。 当`fn`返回时，应用应该从它离开的地方重新恢复。在`xv`6中一个`trick`是一个相当任意的时间单位，取决于硬件定时器多久生成一个中断。 如果一个应用调用s`igalarm(0,0)`，kernel应该停止生成周期`alarm call`。 你将找到一个文件`user/alarmtest.c`在`xv6`代码库。把它添加到`Makefile`。它将不会编译成功，直到你添加`sigalarm`和`sigreturn system calls`。 `alarmtest`在`test0`中调用`sigalarm(2, periodic)`，来告知`kernel`每两个`tick`强制调用`periodic()`，然后自旋一会。 你可以在`user/alarmtest.asm`中看到`allarmtest`的汇编代码，可以用来调试。当`alarmtest`生成下面输出并且`usertests`正确运行，你的方案就是正确的。

### 实验内容

#### 实验提示

1. 你将需要更改`Makefile`，来让`alarmtest.c`被当作一个`xv6`用户程序被编译。

2. `user/user.h`中的定义：

   ```c
   int sigalarm(int ticks, void (*handler)());
       int sigreturn(void);
   ```

   

3. 更新`user/usys.pl`（可以生成`ser/usys.S`），`kernel/syscall.h`，和`kernel/syscall.c`来允许`alarmtest`调用调用`sigalarm`和`sigreturn system calls`。

4. 现在你的`sys_return`应该只返回0

5. 你的`sys_sigalarm()`应该存储`alarm interval`和`handler function`的指针，在`proc`结构的新`field`中（`kernel/proc.h`）。

6. 在上一次进程的`alarm handler`调用后（或在下次调用前），你将需要保持跟踪度过了多少个`ticks`；你将也需要一个新`field`在`struct proc`。你可以初始化`proc fields`在`proc.c`中的`allocproc()`中。

7. 每个`tick`，硬件时钟强制一个中断，被在`kernel/trap.c的usertrap()`中处理

8. 你仅想在定时器中断时操纵进程的`tick`，您想要某些如下所示：
   `if(which_dev == 2) …`

9. 仅当进程有明显的定时器时，才调用alarm函数。注意`user alarm`函数地址可能是0（例如：在`user/alarmtest.asm，periodic`在地址0）

10. 你将需要更改`usertrap()`，以便于当一个进程的a`larm interval`到期时，用户进程执行`handler`函数。当一个`RISC-V`的`trap`返回到用户空间时，什么决定指令地址（用户空间代码恢复执行)？

11. 如果你告诉`qemu`仅使用一个`cpu`，用`gdb`看`traps`会更简单，用以下方式： `make CPUS=1 qemu-gdb`

12. 如果`alarmtest`打印`“alarm”`那么你就成功了

#### 实验过程

根据提示1首先在Makefile文件，增加如下代码：

```makefile
$U/_alarmtest\
```

根据提示2在`user/user.h`中的定义：下面的函数

```c
int sigalarm(int ticks, void (*handler)());
int sigreturn(void);
```

根据提示3更新`user/usys.pl`

```c
entry("sigalarm");
entry("sigreturn");
```

在文件`kernel/syscall.h`中，增加如下内容

```c
#define SYS_sigalarm 24
#define SYS_sigreturn 25
```

在`kernel/syscall.c`中增加如下内容。

```c
//extern声明
extern uint64 sys_sigalarm(void);
extern uint64 sys_sigreturn(void);
//添加syscalls数组成员
static uint64 (*syscalls[])(void) = {
 ....    
 [SYS_sigalarm] sys_sigalarm,
 [SYS_sigreturn] sys_sigreturn,  
}
//添加syscall的名字数组成员
static char *syscall_names[] = {
    "fork", "exit", "wait", "pipe",
    "read", "kill", "exec", "fstat", "chdir",
    "dup", "getpid", "sbrk", "sleep", "uptime",
    "open", "write", "mknod", "unlink", "link",
    "mkdir", "close", "trace", "sysinfo", "sigalarm", "sigreturn"};
```

根据提示5-7，在` proc `结构体的定义中，增加 alarm 相关字段：

- `alarm_interval`：时钟周期，0 为禁用
- `alarm_handler`：时钟回调处理函数
- `larm_ticks`：下一次时钟响起前还剩下的 `ticks `数
- `alarm_trapframe`：时钟中断时刻的 `trapframe`，用于中断处理完成后恢复原程序的正常执行
- `alarm_goingoff`：是否已经有一个时钟回调正在执行且还未返回（用于防止在 `alarm_handler` 中途闹钟到期再次调用 `alarm_handler`，导致 `alarm_trapframe` 被覆盖）

```c
struct proc {
  // ......
  int alarm_interval;          // 时钟周期
  void(*alarm_handler)();      // 时钟回调处理函数
  int alarm_ticks;             // 还剩的ticks数
  struct trapframe *alarm_trapframe;  //时钟中断的trapframe
  int alarm_goingoff;          //是否已经有一个时钟回调正在执行且还未返回
};
```

然后在`allocproc()`中初始化相应字段，并分配`alarm_trapframe`的页面；

```c
static struct proc*
allocproc(void)
{
  ......
found:
  p->pid = allocpid();

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    release(&p->lock);
    return 0;
  }

  //为alarm_trapframe申请页面.
  if ((p->alarm_trapframe = (struct trapframe *)kalloc()) == 0)
  {
    release(&p->lock);
    return 0;
  }
 //初始化相关字段
  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->alarm_ticks = 0;
  p->alarm_goingoff = 0;

......
  return p;
}
```

相应地在`freecproc()`中还原字段的值，并销毁`alarm_trapframe`的页面：

```c
static void
freeproc(struct proc *p)
{
  .....
  // 释放分配给alarm_trapfram的页面
  if (p->alarm_trapframe)
    kfree((void *)p->alarm_trapframe);
  p->alarm_trapframe = 0;
  ......
  // 清空alarm中断处理相关字段
  p->alarm_interval = 0;
  p->alarm_handler = 0;
  p->alarm_ticks = 0;
  p->alarm_goingoff = 0;
}
```

在文件`kernel/syproc.c`中，增加两个系统调用函数

```c
uint64 sys_sigalarm(void)
{
  int n;
  uint64 fn;
  if (argint(0, &n) < 0)
    return -1;
  if (argaddr(1, &fn) < 0)
    return -1;
  // 设置 myproc 中的相关属性
  struct proc *p = myproc();
  p->alarm_interval = ticks;
  p->alarm_handler = (void (*)())(fn);
  p->alarm_ticks = ticks;
  return 0;
}

uint64 sys_sigreturn(void)
{
  // 将 trapframe 恢复到时钟中断之前的状态，恢复原本正在执行的程序流
  struct proc *p = myproc();
  *p->trapframe = *p->alarm_trapframe;
  p->alarm_goingoff = 0;
  return 0;
}

```

最后根据提示10，在` usertrap()` 函数中，实现时钟机制具体代码.在每次时钟中断的时候，如果进程有已经设置的时钟（`alarm_interval != 0`），则进行 `alarm_ticks` 倒数。当 `alarm_ticks` 倒数到小于等于 0 的时候，如果没有正在处理的时钟，则尝试触发时钟，将原本的程序流保存起来（`*alarm_trapframe = *trapframe`），然后通过修改 pc 寄存器的值，将程序流转跳到 `alarm_handler `中，`alarm_handler `执行完毕后再恢复原本的执行流（`*trapframe = *alarm_trapframe`）。这样从原本程序执行流的视角，就是不可感知的中断了。

```c
void
usertrap(void)
{
  // give up the CPU if this is a timer interrupt.
  if (which_dev == 2)
  {
    if (p->alarm_interval != 0)
    { // 如果设定了时钟事件
      if (--p->alarm_ticks <= 0)
      { // 时钟倒计时 -1 tick，如果已经到达或超过设定的 tick 数
        if (!p->alarm_goingoff)
        { // 确保没有时钟正在运行
          p->alarm_ticks = p->alarm_interval;
          // jump to execute alarm_handler
          *p->alarm_trapframe = *p->trapframe; // backup trapframe
          p->trapframe->epc = (uint64)p->alarm_handler;
          p->alarm_goingoff = 1;
        }
        // 如果一个时钟到期的时候已经有一个时钟处理函数正在运行，则会推迟到原处理函数运行完成后的下一个 tick 才触发这次时钟
      }
    }
    yield();
  }
  usertrapret();
}
```



### 实验结果

```c
xv6 kernel is booting

hart 1 starting
hart 2 starting
init: starting sh

$ alarmtest
test0 start
....................alarm!
test0 passed
test1 start
..alarm!
.alarm!
.alarm!
..alarm!
.alarm!
.alarm!
..alarm!
.alarm!
..alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
```

## 实验感悟

本次实验alarm实验部分很难，通过查询了大量的资料最终才做出来。但做本次实验，通过对实验代码的反复阅读，也让我对实验代码的架构有了一个初步的认知，以及对trap机制有了一个具体化的理解。

### 代码架构

xv6 内核源码在 kernel/子目录下。按照模块化的概念，源码被分成了多个文件，图 2.2 列出了这些文件。模块间的接口在 defs.h(kernel/defs.h)中定义。

![image-20221030122504561](C:\Users\LENOVO\AppData\Roaming\Typora\typora-user-images\image-20221030122504561.png)

（来源于xv6实验指导书）
