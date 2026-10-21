// SPDX-License-Identifier: GPL-2.0
/*
 * nucutil.c — NUC 文件工具
 *   dump <file.nuc>       人类可读 dump
 *   validate <file.nuc>   按 §10 ABI 规则校验
 *   emit-demo <out.nuc>    生成一个带 PC32 重定位、可在真机跑通的最小示例
 *   run <file.nuc>         （仅 Windows）映射 TEXT 段并执行入口，验证加载流程
 */
#if !defined(_WIN32)
#define _DEFAULT_SOURCE 1  /* 让 POSIX 头文件暴露 sysconf/_SC_PAGESIZE/MAP_ANONYMOUS（Linux run 用） */
#endif
#include "nucfmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t* b = (uint8_t*)malloc(sz ? (size_t)sz : 1);
    if (fread(b, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); free(b); return NULL; }
    fclose(f); *out_len = (size_t)sz; return b;
}
static int write_file(const char* path, const uint8_t* b, size_t n) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(b, 1, n, f);
    fclose(f);
    return 0;
}

static int emit_demo(const char* out_path) {
    NucBuilder* b = nuc_builder_new();
    nuc_builder_set_flags(b, NUC_FLAG_64BIT | NUC_FLAG_STATIC);
    nuc_builder_set_entry(b, 0);
    nuc_builder_set_stack(b, 0x800000);
    /* .text：offset0 call main(PC32 重定位)；offset5 mov eax,0x2A；offset10 ret */
    uint8_t text[] = { 0xE8, 0, 0, 0, 0, 0xB8, 0x2A, 0, 0, 0, 0xC3 };
    nuc_builder_add_segment(b, NUC_SEG_TEXT, 0x400000, sizeof(text),
                            NUC_VM_READ | NUC_VM_EXEC | NUC_VM_USER, text, (uint32_t)sizeof(text));
    nuc_builder_add_symbol(b, "main", NUC_SYM_FUNC, NUC_BIND_LOCAL, 0, 5, 6);
    nuc_builder_add_reloc(b, 0x400000 + 1, R_X86_64_PC32, 0);

    size_t n; char err[128];
    uint8_t* blob = nuc_builder_build(b, &n, err, sizeof err);
    nuc_builder_free(b);
    if (!blob) { fprintf(stderr, "build failed: %s\n", err); return 1; }
    if (write_file(out_path, blob, n) != 0) { fprintf(stderr, "write failed\n"); free(blob); return 1; }
    printf("wrote %s (%zu bytes)\n", out_path, n);
    free(blob);
    return 0;
}

/* ===== 跨平台共享的 .nuc 真机执行辅助 ===== */
static NucSegment* find_text_seg(NucFile* f) {
    for (uint16_t i = 0; i < f->seg_count; i++)
        if (f->segments[i].type == NUC_SEG_TEXT) return &f->segments[i];
    return NULL;
}
static NucSegment* find_rodata_seg(NucFile* f) {
    for (uint16_t i = 0; i < f->seg_count; i++)
        if (f->segments[i].type == NUC_SEG_RODATA) return &f->segments[i];
    return NULL;
}
/* 宿主执行时给 RODATA 找的映射位置：TEXT 之后紧跟，保证 lea rax,[rip+d] 能落到数据上。
 * 必须与编译期假定的「RODATA_vaddr - TEXT_vaddr」差值一致，否则 RIP 相对寻址会指错。
 * 编译期用 DATA_VADDR(0x500000) - TEXT_VADDR(0x400000) = 0x100000，
 * 故这里取 TEXT 实际基址 + 0x100000（并确保有足够空间）。 */
#define RODATA_VA_DELTA 0x100000u

static void apply_relocs(NucFile* f, NucSegment* text, uint8_t* base) {
    /* 宿主实际映射地址与规范里的逻辑 vaddr 未必相等（VirtualAlloc/mmap 给的是随机地址）。
     * 重定位算的是「字节偏移差」，与绝对地址无关，因此先把符号值换算成段内偏移，
     * 再把调用点也换算成段内偏移，二者相减即得正确的 rel32。
     *   （早前实现直接拿规范 vaddr（0x400000+）参与运算，在真实映射地址不同的机器上
     *     会算出错误的跳��，是 nucutil run 偶发段错误的历史根因。） */
    for (uint32_t i = 0; i < f->rel_count; i++) {
        NucReloc* r = &f->relocs[i];
        uint64_t sym_off = 0;                 /* 符号在 TEXT 段内的偏移 */
        if (r->sym_idx < f->symtab_count) {
            NucSymbol* s = &f->symbols[r->sym_idx];
            sym_off = s->value + f->segments[s->shndx].vaddr - text->vaddr;
        }
        uint64_t site_off = r->r_offset - text->vaddr;   /* 调用点在 TEXT 段内的偏移 */
        uint8_t* p = base + site_off;
        if (r->r_type == R_X86_64_PC32) {
            /* §7.2: *((int32_t*)site) = S + A - P - 4（此处 S、P 均为段内偏移） */
            *(int32_t*)p = (int32_t)((int64_t)sym_off - (int64_t)site_off - 4);
        } else if (r->r_type == R_X86_64_32) {
            /* §7.2: *((uint32_t*)site) = S（加载器可再叠加实际加载基址） */
            *(uint32_t*)p = (uint32_t)sym_off;
        } else {
            fprintf(stderr, "reloc type %u unsupported by host runner (skipped)\n", r->r_type);
        }
    }
}
/* 拷贝 TEXT 到 base、应用重定位并调用入口；返回入口返回值（64 位，勿用 int 再接！） */
static int64_t exec_text(NucFile* f, NucSegment* text, uint8_t* base) {
    memcpy(base, f->raw + text->file_offset, text->size);
    apply_relocs(f, text, base);
    uint8_t* entry = base + f->entry_point;
    int64_t (*fn)(void) = (int64_t (*)(void))entry;
    int64_t rc = fn();
    printf("entry returned: %lld (0x%llX)\n", (long long)rc, (unsigned long long)rc);
    return rc;
}
/* 打开+解析+校验+定位 TEXT 段；失败返回 NULL；成功置 *out_text */
static NucFile* run_load(const char* path, NucSegment** out_text) {
    size_t len; uint8_t* raw = read_file(path, &len); if (!raw) return NULL;
    NucFile* f = nuc_parse(raw, len); free(raw);
    if (!f) { fprintf(stderr, "parse failed (bad magic / too small)\n"); return NULL; }
    NucValidate v = nuc_validate(f);
    if (v.code != NOVUS_OK) { fprintf(stderr, "validate: %s\n", v.msg); nuc_file_free(f); return NULL; }
    NucSegment* text = find_text_seg(f);
    if (!text) { fprintf(stderr, "no TEXT segment\n"); nuc_file_free(f); return NULL; }
    *out_text = text;
    return f;
}

#if defined(_WIN32) && !defined(NO_WIN_RUN)
#include <windows.h>
static int run_nuc(const char* path) {
    NucSegment* text; NucFile* f = run_load(path, &text); if (!f) return 1;
    /* 一次性映射 TEXT 及其后的 RODATA 空隙（RODATA 被放在 TEXT 基址 + RODATA_VA_DELTA，
     * 与编译期假定的段间虚拟地址差一致，lea rax,[rip+d] 才能正确解析到字符串）。 */
    size_t span = (size_t)RODATA_VA_DELTA + 0x1000;
    if ((size_t)text->size > span) span = text->size;
    uint8_t* base = (uint8_t*)VirtualAlloc(NULL, span,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!base) { fprintf(stderr, "VirtualAlloc failed\n"); nuc_file_free(f); return 1; }
    /* 把 RODATA 拷到 base + RODATA_VA_DELTA */
    NucSegment* ro = find_rodata_seg(f);
    if (ro && ro->file_offset)
        memcpy(base + RODATA_VA_DELTA, f->raw + ro->file_offset, ro->size);
    exec_text(f, text, base);
    VirtualFree(base, 0, MEM_RELEASE);
    nuc_file_free(f);
    return 0;
}
#elif defined(__linux__) || defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON  /* BSD/macOS 用 MAP_ANON */
#endif
static int run_nuc(const char* path) {
    NucSegment* text; NucFile* f = run_load(path, &text); if (!f) return 1;
    long pg = sysconf(_SC_PAGESIZE); if (pg <= 0) pg = 4096;
    size_t asize = ((size_t)text->size + (size_t)pg - 1u) & ~((size_t)pg - 1u); /* 页对齐 */
    void* m = mmap(NULL, asize, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) { fprintf(stderr, "mmap(RWX) failed (需要可执行内存)\n"); nuc_file_free(f); return 1; }
    exec_text(f, text, (uint8_t*)m);
    munmap(m, asize);
    nuc_file_free(f);
    return 0;
}
#else
static int run_nuc(const char* path) {
    (void)path;
    fprintf(stderr, "run: 当前平台暂不支持真机执行（支持 Windows / Linux / macOS）\n");
    return 1;
}
#endif

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: nucutil <dump|validate|emit-demo|run> <file>\n");
        return 1;
    }
    if (strcmp(argv[1], "dump") == 0) {
        size_t len; uint8_t* raw = read_file(argv[2], &len);
        if (!raw) return 1;
        NucFile* f = nuc_parse(raw, len); free(raw);
        if (!f) { fprintf(stderr, "parse failed (bad magic / too small)\n"); return 1; }
        nuc_dump(f, stdout);
        nuc_file_free(f);
        return 0;
    }
    if (strcmp(argv[1], "validate") == 0) {
        size_t len; uint8_t* raw = read_file(argv[2], &len);
        if (!raw) return 1;
        NucFile* f = nuc_parse(raw, len); free(raw);
        if (!f) { fprintf(stderr, "parse failed (bad magic / too small)\n"); return 1; }
        NucValidate v = nuc_validate(f);
        printf("[%d] %s\n", v.code, v.msg);
        nuc_file_free(f);
        return v.code == NOVUS_OK ? 0 : 1;
    }
    if (strcmp(argv[1], "emit-demo") == 0) return emit_demo(argv[2]);
    if (strcmp(argv[1], "run") == 0) return run_nuc(argv[2]);
    fprintf(stderr, "unknown command %s\n", argv[1]);
    return 1;
}
