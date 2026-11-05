# Nuomi —— 纯 C 极简 Lisp 语言 + NUC 工具链 + GUI 窗口

Nuomi 是一门参考 Scheme / Clojure 的 **Lisp 风格**语言，用纯 C 从零实现（树遍历解释器 + 引用计数 GC）。

内置 **窗口 / GUI 原语**（Windows 上是真 Win32 窗口，将来 novus 上走 Novus-WM IPC）——窗口版贪吃蛇 `samples/snake-wm.nuomi` 开箱即玩；另有 **HTTP / 进程 / SHA-1 / unzip / 字符串 / JSON / 文件** 等应用层原语。

> 🐍 旗舰应用：[**NuomiLauncher**](https://github.com/Ryan-cpu-lang/NuomiLauncher) —— 用糯米语言写的 Minecraft 启动器（GUI + CLI），就是靠下面这些原语搭出来的。

本仓库同时包含 **NUC 可执行二进制格式（novus-os）** 的读写 / 校验 / dump / 编译工具链，方便把它接入你自己的系统。

> NUC 规范基准：<https://github.com/chen-xin-Liam/novus-os/blob/main/docs/NUC%20%E5%8F%AF%E6%89%A7%E8%A1%8C%E4%BA%8C%E8%BF%9B%E5%88%B6%E6%A0%BC%E5%BC%8F%E8%A7%84%E8%8C%83%20V1.0.md>
>
> 📖 语言文档：[`docs/Nuomi语言规范.md`](docs/Nuomi语言规范.md)（**权威规范**，冲突以此为准） ｜ [`Nuomi语言手册.md`](Nuomi语言手册.md)（上手教程）



---

## 📥 下载（Windows / Novus / Linux）

> 当前版本：**v0.3.0** —— 解释器全功能（含 **窗口 GUI** / HTTP / 进程 / JSON / 文件 / 实时游戏原语）；编译器（Route A）支持 整数 / 函数(递归) / 局部变量 / if·while / 比较 / let·let*·letrec / 字符串字面量 / **print·println（走 syscall write）** → `.nuc`

### 🪟 Windows

1. 下载本仓库（Code → Download ZIP，或 `git clone`）
2. 双击 **`build.bat`**（自动探测 clang / gcc / 绿色版 TCC）→ 得到单一 `nuomi.exe`（已内置 NUC 工具链，用 `nuomi nuc` 调用）
3. 双击 **`run-guess.bat`** 玩猜数字、**`run-games.bat`** 看游戏合集
4. `nuomi samples/snake-wm.nuomi` —— 弹出**真窗口**的彩色贪吃蛇（Win32 后端；方向键/WASD 移动、R 重开、Esc 退出）
5. （可选）双击 **`make-shortcuts.bat`** 生成带 Nuomi 图标的快捷方式

💾 **不想自己编译？**&#x76F4;接下载预编译版（Windows x64，单一文件）：\*\*[`nuomi.exe`](https://github.com/Ryan-cpu-lang/Nuomi/releases/download/v0.3.0/nuomi.exe)（来自 [Release v0.3.0](https://github.com/Ryan-cpu-lang/Nuomi/releases/tag/v0.3.0)）—— 解释器、编译后端、NUC 工具链都在这一个 exe 里，用 `nuomi nuc` 调用 NUC 命令。

> 完整版本历史与各平台预编译包见 [**Releases**](https://github.com/Ryan-cpu-lang/Nuomi/releases) 页面。

### 🔄 更新 Nuomi

装好之后，一条命令就能把自己升级到 GitHub 上的最新 release（自更新，无需重下安装包）：

```bat
nuomi version          # 显示当前版本
nuomi update           # 检查 GitHub latest release，有新版本就下载并替换自己
nuomi update --check   # 只检查、不下载
```

- `nuomi update` 会查 [Releases](https://github.com/Ryan-cpu-lang/Nuomi/releases) 里最新的 `nuomi.exe`（单一文件，已含全部功能），下载后用「重命名备份 + 改名替换」的方式就地替换**正在运行的这份 exe**——Windows 下不能直接删除运行中的 exe，但重命名可以，所以这套做法安全，失败会自动回滚。
- 替换的是**当前正在运行的 nuomi.exe**（自动用 `GetModuleFileName` 定位自身），因此无论你是安装版（`%LOCALAPPDATA%\Programs\Nuomi`）还是便携版，跑 `nuomi update` 都更新对应的那份。
- 当前进程仍是旧版本，**重启 nuomi 后生效**。
- 开发 / 自托管：`nuomi update --url <api地址>` 可指向自定义更新源；`nuomi update --exe <路径>` 可指定更新目标（测试用）。
- 🇨🇳 **国内直连 `api.github.com` 被墙？** update 会自动回退到 ghproxy 系镜像（ghproxy.net / ghfast.top / gh-proxy.com）重试，查询与下载都走镜像。镜像列表在 `src/update.c` 的 `NUOMI_UPDATE_PROXIES`，失效可增删。

#### 📦 一键安装（推荐）：装完 cmd 里直接打 `nuomi`

下载 **[`Nuomi-Setup.exe`](installer/out/Nuomi-Setup.exe)** → 双击 → 一路下一步即可。

- 装到 `%LOCALAPPDATA%\Programs\Nuomi`，**用户级安装，不需要管理员权限**
- **自动把该目录加进你的用户 PATH** —— 装完新开一个 cmd，直接敲 `nuomi 脚本.nuomi` 就能跑（不用敲全路径、不用配环境变量）
- 附带开始菜单快捷方式与卸载程序；**卸载时会自动把自己从 PATH 里摘掉**，不留垃圾路径
- 中英文安装界面（默认简体中文）

> 装了之后老窗口不生效是正常的：PATH 改动只对**新开**的命令行窗口可见，重开一个 cmd / PowerShell 即可。

**不想用安装包？** 用便携脚本 [`installer/install.cmd`](installer/install.cmd)（同样是"复制到用户目录 + 加 PATH"，不需要 Inno Setup）。

装完后开始菜单里多了一个 **「运行 Nuomi 脚本」** 快捷方式：把任意 `.nuomi` 文件拖到它上面就能运行，而且跑完窗口不自动关闭（方便看输出）。它等价于以前的「运行Nuomi脚本.bat」，但不再写死仓库路径、也不依赖那个 bat 了。

**想自己重编安装包？** 用 [Inno Setup](https://jrsoftware.org/isdl.php) 打开 [`installer/Nuomi.iss`](installer/Nuomi.iss) 按 F9，或命令行：

```bat
"C:\Program Files\Inno Setup 7\ISCC.exe" installer\Nuomi.iss
```

产出 `installer\out\Nuomi-Setup.exe`。

### 🖥️ Novus OS（诺维斯）

直接下载 [`dist/novus/`](dist/novus/) 下**已编译好的 .nuc**，拷进 novus-os 用 shell exec 执行：

| 文件                                | 功能               | 入口返回值   |
| --------------------------------- | ---------------- | ------- |
| [`demo.nuc`](dist/novus/demo.nuc) | 算术演示             | 142     |
| [`fact.nuc`](dist/novus/fact.nuc) | 递归阶乘 fact(10)    | 3628800 |
| [`fib.nuc`](dist/novus/fib.nuc)   | 双递归斐波那契 fib(10)  | 55      |
| [`sum.nuc`](dist/novus/sum.nuc)   | while 循环求和 0..99 | 4950    |

- 详见 [`dist/novus/README.md`](dist/novus/README.md)（校验 / 重新编译方法）
- 自编程序：宿主机上 `nuomi -c 程序.nuomi -o 程序.nuc`
- **print / println / print-int 已可编译**（走 novus `write` syscall；号表与入口已按官方《Novus-WM 与 IPC 协议规范 V1.0》扶正）。文件 I/O（open/read/fwrite/close/stat/opendir/readdir/mkdir）已有 syscall 号但待内核确认实现；delete/rmdir/mv 暂缺 unlink/rmdir/rename syscall。

### 🐧 Linux

```bash
git clone https://github.com/Ryan-cpu-lang/nuomi.git && cd nuomi
make          # 需要 gcc + make，产出单一 nuomi（已含 NUC 工具链）
make test     # 冒烟：全部样例 + NUC 流水线
```

（或 `./test-linux.sh` 一键）

> 预编译交付物直接入库：`dist/windows/*.exe`（Windows x64 预编译包）、`dist/novus/*.nuc`（诺维斯渠道）。其余本地 build 产物（*.exe/*.o 等）仍不入库，三平台统一从源码一键构建。

---

## 1. 目录结构（多文件打包）

```
.
├── include/             # nuomi.h / nucfmt.h / codegen.h / novus_syscall.h / novus_wm.h
├── src/
│   ├── value.c          # 引用计数 GC、字符串驻留、值打印
│   ├── reader.c         # 读取器（S 表达式 / 向量 / 引号）
│   ├── env.c            # 作用域环境
│   ├── eval.c           # 求值器、特殊形式、闭包、apply
│   ├── native.c         # 内置原语总注册（defNative）
│   ├── json.c           # JSON 解析 / 取值 / 序列化
│   ├── http.c           # HTTP（http-get / http-download 等，WinINet）
│   ├── proc.c           # 进程（proc-spawn / proc-run 等）
│   ├── sha1.c           # SHA-1（sha1-string / sha1-file）
│   ├── zip.c            # 解压 zip/jar（自带 deflate，无 zlib 依赖）
│   ├── strutil.c        # 字符串工具（str-split / str-replace）
│   ├── wm.c             # 窗口原语（Windows: Win32 真窗口；非 Windows: 假 WM）
│   ├── update.c         # nuomi update 自更新（官方 + 国内镜像回退）
│   ├── main.c           # REPL / 脚本执行 / -c 编译模式
│   ├── codegen.c        # Route-A 编译后端（Nuomi 子集 → x86_64 → .nuc）
│   ├── nucfmt.c         # NUC 格式：构建 / 解析 / 校验 / dump
│   └── nucutil.c        # NUC 工具链（已并入 nuomi.exe，用 `nuomi nuc` 调用）
├── samples/             # 示例 22 个：snake-wm.nuomi（窗口贪吃蛇）/ wm-demo.nuomi / snake.nuomi ...
├── installer/           # Inno Setup 安装包（Nuomi.iss：文件关联 + PATH + 开始菜单）
├── dist/novus/          # 预编译示例 .nuc（诺维斯渠道）+ 源码
├── dist/windows/        # 预编译 nuomi.exe（入库交付物）
├── tools/               # gen-icon.py / make-shortcuts.ps1
├── docs/                # Nuomi 语言规范（权威）
├── Makefile             # Linux/macOS 构建（make / make test）
├── build.bat            # Windows 一键构建（clang / gcc / TCC）
├── nuomi-file-assoc.reg # 便携版文件关联（.nuomi / .nuc，不装安装包时用）
├── run-games.bat        # 双击：游戏合集
├── run-guess.bat        # 双击：猜数字
├── make-shortcuts.bat   # 双击：生成带图标快捷方式
└── test-linux.sh        # Linux 一键冒烟
```

## 2. 构建

需要 C 编译器（clang 或 gcc）：

```bash
# Linux / macOS / 有 make 的环境
make            # 产出单一 nuomi（已含 NUC 工具链）

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

| 形式                                                                                          | 含义                                                                                   |
| ------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| `(+ - * / %)`                                                                               | 算术（可变参数）                                                                             |
| `(= < > <= >=)`                                                                             | 数值比较                                                                                 |
| `(and or not)`                                                                              | 逻辑（`and`/`or` 短路）                                                                    |
| `(if c t e)`                                                                                | 分支                                                                                   |
| `(while c body...)`                                                                         | 循环                                                                                   |
| `(define x 1)` / `(define (f a b) body...)`                                                 | 变量 / 函数定义                                                                            |
| `(set! x v)`                                                                                | 重新赋值                                                                                 |
| `(let ((x 1)) body...)`                                                                     | **局部作用域**（并行绑定，不污染全局）                                                                |
| `(let* ((x 1)) body...)`                                                                    | 局部作用域（顺序绑定，后项可见前项）                                                                   |
| `(letrec ((f ...)) body...)`                                                                | 局部作用域（可自引用 / 互递归）                                                                    |
| `(lambda (a) body...)`                                                                      | 匿名函数（闭包）                                                                             |
| `(begin a b c)`                                                                             | 顺序执行，返回最后一个                                                                          |
| `'x`                                                                                        | 引号（不求值）                                                                              |
| `❝hello❞`                                                                                   | 字符串（**推荐定界符**，免转义）/ `"hello"` 传统写法同样可用                                               |
| `(map fn coll)` `(filter pred coll)` `(reduce fn coll [init])`                              | **高阶函数**                                                                             |
| `(range n [end [step]])`                                                                    | 数字序列（支持负步长）                                                                          |
| `(reverse coll)` `(append a b)` `(last coll)` `(null? x)` `(member? x coll)`                | 序列工具                                                                                 |
| `(assoc lst key val)`                                                                       | 关联表（两参为查询，未命中返回 `nil`）                                                               |
| `(abs x)` `(floor x)` `(ceil x)` `(round x)` `(sqrt x)` `(pow a b)` `(min ...)` `(max ...)` | 数学函数                                                                                 |
| `(number? x)` `(string? x)` `(list? x)` `(vector? x)` `(function? x)` `(bool? x)`           | 类型判断                                                                                 |
| `(list 1 2 3)`                                                                              | 链表                                                                                   |
| `[1 2 3]`                                                                                   | 向量（数组，随机访问）                                                                          |
| `(cons car cdr)` `(car l)` `(cdr l)` `(nth l i)` `(get v i)` `(set v i x)` `(length x)`     | 容器操作                                                                                 |
| `(str a b c)`                                                                               | 拼接成字符串                                                                               |
| `(substring s [start [end]])`                                                               | 按**字节**切片；start/end 支持负数（`-1`=末尾）                                                    |
| `(string-search hay needle [start])`                                                        | 找子串，返回下标；找不到 `-1`                                                                    |
| `(print ...) (println ...)`                                                                 | 输出                                                                                   |
| `(input [提示])` `(input-int [提示])`                                                           | 行式输入（读到回车为止）                                                                         |
| `(cls)` `(home)` `(show-cursor)`                                                            | 清屏+归位+隐光标 / 只归位+隐光标 / 恢复光标                                                           |
| `(sleep 毫秒)`                                                                                | 延时                                                                                   |
| `(key)`                                                                                     | **非阻塞**键盘：无按键返回 `nil`；方向键返回 `'up` `'down` `'left` `'right`；Esc 返回 `'esc`；普通键返回单字符字符串 |
| `(random n)`                                                                                | `[0, n)` 随机整数（LCG + 时间种子）                                                            |
| `(draw s)`                                                                                  | **原样**输出字符串（不加引号），一次 `fwrite` 写完——游戏整帧输出用                                            |
| `(exists? p)` `(is-dir? p)` `(file-size p)`                                                 | 查询：存在 / 是目录 / 字节数（失败 -1）                                                             |
| `(read-file p)` `(write-file p s)` `(append-file p s)`                                      | 读整文件 / 覆盖写 / 追加写                                                                     |
| `(ls dir)`                                                                                  | 列目录 → 文件名列表（不含 `.` `..`；失败 `nil`）                                                    |
| `(mkdir p)` `(rmdir p)` `(delete p)` `(mv a b)` `(cp a b)`                                  | 建目录 / 删空目录 / 删文件 / 移动 / 复制（返回布尔）                                                     |
| `(json-parse s)` `(json-get v k...)` `(json-stringify v)` `(json-obj k v...)`               | JSON 解析 / 多层路径取值 / 序列化 / 构造对象                                                        |
| `(http-get url [ua])` `(http-head url)` `(http-length)` `(http-download url path)` `(http-download-many ...)` `(http-download-from ...)` | **HTTP**：拉响应体 / 头 / 长度 / 下载落盘 / 批量并发下载（Windows WinINet）                                       |
| `(proc-spawn cmd args)` `(proc-read p)` `(proc-read-all p)` `(proc-alive? p)` `(proc-wait p)` `(proc-kill p)` `(proc-close p)` `(proc-run cmd args)` | **进程**：启动 / 读输出 / 存活 / 等待 / 终止 / 一把跑完返回退出码                                          |
| `(sha1-string s)` `(sha1-file p)` `(unzip zip dest)`                                       | **SHA-1 校验**（字符串/文件）/ **解压 zip·jar**（自带 deflate，无 zlib 依赖）                            |
| `(str-split s sep)` `(str-replace s old new)`                                               | 字符串切分（→ 列表）/ 替换                                                                      |
| `(getenv name)` `(to-number s)`                                                            | 读环境变量 / 字符串转数字                                                                         |
| `(window-create x y w h [title])` 等 13 个 `window-*`                                        | **GUI 窗口**：建窗 / 画图元 / 文字 / 清屏 / 重绘 / 移动 / 标题 / 关闭 / 取键 / **鼠标点击 / 字符输入**（详见下文） |

值类型：数字（double）、布尔 `true`/`false`、空 `nil`、字符串、符号、链表、向量、函数/闭包。

`nil` 与 `false` 视为假，其余为真（含数字 0 与空字符串）。

> ⚠️ **链表 vs 向量是两套操作**：`car`/`cdr`/`cons`/`null?` 只作用于链表；`get`/`set` 只作用于向量。`(car [1 2])` 不报错、静默返回 `nil`，别混用。
>
> 🎮 `cls`/`home`/`sleep`/`key`/`random`/`draw` 是 2026-10-21 为实时游戏加的原语（Windows 走 Win32 控制台 + conio，其他平台 ANSI 兜底、无键盘）。贪吃蛇示例见 `samples/snake.nuomi`。
>
> 📁 **文件系统**：糯米在 novus 上是**用户级应用语言**（团队定位：专注应用层、不当系统 shell，图形化窗口走 Novus-WM 协议），文件操作是基本功。宿主用 Win32 API、其他平台 POSIX，将来上 novus 换 syscall、**脚本接口不变**。软错误语义：失败返回 `nil`/`false`/`-1`，不中止程序。**故意不提供递归删除**——`rm -rf` 请用 `ls`+`delete`+`rmdir` 在脚本层自己组合（`samples/files.nuomi` 有 6 行实现）。这是真实破坏力，删了不可恢复。
>
> 💡 **游戏防闪烁三件套**（`samples/snake.nuomi` 实测有效）：每帧**不要** `cls` 清屏（抹全屏就是闪瞎眼的根因）——用 `home` 只把光标移回左上角，所有行等宽重绘自然覆盖旧画面；再把整帧拼成**一个**字符串用 `draw` 一次写出，控制台整帧呈现，不会看到画到一半的半成品。

### 🪟 窗口 / GUI（Novus-WM）

Nuomi 内置 13 个窗口原语，**Windows 上是真 Win32 窗口**（CreateWindow + GDI 双缓冲 + UTF-8 中文渲染），将来 novus 上同一套 API 走 Novus-WM IPC——脚本不用改：

| 原语 | 作用 |
| --- | --- |
| `(window-create x y w h [title])` | 建窗 → 返回窗口号 |
| `(window-fill win x y w h color)` | 填充矩形（最常用） |
| `(window-shape win op x y w h color [radius])` | 图元：1 填充 / 2 边框 / 3 横线 / 4 竖线 / 5 圆 / 6 圆角填充 / 7 圆角描边 |
| `(window-text win x y scale text [color])` | 文字（scale 1/2；第 6 参颜色，默认白；**中文正常**） |
| `(window-clear win)` / `(window-invalidate win)` | 清显示列表（开帧）/ 帧尾重绘 |
| `(window-move win x y)` `(window-title win "s")` `(window-close win)` | 移动 / 改标题 / 关窗 |
| `(window-event)` | 非阻塞取键：方向键 `'up` `'down` `'left` `'right`、`'esc`、`'back`，字母返回 `"r"` 等小写字符串；无键 `nil` |
| `(window-click [win])` | **鼠标左键点击** → `x*100000+y`（无点击 `nil`） |
| `(window-char [win])` | **键盘字符**（含中文输入法）→ 单字符字符串 |
| `(window-trace 0/1)` | 开关宿主消息摘要 |

颜色是 `0xRRGGBB` 的**十进制**写法（Nuomi 不支持 `0x` 字面量），如亮绿 `65280`、紫 `6953882`。

**开箱即玩**：`nuomi samples/snake-wm.nuomi` —— 彩色窗口贪吃蛇（浅蓝棋盘底 / 紫信息条 / 彩虹蛇身，方向键/WASD，R 重开，Esc 退出）。窗口原语示例见 `samples/wm-demo.nuomi`。

> 🐍 **旗舰应用**：[**NuomiLauncher**](https://github.com/Ryan-cpu-lang/NuomiLauncher) —— 用糯米语言写的 Minecraft 启动器（GUI + CLI，离线 Piston API），就是靠上面这批原语（窗口 / SHA-1 / unzip / 进程）搭出来的。

> 实现：Windows 走 `src/wm.c` 的 Win32 后端（真窗口）；novus 真机 IPC 路径待接（`include/novus_wm.h` 已备好协议常量与载荷结构）。**2026-11-05 从启动器分叉回流两处修复**：① 字符串字面量补齐 `\r` 转义（此前 `"\r"` 会退化成字面 `r`，导致 `str-replace s "\r" ""` 把所有字母 r 删掉）；② 标题栏改 `CreateWindowExW` / `SetWindowTextW` + `utf8_to_wcs`，中文标题不再乱码。窗口主体文字一直是 `TextOutW`，消息泵仍是 `PeekMessageA`。

### 📋 JSON 与「糯米写启动器」路线图

`json-parse` / `json-get` / `json-stringify` / `json-obj`。对象 → alist（`(键 . 值)` 链节），数组 → 向量。

```lisp
;; 多层路径取值，启动器里最常用的一招
(json-get (json-parse 文本) "downloads" "client" "sha1")
```

启动器全靠 JSON 吃饭（Mojang 版本清单、Modrinth API、实例配置）。示例见 `samples/json.nuomi`，回归测试见 `tests/json-test.nuomi`。

⚠️ 两个已知取舍：对象取值是 **O(n) 线性扫描**（还没做字典类型，将来版本索引几千项会慢）；`{}` 解析成 `nil`，序列化回去是 `null`（空对象与空值不可区分）。

**通往启动器的路线图** —— 糯米写 MC 启动器还缺什么，按依赖顺序：

| 能力                          | 状态        |
| --------------------------- | --------- |
| 文件操作（下载落盘、管理版本目录）           | ✅ 12 个原语  |
| JSON（读懂版本清单 / Modrinth API） | ✅         |
| **HTTP 请求**（拉清单、下载文件）       | ✅ http-get 等 5 原语 |
| **进程启动**（spawn java、传参、抓日志） | ✅ proc-* 8 原语   |
| ZIP（解压 natives / 资源）       | ✅ unzip 原语（启动器已解压 natives/库）｜mods 待做 |
| SHA-1（文件完整性校验）              | ✅ sha1-file（启动器资源校验已用） |
| 并发下载                        | ✅ 改用系统 `curl --parallel`（并发 24）；Nuomi 自带 `http-download-many` 因 WinINet 全局锁实测慢 4 倍，弃用 |
| **GUI 窗口**（建窗/画图元/文字/事件）    | ✅ Windows 真窗口（贪吃蛇可玩）｜⬜ novus 真机 IPC |

> 🎉 以上带 ✅ 的原语已全部由 [NuomiLauncher](https://github.com/Ryan-cpu-lang/NuomiLauncher) 实证可用 —— GUI + CLI 离线 Piston API 真拉起 Minecraft 1.21.4（资源并发下载 + SHA-1 全量校验零失败）。

> **递归深度**：约 3700+ 层函数递归安全可用（早期版本约 500 层即栈溢出）；超出会输出可读报错 `maximum recursion depth exceeded` 并终止，不会崩溃。

完整示例见 `samples/stdlib.nuomi`（标准库）与 `samples/scope.nuomi`（作用域/闭包）。

## 4. NUC 工具链（`nuomi nuc`）

NUC 是 novus-os 的原生可执行格式（魔数 `NUC!`，x86_64）。NUC 工具链已并入 `nuomi.exe`，通过 `nuomi nuc` 子命令调用（不再有独立的 `nucutil` 可执行文件）。布局铁律：

```
Header(48) → SegmentTable → SymbolTable → RelocationTable → StringTable → 段数据(4KiB 页对齐)
```

| 表          | 单条大小  |
| ---------- | ----- |
| Header     | 48 字节 |
| Segment    | 32 字节 |
| Symbol     | 24 字节 |
| Relocation | 16 字节 |

入口地址：`entry_rip = text.vaddr + entry_point`（`entry_point` 是 `.text` 段内偏移）。

命令：

```bash
# 生成一个带 PC32 重定位、可在真机跑通的最小示例 .nuc
nuomi nuc emit-demo demo.nuc

# 校验（按 §10 ABI 规则：magic / abi 主版本=1 / 64 位 / 段权限 W^X 等）
nuomi nuc validate demo.nuc

# 人类可读 dump（头/段表/符号表/重定位表/entry_rip）
nuomi nuc dump demo.nuc

# （仅 Windows x86_64）把 TEXT 段映射到 RWX 并执行入口，验证整条加载链路
nuomi nuc run demo.nuc        # 应输出：entry returned: 42 (0x2A)
```


```

`emit-demo` 产出的 `demo.nuc`：`.text` 里 `call main` 用 `R_X86_64_PC32` 重定位，加载器解析后跳到 `main`（`mov eax,0x2A; ret`），所以执行返回 42——这同时验证了 **格式生成 → 重定位 → 真机执行** 全链路。

## 5. Route A 编译后端（已落地）：Nuomi → .nuc → novus-os

已选定**路线 A（原生代码生成）**并落地：`nuomi -c 程序.nuomi -o 程序.nuc` 把 Nuomi 子集直编成 x86_64 机器码塞进 `.text`（`src/codegen.c`），产物与 liam 官方 `DEMO.NUC` 逐字段对齐（`NUC!` / abi 1.0 / TEXT@0x400000 / entry_rip 一致，互操作实证）。

- **已支持**：整数 / 函数(递归) / 局部变量 / if·while / 比较 / let·let*·letrec / 字符串字面量 / **print·println**（syscall `write`；号表与入口按官方《Novus-WM 与 IPC 协议规范 V1.0》扶正——`syscall` 指令、第 4 参 `r10`、write=2 / exit=60）
- **待接 novus 真机**：文件 I/O（open/read/... 号已有，待内核确认实现）、delete/mv（缺 unlink/rename syscall）、窗口（IPC 路径）
- 验证：`nuomi nuc dump`（格式）/ `nuomi nuc run`（宿主真机执行）；`(print "hi")` 产物已端到端验证符合官方 ABI

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

### 7.3 ✅ 规范矛盾已解决（2026-10-24 实锤）

- 此前 novus-os **README** 写"内核内置 Nuc 字节码执行引擎"，与 **NUC 规范 V1.0**（`.text` = x86_64 原生机器码）矛盾。
- 解析官方 `novus-live_v0.1.56` ISO 里 liam 产的 `DEMO.NUC`：`.text` 是标准 x86_64 指令流（`call`/`syscall`/`ret`，write+exit 序列），重定位 `R_X86_64_PC32/32`，完全符合**规范 V1.0 原生码路线**。
- **结论：规范 V1.0 赢，Route A（原生码）方向正确**；novus README 的"字节码引擎"说法过时。Nuomi 的 `nucutil` 已能无缝解析官方 `DEMO.NUC`（互操作实证）。

### 7.4 当前进度（2026-10 更新）

- 解释器：全功能（窗口 GUI / HTTP / 进程 / JSON / 文件 / 实时游戏原语），窗口贪吃蛇实测稳跑 2 小时+ 无崩溃。
- NUC 工具链：`emit-demo → validate → dump → run` 全通，`run` 返回 42 (0x2A)。
- Route A 后端：函数/控制流/字符串/print(syscall write) 已编通，产物与官方 DEMO.NUC 格式对齐。
- 待做：文件 I/O 与窗口的 novus 真机 IPC 路径；编译子集继续追平解释器。
