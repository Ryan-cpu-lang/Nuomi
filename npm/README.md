# nuomi-ryan

> Nuomi 语言解释器（Windows 命令行即用）。装完敲 `nuomi` 就能用。

Nuomi 是一个极简、类 Lisp 的脚本语言，自带 REPL 与游戏 / 绘图原语，适合写小工具与终端小游戏。

## 安装（需先装 Node.js ≥ 14）

```bash
npm install -g nuomi-ryan
```

装完直接敲：

```bash
nuomi            # 进入交互式 REPL
nuomi game.nuomi # 运行一个 .nuomi 脚本
nuomi version    # 查看当前版本
nuomi update     # 一键升级到 GitHub 最新版
```

也可以**不安装、临时跑**（适合想先试一试）：

```bash
npx nuomi-ryan
```

## 平台支持

仅支持 **Windows x64**。Nuomi 解释器的原生层使用 Win32 控制台 API，macOS / Linux 暂不支持（非 Windows 上 `npm install` 会被平台限制拒绝）。

## 自更新

`nuomi update` 会查询 GitHub Releases 上的最新 `nuomi.exe`，下载后用「重命名备份 + 改名替换」的方式就地替换正在运行的这份 exe（Windows 下不能直接删除运行中的 exe，但重命名可以），失败自动回滚。

## 完整文档

语言手册、示例与更多用法见 [GitHub 仓库](https://github.com/Ryan-cpu-lang/Nuomi)。
