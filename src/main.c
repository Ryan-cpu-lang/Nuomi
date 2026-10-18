// SPDX-License-Identifier: GPL-2.0
/*
 * main.c — 程序入口：REPL、脚本执行、以及编译模式（-c 输出 NUC 原生文件）
 */
#include "nuomi.h"
#include "codegen.h"

#ifdef _WIN32
#include <windows.h>
#ifndef CP_UTF8
#define CP_UTF8 65001u
#endif
/* Windows 控制台默认按 ANSI/GBK 代码页输出，源码中的 UTF-8 中文字面量会被
 * 误解码成乱码。把输出/输入代码页设为 UTF-8 即可正确渲染。
 * （Linux/macOS 终端本就按 UTF-8，无需处理。） */
static void setup_console_utf8(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
#else
static void setup_console_utf8(void) { }
#endif

void runFile(const char* path, Env* global) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (fread(buf, 1, sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); exit(1); }
    buf[sz] = '\0';
    fclose(f);

    Reader r;
    r.src = buf; r.len = sz; r.pos = 0; r.error = false; r.done = false; r.errmsg = NULL;
    while (r.pos < r.len) {
        skipWs(&r);
        if (r.pos >= r.len) break;
        Value form = readForm(&r);
        if (r.error) { fprintf(stderr, "Parse error: %s\n", r.errmsg); break; }
        Value res = evalValue(form, global);
        g_dbg_phase = "runFile-release-res";
        release(res);
#ifdef NUOMI_DEBUG
        fprintf(stderr, "  [runFile] release(form) seq=%lu\n", IS_OBJ(form) ? AS_OBJ(form)->dbgSeq : 0);
#endif
        g_dbg_phase = "runFile-release-form";
        release(form);
    }
    free(buf);
}

void repl(Env* global) {
    char* buf = NULL;
    size_t buflen = 0, bufcaps = 0;
    char line[8192];
    printf("Nuomi 0.1 — 输入表达式回车求值，Ctrl+Z 回车 或 Ctrl+C 退出\n");
    for (;;) {
        printf("nuomi> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t ll = strlen(line);
        if (buflen + ll + 1 > bufcaps) {
            bufcaps = buflen + ll + 1 > 128 ? buflen + ll + 1 : 128;
            buf = (char*)realloc(buf, bufcaps);
        }
        memcpy(buf + buflen, line, ll);
        buflen += ll;
        buf[buflen] = '\0';

        Reader r;
        r.src = buf; r.len = buflen; r.pos = 0; r.error = false; r.done = false; r.errmsg = NULL;
        bool anyForm = false;
        while (r.pos < r.len) {
            skipWs(&r);
            if (r.pos >= r.len) break;
            Value form = readForm(&r);
            if (r.done) break;
            if (r.error) {
                if (strcmp(r.errmsg, "unexpected EOF") == 0) break; /* 不完整，继续读下一行 */
                fprintf(stderr, "Parse error: %s\n", r.errmsg);
                buflen = 0; buf[0] = '\0';
                anyForm = true;
                break;
            }
            Value res = evalValue(form, global);
            printf("=> ");
            printValue(res, stdout);
            printf("\n");
            fflush(stdout);
            release(res);
            release(form);
            anyForm = true;
        }
        if (anyForm && !r.error) {
            size_t consumed = r.pos;
            memmove(buf, buf + consumed, buflen - consumed);
            buflen -= consumed;
            buf[buflen] = '\0';
        }
    }
    free(buf);
}

int main(int argc, char** argv) {
    setup_console_utf8();   /* Windows: 控制台按 UTF-8 输出，修复 REPL 中文标语乱码 */
    Env* global = newEnv(NULL);
    registerNatives(global);

    /* 解析参数：编译模式 nuomi -c <in> -o <out.nuc> */
    bool compileMode = false;
    const char* inPath = NULL;
    const char* outPath = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) compileMode = true;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outPath = argv[++i];
        else if (!inPath) inPath = argv[i];
    }

    if (compileMode) {
        if (!inPath) { fprintf(stderr, "compile mode needs an input file: nuomi -c <in.nuomi> -o <out.nuc>\n"); return 1; }
        FILE* f = fopen(inPath, "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", inPath); return 1; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        char* src = (char*)malloc((size_t)sz + 1);
        if (fread(src, 1, (size_t)sz, f) != (size_t)sz) { fprintf(stderr, "read error\n"); fclose(f); free(src); return 1; }
        src[sz] = '\0'; fclose(f);

        uint8_t* blob = NULL; size_t n = 0; char err[256];
        int rc = nuomi_compile_to_nuc(src, &blob, &n, err, sizeof err);
        free(src);
        if (rc != 0) { fprintf(stderr, "compile failed: %s\n", err); return 1; }

        const char* op = outPath ? outPath : "a.nuc";
        FILE* of = fopen(op, "wb");
        if (!of) { fprintf(stderr, "cannot write %s\n", op); free(blob); return 1; }
        fwrite(blob, 1, n, of); fclose(of); free(blob);
        printf("compiled %s -> %s (%zu bytes)\n", inPath, op, n);
        release(OBJ_VAL(global));
        freeAllObjects();
        return 0;
    }

    if (inPath) {
        runFile(inPath, global);
    } else {
        repl(global);
    }

#ifdef NUOMI_DEBUG
    fprintf(stderr, "=== main: release(global) ===\n");
#endif
    g_dbg_phase = "main-release-global";
    release(OBJ_VAL(global));
    freeAllObjects();
    return 0;
}
