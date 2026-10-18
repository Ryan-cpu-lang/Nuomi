// SPDX-License-Identifier: GPL-2.0
/*
 * nucutil.c — NUC 文件工具
 *   dump <file.nuc>       人类可读 dump
 *   validate <file.nuc>   按 §10 ABI 规则校验
 *   emit-demo <out.nuc>    生成一个带 PC32 重定位、可在真机跑通的最小示例
 *   run <file.nuc>         （仅 Windows）映射 TEXT 段并执行入口，验证加载流程
 */
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

#if defined(_WIN32) && !defined(NO_WIN_RUN)
#include <windows.h>
static int run_nuc(const char* path) {
    size_t len; uint8_t* raw = read_file(path, &len); if (!raw) return 1;
    NucFile* f = nuc_parse(raw, len); free(raw);
    if (!f) { fprintf(stderr, "parse failed (bad magic / too small)\n"); return 1; }
    NucValidate v = nuc_validate(f);
    if (v.code != NOVUS_OK) { fprintf(stderr, "validate: %s\n", v.msg); nuc_file_free(f); return 1; }

    NucSegment* text = NULL;
    for (uint16_t i = 0; i < f->seg_count; i++)
        if (f->segments[i].type == NUC_SEG_TEXT) { text = &f->segments[i]; break; }
    if (!text) { fprintf(stderr, "no TEXT segment\n"); nuc_file_free(f); return 1; }

    uint8_t* base = (uint8_t*)VirtualAlloc(NULL, (SIZE_T)text->size,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!base) { fprintf(stderr, "VirtualAlloc failed\n"); nuc_file_free(f); return 1; }
    memcpy(base, f->raw + text->file_offset, text->size);

    for (uint32_t i = 0; i < f->rel_count; i++) {
        NucReloc* r = &f->relocs[i];
        if (r->r_type != R_X86_64_PC32) {
            fprintf(stderr, "reloc type %u unsupported by host runner (skipped)\n", r->r_type);
            continue;
        }
        uint64_t sym_vaddr = 0;
        if (r->sym_idx < f->symtab_count) {
            NucSymbol* s = &f->symbols[r->sym_idx];
            sym_vaddr = s->value + f->segments[s->shndx].vaddr;
        }
        int64_t rel32 = (int64_t)sym_vaddr - (int64_t)r->r_offset - 4;
        uint8_t* p = base + (r->r_offset - text->vaddr);
        *(int32_t*)p = (int32_t)rel32;
    }

    uint8_t* entry = base + f->entry_point;
    int (*fn)(void) = (int (*)(void))entry;
    int rc = fn();
    printf("entry returned: %d (0x%X)\n", rc, (unsigned)rc);
    VirtualFree(base, 0, MEM_RELEASE);
    nuc_file_free(f);
    return 0;
}
#else
static int run_nuc(const char* path) {
    (void)path;
    fprintf(stderr, "run: 仅 Windows x86_64 支持真机执行\n");
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
