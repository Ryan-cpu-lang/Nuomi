// SPDX-License-Identifier: GPL-2.0
/*
 * native.c — 内置原生函数
 */
#include "nuomi.h"

/* ---------- 算术 ---------- */
static Value natAdd(int c, Value* a, Env* e) {
    (void)e; double s = 0;
    for (int i = 0; i < c; i++) { if (!IS_NUM(a[i])) { runtimeError("+ expects numbers"); return NIL_VAL; } s += AS_NUM(a[i]); }
    return NUM_VAL(s);
}
static Value natSub(int c, Value* a, Env* e) {
    (void)e;
    if (c == 1) { if (!IS_NUM(a[0])) { runtimeError("- expects numbers"); return NIL_VAL; } return NUM_VAL(-AS_NUM(a[0])); }
    double s = AS_NUM(a[0]);
    for (int i = 1; i < c; i++) { if (!IS_NUM(a[i])) { runtimeError("- expects numbers"); return NIL_VAL; } s -= AS_NUM(a[i]); }
    return NUM_VAL(s);
}
static Value natMul(int c, Value* a, Env* e) {
    (void)e; double s = 1;
    for (int i = 0; i < c; i++) { if (!IS_NUM(a[i])) { runtimeError("* expects numbers"); return NIL_VAL; } s *= AS_NUM(a[i]); }
    return NUM_VAL(s);
}
static Value natDiv(int c, Value* a, Env* e) {
    (void)e; double s = (c == 1) ? 1.0 : AS_NUM(a[0]);
    for (int i = (c == 1 ? 0 : 1); i < c; i++) {
        if (!IS_NUM(a[i])) { runtimeError("/ expects numbers"); return NIL_VAL; }
        if (AS_NUM(a[i]) == 0) { runtimeError("division by zero"); return NIL_VAL; }
        s /= AS_NUM(a[i]);
    }
    return NUM_VAL(s);
}
static Value natMod(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2 || !IS_NUM(a[0]) || !IS_NUM(a[1])) { runtimeError("% expects two numbers"); return NIL_VAL; }
    if (AS_NUM(a[1]) == 0) { runtimeError("modulo by zero"); return NIL_VAL; }
    return NUM_VAL(fmod(AS_NUM(a[0]), AS_NUM(a[1])));
}

/* ---------- 比较 ---------- */
static Value natNumEq(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 1; i < c; i++)
        if (!IS_NUM(a[0]) || !IS_NUM(a[i]) || AS_NUM(a[0]) != AS_NUM(a[i]))
            return BOOL_VAL(false);
    return BOOL_VAL(true);
}
static Value natLt(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 1; i < c; i++)
        if (!IS_NUM(a[i-1]) || !IS_NUM(a[i]) || !(AS_NUM(a[i-1]) < AS_NUM(a[i])))
            return BOOL_VAL(false);
    return BOOL_VAL(true);
}
static Value natGt(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 1; i < c; i++)
        if (!IS_NUM(a[i-1]) || !IS_NUM(a[i]) || !(AS_NUM(a[i-1]) > AS_NUM(a[i])))
            return BOOL_VAL(false);
    return BOOL_VAL(true);
}
static Value natLe(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 1; i < c; i++)
        if (!IS_NUM(a[i-1]) || !IS_NUM(a[i]) || !(AS_NUM(a[i-1]) <= AS_NUM(a[i])))
            return BOOL_VAL(false);
    return BOOL_VAL(true);
}
static Value natGe(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 1; i < c; i++)
        if (!IS_NUM(a[i-1]) || !IS_NUM(a[i]) || !(AS_NUM(a[i-1]) >= AS_NUM(a[i])))
            return BOOL_VAL(false);
    return BOOL_VAL(true);
}
static Value natNot(int c, Value* a, Env* e) {
    (void)e; (void)c; return BOOL_VAL(isFalsey(a[0]));
}

/* ---------- 相等性 ---------- */
/* 结构相等：列表/向量递归比较，其余按值 */
static bool valueEqual(Value x, Value y) {
    if (x.type != y.type) return false;
    if (IS_NIL(x) || IS_BOOL(x) || IS_NUM(x)) {
        if (IS_NIL(x)) return IS_NIL(y);
        if (IS_BOOL(x)) return AS_BOOL(x) == AS_BOOL(y);
        return AS_NUM(x) == AS_NUM(y);
    }
    if (IS_OBJ(x)) {
        Obj* ox = AS_OBJ(x); Obj* oy = AS_OBJ(y);
        if (ox->type != oy->type) return false;
        if (ox->type == OBJ_STRING) {
            ObjString* sx = (ObjString*)ox, *sy = (ObjString*)oy;
            return sx->length == sy->length && memcmp(sx->chars, sy->chars, sx->length) == 0;
        }
        if (ox->type == OBJ_LIST) {
            Value cx = x, cy = y;
            while (isList(cx) && isList(cy)) {
                if (!valueEqual(listCar(cx), listCar(cy))) return false;
                cx = listCdr(cx); cy = listCdr(cy);
            }
            return IS_NIL(cx) && IS_NIL(cy);
        }
        if (ox->type == OBJ_VECTOR) {
            ObjVector* vx = (ObjVector*)ox, *vy = (ObjVector*)oy;
            if (vx->count != vy->count) return false;
            for (int i = 0; i < vx->count; i++)
                if (!valueEqual(vx->items[i], vy->items[i])) return false;
            return true;
        }
        return ox == oy;
    }
    return false;
}
static Value natEqual(int c, Value* a, Env* e) {
    (void)e;
    if (c < 2) return BOOL_VAL(true);
    for (int i = 1; i < c; i++) if (!valueEqual(a[0], a[i])) return BOOL_VAL(false);
    return BOOL_VAL(true);
}
/* 同一性（指针相等） */
static Value natEq(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 1; i < c; i++)
        if (!(IS_OBJ(a[0]) && IS_OBJ(a[i]) && AS_OBJ(a[0]) == AS_OBJ(a[i])) &&
            !(IS_NIL(a[0]) && IS_NIL(a[i])) &&
            !(IS_BOOL(a[0]) && IS_BOOL(a[i]) && AS_BOOL(a[0]) == AS_BOOL(a[i])) &&
            !(IS_NUM(a[0]) && IS_NUM(a[i]) && AS_NUM(a[0]) == AS_NUM(a[i])))
            return BOOL_VAL(false);
    return BOOL_VAL(true);
}

/* ---------- 输出 ---------- */
static Value natPrint(int c, Value* a, Env* e) {
    (void)e;
    for (int i = 0; i < c; i++) { if (i) putchar(' '); printValue(a[i], stdout); }
    return NIL_VAL;
}
static Value natPrintln(int c, Value* a, Env* e) {
    (void)e;
    natPrint(c, a, e);
    putchar('\n');
    return NIL_VAL;
}

/* ---------- 列表 / 向量 ---------- */
static Value natCons(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2) { runtimeError("cons expects 2 args"); return NIL_VAL; }
    ObjList* cell = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
    /* a[0]/a[1] 由调用方(argv)持有，apply 返回后会被 release；
       本 cell 要长期持有它们，必须各自 retain 取得所有权。 */
    cell->car = a[0]; retain(a[0]);
    cell->cdr = a[1]; retain(a[1]);
    return OBJ_VAL(cell);
}
static Value natCar(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !isList(a[0]) || IS_NIL(a[0])) { runtimeError("car: not a pair"); return NIL_VAL; }
    Value v = listCar(a[0]); retain(v); return v;
}
static Value natCdr(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !isList(a[0]) || IS_NIL(a[0])) { runtimeError("cdr: not a pair"); return NIL_VAL; }
    Value v = listCdr(a[0]); retain(v); return v;
}
static Value natList(int c, Value* a, Env* e) {
    (void)e;
    Value head = NIL_VAL;
    for (int i = c - 1; i >= 0; i--) {
        ObjList* cell = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
        /* a[i] 由 argv 持有，apply 返回后会被 release；cell 要长期持有 car，
           必须 retain。cdr=head 是链表的后继链节，用「转移所有权」不额外 retain
           （否则尾节点会泄漏）。 */
        cell->car = a[i]; retain(a[i]);
        cell->cdr = head;
        head = OBJ_VAL(cell);
    }
    return head;
}
static Value natLength(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1) { runtimeError("length expects 1 arg"); return NIL_VAL; }
    Value v = a[0];
    if (isList(v)) return NUM_VAL(listLen(v));
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_VECTOR) return NUM_VAL(((ObjVector*)AS_OBJ(v))->count);
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) return NUM_VAL(((ObjString*)AS_OBJ(v))->length);
    runtimeError("length: bad type"); return NIL_VAL;
}
static Value natNth(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2 || !IS_NUM(a[1])) { runtimeError("nth expects (coll index)"); return NIL_VAL; }
    int idx = (int)AS_NUM(a[1]);
    Value v = a[0];
    if (isList(v)) {
        Value cur = listRef(v, idx);
        if (!isList(cur)) { runtimeError("nth: index out of range"); return NIL_VAL; }
        Value r = listCar(cur); retain(r); return r;
    }
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_VECTOR) {
        ObjVector* vec = (ObjVector*)AS_OBJ(v);
        if (idx < 0 || idx >= vec->count) { runtimeError("nth: index out of range"); return NIL_VAL; }
        Value r = vec->items[idx]; retain(r); return r;
    }
    runtimeError("nth: not indexable"); return NIL_VAL;
}
static Value natGet(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2 || !IS_NUM(a[1])) { runtimeError("get expects (coll index)"); return NIL_VAL; }
    int idx = (int)AS_NUM(a[1]);
    Value v = a[0];
    if (isList(v)) {
        Value cur = listRef(v, idx);
        if (!isList(cur)) { runtimeError("get: index out of range"); return NIL_VAL; }
        Value r = listCar(cur); retain(r); return r;
    }
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_VECTOR) {
        ObjVector* vec = (ObjVector*)AS_OBJ(v);
        if (idx < 0 || idx >= vec->count) { runtimeError("get: index out of range"); return NIL_VAL; }
        Value r = vec->items[idx]; retain(r); return r;
    }
    if (IS_OBJ(v) && AS_OBJ(v)->type == OBJ_STRING) {
        ObjString* s = (ObjString*)AS_OBJ(v);
        if (idx < 0 || idx >= s->length) { runtimeError("get: index out of range"); return NIL_VAL; }
        ObjString* ch = internString(s->chars + idx, 1, false);
        retain(OBJ_VAL(ch));
        return OBJ_VAL(ch);
    }
    runtimeError("get: not indexable"); return NIL_VAL;
}
static Value natSet(int c, Value* a, Env* e) {
    (void)e;
    if (c != 3 || !IS_NUM(a[1])) { runtimeError("set expects (vec index val)"); return NIL_VAL; }
    if (!(IS_OBJ(a[0]) && AS_OBJ(a[0])->type == OBJ_VECTOR)) { runtimeError("set: not a vector"); return NIL_VAL; }
    ObjVector* vec = (ObjVector*)AS_OBJ(a[0]);
    int idx = (int)AS_NUM(a[1]);
    if (idx < 0 || idx >= vec->count) { runtimeError("set: index out of range"); return NIL_VAL; }
    release(vec->items[idx]);
    vec->items[idx] = a[2]; retain(a[2]);   /* 向量持有 */
    retain(a[2]);                           /* 返回值持有 */
    return a[2];
}
static Value natVec(int c, Value* a, Env* e) {
    (void)e;
    ObjVector* v = (ObjVector*)allocObj(sizeof(ObjVector), OBJ_VECTOR);
    v->count = c; v->capacity = c;
    v->items = (Value*)malloc(c * sizeof(Value));
    /* a[i] 由 argv 持有，apply 返回后会被 release；向量要长期持有每个元素，
       必须各自 retain 取得所有权（不能只「转移」，否则 vector 内的指针会悬空）。 */
    for (int i = 0; i < c; i++) { v->items[i] = a[i]; retain(a[i]); }
    return OBJ_VAL(v);
}
static Value natStr(int c, Value* a, Env* e) {
    (void)e;
    char** parts = (char**)malloc((c > 0 ? c : 1) * sizeof(char*));
    size_t total = 0;
    for (int i = 0; i < c; i++) {
        parts[i] = valueToCString(a[i]);
        total += strlen(parts[i]);
    }
    char* s = (char*)malloc(total + 1);
    size_t off = 0;
    for (int i = 0; i < c; i++) {
        size_t l = strlen(parts[i]);
        memcpy(s + off, parts[i], l);
        off += l;
        free(parts[i]);
    }
    s[off] = '\0';
    ObjString* str = internString(s, (int)off, false);
    free(s);
    free(parts);
    retain(OBJ_VAL(str));
    return OBJ_VAL(str);
}

/* ---------- 输入 ---------- */
/* input [提示]：从 stdin 读一行（去掉换行），返回字符串。
   可选的第一个参数作为提示（原样输出、不带引号）。 */
static Value natInput(int c, Value* a, Env* e) {
    (void)e;
    if (c >= 1 && IS_OBJ(a[0]) && AS_OBJ(a[0])->type == OBJ_STRING) {
        fputs(((ObjString*)AS_OBJ(a[0]))->chars, stdout);
        fflush(stdout);
    }
    char buf[512];
    if (!fgets(buf, sizeof(buf), stdin)) {
        ObjString* s = internString("", 0, false);
        retain(OBJ_VAL(s));
        return OBJ_VAL(s);
    }
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    ObjString* s = internString(buf, len, false);
    retain(OBJ_VAL(s));
    return OBJ_VAL(s);
}
/* input-int [提示]：从 stdin 读一行并解析为整数，返回数字。
   非数字或空行返回 0。可选的第一个参数作为提示。 */
static Value natInputInt(int c, Value* a, Env* e) {
    (void)e;
    if (c >= 1 && IS_OBJ(a[0]) && AS_OBJ(a[0])->type == OBJ_STRING) {
        fputs(((ObjString*)AS_OBJ(a[0]))->chars, stdout);
        fflush(stdout);
    }
    char buf[512];
    if (!fgets(buf, sizeof(buf), stdin)) return NUM_VAL(0);
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
    char* p = buf;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') return NUM_VAL(0);
    double v = strtol(p, NULL, 10);
    return NUM_VAL(v);
}

/* ---------- 注册 ---------- */
static void defNative(Env* g, const char* name, NativeFn fn) {
    ObjNative* nat = (ObjNative*)allocObj(sizeof(ObjNative), OBJ_NATIVE);
    nat->fn = fn;
    nat->name = (char*)malloc(strlen(name) + 1);
    strcpy(nat->name, name);
    ObjString* s = internString(name, (int)strlen(name), true);
    envDefine(g, s, OBJ_VAL(nat));
}

void registerNatives(Env* g) {
    defNative(g, "+", natAdd);
    defNative(g, "-", natSub);
    defNative(g, "*", natMul);
    defNative(g, "/", natDiv);
    defNative(g, "%", natMod);
    defNative(g, "=", natNumEq);
    defNative(g, "<", natLt);
    defNative(g, ">", natGt);
    defNative(g, "<=", natLe);
    defNative(g, ">=", natGe);
    defNative(g, "not", natNot);
    defNative(g, "equal?", natEqual);
    defNative(g, "eq?", natEq);
    defNative(g, "print", natPrint);
    defNative(g, "println", natPrintln);
    defNative(g, "cons", natCons);
    defNative(g, "car", natCar);
    defNative(g, "cdr", natCdr);
    defNative(g, "list", natList);
    defNative(g, "length", natLength);
    defNative(g, "nth", natNth);
    defNative(g, "get", natGet);
    defNative(g, "set", natSet);
    defNative(g, "vec", natVec);
    defNative(g, "str", natStr);
    defNative(g, "input", natInput);
    defNative(g, "input-int", natInputInt);
}
