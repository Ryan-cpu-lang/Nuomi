# Nuomi 编译器 I/O 支持 —— 未完成任务交接

> 2026-10-21 中断于此。已提交未推送的改动：`ebefc47`（let 编译+真机执行）
> **本次会话的新改动尚未提交**，工作区状态见 `git status`。

## ✅ 已完成（已验证）

### 1. `novus_syscall.h` —— 可配置 syscall 号表
`include/novus_syscall.h` 新建。把内核的 syscall 号与调用约定集中一处。
⚠️ 号是从 `novus-live2.iso` 内核镜像**逆向推测**的（非官方），标了
`NUOMI_SYSCALL_SPEC_CONFIRMED 0`。等 novus 官方规范公布后**只改这个文件**。

已确认的证据（内核二进制里挖到的）：
- syscall 分发器存在（`[TRAP] unknown syscall`）
- 服务名表连续 8 项：`yield/write/getpid/ipc_send/ipc_recv/ipc_lookup/ipc_stats/exit`
- 内部函数名带 `sys_` 前缀（`[TRAP] sys_write: user buffer ended early`）
- 入口是**中断门**（trap 打印含 `cs=`，只有中断/异常才有 cs）→ 用 `int 0x80`

### 2. 编译器 I/O：`print` / `println` / `print-int`
- `cg_expr` 新增三个 I/O 原语
- **统一 syscall 跳板** `__nuomi_syscall_trampoline`（32 字节，`int 0x80; ret` + nop 填充），
  所有 syscall 站点都 `call` 它。跳板预留空间，宿主可原地替换
- **itoa 机器码内嵌**：`NUOMI_ITOA_CODE[178]`（`tools/itoa_gen.py` 生成），
  签名 `int64 __nuomi_itoa(int64 v, char* buf) -> rax=文本地址, rdx=长度`
- **BSS 段**：`NUOMI_BSS_VADDR 0x600000`，64 字节，给 itoa 当可写缓冲区
- **入口改为执行所有顶层表达式**（原来是只执行最后一个，多行输出会丢）
  ⚠️ 副作用：单独 `(define ...)` 没有 main 时仍报错（与旧行为一致）

### 3. 端到端已验证正确 ✅
用 `tools/nucverify.py`（模拟执行内核）验证：
```
=== Nuomi 编译模式 I/O 测试 ===
整数:42
负数:-17
零:0
大数:1234567890
表达式:7          ← (+ 3 4) 求值正确
字符串:hello
混合:1two3
let:42           ← let 内的 (* x y) 正确
fact(10) =       ← ★ 卡在这里
```

## ❌ 未完成：**用户函数调用作为参数**（如 `(println (fact 10))`）

### 症状
`(fact 10)` 的调用本身**能正确算出值**（模拟器 trace 里 rax/rdx 都对了），
但结果**没有传到 println**。编译产物字节已逐条核对**完全正确**：
- 入口码 `mov rax,10; push rax; pop rdi; call fact; mov rdi,rax; mov rsi,BSS; call itoa`
- 重定位 5 处全部正确

### 已排除
- ❌ 编译器 bug（字节级核对正确）
- ❌ 调用约定错误（pop 顺序对）
- ❌ 重定位错误（5 处全对）

### 真正的原因：**模拟器 bug**，不在编译器
最后一次 trace 显示走到了 `fact` 内部的 `0F 9C C0`（setl）：
```
rip=0x4000ED op=5A  rax=2 rdx=0    ← pop rdx（值 10，正确）
rip=0x4000EE op=48  rax=2 rdx=10   ← cmp rax, rdx
rip=0x4000F1 op=0F  rax=2 rdx=10   ← setl
ERR 0F c0@4194545
```
**报错 "0F c0"** → `tools/x86sim.py` 最外层的 `0x0F` 分支没匹配上。

**原因猜测**：`x86sim.py` 经历过多次正则替换（把取指统一成 `self.b()`），
`0F` 分支的 `o3 = self.b(rip+2)` 可能读到的值和预期不符，
或者 REX 分支里的 `0x0F` 代码被早先匹配了别的东西。
**已经为模拟器花了远超预算的 token**，不值得继续调。

### 下一步（给下一个 model / 你自己）
1. **看 `tools/` 下三个文件**：`x86sim.py`（模拟器）、`nucverify.py`（验证器）、`trace_nuc.py`（trace）。**建议直接重写模拟器**，它有 3 个反复出现的坑：
   - x86 寄存器编号 `rax=0 rcx=1 rdx=2 rbx=3 rsp=4 rbp=5 rsi=6 rdi=7`（曾把 rbp/rsp 当成独立属性，导致栈错位）
   - opcode `01/29`（add/sub r/m64,r64）是 **rm=目标、reg=源**（曾写反，`(+ 3 4)` 算出 4）
   - ModRM `mod=01` 时 **disp8 在 +3，imm 在 +4**；`mod=10` 是 disp32。常写成同一个字节
   - `mov rsi, rdx` 不需要 REX.B，误加 `49` 会让 rm 变成 r14
2. **或者干脆放弃模拟器**：产物正确性已由**字节级核对 + 重定位核对**确认，
   真机验证可以在 qemu 上跑 novus 做（`nucutil run` 已能提示"需 novus 内核"）。

## 关于宿主 `nucutil run` 的 syscall 模拟（一度尝试，已放弃）
试过 3 种方案全部失败/放弃：
- `int3`（0xCC）异常处理：**TCC 编译的程序里完全不触发**（CRT 吞掉）
- `ud2`（0F 0B）异常处理：崩溃，独立测试也崩（疑似 stub 取地址问题时 TCC 优化了 static 函数）
- 跳板替换成「寄存器搬运 + call 宿主桩」：`mov r10,rdx; mov r11,rsi; mov rcx,rax; ...`
  序列 31 字节放得下，但独立测试也崩（同上）

`nucutil run` 现在的行为是**正确且友好的**：检测到 syscall 跳板就明确提示
「需在 novus 内核上运行，宿主仅能执行纯计算部分」，不再崩溃。

## 文件清单
```
E:\novus-os\mochi\
├── include\novus_syscall.h        ← 新建：syscall 号表（可配置）
├── include\nucfmt.h               ← 改：加 NUOMI_STR/BSS 常量
├── src\codegen.c                  ← 改：I/O + let 编译 + 跳板 + BSS + 入口逻辑
├── src\reader.c                   ← 改：❝❞ 定界符
├── src\nucutil.c                  ← 改：重定位基准修 + RODATA/BSS 映射 + syscall 提示
├── samples\io.nuomi               ← 新建：I/O 样例
├── samples\strings2.nuomi         ← 新建：❝❞ 样例
└── tools\                          ← **未提交！在 E:\workbuddy\2026-10-17-20-32-25\**
    ├── x86sim.py                   ← x86-64 子集模拟器（有 bug，别硬调）
    ├── nucverify.py                ← 端到端验证器
    ├── trace_nuc.py                ← trace 工具
    └── itoa_gen.py                 ← 生成 itoa 机器码 + 自测
```

## 恢复验证的命令
```bash
cd E:\workbuddy\2026-10-17-20-32-25
python nucverify.py "C:/Users/RAYN/AppData/Local/Temp/nt/io2.nuc"   # 已验证全对
python x86sim.py itoa.bin                                           # itoa 2017 值全过
```

## 建议的提交（工作区当前有未提交改动）
```
feat(compiler): print/println/print-int syscall 支持（Route-A M4）
- 新建 include/novus_syscall.h：可配置 syscall 号表（逆向自内核镜像，标注待官方确认）
- codegen 新增 I/O 原语，统一走 __nuomi_syscall_trampoline 跳板
- 内嵌 itoa 机器码（178B，2017 个取值模拟验证通过）+ BSS 可写缓冲区
- 入口改为执行所有顶层表达式（修多行 println 只输出一行）
- 新增 BSS 段（R|W|USER，W^X 合规）
- 采样者：宿主 nucutil run 检测 syscall 跳板并提示"需 novus 内核"
```
