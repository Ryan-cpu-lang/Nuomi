// SPDX-License-Identifier: GPL-2.0
/*
 * strutil.c — 字符串工具原语
 *
 * 启动器写配置、替换占位符（${auth_player_name} 等）、按分隔符拆坐标
 * （group:artifact:version）都离不开它们。纯 C 实现，行为可预测、好测试。
 *
 * 暴露的原语：
 *   (str-split s sep)        -> 按 sep（字符串）切分，返回字符串链表
 *   (str-replace s old new)  -> 把 s 中所有 old 替换为 new，返回新串
 */
#include "nuomi.h"
#include <stdlib.h>
#include <string.h>

/* (str-split s sep) -> 链表 */
static Value natStrSplit(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2) { runtimeError("str-split expects (s sep)"); return NIL_VAL; }
    if (!IS_OBJ(a[0]) || AS_OBJ(a[0])->type != OBJ_STRING) { runtimeError("str-split: s must be a string"); return NIL_VAL; }
    if (!IS_OBJ(a[1]) || AS_OBJ(a[1])->type != OBJ_STRING) { runtimeError("str-split: sep must be a string"); return NIL_VAL; }
    ObjString* s = (ObjString*)AS_OBJ(a[0]);
    ObjString* sep = (ObjString*)AS_OBJ(a[1]);

    if (sep->length == 0) {  /* 空分隔符：整串作为单元素 */
        ObjString* one = internString(s->chars, s->length, false);
        retain(OBJ_VAL(one));
        ObjList* cell = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
        cell->car = OBJ_VAL(one); cell->cdr = NIL_VAL;
        retain(OBJ_VAL(one));
        return OBJ_VAL(cell);
    }

    Value rev = NIL_VAL;
    int start = 0, i = 0;
    while (i + sep->length <= s->length) {
        int match = 1;
        for (int k = 0; k < sep->length; k++)
            if (s->chars[i + k] != sep->chars[k]) { match = 0; break; }
        if (match) {
            ObjString* part = internString(s->chars + start, i - start, false);
            retain(OBJ_VAL(part));
            ObjList* cell = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
            cell->car = OBJ_VAL(part); cell->cdr = rev;   /* rev 所有权转移 */
            rev = OBJ_VAL(cell);
            i += sep->length; start = i;
        } else i++;
    }
    /* 末段 */
    ObjString* part = internString(s->chars + start, s->length - start, false);
    retain(OBJ_VAL(part));
    ObjList* cell = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
    cell->car = OBJ_VAL(part); cell->cdr = rev;
    rev = OBJ_VAL(cell);

    /* 反转回正序 */
    Value out = NIL_VAL;
    while (isList(rev)) {
        Value p = listCar(rev);
        Value tail = listCdr(rev);
        ObjList* nc = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
        nc->car = p; retain(p);
        nc->cdr = out;
        out = OBJ_VAL(nc);
        rev = tail;
    }
    return out;
}

/* (str-replace s old new) -> 新串 */
static Value natStrReplace(int c, Value* a, Env* e) {
    (void)e;
    if (c != 3) { runtimeError("str-replace expects (s old new)"); return NIL_VAL; }
    if (!IS_OBJ(a[0]) || AS_OBJ(a[0])->type != OBJ_STRING ||
        !IS_OBJ(a[1]) || AS_OBJ(a[1])->type != OBJ_STRING ||
        !IS_OBJ(a[2]) || AS_OBJ(a[2])->type != OBJ_STRING) {
        runtimeError("str-replace: all args must be strings");
        return NIL_VAL;
    }
    ObjString* s   = (ObjString*)AS_OBJ(a[0]);
    ObjString* old = (ObjString*)AS_OBJ(a[1]);
    ObjString* nw  = (ObjString*)AS_OBJ(a[2]);
    if (old->length == 0) { retain(OBJ_VAL(s)); return OBJ_VAL(s); }  /* 防死循环 */

    size_t cap = (s->length > 0 ? s->length : 1) + nw->length + 1;
    char* buf = (char*)malloc(cap);
    if (!buf) return NIL_VAL;
    size_t len = 0;
    int start = 0, i = 0;
    while (i + old->length <= s->length) {
        int match = 1;
        for (int k = 0; k < old->length; k++)
            if (s->chars[i + k] != old->chars[k]) { match = 0; break; }
        if (match) {
            /* 前缀 */
            if ((size_t)(i - start) + len + nw->length + 1 > cap) {
                size_t nc = (len + (size_t)(i - start) + nw->length) * 2 + 1;
                char* nb = (char*)realloc(buf, nc); if (!nb) { free(buf); return NIL_VAL; }
                buf = nb; cap = nc;
            }
            memcpy(buf + len, s->chars + start, (size_t)(i - start)); len += (size_t)(i - start);
            memcpy(buf + len, nw->chars, nw->length); len += nw->length;
            i += old->length; start = i;
        } else i++;
    }
    if (len + (size_t)(s->length - start) + 1 > cap) {
        size_t nc = len + (size_t)(s->length - start) + 1;
        char* nb = (char*)realloc(buf, nc); if (!nb) { free(buf); return NIL_VAL; }
        buf = nb; cap = nc;
    }
    memcpy(buf + len, s->chars + start, (size_t)(s->length - start)); len += (size_t)(s->length - start);

    ObjString* res = internString(buf, (int)len, false);
    free(buf);
    retain(OBJ_VAL(res));
    return OBJ_VAL(res);
}

void registerStrUtilNatives(Env* g) {
    defNative(g, "str-split",  natStrSplit);
    defNative(g, "str-replace", natStrReplace);
}
