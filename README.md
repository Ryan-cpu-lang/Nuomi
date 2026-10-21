# Nuomi —— 一门纯 C 制作的极简编程语言 + NUC 工具链

Nuomi 是一门参考 Scheme / Clojure 的 **Lisp 风格**语言，用纯 C 从零实现（树遍历解释器 + 引用计数 GC）。
本仓库同时包含 **NUC 可执行二进制格式（novus-os）** 的读写 / 校验 / dump 工具链，方便把它接入你自己的系统。

> NUC 规范基准：<https://github.com/chen-xin-Liam/novus-os/blob/main/docs/NUC%20%E5%8F%AF%E6%89%A7%E8%A1%8C%E4%BA%8C%E8%BF%9B%E5%88%B6%E6%A0%BC%E5%BC%8F%E8%A7%84%E8%8C%83%20V1.0.md>
>
> 📖 语言文档：[`docs/Nuomi语言规范.md`](docs/Nuomi语言规范.md)（**权威规范**，冲突以此为准） ｜ [`Nuomi语言手册.md`](Nuomi语言手册.md)（上手教程）

---

## 📥 下载（Windows / Novus / Linux）

> 当前版本：**Route-A M3** —— 解释器全功能；编译器支持 整数 / 函数(递归) / 局部变量 / if·while / 比较 / **let·let\*·letrec** / **字符串字面量** → `.nuc`

### 🪟 Windows

1. 下载本仓库（Code → Download ZIP，或 `git clone`）
2. 双击 **`build.bat`**（自动探测 clang / gcc / 绿色版 TCC）→ 得到 `nuomi.exe` + `nucutil.exe`
3. 双击 **`run-guess.bat`** 玩猜数字、**`run-games.bat`** 看游戏合集
4. （可选）双击 **`make-shortcuts.bat`** 生成带 Nuomi 图标的快捷方式

💾 **不想自己编译？**直接下载预编译版（Windows x64）：**[`nuomi.exe`](https://github.com/Ryan-cpu-lang/Nuomi/releases/download/v0.2.0-m2/nuomi.exe)** · [`nucutil.exe`](https://github.com/Ryan-cpu-lang/Nuomi/releases/download/v0.2.0-m2/nucutil.exe)（来自 [Release v0.2.0-m2](https://github.com/Ryan-cpu-lang/Nuomi/releases/tag/v0.2.0-m2)）

> 完整版本历史与各平台预编译包见 [**Releases**](https://github.com/Ryan-cpu-lang/Nuomi/releases) 页面。

### 🖥️ Novus OS（诺维斯）

直接下载 [`dist/novus/`](dist/novus/) 下**已编译好的 .nuc**，拷进 novus-os 用 shell exec 执行：

| 文件 | 功能 | 入口返回值 |
|---|---|---|
| [`demo.nuc`](dist/novus/demo.nuc) | 算术演示 | 142 |
| [`fact.nuc`](dist/novus/fact.nuc) | 递归阶乘 fact(10) | 3628800 |
| [`fib.nuc`](dist/novus/fib.nuc) | 双递归斐波那契 fib(10) | 55 |
| [`sum.nuc`](dist/novus/sum.nuc) | while 循环求和 0..99 | 4950 |

- 详见 [`dist/novus/README.md`](dist/novus/README.md)（校验 / 重新编译方法）
- 自编程序：宿主机上 `nuomi -c 程序.nuomi -o 程序.nuc`
- ⚠️ 编译子集暂无 I/O（`print`/`println` 等内置函数暂不能编译，待 novus 系统调用号表公布）

### 🐧 Linux

```bash
git clone https://github.com/Ryan-cpu-lang/nuomi.git && cd nuomi
make          # 需要 gcc + make，产出 nuomi 与 nucutil
make test     # 冒烟：全部样例 + NUC 流水线
```
（或 `./test-linux.sh` 一键）

> 预编译交付物直接入库：`dist/windows/*.exe`（Windows x64 预编译包）、`dist/novus/*.nuc`（诺维斯渠道）。其余本地 build 产物（*.exe/*.o 等）仍不入库，三平台统一从源码一键构建。

---

## 1. 目录结构（多文件打包）

```
.
├── include/             # nuomi.h / nucfmt.h / codegen.h
├── src/
│   ├── value.c          # 引用计数 GC、字符串驻留、值打印
│   ├── reader.c         # 读取器（S 表达式 / 向量 / 引号）
│   ├── env.c            # 作用域环境
│   ├── eval.c           # 求值器、特殊形式、闭包、apply
│   ├── native.c         # 内置原生函数（含 input / input-int）
│   ├── main.c           # REPL / 脚本执行 / -c 编译模式
│   ├── codegen.c        # Route-A 编译后端（Nuomi 子集 → x86_64 → .nuc）
│   ├── nucfmt.c         # NUC 格式：构建 / 解析 / 校验 / dump
│   └── nucutil.c        # NUC 命令行工具（emit/validate/dump/run）
├── samples/             # Nuomi 示例程序（.nuomi，可解释运行）
├── dist/novus/          # 预编译示例 .nuc（诺维斯渠道）+ 源码
├── tools/               # gen-icon.py / make-shortcuts.ps1
├── docs/                # Nuomi 语言规范（权威）
├── Makefile             # Linux/macOS 构建（make / make test）
├── build.bat            # Windows 一键构建
├── run-games.bat        # 双击：游戏合集
├── run-guess.bat        # 双击：猜数字
├── make-shortcuts.bat   # 双击：生成带图标快捷方式
└── test-linux.sh        # Linux 一键冒烟
```

## 2. 构建

需要 C 编译器（clang 或 gcc）：

```bash
# Linux / macOS / 有 make 的环境
make            # 产出 nuomi 与 nucutil

# Windows（没有 make 也行）
build.bat       # 自动找 clang，找不到再用 gcc
```

## 3. 用 Nuomi 语言

**REPL：**
```bash
./nuomi
nuomi> (+ 1 2 3)
=> 6
nuomi> (define (sq x) (* x x))
=> <fn sq>
nuomi> (sq 9)
=> 81
```

**跑脚本：**
```bash
./nuomi samples/factorial.nuomi
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
| `(let ((x 1)) body...)` | **局部作用域**（并行绑定，不污染全局） |
| `(let* ((x 1)) body...)` | 局部作用域（顺序绑定，后项可见前项） |
| `(letrec ((f ...)) body...)` | 局部作用域（可自引用 / 互递归） |
| `(lambda (a) body...)` | 匿名函数（闭包） |
| `(begin a b c)` | 顺序执行，返回最后一个 |
| `'x` | 引号（不求值） |
| `❝hello❞` | 字符串（**推荐定界符**，免转义）/ `"hello"` 传统写法同样可用 |
| `(map fn coll)` `(filter pred coll)` `(reduce fn coll [init])` | **高阶函数** |
| `(range n [end [step]])` | 数字序列（支持负步长） |
| `(reverse coll)` `(append a b)` `(last coll)` `(null? x)` `(member? x coll)` | 序列工具 |
| `(assoc lst key val)` | 关联表（两参为查询，未命中返回 `nil`） |
| `(abs x)` `(floor x)` `(ceil x)` `(round x)` `(sqrt x)` `(pow a b)` `(min ...)` `(max ...)` | 数学函数 |
| `(number? x)` `(string? x)` `(list? x)` `(vector? x)` `(function? x)` `(bool? x)` | 类型判断 |
| `(list 1 2 3)` | 链表 |
| `[1 2 3]` | 向量（数组，随机访问） |
| `(cons car cdr)` `(car l)` `(cdr l)` `(nth l i)` `(get v i)` `(set v i x)` `(length x)` | 容器操作 |
| `(str a b c)` | 拼接成字符串 |
| `(print ...) (println ...)` | 输出 |

值类型：数字（double）、布尔 `true`/`false`、空 `nil`、字符串、符号、链表、向量、函数/闭包。

`nil` 与 `false` 视为假，其余为真（含数字 0 与空字符串）。

> **递归深度**：约 3700+ 层函数递归安全可用（早期版本约 500 层即栈溢出）；超出会输出可读报错 `maximum recursion depth exceeded` 并终止，不会崩溃。

完整示例见 `samples/stdlib.nuomi`（标准库）与 `samples/scope.nuomi`（作用域/闭包）。

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

## 5. 下一步：让 Nuomi 真正产出 NUC 运行在 novus-os

目前 Nuomi 是**解释执行**（在宿主上直接跑源码），`nucutil` 负责 **NUC 格式本身**。
要让 Nuomi 程序作为 `.nuc` 在 novus-os 上跑，需要做编译后端，两条路线：

- **路线 A（原生代码生成）**：把 Nuomi 直接编译成 x86_64 机器码塞进 `.text`，对齐你们 `cppcc` 的 syscall ABI。最贴近原生、性能最好，但工作量最大（寄存器分配、指令选择、系统调用）。
- **路线 B（内嵌字节码 VM）**：把 Nuomi 编译成紧凑字节码，把一个小型字节码解释器（用 C 写、由 `cppcc` 编成 `.nuc`）作为运行时，字节码放进 `.data`，入口跳到 VM。实现快、可移植，先让语言在系统上跑起来。

告诉我你们倾向哪条路线，我接着往下做。

## 6. 已知限制

- 引用计数 GC：正常释放局部数据；**顶层闭包与全局绑定形成的环**在运行期不释放（程序结束统一回收），不造成运行期泄漏。
- 数字统一用 `double`。
- `nucutil run` 仅在 Windows x86_64 上做真机执行验证；其它平台只能 `dump`/`validate`。

## 7. 作为 novus-os 模块接入（user/nuomi/ + tools/nucutil/）

本仓库目标作为 novus-os 的用户态语言模块合入官方仓库，遵循其 GPLv2 + Kbuild/Makefile 模块化惯例。

### 7.1 目录映射（drop-in）

| 本仓库内容 | 合入 novus-os 的位置 | 说明 |
|------------|----------------------|------|
| `src/*.c` `include/nuomi.h` `samples/` `Makefile` `Kconfig` `LICENSE` | `user/nuomi/` | Nuomi 编译器 + 运行时（用户态 `.nuc` 生产者） |
| `src/nucfmt.c` `src/nucutil.c` `include/nucfmt.h` | `tools/nucutil/` | 主机端 NUC 格式工具（构建期用） |

> 注意：`nucfmt`（NUC 格式库）被**编译器（codegen）和 nucutil 共用**。
> 合并时可二选一：① 在 `lib/nucfmt/` 放单一副本，两处都引用；② 各自包含一份。
> 具体由系统制作人定，本仓库暂保持单仓自包含形态。

### 7.2 接线方式（给 novus-os 维护者）

1. 顶层 `Makefile` 的子目录列表里加入 `user/nuomi` 与 `tools/nucutil`；
2. 对应 `Kconfig` 里 `source` 本目录的 `Kconfig`（见本仓库 `Kconfig`）；
3. 构建：`make`（交叉编译 `CROSS_COMPILE=x86_64-elf-`，默认 `gcc`）。

### 7.3 ⚠️ 必须跟系统制作人确认的一处规范矛盾

- novus-os **README** 写的是："内核内置 **Nuc 字节码执行引擎**，无独立 nucvm"。
- 但**冻结的 NUC 规范 V1.0**（唯一基准文档）明确：`.text` 段装的是 **x86_64 原生机器码**，
  重定位类型是 `R_X86_64_32` / `R_X86_64_PC32`，加载流程是"上下文切换到用户态，跳转 entry_rip"。

两者自相矛盾。Nuomi 后端目前按**规范**走原生机器码（Route A，x86_64 直编 `.text`），
已验证算术用例全对。合入前请系统制作人拍板：内核加载器到底执行原生码还是字节码，
以免我们对齐错方向。

### 7.4 当前进度

- 解释器：引用计数 UAF 已修复并连跑验证通过；9 个样例全过。
- NUC 工具链：`emit-demo → validate → dump → run` 全通，`run` 返回 42 (0x2A)。
- 后端 Route A 垂直切片：x86_64 直编 `.text`，算术 10 组用例全对（含嵌套/减法/大整数/符号引用）。
- 待做：函数调用 + 运行时（GC、内置函数）、控制流、字符串/向量/闭包、对接 novus-os 系统调用 ABI。
