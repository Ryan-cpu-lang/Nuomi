// SPDX-License-Identifier: GPL-2.0
/*
 * novus_syscall.h — novus-os 系统调用号表与调用约定
 *
 * ====================== 为什么单独一个文件 ======================
 * syscall 号由**内核**定义，用户态（Nuomi 编译器）必须知道编号才能调用。
 * 这里把「号」和「参数约定」集中到唯一一处：
 *   · 内核改了号 / 加了服务 → 只改这个文件，编译器不用动；
 *   · 编译器所有 syscall 相关代码只依赖本文件的宏，不出现裸数字。
 *
 * ====================== 目前的可信度 ======================
 * 下面的号是**从 novus 内核镜像 novus-live2.iso 逆向推导**的，不是官方文档：
 *
 *   已确认的事实（内核二进制里有直接证据）：
 *     · 内核存在 syscall 分发器（报错串 "[TRAP] unknown syscall"）
 *     · 服务名表为连续 8 项，顺序固定：
 *         yield / write / getpid / ipc_send / ipc_recv / ipc_lookup / ipc_stats / exit
 *       （在镜像 0x5DE80 处为一张连续的名字表，偏移按名字长度递增，
 *        典型「按序号索引的表」布局；其前方还跟着一张 8 字节的函数指针表）
 *     · 内部实现函数名带 sys_ 前缀（"[TRAP] sys_write: user buffer ended early"）
 *     · 入口是中断门（trap 打印含 cs= 代码段寄存器，只有中断/异常才有 cs）
 *
 *   属于**推测**的部分（标 [推测]）：具体编号数值、参数个数与寄存器约定。
 *
 * ⚠️ 等 novus 官方公布 syscall 规范后，请把本文件的号与参数约定替换为正式值，
 *    并把下面的 NUOMI_SYSCALL_SPEC_CONFIRMED 置1；编译器无需任何改动。
 */

/* 号表是否已被官方确认？0 = 尚属逆向推测，1 = 官方规范已确认 */
#define NUOMI_SYSCALL_SPEC_CONFIRMED 0

/* ---------- 系统调用号 ---------- */
/* [推测] 依据：名字表里 yield 排第 0、write 排第 1…（按表序） */
#define NOVUS_SYS_YIELD      0   /* 让出 CPU（调度） */
#define NOVUS_SYS_WRITE      1   /* 写文件/终端：[推测] (fd, buf, len) -> written */
#define NOVUS_SYS_GETPID     2   /* 取进程号 */
#define NOVUS_SYS_IPC_SEND   3   /* 进程间通信发送 */
#define NOVUS_SYS_IPC_RECV   4   /* 进程间通信接收 */
#define NOVUS_SYS_IPC_LOOKUP 5   /* IPC 查找 */
#define NOVUS_SYS_IPC_STATS  6   /* IPC 统计 */
#define NOVUS_SYS_EXIT       7   /* 退出当前任务 */

/* 名字表里没有的号 → 内核会报 "unknown syscall" */
#define NOVUS_SYS_UNKNOWN    (-1)

/* ---------- 调用约定 ---------- */
/*
 * [推测] 与 NUC 规范 §1 声明的 SysV AMD64 参数寄存器一致（见 docs/Nuomi语言规范.md）：
 *   rax = syscall 号
 *   rdi, rsi, rdx, rcx, r8, r9 = 第 1..6 个参数
 *   rax = 返回值
 * 之所以沿用 SysV：Nuomi 的 NUC 规范已经冻结了这个约定（内核加载 .nuc 时
 * 不会另立一套），用它才能让用户态与内核侧对得上。
 */
#define NOVUS_SYSCALL_REG_NUM     "rax"
#define NOVUS_SYSCALL_ARG_REGS    "rdi, rsi, rdx, rcx, r8, r9"

/* 入口指令：内核 trap 打印含 cs=（代码段），说明是**中断门**而非 syscall 指令。
 * 汇编形式（x86-64）：
 *     mov rax, <号>
 *     mov rdi, <arg1>
 *     ...
 *     int  0x80            ; 进入内核（号在 rax，参数在 rdi..r9）
 *     ; 返回后 rax = 结果
 * [推测] 中断向量号 0x80；内核 0-31 是异常、32 是定时器（字符串
 *         "ctors (0-31 exceptions, 32 timer)"），0x80 落在用户自定义区，不冲突。
 */
#define NOVUS_SYSCALL_INTR_VECTOR 0x80

/* ---------- 供编译器使用的服务描述 ---------- */
/* 每个服务的参数个数（[推测]），用于生成调用点时压几个参数 */
#define NOVUS_ARGC_WRITE      3   /* (fd, buf, len) */
#define NOVUS_ARGC_GETPID     0
#define NOVUS_ARGC_EXIT       1   /* (code) */
#define NOVUS_ARGC_YIELD      0
#define NOVUS_ARGC_IPC_SEND   3   /* (port, buf, len) [推测] */
#define NOVUS_ARGC_IPC_RECV   2   /* (port, buf) [推测] */
#define NOVUS_ARGC_IPC_LOOKUP 2
#define NOVUS_ARGC_IPC_STATS  1

/* 写文件用的标准描述符 */
#define NOVUS_FD_STDOUT       1   /* [推测] 1 = stdout */
#define NOVUS_FD_STDERR       2   /* [推测] 2 = stderr */
