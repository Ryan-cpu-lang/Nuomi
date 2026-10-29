// SPDX-License-Identifier: GPL-2.0
/*
 * sha1.c — SHA-1 校验原语
 *
 * 为什么糯米需要它：启动器下载 client.jar / 依赖库 / 资源后，必须用清单里
 * 提供的 sha1 校验文件完整性（防中间人、防损坏）。Mojang 的版本元数据每个
 * 下载项都带 sha1 字段，没有它「下载+校验」这一步就缺一角。
 *
 * 实现：纯 C 的 SHA-1（FIPS 180-1），无第三方依赖。喂任意字节序列，
 * 输出 40 字符小写十六进制串，与 `certutil -hashfile x sha1` / Python hashlib
 * 结果一致，方便对拍测试。
 *
 * 暴露的原语：
 *   (sha1-string s)      -> 字符串/字节的 sha1 十六进制；非字符串参数返回 nil
 *   (sha1-file path)     -> 文件的 sha1 十六进制；文件打不开返回 nil
 */
#include "nuomi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- 核心：一次性 SHA-1（任意长度字节） ---------- */
static void sha1_once(const unsigned char* data, size_t len, unsigned char out[20]) {
    uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu,
             h3 = 0x10325476u, h4 = 0xC3D2E1F0u;
    uint32_t w[80];
    unsigned char block[64];

    /* 填充后总字节数：len + 0x80 + 若干 0 + 8 字节(64位)大端位长，且是 64 的倍数 */
    size_t total = len + 1;                 /* 先放 0x80 */
    while (total % 64 != 56) total++;
    total += 8;
    uint64_t bitlen = (uint64_t)len * 8;

    size_t blocks = total / 64;
    for (size_t bi = 0; bi < blocks; bi++) {
        /* 逐字节生成填充后的块（data 字节 → 0x80 → 0 → 64位大端位长） */
        for (int i = 0; i < 64; i++) {
            size_t mp = bi * 64 + (size_t)i;
            if (mp < len)               block[i] = data[mp];
            else if (mp == len)         block[i] = 0x80;
            else if (mp < total - 8)    block[i] = 0x00;
            else {
                int off = (int)(mp - (total - 8));        /* 0..7 */
                block[i] = (unsigned char)((bitlen >> (56 - off * 8)) & 0xFFu);
            }
        }
        for (int i = 0; i < 16; i++) {
            w[i] = ((uint32_t)block[i*4]   << 24) |
                   ((uint32_t)block[i*4+1] << 16) |
                   ((uint32_t)block[i*4+2] << 8)  |
                   ((uint32_t)block[i*4+3]);
        }
        for (int i = 16; i < 80; i++) {
            uint32_t x = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (x << 1) | (x >> 31);
        }
        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);            k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                       k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);     k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                       k = 0xCA62C1D6u; }
            uint32_t tmp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = tmp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    out[0]  = (unsigned char)(h0 >> 24); out[1]  = (unsigned char)(h0 >> 16);
    out[2]  = (unsigned char)(h0 >> 8);  out[3]  = (unsigned char)(h0);
    out[4]  = (unsigned char)(h1 >> 24); out[5]  = (unsigned char)(h1 >> 16);
    out[6]  = (unsigned char)(h1 >> 8);  out[7]  = (unsigned char)(h1);
    out[8]  = (unsigned char)(h2 >> 24); out[9]  = (unsigned char)(h2 >> 16);
    out[10] = (unsigned char)(h2 >> 8);  out[11] = (unsigned char)(h2);
    out[12] = (unsigned char)(h3 >> 24); out[13] = (unsigned char)(h3 >> 16);
    out[14] = (unsigned char)(h3 >> 8);  out[15] = (unsigned char)(h3);
    out[16] = (unsigned char)(h4 >> 24); out[17] = (unsigned char)(h4 >> 16);
    out[18] = (unsigned char)(h4 >> 8);  out[19] = (unsigned char)(h4);
}

static ObjString* sha1_to_hex(const unsigned char dig[20]) {
    static const char* hexd = "0123456789abcdef";
    char buf[41];
    for (int i = 0; i < 20; i++) {
        buf[i*2]   = hexd[dig[i] >> 4];
        buf[i*2+1] = hexd[dig[i] & 0xF];
    }
    ObjString* s = internString(buf, 40, false);
    retain(OBJ_VAL(s));
    return s;
}

/* (sha1-string s) -> 十六进制串 */
static Value natSha1String(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_OBJ(a[0]) || AS_OBJ(a[0])->type != OBJ_STRING) {
        runtimeError("sha1-string expects a string");
        return NIL_VAL;
    }
    ObjString* s = (ObjString*)AS_OBJ(a[0]);
    unsigned char dig[20];
    sha1_once((const unsigned char*)s->chars, (size_t)s->length, dig);
    return OBJ_VAL(sha1_to_hex(dig));
}

/* (sha1-file path) -> 十六进制串；打不开返回 nil */
static Value natSha1File(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_OBJ(a[0]) || AS_OBJ(a[0])->type != OBJ_STRING) {
        runtimeError("sha1-file expects a path string");
        return NIL_VAL;
    }
    ObjString* p = (ObjString*)AS_OBJ(a[0]);
    FILE* f = fopen(p->chars, "rb");
    if (!f) return NIL_VAL;
    unsigned char* buf = (unsigned char*)malloc(1 << 16);
    if (!buf) { fclose(f); return NIL_VAL; }
    size_t got;
    unsigned char dig[20];
    /* sha1_once 是一性次的，文件需流式处理：这里简单起见整文件读入内存后一次性算。
       启动器下载的 jar 通常几十 MB，整读可接受；超大文件可改流式。 */
    size_t cap = 1 << 16, len = 0;
    unsigned char* all = (unsigned char*)malloc(cap);
    if (!all) { free(buf); fclose(f); return NIL_VAL; }
    while ((got = fread(buf, 1, 1 << 16, f)) > 0) {
        if (len + got > cap) {
            while (len + got > cap) cap *= 2;
            unsigned char* na = (unsigned char*)realloc(all, cap);
            if (!na) { free(all); free(buf); fclose(f); return NIL_VAL; }
            all = na;
        }
        memcpy(all + len, buf, got);
        len += got;
    }
    fclose(f);
    free(buf);
    sha1_once(all, len, dig);
    free(all);
    return OBJ_VAL(sha1_to_hex(dig));
}

void registerSha1Natives(Env* g) {
    defNative(g, "sha1-string", natSha1String);
    defNative(g, "sha1-file",    natSha1File);
}
