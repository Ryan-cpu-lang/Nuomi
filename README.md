# Mochi —— 一门用 C 手撸的极简编程语言 + NUC 工具链

Mochi 是一门参考 Scheme / Clojure 的 **Lisp 风格**语言，用纯 C 从零实现（树遍历解释器 + 引用计数 GC）。
本仓库同时包含 **NUC 可执行二进制格式（novus-os）** 的读写 / 校验 / dump 工具链，方便把它接入你自己的系统。

> NUC 规范基准：<https://github.com/chen-xin-Liam/novus-os/blob/main/docs/NUC%20%E5%8F%AF%E6%89%A7%E8%A1%8C%E4%BA%8C%E8%BF%9B%E5%88%B6%E6%A0%BC%E5%BC%8F%E8%A7%84%E8%8C%83%20V1.0.md>

---

## 1. 目录结构（多文件打包）

```
.
├── include/
│   ├── mochi.h          # 语言：值 / 对象 / 环境 / 求值器 公共头
│   └── nucfmt.h         # NUC 格式库公共头
├── src/
│   ├── value.c          # 引用计数 GC、字符串驻留、值打印
│   ├── reader.c         # 读取器（S 表达式 / 向量 / 引号）
│   ├── env.c            # 作用域环境
│   ├── eval.c           # 求值器、特殊形式、闭包、apply
│   ├── native.c         # 内置原生函数
│   ├── main.c           # REPL 与脚本执行
│   ├── nucfmt.c         # NUC 格式：构建 / 解析 / 校验 / dump
│   └── nucutil.c        # NUC 命令行工具
├── samples/             # Mochi 示例程序（.mochi）
├── Makefile
└── build.bat           # Windows 一键构建
```

## 2. 构建

需要 C 编译器（clang 或 gcc）：

```bash
# Linux / macOS / 有 make 的环境
make            # 产出 mochi 与 nucutil

# Windows（没有 make 也行）
build.bat       # 自动找 clang，找不到再用 gcc
```

## 3. 用 Mochi 语言

**REPL：**
```bash
./mochi
mochi> (+ 1 2 3)
=> 6
mochi> (define (sq x) (* x x))
=> <fn sq>
mochi> (sq 9)
=> 81
```

**跑脚本：**
```bash
./mochi samples/factorial.mochi
# fact(10) = 3628800
```

### 3.1 语法速览

| 形式 | 含义 |
|------|------|
| `(+ - * / %)` | 算术（可变参数） |
| `(= < > <= >=)` | 数值比较 |
| `(and or not)` | 逻辑（`and`/`or` 短路） |
| `(if c t e)` | 分支 |
| `(while c body...)` | 循环 |
| `(define x 1)` / `(define (f a b) body...)` | 变量 / 函数定义 |
| `(set! x v)` | 重新赋值 |
| `(lambda (a) body...)` | 匿名函数（闭包） |
| `(begin a b c)` | 顺序执行，返回最后一个 |
| `'x` | 引号（不求值） |
| `(list 1 2 3)` | 链表 |
| `[1 2 3]` | 向量（数组，随机访问） |
| `(cons car cdr)` `(car l)` `(cdr l)` `(nth l i)` `(get v i)` `(set v i x)` `(length x)` | 容器操作 |
| `(str a b c)` | 拼接成字符串 |
| `(print ...) (println ...)` | 输出 |

值类型：数字（double）、布尔 `true`/`false`、空 `nil`、字符串、符号、链表、向量、函数/闭包。

`nil` 与 `false` 视为假，其余为真（含数字 0 与空字符串）。

## 4. NUC 工具链（`nucutil`）

NUC 是 novus-os 的原生可执行格式（魔数 `NUC!`，x86_64）。布局铁律：

```
Header(48) → SegmentTable → SymbolTable → RelocationTable → StringTable → 段数据(4KiB 页对齐)
```

| 表 | 单条大小 |
|----|----------|
| Header | 48 字节 |
| Segment | 32 字节 |
| Symbol | 24 字节 |
| Relocation | 16 字节 |

入口地址：`entry_rip = text.vaddr + entry_point`（`entry_point` 是 `.text` 段内偏移）。

命令：
```bash
# 生成一个带 PC32 重定位、可在真机跑通的最小示例 .nuc
./nucutil emit-demo demo.nuc

# 校验（按 §10 ABI 规则：magic / abi 主版本=1 / 64 位 / 段权限 W^X 等）
./nucutil validate demo.nuc

# 人类可读 dump（头/段表/符号表/重定位表/entry_rip）
./nucutil dump demo.nuc

# （仅 Windows x86_64）把 TEXT 段映射到 RWX 并执行入口，验证整条加载链路
./nucutil run demo.nuc        # 应输出：entry returned: 42 (0x2A)
```

`emit-demo` 产出的 `demo.nuc`：`.text` 里 `call main` 用 `R_X86_64_PC32` 重定位，加载器解析后跳到 `main`（`mov eax,0x2A; ret`），所以执行返回 42——这同时验证了 **格式生成 → 重定位 → 真机执行** 全链路。

## 5. 下一步：让 Mochi 真正产出 NUC 运行在 novus-os

目前 Mochi 是**解释执行**（在宿主上直接跑源码），`nucutil` 负责 **NUC 格式本身**。
要让 Mochi 程序作为 `.nuc` 在 novus-os 上跑，需要做编译后端，两条路线：

- **路线 A（原生代码生成）**：把 Mochi 直接编译成 x86_64 机器码塞进 `.text`，对齐你们 `cppcc` 的 syscall ABI。最贴近原生、性能最好，但工作量最大（寄存器分配、指令选择、系统调用）。
- **路线 B（内嵌字节码 VM）**：把 Mochi 编译成紧凑字节码，把一个小型字节码解释器（用 C 写、由 `cppcc` 编成 `.nuc`）作为运行时，字节码放进 `.data`，入口跳到 VM。实现快、可移植，先让语言在系统上跑起来。

告诉我你们倾向哪条路线，我接着往下做。

## 6. 已知限制

- 引用计数 GC：正常释放局部数据；**顶层闭包与全局绑定形成的环**在运行期不释放（程序结束统一回收），不造成运行期泄漏。
- 数字统一用 `double`。
- `nucutil run` 仅在 Windows x86_64 上做真机执行验证；其它平台只能 `dump`/`validate`。
