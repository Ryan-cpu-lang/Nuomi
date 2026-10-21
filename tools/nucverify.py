"""
端到端验证编译产物：模拟执行 .nuc 的 TEXT 段（含 syscall 拦截），
把 write 的内容打印出来 —— 用来验证「编译 → syscall 发射 → 号表 →
字符串布局 → itoa」整条链路。

用法： python nucverify.py <file.nuc>
"""
import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from x86sim import Sim

TEXT_VADDR = 0x400000
DATA_VADDR = 0x500000
BSS_VADDR  = 0x600000
HDR_SIZE   = 48          # NUC 头部固定 48 字节
SEG_ENTRY  = 32          # 段表项 32 字节
SYM_ENTRY  = 24          # 符号表项 24 字节
REL_ENTRY  = 16          # 重定位项 16 字节

NOVUS_SYS_WRITE = 1


def main():
    path = sys.argv[1]
    d = open(path, 'rb').read()
    assert d[:4] == b'NUC!', "不是 NUC 文件"

    entry   = struct.unpack_from('<I', d, 8)[0]
    segc    = struct.unpack_from('<H', d, 12)[0]
    symc    = struct.unpack_from('<H', d, 14)[0]
    stroff  = struct.unpack_from('<I', d, 16)[0]
    reloff  = struct.unpack_from('<I', d, 20)[0]
    relc    = struct.unpack_from('<H', d, 28)[0]

    # ---- 段表 ----
    segs = []
    for i in range(segc):
        o = HDR_SIZE + i * SEG_ENTRY
        t, foff = struct.unpack_from('<II', d, o)
        vaddr, size = struct.unpack_from('<QQ', d, o + 8)
        flags, = struct.unpack_from('<I', d, o + 24)
        segs.append({'type': t, 'vaddr': vaddr, 'size': size,
                     'file_off': foff, 'flags': flags})
        print("段 %d: type=%d vaddr=0x%x size=%d file_off=0x%x flags=0x%x"
              % (i, t, vaddr, size, foff, flags))

    text = next(s for s in segs if s['type'] == 0)
    rodata = next((s for s in segs if s['type'] == 3), None)
    bss = next((s for s in segs if s['type'] == 2), None)

    # ---- 符号表 ----
    symtab_off = HDR_SIZE + segc * SEG_ENTRY
    symtab = []
    for i in range(symc):
        o = symtab_off + i * SYM_ENTRY
        name_off, = struct.unpack_from('<I', d, o)
        value, size = struct.unpack_from('<QQ', d, o + 8)
        symtab.append((name_off, value, size))
        end = d.index(b'\x00', stroff + name_off)
        nm = d[stroff + name_off:end].decode('latin1')
        print("符号 %d: %-28s value=0x%x size=0x%x" % (i, nm, value, size))

    # ---- 载入 TEXT 并应用重定位 ----
    tcode = bytearray(d[text['file_off']:text['file_off'] + text['size']])
    for i in range(relc):
        o = reloff + i * REL_ENTRY
        r_off, = struct.unpack_from('<Q', d, o)
        r_type, sym_idx = struct.unpack_from('<II', d, o + 8)
        site = r_off - text['vaddr']          # 调用点在 TEXT 段内的偏移
        name_off, value, size = symtab[sym_idx]
        if r_type == 2:                        # R_X86_64_PC32 (值是 2，不是 4)
            # 符号 value = 符号所在段的段内偏移；符号都在 TEXT 段
            rel = value - site - 4
            struct.pack_into('<i', tcode, site, rel)
            print("重定位: call@段内0x%X → 符号段内0x%X (rel=%d)" % (site, value, rel))

    # ---- 建模拟内存（按真实虚拟地址摆放各段）----
    MEM = 0x1000000
    # mem_as_code=True：指令按虚拟地址取指，TEXT 段落在 TEXT_VADDR 处
    sim = Sim(b'', memsize=MEM, mem_as_code=True)
    sim.mem[TEXT_VADDR:TEXT_VADDR + len(tcode)] = tcode
    if rodata:
        ro = d[rodata['file_off']:rodata['file_off'] + rodata['size']]
        sim.mem[DATA_VADDR:DATA_VADDR + len(ro)] = ro
    if bss:
        sim.mem[BSS_VADDR:BSS_VADDR + bss['size']] = b'\x00' * bss['size']

    sim.rip = TEXT_VADDR + entry
    try:
        r = sim.run({}, maxsteps=2000000)
        print("\n入口返回 rax = 0x%x" % (r[0] & ((1 << 64) - 1)))
    except Exception as e:
        print("\n执行异常: %s (rip=0x%x)" % (e, sim.rip))

    print("\n=== syscall 拦截记录 (号, fd, buf, len) ===")
    print("  (注：write 的内容在拦截瞬间快照 —— 真实内核立刻复制，"
          "而 itoa 复用同一 BSS 缓冲区，事后读会只剩最后一次)")
    out = bytearray()
    for idx, (nr, fd, buf, ln) in enumerate(sim.syscalls):
        print("  号=%d fd=%d buf=0x%x len=%d" % (nr, fd, buf, ln))
        if nr == NOVUS_SYS_WRITE:
            out += sim.snapshots[idx]
    print("\n=== 写出的内容 ===")
    sys.stdout.write(out.decode('utf-8', 'replace'))
    print()
    return 0


if __name__ == '__main__':
    sys.exit(main())
