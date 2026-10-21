"""
x86-64 子集模拟器 —— 用于在把机器码嵌入编译器之前验证其语义正确性。

比真实执行更好用：能精确指出是哪条指令出错，且不依赖宿主 ABI。
栈语义严格按 x86-64 实现（push/pop/call/ret/leave 走真实内存栈），
因此可以执行含函数调用、递归的完整代码。

已实现的指令子集：
  55             push rbp
  50/58/5A/5F…   push/pop r64
  48 89 /r       mov r/m64, r64      （含 mod=01 [rbp+disp8]）
  48 8B /r       mov r64, r/m64
  48 83 /r ib    add/sub/cmp r/m64, imm8
  48 81 /r id    add/sub/cmp r/m64, imm32
  48 C7 /r id    mov r/m64, imm32
  48 B8+r imm64  mov r64, imm64
  48 F7 /3       neg r64
  48 85 /r       test r64, r64
  48 99          cqo
  48 2B /r       sub r64, r/m64
  48 8D 05       lea rax, [rip+disp32]
  49 89 /r       mov r/m64, r64（REX.B 扩 rm → r8..r15）
  49 8B /r       mov r64, r/m64（REX.B 扩 rm）
  49 C7 /0 id    mov r/m64, imm32（REX.B 扩 rm）
  49 F7 /7       idiv r/m64（REX.B 扩 rm）
  31 /r          xor r/m32, r32
  88 /r          mov r/m8, r8
  C6 /0 ib       mov byte [rax], imm8
  E8 cd          call rel32
  C9             leave
  C3             ret
  CD 80          int 0x80（syscall 拦截点）
  CC             int3（宿主补丁后的 syscall 点）
  7D/74/75/EB   jge/je/jne/jmp rel8
"""
import sys


class Sim:
    def __init__(self, code, memsize=1 << 23, mem_as_code=False):
        self.code = code
        # mem_as_code=True 时：指令从 self.mem[rip] 取，rip 是「虚拟地址」，
        # 用于模拟按真实虚拟地址布局的 .nuc 段（TEXT/RODATA/BSS 各自在自己的 vaddr）。
        self.mem_as_code = mem_as_code
        self.regs = {i: 0 for i in range(16)}
        self.mem = bytearray(memsize)
        self.regs[4] = 0x2000      # rsp
        self.regs[5] = 0x1000      # rbp
        self.rip = 0
        self.flags_cmp = 0
        self.flags_zero = True
        self.syscalls = []
        self.snapshots = []

    # ---- 内存与栈 ----
    # x86-64 寄存器编号：rax=0 rcx=1 rdx=2 rbx=3 rsp=4 rbp=5 rsi=6 rdi=7
    # rbp 相对寻址必须用**真正的 rbp 寄存器**（r[5]），栈指针用 r[4]；
    # 之前把两者当成独立属性，导致 [rbp-8] 的读写位置与 push/pop 的位置错位。
    def rb(self, o):
        base = self.regs[5]
        u = int.from_bytes(self.mem[base + o:base + o + 8], 'little')
        return u - (1 << 64) if u >= (1 << 63) else u

    def wb(self, o, v):
        base = self.regs[5]
        self.mem[base + o:base + o + 8] = (int(v) & ((1 << 64) - 1)).to_bytes(8, 'little')

    def push(self, v):
        self.regs[4] -= 8
        base = self.regs[4]
        self.mem[base:base + 8] = (int(v) & ((1 << 64) - 1)).to_bytes(8, 'little')

    def pop(self):
        base = self.regs[4]
        v = int.from_bytes(self.mem[base:base + 8], 'little')
        self.regs[4] += 8
        return v

    def b(self, i):
        """取 1 字节指令：mem_as_code 模式下从内存取，否则从 code 取"""
        return self.mem[i] if self.mem_as_code else self.code[i]

    def bs(self, a, bnd):
        """取 [a, bnd) 字节序列"""
        return self.mem[a:bnd] if self.mem_as_code else self.code[a:bnd]

    def imm8(self, i):
        b = self.b(i)
        return b - 256 if b > 127 else b

    def run(self, regs_init=None, maxsteps=4000000):
        if regs_init:
            self.regs.update(regs_init)
        r = self.regs
        code = self.code
        rip = self.rip
        for _ in range(maxsteps):
            op = self.mem[rip] if self.mem_as_code else code[rip]
            if op == 0x55:                       # push rbp（存的是 rbp 本身，rbp=rsp 由下一条 mov 完成）
                self.push(r[5]); rip += 1
            elif op == 0x50:                     # push rax
                self.push(r[0]); rip += 1
            elif op == 0x58:                     # pop rax
                r[0] = self.pop(); rip += 1
            elif op == 0x5A:                     # pop rdx
                r[2] = self.pop(); rip += 1
            elif op == 0x5F:                     # pop rdi
                r[7] = self.pop(); rip += 1
            elif op == 0x5E:                     # pop rsi
                r[6] = self.pop(); rip += 1
            elif op == 0x59:                     # pop rcx
                r[1] = self.pop(); rip += 1
            elif op == 0x41 and self.b(rip+1) == 0x58:   # pop r8
                r[8] = self.pop(); rip += 2
            elif op == 0x41 and self.b(rip+1) == 0x59:   # pop r9
                r[9] = self.pop(); rip += 2
            elif op == 0x48:
                o2 = self.b(rip+1)
                if o2 == 0x89:                   # mov r/m64, r64
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 3:
                        r[rm] = r[reg]; rip += 3
                    elif rm == 5 and mod == 1:
                        d = self.imm8(rip+3); self.wb(d, r[reg]); rip += 4
                    else: raise Exception("89@%d mod=%d rm=%d" % (rip, mod, rm))
                elif o2 == 0x8B:                 # mov r64, r/m64
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 1 and rm == 5:
                        d = self.imm8(rip+3); r[reg] = self.rb(d); rip += 4
                    elif mod == 3:
                        r[reg] = r[rm]; rip += 3
                    else: raise Exception("8B@%d mod=%d" % (rip, mod))
                elif o2 == 0x83:                 # add/sub/cmp r/m64, imm8
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    imm = self.b(rip+3)
                    if mod == 1 and rm == 5:
                        d = self.imm8(rip+3); imm = self.b(rip+4)
                        cur = self.rb(d)
                        if reg == 0: self.wb(d, cur+imm)
                        elif reg == 5: self.wb(d, cur-imm)
                        elif reg == 7: self.flags_cmp = cur-imm; self.flags_zero = (cur == imm)
                        else: raise Exception("83 op%d@%d" % (reg, rip))
                        rip += 5
                    elif mod == 3:
                        if reg == 0: r[rm] += imm
                        elif reg == 5: r[rm] -= imm
                        elif reg == 7: self.flags_cmp = r[rm]-imm; self.flags_zero = (r[rm] == imm)
                        else: raise Exception("83 op%d@%d" % (reg, rip))
                        rip += 4
                    else: raise Exception("83@%d mod=%d" % (rip, mod))
                elif o2 == 0x81:                 # add/sub/cmp r/m64, imm32
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    imm = int.from_bytes(self.bs(rip+3, rip+7), 'little', signed=True)
                    if mod == 3:
                        if reg == 0: r[rm] += imm
                        elif reg == 5: r[rm] -= imm
                        elif reg == 7: self.flags_cmp = r[rm]-imm; self.flags_zero = (r[rm] == imm)
                        rip += 7
                    else: raise Exception("81@%d" % rip)
                elif o2 == 0xC7:                 # mov r/m64, imm32
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 1 and rm == 5:
                        d = self.imm8(rip+3)
                        imm = int.from_bytes(self.bs(rip+4, rip+8), 'little', signed=True)
                        self.wb(d, imm); rip += 8
                    elif mod == 3:
                        imm = int.from_bytes(self.bs(rip+3, rip+7), 'little', signed=True)
                        r[rm] = imm & ((1 << 64) - 1)
                        if imm >= (1 << 63): r[rm] -= (1 << 64)
                        rip += 7
                    else: raise Exception("C7@%d" % rip)
                elif o2 == 0xF7:                 # neg r64
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7
                    if reg == 3: r[rm] = (-r[rm]) & ((1 << 64) - 1)
                    else: raise Exception("F7 op%d@%d" % (reg, rip))
                    rip += 3
                elif o2 == 0x85:                 # test r64, r64
                    m = self.b(rip+2); r1=(m>>3)&7; r2=m&7
                    self.flags_zero = (r[r1] & r[r2]) == 0
                    rip += 3
                elif o2 == 0x8D and self.b(rip+2) == 0x05:   # lea rax,[rip+disp32]
                    disp = int.from_bytes(self.bs(rip+3, rip+7), 'little', signed=True)
                    r[0] = (rip + 7 + disp) & ((1 << 64) - 1); rip += 7
                elif 0xB8 <= o2 <= 0xBF:          # mov r64, imm64
                    reg = o2 - 0xB8
                    r[reg] = int.from_bytes(self.bs(rip+2, rip+10), 'little')
                    rip += 10
                elif o2 == 0x99: rip += 2          # cqo
                elif o2 == 0x0F:                 # 0F xx 两字节组（imul 等）
                    o3 = self.b(rip+2)
                    if 0x90 <= o3 <= 0x9F:         # setcc r8 (0F 90..9F)
                        # setcc 只写 al（低8位），高 64 位需清零
                        m = self.b(rip+3); rm = m & 7
                        cf = self.flags_cmp
                        zf = self.flags_zero
                        # 有符号比较：flags_cmp <0 表示「小于」
                        val = 0
                        if o3 == 0x94: val = 1 if zf else 0                 # sete
                        elif o3 == 0x9C: val = 1 if cf < 0 else 0             # setl
                        elif o3 == 0x9D: val = 1 if cf >= 0 else 0            # setge
                        elif o3 == 0x9E: val = 1 if cf <= 0 else 0            # setle
                        elif o3 == 0x9F: val = 1 if cf > 0 else 0             # setg
                        else: val = 0
                        r[rm] = val & 0xFF
                        rip += 4
                    elif o3 == 0xAF:                 # imul r64, r/m64
                        m = self.b(rip+3); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                        if mod == 3:
                            r[reg] = (r[reg] * r[rm]) & ((1<<64)-1)
                            rip += 4
                        else: raise Exception("0F AF@%d mod=%d" % (rip, mod))
                    else: raise Exception("0F %02x@%d" % (o3, rip))
                elif o2 == 0x01:                 # add r64, r/m64
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 3:
                        r[rm] = (r[rm] + r[reg]) & ((1<<64)-1)   # rm=目标, reg=源
                        rip += 3
                    elif mod == 1 and rm == 5:
                        d = self.imm8(rip+3)
                        r[rm] = (r[rm] + self.rb(d)) & ((1<<64)-1)
                        rip += 4
                    else: raise Exception("01@%d mod=%d" % (rip, mod))
                elif o2 == 0x29:                 # sub r64, r/m64 (opcode 0x29)
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 3:
                        r[rm] = (r[rm] - r[reg]) & ((1<<64)-1)   # rm=目标, reg=源
                        rip += 3
                    else: raise Exception("29@%d" % rip)
                elif o2 == 0x39:                 # cmp r64, r/m64
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 3:
                        self.flags_cmp = r[reg] - r[rm]; self.flags_zero = (r[reg] == r[rm])
                        rip += 3
                    elif mod == 1 and rm == 5:
                        d = self.imm8(rip+3)
                        self.flags_cmp = r[reg] - self.rb(d); self.flags_zero = (r[reg] == self.rb(d))
                        rip += 4
                    else: raise Exception("39@%d mod=%d" % (rip, mod))
                elif o2 == 0x2B:                 # sub r64, r/m64
                    m = self.b(rip+2); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 1 and rm == 5:
                        d = self.imm8(rip+3); r[reg] -= self.rb(d); rip += 4
                    else: raise Exception("2B@%d" % rip)
                else: raise Exception("REX48 op%02x@%d" % (o2, rip))
            elif op == 0x49:                       # REX.WB
                o2 = self.b(rip+1)
                if o2 == 0x89:                     # mov r/m64, r64（REX.B 扩 rm）
                    m = self.b(rip+2); reg=(m>>3)&7; rm=(m&7)+8; mod=(m>>6)&3
                    if mod == 3: r[rm] = r[reg]; rip += 3
                    else: raise Exception("49 89@%d mod=%d" % (rip, mod))
                elif o2 == 0x8B:                   # mov r64, r/m64（REX.B 扩 rm）
                    m = self.b(rip+2); reg=(m>>3)&7; rm=(m&7)+8; mod=(m>>6)&3
                    if mod == 1:
                        # rm 是寄存器（r8..r15）+ disp8 → 内存地址 = rm + disp
                        d = self.imm8(rip+3)
                        addr = (r[rm] + d) & ((1 << 64) - 1)
                        r[reg] = int.from_bytes(self.mem[addr:addr+8], 'little')
                        if r[reg] >= (1 << 63): r[reg] -= (1 << 64)
                        rip += 4
                    elif mod == 3: r[reg] = r[rm]; rip += 3
                    else: raise Exception("49 8B@%d mod=%d" % (rip, mod))
                elif o2 == 0xC7:                   # mov r/m64, imm32（REX.B 扩 rm）
                    m = self.b(rip+2); rm=(m&7)+8; mod=(m>>6)&3
                    imm = int.from_bytes(self.bs(rip+3, rip+7), 'little', signed=True)
                    if mod == 3:
                        r[rm] = imm & ((1 << 64) - 1)
                        if imm >= (1 << 63): r[rm] -= (1 << 64)
                        rip += 7
                    else: raise Exception("49 C7@%d" % rip)
                elif o2 == 0xF7:                   # idiv r/m64（REX.B 扩 rm）
                    m = self.b(rip+2); reg=(m>>3)&7; rm=(m&7)+8
                    dv = r[0]; ds = r[rm]
                    if ds == 0: raise Exception("div0")
                    q = abs(dv) // abs(ds)
                    if (dv < 0) != (ds < 0): q = -q
                    rem = dv - q*ds
                    r[0] = q & ((1 << 64) - 1); r[2] = rem & ((1 << 64) - 1)
                    rip += 3
                else: raise Exception("REX49 op%02x@%d" % (o2, rip))
            elif op == 0x0F:                       # 0F xx 两字节组（无 REX 前缀）
                o3 = self.b(rip+2)
                if 0x90 <= o3 <= 0x9F:             # setcc r8 (0F 90..9F)
                    m = self.b(rip+3); rm = m & 7
                    cf = self.flags_cmp; zf = self.flags_zero
                    val = 0
                    if o3 == 0x94: val = 1 if zf else 0            # sete
                    elif o3 == 0x95: val = 0 if zf else 1          # setne
                    elif o3 == 0x9C: val = 1 if cf < 0 else 0        # setl
                    elif o3 == 0x9D: val = 1 if cf >= 0 else 0       # setge
                    elif o3 == 0x9E: val = 1 if cf <= 0 else 0       # setle
                    elif o3 == 0x9F: val = 1 if cf > 0 else 0        # setg
                    r[rm] = val & 0xFF
                    rip += 4
                elif o3 == 0xAF:                     # imul r64, r/m64
                    m = self.b(rip+3); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                    if mod == 3:
                        r[reg] = (r[reg] * r[rm]) & ((1<<64)-1)
                        rip += 4
                    else: raise Exception("0F AF@%d mod=%d" % (rip, mod))
                else: raise Exception("0F %02x@%d" % (o3, rip))
            elif op == 0x31:                       # xor r/m32, r32
                m = self.b(rip+1); reg=(m>>3)&7; rm=m&7; mod=(m>>6)&3
                if mod == 3:
                    r[rm] = (r[rm] ^ r[reg]) & 0xFFFFFFFF; rip += 2
                else: raise Exception("31@%d" % rip)
            elif op == 0x88:                       # mov r/m8, r8
                m = self.b(rip+1); src=(m>>3)&7; rm=m&7
                self.mem[r[rm]] = r[src] & 0xFF; rip += 2
            elif op == 0xC6:                       # mov byte [rax], imm8
                self.mem[r[0]] = self.b(rip+2); rip += 3
            elif op == 0xE8:                       # call rel32
                rel = int.from_bytes(self.bs(rip+1, rip+5), 'little', signed=True)
                target = (rip + 5 + rel) & ((1 << 64) - 1)
                self.push(rip + 5)
                rip = target
            elif op == 0xC9:                       # leave: rsp = rbp; pop rbp
                r[4] = r[5]
                r[5] = self.pop()
                rip += 1
            elif op == 0xC3:                       # ret
                ret_addr = self.pop()
                if ret_addr == 0:                  # 栈底哨兵：主程序返回
                    return r
                rip = ret_addr
            elif op == 0xCD:                       # int 0x80 —— syscall 拦截点
                # 立刻快照参数内容（真实内核会复制走；缓冲区随后可能被复用）
                snap = bytes(self.mem[r[6]:r[6] + r[2]]) if r[2] else b''
                self.syscalls.append((r[0], r[7], r[6], r[2]))
                self.snapshots.append(snap)
                r[0] = 0
                rip += 2
            elif op == 0xCC:                       # int3（宿主补丁后的 syscall 点）
                snap = bytes(self.mem[r[6]:r[6] + r[2]]) if r[2] else b''
                self.syscalls.append((r[0], r[7], r[6], r[2]))
                self.snapshots.append(snap)
                r[0] = 0
                rip += 2
            elif op == 0x7D:                       # jge rel8
                rel = self.imm8(rip+1); rip += 2+rel if self.flags_cmp >= 0 else 2
            elif op == 0x75:                       # jne rel8
                rel = self.imm8(rip+1); rip += 2+rel if not self.flags_zero else 2
            elif op == 0x74:                       # je rel8
                rel = self.imm8(rip+1); rip += 2+rel if self.flags_zero else 2
            elif op == 0xEB:                       # jmp rel8
                rel = self.imm8(rip+1); rip += 2+rel
            else:
                raise Exception("op%02x@%d" % (op, rip))
        raise Exception("step limit")


if __name__ == '__main__':
    code = open(sys.argv[1], 'rb').read()
    BUF = 0x800
    fails = 0; total = 0
    vals = [0,1,9,10,99,100,-1,-9,-10,42,12345,-99999,
            2147483647,-2147483648,10**12,-10**12] + list(range(-1000,1001))
    for v in vals:
        total += 1
        s = Sim(code)
        s.mem[BUF:BUF+64] = b'\x00'*64
        try:
            r = s.run({7: v & ((1<<64)-1), 6: BUF})
            ptr, ln = r[0] & ((1<<64)-1), r[2] & ((1<<64)-1)
            got = bytes(s.mem[ptr:ptr+ln]).decode('latin1')
        except Exception as e:
            got = "ERR:%s" % e
        if got != str(v):
            fails += 1
            if fails <= 5:
                print("FAIL v=%d got=%r want=%r" % (v, got[:24], str(v)))
    print("模拟执行: 共测 %d 个值, 失败 %d" % (total, fails))
    sys.exit(1 if fails else 0)
