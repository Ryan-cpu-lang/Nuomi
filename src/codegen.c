/*
 * codegen.c — 里程碑 1：整数算术子集 → x86_64 机器码 → NUC 文件
 *
 * 设计要点（路线 A：直接机器码，不解释）：
 *   - 编译期符号表：处理顶层 (define name expr)，expr 必须是可常量折叠的整数
 *     算术（字面量 / 已定义符号 / + - *），结果登记为常量。
 *   - 入口表达式：递归遍历 AST，把结果算进 RAX；子表达式结果也留在 RAX，
 *     用 RBX 暂存左操作数。这样不需要通用寄存器分配就能生成正确的算术指令。
 *   - 生成的 .text：编译出的指令 + ret；入口偏移 0。无外部符号，无需重定位。
 *
 * x86_64 指令编码（System V AMD64）：
 *   mov eax, imm32 (符号扩展)         B8 + imm32        ; 值在 int32 范围内
 *   movabs rax, imm64                48 B8 + imm64      ; 超出 int32
 *   mov ebx, eax                     89 C3
 *   add eax, ebx                     01 D8
 *   sub eax, ebx                     29 D8
 *   imul eax, ebx                    0F AF C3
 *   ret                              C3
 */
#include "mochi.h"
#include "nucfmt.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* ====================== 字节缓冲 ====================== */
typedef struct { uint8_t* p; size_t len, cap; } CGBuf;
static void cg_init(CGBuf* b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void cg_put(CGBuf* b, const uint8_t* d, size_t n) {
    if (b->len + n > b->cap) {
        b->cap = (b->len + n) > 64 ? (b->len + n) * 2 : 64;
        b->p = (uint8_t*)realloc(b->p, b->cap);
    }
    memcpy(b->p + b->len, d, n);
    b->len += n;
}
static void cg_u8(CGBuf* b, uint8_t v)   { cg_put(b, &v, 1); }
static void cg_u32(CGBuf* b, uint32_t v) { uint8_t t[4] = { (uint8_t)(v&0xff),(uint8_t)((v>>8)&0xff),(uint8_t)((v>>16)&0xff),(uint8_t)((v>>24)&0xff) }; cg_put(b, t, 4); }
static void cg_u64(CGBuf* b, uint64_t v) { uint8_t t[8]; for (int i=0;i<8;i++) t[i]=(uint8_t)((v>>(8*i))&0xff); cg_put(b, t, 8); }

/* ====================== 指令编码 ====================== */
static void cg_mov_eax_imm(CGBuf* b, int64_t v) {
    if (v >= -2147483648LL && v <= 2147483647LL) {
        cg_u8(b, 0xB8); cg_u32(b, (uint32_t)(int32_t)v);   /* mov eax, imm32（符号扩展至 rax） */
    } else {
        cg_u8(b, 0x48); cg_u8(b, 0xB8); cg_u64(b, (uint64_t)v); /* movabs rax, imm64 */
    }
}
/* 只用调用方保存寄存器 RAX/RDX/RCX；绝不动 RBX/RBP/R12-R15（被调用方保存，
   本入口是叶子函数，clobber 它们会破坏 SysV ABI）。左操作数用栈暂存(push/pop)，
   这样任意深度的嵌套算术都不会寄存器互踩。 */
static void cg_push_rax(CGBuf* b)  { cg_u8(b, 0x50); }                 /* push rax */
static void cg_pop_rdx(CGBuf* b)   { cg_u8(b, 0x5A); }                 /* pop rdx  (5A = pop rdx) */
static void cg_add_eax_edx(CGBuf* b){ cg_u8(b,0x01); cg_u8(b,0xD0); }  /* add eax, edx */
/* 注意：双操作数 imul 的 Reg 字段是「目的」(与 add/mov 相反)，
   故 imul eax, edx 应为 0F AF C2（reg=eax, r/m=edx），而非 D0。 */
static void cg_imul_eax_edx(CGBuf* b){ cg_u8(b,0x0F); cg_u8(b,0xAF); cg_u8(b,0xC2); } /* imul eax, edx */
static void cg_neg_eax(CGBuf* b)   { cg_u8(b,0xF7); cg_u8(b,0xD8); }   /* neg eax */
static void cg_ret(CGBuf* b)       { cg_u8(b,0xC3); }

/* ====================== 编译期常量表 ====================== */
typedef struct { char* name; int64_t val; } Const;
typedef struct { Const* c; int n, cap; } ConstTab;
static char* cg_strdup(const char* s) { size_t l = strlen(s)+1; char* p = (char*)malloc(l); memcpy(p,s,l); return p; }
static void ct_add(ConstTab* t, const char* name, int64_t v) {
    for (int i=0;i<t->n;i++) if (strcmp(t->c[i].name,name)==0){ t->c[i].val=v; return; }
    if (t->n==t->cap){ t->cap = t->cap<4?4:t->cap*2; t->c=(Const*)realloc(t->c,t->cap*sizeof(Const)); }
    t->c[t->n].name = cg_strdup(name); t->c[t->n].val = v; t->n++;
}
static bool ct_find(ConstTab* t, const char* name, int64_t* out) {
    for (int i=0;i<t->n;i++) if (strcmp(t->c[i].name,name)==0){ *out=t->c[i].val; return true; }
    return false;
}

static bool isSymbol(Value v) {
    return IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING && ((ObjString*)AS_OBJ(v))->isSymbol;
}
static const char* symChars(Value v) { return ((ObjString*)AS_OBJ(v))->chars; }

/* 常量折叠：编译期把整数算术表达式算成 int64（仅字面量 / 已知符号 / + - *） */
static bool fold(Value node, ConstTab* tab, int64_t* out, char* err, size_t errlen) {
    if (IS_NUM(node)) { *out = (int64_t)AS_NUM(node); return true; }
    if (isSymbol(node)) {
        int64_t v;
        if (!ct_find(tab, symChars(node), &v)) { snprintf(err, errlen, "undefined symbol '%s'", symChars(node)); return false; }
        *out = v; return true;
    }
    if (isList(node)) {
        Value head = listCar(node);
        if (!isSymbol(head)) { snprintf(err, errlen, "bad form head"); return false; }
        const char* op = symChars(head);
        Value args = listCdr(node);
        int cnt = 0; for (Value c = args; isList(c); c = listCdr(c)) cnt++;
        if (cnt == 0) { snprintf(err, errlen, "%s expects >=1 argument", op); return false; }
        int64_t acc = 0;
        bool first = true;
        for (Value c = args; isList(c); c = listCdr(c)) {
            int64_t v; if (!fold(listCar(c), tab, &v, err, errlen)) return false;
            if (strcmp(op,"+")==0)      acc = first ? v : acc + v;
            else if (strcmp(op,"-")==0) acc = first ? v : acc - v;
            else if (strcmp(op,"*")==0) acc = first ? v : acc * v;
            else { snprintf(err, errlen, "operator '%s' not supported in compile mode", op); return false; }
            first = false;
        }
        if (strcmp(op,"-")==0 && cnt==1) acc = -acc;  /* 单目取负 */
        *out = acc; return true;
    }
    snprintf(err, errlen, "unsupported expression in compile mode");
    return false;
}

/* 代码生成：把表达式编译成 x86_64 指令，结果留在 RAX */
static bool cg_expr(Value node, ConstTab* tab, CGBuf* b, char* err, size_t errlen) {
    if (IS_NUM(node)) { cg_mov_eax_imm(b, (int64_t)AS_NUM(node)); return true; }
    if (isSymbol(node)) {
        int64_t v;
        if (!ct_find(tab, symChars(node), &v)) { snprintf(err, errlen, "undefined symbol '%s'", symChars(node)); return false; }
        cg_mov_eax_imm(b, v); return true;
    }
    if (isList(node)) {
        Value head = listCar(node);
        if (!isSymbol(head)) { snprintf(err, errlen, "bad form head"); return false; }
        const char* op = symChars(head);
        Value args = listCdr(node);
        if (strcmp(op,"+")==0 || strcmp(op,"-")==0 || strcmp(op,"*")==0) {
            int cnt = 0; for (Value c = args; isList(c); c = listCdr(c)) cnt++;
            if (cnt == 0) { snprintf(err, errlen, "%s expects >=1 argument", op); return false; }
            bool isSub = (strcmp(op,"-")==0), isMul = (strcmp(op,"*")==0);
            /* 编译第一个操作数 → RAX（左值） */
            if (!cg_expr(listCar(args), tab, b, err, errlen)) return false;
            for (Value c = listCdr(args); isList(c); c = listCdr(c)) {
                cg_push_rax(b);                                  /* 左值压栈保存（RAX 是调用方保存，安全） */
                if (!cg_expr(listCar(c), tab, b, err, errlen)) return false;  /* 右值 → RAX */
                cg_pop_rdx(b);                                   /* rdx = 左值；右值仍在 RAX */
                if (isSub)      { cg_neg_eax(b); cg_add_eax_edx(b); } /* RAX = 左 - 右 */
                else if (isMul) cg_imul_eax_edx(b);              /* RAX = 左 * 右 */
                else            cg_add_eax_edx(b);               /* RAX = 左 + 右 */
            }
            if (isSub && cnt == 1) cg_neg_eax(b);                /* 单目取负：RAX = -RAX */
            return true;
        }
        snprintf(err, errlen, "operator '%s' not supported in compile mode", op);
        return false;
    }
    snprintf(err, errlen, "unsupported expression in compile mode");
    return false;
}

int mochi_compile_to_nuc(const char* src, uint8_t** out, size_t* out_size, char* err, size_t errlen) {
    if (err && errlen) err[0] = '\0';
    Reader r;
    r.src = src; r.len = strlen(src); r.pos = 0; r.error = false; r.done = false; r.errmsg = NULL;

    ConstTab tab; tab.c = NULL; tab.n = 0; tab.cap = 0;
    CGBuf text; cg_init(&text);

    /* 解析全部顶层表单（上限 256，里程碑足够） */
    Value forms[256]; int nf = 0;
    while (!r.done && !r.error) {
        Value f = readForm(&r);
        if (r.error) { snprintf(err, errlen, "parse error: %s", r.errmsg ? r.errmsg : "?"); goto fail; }
        if (IS_NIL(f) && r.done) break;
        if (nf < 256) forms[nf++] = f;
    }

    /* 处理 define（常量折叠登记），并记录最后一个非 define 表单作为入口 */
    int entryIdx = -1;
    for (int i = 0; i < nf; i++) {
        Value f = forms[i];
        if (isList(f) && isSymbol(listCar(f)) && strcmp(symChars(listCar(f)), "define") == 0) {
            Value rest = listCdr(f);
            if (!isList(rest) || !isSymbol(listCar(rest))) { snprintf(err, errlen, "define: (define name expr)"); goto fail; }
            const char* nm = symChars(listCar(rest));
            int64_t v;
            if (!fold(listCar(listCdr(rest)), &tab, &v, err, errlen)) goto fail;
            ct_add(&tab, nm, v);
            continue;
        }
        entryIdx = i;   /* 最后一个非 define 表单即入口 */
    }
    if (entryIdx < 0) { snprintf(err, errlen, "no entry expression to compile"); goto fail; }

    /* 编译入口表达式 → x86_64，并以 ret 结尾 */
    if (!cg_expr(forms[entryIdx], &tab, &text, err, errlen)) goto fail;
    cg_ret(&text);

    /* 组装 NUC：.text = 编译出的机器码，入口偏移 0，无重定位 */
    NucBuilder* nb = nuc_builder_new();
    nuc_builder_set_flags(nb, NUC_FLAG_64BIT | NUC_FLAG_STATIC);
    nuc_builder_set_entry(nb, 0);
    nuc_builder_set_stack(nb, 0x800000);
    nuc_builder_add_segment(nb, NUC_SEG_TEXT, 0x400000, text.len,
                            NUC_VM_READ | NUC_VM_EXEC | NUC_VM_USER, text.p, (uint32_t)text.len);
    size_t n; uint8_t* blob = nuc_builder_build(nb, &n, err, errlen);
    nuc_builder_free(nb);
    free(text.p);
    if (!blob) goto fail;
    *out = blob; *out_size = n;

    for (int i = 0; i < tab.n; i++) free(tab.c[i].name);
    free(tab.c);
    return 0;

fail:
    free(text.p);
    for (int i = 0; i < tab.n; i++) free(tab.c[i].name);
    free(tab.c);
    return -1;
}
