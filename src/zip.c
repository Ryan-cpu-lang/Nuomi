// SPDX-License-Identifier: GPL-2.0
/*
 * zip.c — ZIP 解压原语
 *
 * 为什么糯米需要它：启动器要把游戏跑起来，必须解压 LWJGL 的原生库
 * （lwjgl-natives-windows.jar 里是一堆 .dll）。这些 jar 是 deflate 压缩的 zip。
 * 没有解压能力，java 就加载不到原生库，游戏起不来。
 *
 * 实现：纯 C 的 ZIP 读取 + 自带最小 deflate 解压器（RFC 1951，支持 stored 与
 *        fixed/dynamic Huffman 的 deflate），无 zlib 依赖。脚本侧：
 *   (unzip archive dest)  -> 解压到 dest 目录，返回解出的条目数；失败 -1
 *
 * 路径安全：跳过含 ".." 的条目（防 zip slip），目录型条目只建目录不写文件。
 */
#include "nuomi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

/* ---------------- deflate 解压器（raw deflate，无 zlib 头） ---------------- */
#define ZMAXBITS 15
#define ZMAXCODES 288

typedef struct {
    short count[ZMAXBITS + 1];
    short symbol[ZMAXCODES];
} Huff;

typedef struct {
    const unsigned char* in;
    size_t inLen;
    size_t bitpos;   /* 当前位位置 */
} Br;

static int br_bit(Br* b) {
    if (b->bitpos >= b->inLen * 8) return 0;
    unsigned char byte = b->in[b->bitpos >> 3];
    int bit = (byte >> (b->bitpos & 7)) & 1;
    b->bitpos++;
    return bit;
}
static int br_bits(Br* b, int n) { /* LSB-first */
    int v = 0;
    for (int i = 0; i < n; i++) v |= br_bit(b) << i;
    return v;
}

/* 由码长数组构建规范 Huffman 表（Adler 的 puff 算法） */
static int build_huff(Huff* h, const unsigned char* lengths, int n) {
    for (int i = 0; i <= ZMAXBITS; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[lengths[i]]++;
    short offs[ZMAXBITS + 1];
    int left = 1;
    for (int len = 1; len <= ZMAXBITS; len++) {
        left <<= 1;
        left -= h->count[len];
        if (left < 0) return -1;            /* 码长超订（over-subscribed） */
    }
    offs[1] = 0;
    for (int len = 1; len < ZMAXBITS; len++) offs[len + 1] = offs[len] + h->count[len];
    for (int i = 0; i < n; i++) {
        int len = lengths[i];
        if (len) h->symbol[offs[len]++] = (short)i;
    }
    return 0;
}

static int decode_sym(Huff* h, Br* b) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= ZMAXBITS; len++) {
        code |= br_bit(b);
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

static const int LEN_BASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,
                                 51,59,67,83,99,115,131,163,195,227,258};
static const int LEN_EXTRA[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const int DIST_BASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,
                                  513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const int DIST_EXTRA[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static void push_out(unsigned char** ob, size_t* cap, size_t* len, unsigned char c) {
    if (*len + 1 > *cap) {
        size_t nc = *cap ? *cap * 2 : (1u << 16);
        unsigned char* nb = (unsigned char*)realloc(*ob, nc);
        if (!nb) return;
        *ob = nb; *cap = nc;
    }
    (*ob)[(*len)++] = c;
}

/* 解压 raw deflate；成功返回 0，*out 为 malloc 的缓冲区（调用方 free） */
static int inflate_raw(const unsigned char* in, size_t inLen,
                       unsigned char** out, size_t* outLen) {
    Br b = { in, inLen, 0 };
    size_t ocap = 1 << 16, olen = 0;
    unsigned char* ob = (unsigned char*)malloc(ocap);
    if (!ob) return -1;
    unsigned char window[32768];
    size_t wpos = 0;
    int final = 0;

    do {
        final = br_bits(&b, 1);
        int type = br_bits(&b, 2);
        if (type == 0) {
            /* stored：对齐到字节边界，读 LEN 后原样拷贝 */
            b.bitpos = (b.bitpos + 7) & ~((size_t)7);
            if (b.bitpos + 16 > b.inLen * 8) { free(ob); return -1; }
            size_t bp = b.bitpos >> 3;
            int l = in[bp] | (in[bp + 1] << 8);
            b.bitpos += 16;
            for (int i = 0; i < l; i++) {
                if ((b.bitpos >> 3) >= b.inLen) { free(ob); return -1; }
                unsigned char c = in[b.bitpos >> 3];
                b.bitpos += 8;
                push_out(&ob, &ocap, &olen, c);
                window[wpos & 32767] = c; wpos++;
            }
        } else if (type == 1 || type == 2) {
            Huff lit, dist;
            if (type == 1) {
                unsigned char fl[288]; unsigned char fd[32];
                for (int i = 0;  i < 144; i++) fl[i] = 8;
                for (int i = 144; i < 256; i++) fl[i] = 9;
                for (int i = 256; i < 280; i++) fl[i] = 7;
                for (int i = 280; i < 288; i++) fl[i] = 8;
                for (int i = 0;   i < 32;  i++) fd[i] = 5;
                if (build_huff(&lit, fl, 288)) { free(ob); return -1; }
                if (build_huff(&dist, fd, 32)) { free(ob); return -1; }
            } else {
                int hlit  = br_bits(&b, 5) + 257;
                int hdist = br_bits(&b, 5) + 1;
                int hclen = br_bits(&b, 4) + 4;
                static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                unsigned char cl[19]; for (int i = 0; i < 19; i++) cl[i] = 0;
                for (int i = 0; i < hclen; i++) cl[order[i]] = (unsigned char)br_bits(&b, 3);
                Huff clh;
                if (build_huff(&clh, cl, 19)) { free(ob); return -1; }
                unsigned char lengths[288 + 32]; int n = 0;
                while (n < hlit + hdist) {
                    int sym = decode_sym(&clh, &b);
                    if (sym < 0) { free(ob); return -1; }
                    if (sym < 16) {
                        lengths[n++] = (unsigned char)sym;
                    } else if (sym == 16) {
                        if (n == 0) { free(ob); return -1; }
                        int rep = br_bits(&b, 2) + 3;
                        unsigned char prev = lengths[n - 1];
                        while (rep-- > 0 && n < hlit + hdist) lengths[n++] = prev;
                    } else if (sym == 17) {
                        int rep = br_bits(&b, 3) + 3;
                        while (rep-- > 0 && n < hlit + hdist) lengths[n++] = 0;
                    } else if (sym == 18) {
                        int rep = br_bits(&b, 7) + 11;
                        while (rep-- > 0 && n < hlit + hdist) lengths[n++] = 0;
                    } else { free(ob); return -1; }
                }
                if (build_huff(&lit, lengths, hlit)) { free(ob); return -1; }
                if (build_huff(&dist, lengths + hlit, hdist)) { free(ob); return -1; }
            }
            /* 解码块 */
            while (1) {
                int sym = decode_sym(&lit, &b);
                if (sym < 0) { free(ob); return -1; }
                if (sym == 256) break;
                if (sym < 256) {
                    unsigned char c = (unsigned char)sym;
                    push_out(&ob, &ocap, &olen, c);
                    window[wpos & 32767] = c; wpos++;
                    continue;
                }
                sym -= 257;
                if (sym >= 29) { free(ob); return -1; }
                int length = LEN_BASE[sym] + br_bits(&b, LEN_EXTRA[sym]);
                int dsym = decode_sym(&dist, &b);
                if (dsym < 0 || dsym >= 30) { free(ob); return -1; }
                int distance = DIST_BASE[dsym] + br_bits(&b, DIST_EXTRA[dsym]);
                if ((size_t)distance > wpos) { free(ob); return -1; }
                for (int i = 0; i < length; i++) {
                    unsigned char c = window[(wpos - distance) & 32767];
                    push_out(&ob, &ocap, &olen, c);
                    window[wpos & 32767] = c; wpos++;
                }
            }
        } else {
            free(ob);
            return -1;   /* type == 3 非法 */
        }
    } while (!final);

    *out = ob; *outLen = olen;
    return 0;
}

/* ---------------- ZIP 容器解析 + 解压 ---------------- */
static uint16_t rd16(const unsigned char* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t rd32(const unsigned char* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#if defined(_WIN32)
static int make_dirs(const char* path) {
    /* 逐级创建目录（path 用 \\ 或 / 分隔均可）
     *
     * 关键：必须跳过盘符前缀 "C:"。CreateDirectoryA("C:") 指的是
     * 「C 盘的当前目录」而不是根目录 C:\，会失败并让整段建目录中断，
     * 结果是后面的每一级目录都没建出来、解压全部静默失败。 */
    char buf[1024];
    size_t n = strlen(path);
    if (n >= sizeof buf) return -1;
    memcpy(buf, path, n + 1);
    size_t start = 0;
    if (n >= 2 && buf[1] == ':') start = 2;   /* 盘符之后才开始建 */
    for (size_t i = start; i < n; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char save = buf[i];
            buf[i] = '\0';
            if (i > start) {                  /* 跳过空段和纯盘符段 */
                if (!CreateDirectoryA(buf, NULL)) {
                    if (GetLastError() != ERROR_ALREADY_EXISTS) { buf[i] = save; return -1; }
                }
            }
            buf[i] = save;
        }
    }
    if (buf[0] && n > start) {
        if (!CreateDirectoryA(buf, NULL)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) return -1;
        }
    }
    return 0;
}
static int write_file(const char* path, const unsigned char* data, size_t len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    DWORD wr = 0; BOOL ok = TRUE;
    if (len) ok = WriteFile(h, data, (DWORD)len, &wr, NULL);
    CloseHandle(h);
    return (ok && wr == (DWORD)len) ? 0 : -1;
}
#else
static int make_dirs(const char* path) {
    char buf[1024];
    size_t n = strlen(path);
    if (n >= sizeof buf) return -1;
    memcpy(buf, path, n + 1);
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char save = buf[i];
            buf[i] = '\0';
            if (i > start) {
                if (mkdir(buf, 0755) != 0 && errno != EEXIST) { buf[i] = save; return -1; }
            }
            buf[i] = save;
        }
    }
    if (buf[0] && n > start) {
        if (mkdir(buf, 0755) != 0 && errno != EEXIST) return -1;
    }
    return 0;
}
static int write_file(const char* path, const unsigned char* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    size_t wr = fwrite(data, 1, len, f);
    fclose(f);
    return (wr == len) ? 0 : -1;
}
#endif

/* (unzip archive dest) -> 解出的条目数；失败 -1 */
static Value natUnzip(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2 || !IS_OBJ(a[0]) || AS_OBJ(a[0])->type != OBJ_STRING ||
                 !IS_OBJ(a[1]) || AS_OBJ(a[1])->type != OBJ_STRING) {
        runtimeError("unzip expects (archive dest)");
        return NUM_VAL(-1);
    }
    const char* apath = ((ObjString*)AS_OBJ(a[0]))->chars;
    const char* dprefix = ((ObjString*)AS_OBJ(a[1]))->chars;

    FILE* f = fopen(apath, "rb");
    if (!f) return NUM_VAL(-1);
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NUM_VAL(-1); }
    unsigned char* buf = (unsigned char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NUM_VAL(-1); }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(buf); return NUM_VAL(-1); }

    /* 找 EOCD（签名 50 4B 05 06），从尾部向前扫 */
    size_t eocd = 0, start = (rd > 65557) ? rd - 65557 : 0;
    for (size_t i = (rd >= 22 ? rd - 22 : 0); i + 4 <= rd && i >= start; i--) {
        if (buf[i] == 0x50 && buf[i+1] == 0x4B && buf[i+2] == 0x05 && buf[i+3] == 0x06) {
            eocd = i; break;
        }
    }
    if (!eocd) { free(buf); return NUM_VAL(-1); }

    uint32_t cd_off = rd32(buf + eocd + 16);
    int entries = (int)rd16(buf + eocd + 10);

    int extracted = 0;
    size_t p = cd_off;
    for (int e = 0; e < entries; e++) {
        if (p + 46 > rd) break;
        if (!(buf[p] == 0x50 && buf[p+1] == 0x4B && buf[p+2] == 0x01 && buf[p+3] == 0x02)) break;
        int method    = rd16(buf + p + 10);
        int compSize  = (int)rd32(buf + p + 20);
        int fnlen     = rd16(buf + p + 28);
        int extralen  = rd16(buf + p + 30);
        int commentlen= rd16(buf + p + 32);
        uint32_t localOff = rd32(buf + p + 42);
        if (p + 46 + fnlen > rd) break;
        const unsigned char* fname = buf + p + 46;

        /* 复制文件名并规范化分隔符（/ -> \\），去掉前导分隔符 */
        char name[1024];
        if (fnlen >= (int)sizeof name) { p += 46 + fnlen + extralen + commentlen; continue; }
        int ni = 0;
        for (int i = 0; i < fnlen; i++) {
            char ch = (char)fname[i];
            if (ch == '/') ch = '\\';
            if (ni == 0 && (ch == '\\' || ch == '.')) continue; /* 跳过前导 / 与 . */
            name[ni++] = ch;
        }
        name[ni] = '\0';
        /* 防 zip slip */
        if (strstr(name, "..") != NULL) { p += 46 + fnlen + extralen + commentlen; continue; }

        /* 目录型条目：只建目录 */
        int isDir = (ni > 0 && name[ni-1] == '\\');
        if (isDir) {
            char dpath[2048];
            snprintf(dpath, sizeof dpath, "%s\\%s", dprefix, name);
            make_dirs(dpath);
            extracted++; p += 46 + fnlen + extralen + commentlen; continue;
        }

        if (localOff + 30 > rd) { p += 46 + fnlen + extralen + commentlen; continue; }
        int lfnlen = rd16(buf + localOff + 26);
        int lextralen = rd16(buf + localOff + 28);
        size_t dataOff = localOff + 30 + lfnlen + lextralen;
        if (dataOff + (size_t)compSize > rd) {
            /* 压缩大小缺失/溢出：尽力用剩余字节 */
            if (dataOff >= rd) { p += 46 + fnlen + extralen + commentlen; continue; }
            compSize = (int)(rd - dataOff);
        }
        const unsigned char* cdata = buf + dataOff;

        char outpath[2048];
        snprintf(outpath, sizeof outpath, "%s\\%s", dprefix, name);
        /* 确保父目录存在 */
        char dcopy[2048]; memcpy(dcopy, outpath, strlen(outpath) + 1);
        for (int i = (int)strlen(dcopy) - 1; i >= 0; i--) {
            if (dcopy[i] == '\\') { dcopy[i] = '\0'; make_dirs(dcopy); break; }
        }

        int ok = -1;
        if (method == 0) {
            ok = write_file(outpath, cdata, (size_t)compSize);
        } else if (method == 8) {
            unsigned char* out = NULL; size_t outLen = 0;
            if (inflate_raw(cdata, (size_t)compSize, &out, &outLen) == 0) {
                ok = write_file(outpath, out, outLen);
                free(out);
            }
        } /* 其它压缩方式跳过 */
        if (ok == 0) extracted++;

        p += 46 + fnlen + extralen + commentlen;
    }

    free(buf);
    return NUM_VAL((double)extracted);
}

void registerZipNatives(Env* g) {
    defNative(g, "unzip", natUnzip);
}
