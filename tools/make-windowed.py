#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
# tools/make-windowed.py -- 从 console 版 nuomi.exe 生成 GUI 版 nuomiw.exe
#
# 做什么：
#   完整复制一份 nuomi.exe，只把 PE 可选头(Optional Header)里的
#   Subsystem 字段由 3 (IMAGE_SUBSYSTEM_WINDOWS_CUI, 控制台) 改成
#   2 (IMAGE_SUBSYSTEM_WINDOWS_GUI, 窗口)，并把 CheckSum 清零。
#   代码、入口点(AddressOfEntryPoint)、图标资源、导入表全部原样不动。
#
# 为什么用「改 PE 头」而不是「加 -mwindows 重新链接」：
#   1) 链接器看到 GUI 子系统会改选 WinMainCRTStartup 作为入口，而 Nuomi
#      用的是标准 main()，会直接链接失败 / 启动异常。改 PE 头不动入口点。
#   2) build.bat 支持 clang / gcc / TCC 三种编译器，各家切子系统的参数
#      (-mwindows / -Wl,--subsystem,windows / -Wl,-subsystem,windows) 不同，
#      改 PE 头与编译器完全无关，一个脚本通吃。
#   3) 保证 nuomi.exe 与 nuomiw.exe 是同一份二进制逻辑，不会出现两份代码漂移。
#
# 用途对照（Python 的 python.exe / pythonw.exe 同款套路）：
#   nuomi.exe   CONSOLE 版 —— REPL、编译、CLI 模式(start.bat dry/download/launch)
#               需要看 say/println 日志时用，保留控制台。
#   nuomiw.exe  GUI 版     —— 启动器 gui.nuomi 双击启动时不再弹 cmd 黑框；
#               配合 proc.c 的 CREATE_NO_WINDOW，curl/java/powershell 子进程
#               也全部静默。GUI 版无控制台时 main.c 会把标准流接到 NUL。
#
# 用法：
#   python tools/make-windowed.py [src.exe] [dst.exe]
#   默认 src=nuomi.exe, dst=nuomiw.exe
import os
import struct
import sys

SUBSYSTEM_WINDOWS_GUI = 2
SUBSYSTEM_WINDOWS_CUI = 3
OPT_MAGIC_PE32 = 0x10B
OPT_MAGIC_PE32PLUS = 0x20B
# Subsystem / CheckSum 在 Optional Header 内的偏移，PE32 与 PE32+ 相同
OFF_SUBSYSTEM = 0x44
OFF_CHECKSUM = 0x40


def fail(msg):
    print("make-windowed: ERROR: " + msg, file=sys.stderr)
    sys.exit(1)


def patch(src, dst):
    if not os.path.isfile(src):
        fail("源文件不存在: " + src)
    with open(src, "rb") as f:
        data = bytearray(f.read())

    if len(data) < 0x100 or data[0:2] != b"MZ":
        fail("不是有效的 PE 文件(缺少 MZ 头): " + src)

    (e_lfanew,) = struct.unpack_from("<I", data, 0x3C)
    if e_lfanew + 24 > len(data) or data[e_lfanew:e_lfanew + 4] != b"PE\x00\x00":
        fail("PE 签名无效，e_lfanew=0x%X" % e_lfanew)

    coff = e_lfanew + 4
    opt = coff + 20                      # IMAGE_FILE_HEADER 固定 20 字节
    if opt + OFF_SUBSYSTEM + 2 > len(data):
        fail("Optional Header 被截断")

    (magic,) = struct.unpack_from("<H", data, opt)
    if magic == OPT_MAGIC_PE32:
        bits = "PE32"
    elif magic == OPT_MAGIC_PE32PLUS:
        bits = "PE32+"
    else:
        fail("未知的 Optional Header Magic=0x%04X" % magic)

    (subsystem,) = struct.unpack_from("<H", data, opt + OFF_SUBSYSTEM)
    if subsystem != SUBSYSTEM_WINDOWS_CUI:
        fail("期望源子系统为 CUI(3)，实际为 %d。请传入 console 版 nuomi.exe，"
             "不要对已处理过的 nuomiw.exe 重复运行。" % subsystem)

    # 只改两处：Subsystem 3 -> 2；CheckSum 清零(改过头就不再有效，0 表示不校验，
    # 普通 EXE 加载器本来也不校验 CheckSum)。
    struct.pack_into("<H", data, opt + OFF_SUBSYSTEM, SUBSYSTEM_WINDOWS_GUI)
    struct.pack_into("<I", data, opt + OFF_CHECKSUM, 0)

    with open(dst, "wb") as f:
        f.write(data)

    print("make-windowed: %s -> %s  [%s, Subsystem 3(CUI) -> 2(GUI), CheckSum=0]"
          % (src, dst, bits))


if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else "nuomi.exe"
    dst = sys.argv[2] if len(sys.argv) > 2 else "nuomiw.exe"
    patch(src, dst)
