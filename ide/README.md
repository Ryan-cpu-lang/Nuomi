# 🍡 Nuomi IDE — 糯米语言集成开发环境

PyCharm 风格的糯米语言（Nuomi）桌面 IDE：**双击启动、零安装、零成本**。
底层是一个本地 Web 服务（Node，纯内置模块，无需 `npm install`）+ 浏览器内核界面。

## ✨ 功能
- **语法高亮编辑器**：括号配色、关键字/内置函数/字符串/数字/注释分类着色，行号、缩进辅助。
- **资源管理器**：左侧文件树，直接打开/新建/保存工作区里的 `.nuomi` 文件。
- **运行**（解释器）：`nuomi.exe 文件.nuomi` 的实时输出。
- **编译运行**：`nuomi -c` 编译为 `.nuc`，再用 `nucutil run` 在真机入口跑，显示入口返回值。
- **交互式 REPL**：持久会话（定义的变量/函数跨行保留），与命令行的 `nuomi` REPL 一致。
- **深色 / 浅色主题**切换（记忆选择）。
- 自带 `welcome.nuomi` 上手引导 + 全部示例程序。

## 🚀 使用
1. 双击 **`ide/launch.bat`**。
2. 首次会编译糯米工具链（若 `nuomi.exe` 不存在），然后自动打开浏览器到 `http://localhost:7177`。
3. 左边选文件 → 中间写代码 → 下方「▶ 运行」或切到「REPL」标签交互。

> 关闭 IDE：直接关掉启动器那个黑窗口（服务进程一并结束）。

## ⌨️ 快捷键
- `Ctrl/Cmd + S`：保存
- `F5`：运行
- `Tab`：插入两个空格缩进
- REPL 输入框回车：求值当前行

## ⚙️ 配置（`ide.config.json`）
| 字段 | 含义 | 默认 |
|------|------|------|
| `port` | 本地服务端口 | `7177` |
| `nuomi_home` | 工具链目录（含 `nuomi.exe`/`nucutil.exe`），相对 `ide/` | `..` |
| `workspace` | 工作区目录（用户文件存放处），相对 `ide/` | `workspace` |

## 🗂️ 目录
```
ide/
├── launch.bat        # 双击启动
├── server.js         # 零依赖 Node 后端（文件/运行/REPL API）
├── ide.config.json   # 配置
├── public/           # 前端界面
│   ├── index.html
│   ├── styles.css
│   └── app.js
└── workspace/        # 用户工作区（首次自动预载示例）
```

## 🔧 手动启动（可选）
```bat
:: 用任意 node 启动
node ide/server.js
:: 或指定端口
set PORT=8080 && node ide/server.js
```

## 📌 说明
- 所有文件读写都限制在 `workspace/` 内（路径穿越会被拒绝），安全。
- REPL 是长驻进程；「↻ REPL」按钮会重启它。
- 与 `novus-os` 的 NUC 工具链天然对接：编辑器里「编译运行」跑的就是 `dist/novus/*.nuc` 同款产物。
