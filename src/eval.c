// SPDX-License-Identifier: GPL-2.0
/*
 * eval.c — 求值器
 * 树遍历解释执行；特殊形式：quote/if/while/define/set!/lambda/begin/and/or
 */
#include "mochi.h"

bool isFalsey(Value v) {
    return IS_NIL(v) || (IS_BOOL(v) && !AS_BOOL(v));
}

void runtimeError(const char* msg) {
    fprintf(stderr, "Runtime error: %s\n", msg);
}

/* (quote X) —— 原样返回，不求值 */
static Value evalQuote(Value args) {
    Value x = listCar(args);
    retain(x);
    return x;
}

/* (if cond then [else]) */
static Value evalIf(Value args, Env* env) {
    Value cond = evalValue(listCar(args), env);
    bool t = !isFalsey(cond);
    release(cond);
    if (t) return evalValue(listCar(listCdr(args)), env);
    Value rest = listCdr(listCdr(args));
    if (!IS_NIL(rest)) return evalValue(listCar(rest), env);
    return NIL_VAL;
}

/* (while cond body...) —— 返回最后一次循环体的值（无循环返回 nil） */
static Value evalWhile(Value args, Env* env) {
    Value condForm = listCar(args);
    Value bodyForms = listCdr(args);
    Value result = NIL_VAL;
    for (;;) {
        Value c = evalValue(condForm, env);
        bool t = !isFalsey(c);
        release(c);
        if (!t) break;
        Value cur = bodyForms;
        Value last = NIL_VAL;
        bool first = true;
        while (isList(cur)) {
            Value f = listCar(cur);
            Value v = evalValue(f, env);
            if (!first) release(last);
            last = v;
            first = false;
            cur = listCdr(cur);
        }
        release(result);
        result = last;
    }
    return result;
}

/* (define name expr) 或 (define (fname params) body...) */
static Value evalDefine(Value args, Env* env) {
    Value target = listCar(args);
    if (IS_OBJ(target) && AS_OBJ(target)->type == OBJ_STRING) {
        ObjString* name = (ObjString*)AS_OBJ(target);
        Value val = evalValue(listCar(listCdr(args)), env);
#ifdef MOCHI_DEBUG
        fprintf(stderr, "  [define] %s -> val seq=%lu objtype=%d (0=list,3=fn,5=native)\n",
                name->chars,
                (IS_OBJ(val) ? AS_OBJ(val)->dbgSeq : 0),
                (IS_OBJ(val) ? (int)AS_OBJ(val)->type : -1));
#endif
        envDefine(env, name, val);
        return val;            /* 转移所有权；env 另持一份 */
    }
    if (isList(target)) {
        Value fnameV = listCar(target);
        if (!(IS_OBJ(fnameV) && AS_OBJ(fnameV)->type == OBJ_STRING)) {
            runtimeError("define: bad function name"); return NIL_VAL;
        }
        ObjString* fname = (ObjString*)AS_OBJ(fnameV);
        Value paramList = listCdr(target);
        Value body = listCdr(args);
        ObjFunction* f = makeClosure(fname, paramList, body, env);
        envDefine(env, fname, OBJ_VAL(f));
        return OBJ_VAL(f);
    }
    runtimeError("define: bad form"); return NIL_VAL;
}

/* (set! name expr) */
static Value evalSet(Value args, Env* env) {
    Value nameV = listCar(args);
    if (!(IS_OBJ(nameV) && AS_OBJ(nameV)->type == OBJ_STRING)) {
        runtimeError("set!: not a symbol"); return NIL_VAL;
    }
    ObjString* name = (ObjString*)AS_OBJ(nameV);
    Value* slot = envFind(env, name);
    if (!slot) { runtimeError("set!: unbound variable"); return NIL_VAL; }
    Value val = evalValue(listCar(listCdr(args)), env);
    /* 关键：先独立持有新值，再释放旧值。否则当新值是旧值的子结构
       （如 (set! xs (cdr xs))）时，释放旧值会级联释放掉新值本身。 */
    retain(val);
    release(*slot);        /* 释放旧值（可能级联，但新值已被独立持有） */
    *slot = val;           /* 存入环境槽位 */
    release(val);          /* 放下我们的临时持有；环境槽位仍持有新值 */
    return NIL_VAL;        /* 赋值结果返回 nil，避免调用方释放破坏环境持有 */
}

/* (lambda (params) body...) */
static Value evalLambda(Value args, Env* env) {
    Value paramList = listCar(args);
    Value body = listCdr(args);
    ObjString* nm = internString("<lambda>", 8, true);
    ObjFunction* f = makeClosure(nm, paramList, body, env);
    return OBJ_VAL(f);
}

/* (begin body...) */
static Value evalBegin(Value args, Env* env) {
    Value cur = args;
    Value result = NIL_VAL;
    bool first = true;
    while (isList(cur)) {
        Value v = evalValue(listCar(cur), env);
        if (!first) release(result);
        result = v;
        first = false;
        cur = listCdr(cur);
    }
    return result;
}

/* (and a b ...) / (or a b ...) —— 短路求值 */
static Value evalAnd(Value args, Env* env) {
    Value cur = args;
    Value result = BOOL_VAL(true);
    bool first = true;
    while (isList(cur)) {
        Value v = evalValue(listCar(cur), env);
        if (first) { release(result); result = v; first = false; }
        else { release(result); result = v; }
        if (isFalsey(v)) return result;   /* 遇到假值直接返回（已持有所有权） */
        cur = listCdr(cur);
    }
    return result;
}
static Value evalOr(Value args, Env* env) {
    Value cur = args;
    Value result = NIL_VAL;
    bool first = true;
    while (isList(cur)) {
        Value v = evalValue(listCar(cur), env);
        if (first) { release(result); result = v; first = false; }
        else { release(result); result = v; }
        if (!isFalsey(v)) return result;  /* 遇到真值直接返回 */
        cur = listCdr(cur);
    }
    return result;
}

/* 求一个“调用”节点 (列表) */
static Value evalListNode(Value listv, Env* env) {
    if (IS_NIL(listv)) return NIL_VAL;
    Value head = listCar(listv);

    /* 特殊形式：head 为符号时优先匹配 */
    if (IS_OBJ(head) && AS_OBJ(head)->type == OBJ_STRING &&
        ((ObjString*)AS_OBJ(head))->isSymbol) {
        ObjString* sym = (ObjString*)AS_OBJ(head);
        Value args = listCdr(listv);
        if (strcmp(sym->chars, "quote") == 0)  return evalQuote(args);
        if (strcmp(sym->chars, "if") == 0)     return evalIf(args, env);
        if (strcmp(sym->chars, "while") == 0)  return evalWhile(args, env);
        if (strcmp(sym->chars, "define") == 0) return evalDefine(args, env);
        if (strcmp(sym->chars, "set!") == 0)   return evalSet(args, env);
        if (strcmp(sym->chars, "lambda") == 0) return evalLambda(args, env);
        if (strcmp(sym->chars, "begin") == 0)  return evalBegin(args, env);
        if (strcmp(sym->chars, "and") == 0)    return evalAnd(args, env);
        if (strcmp(sym->chars, "or") == 0)     return evalOr(args, env);
    }

    /* 普通调用：先求被调用者 */
    Value callee;
    if (IS_OBJ(head) && AS_OBJ(head)->type == OBJ_STRING &&
        ((ObjString*)AS_OBJ(head))->isSymbol) {
        Value* slot = envFind(env, (ObjString*)AS_OBJ(head));
        if (!slot) { runtimeError("unbound variable"); return NIL_VAL; }
        callee = *slot; retain(callee);
    } else {
        callee = evalValue(head, env);
    }

    int argc = listLen(listCdr(listv));
    Value* argv = (Value*)malloc(argc * sizeof(Value));
    Value cur = listCdr(listv);
    int i = 0;
    while (isList(cur)) {
        argv[i] = evalValue(listCar(cur), env);
        i++;
        cur = listCdr(cur);
    }

    Value result = apply(callee, argv, argc, env);

    g_dbg_phase = "evalListNode-release-callee";
    release(callee);
    for (int k = 0; k < argc; k++) {
        g_dbg_phase = "evalListNode-release-argv";
        release(argv[k]);
    }
    g_dbg_phase = "evalListNode-free-argv";
    free(argv);
    g_dbg_phase = "evalListNode-return";
    return result;
}

Value evalValue(Value node, Env* env) {
#ifdef MOCHI_DEBUG
    if (IS_OBJ(node)) {
        Obj* o = AS_OBJ(node);
        if (o->type == OBJ_LIST) {
            Value h = listCar(node);
            const char* hn = (IS_OBJ(h) && AS_OBJ(h)->type == OBJ_STRING) ? ((ObjString*)AS_OBJ(h))->chars : "?";
            fprintf(stderr, "  [eval] LIST head=%s seq=%lu\n", hn, o->dbgSeq);
        } else {
            fprintf(stderr, "  [eval] OBJ type=%d seq=%lu\n", (int)o->type, o->dbgSeq);
        }
    } else {
        fprintf(stderr, "  [eval] scalar type=%d\n", (int)node.type);
    }
#endif
    if (IS_NIL(node) || IS_BOOL(node) || IS_NUM(node)) return node;
    if (!IS_OBJ(node)) return node;
    Obj* o = AS_OBJ(node);
    switch (o->type) {
        case OBJ_STRING: {
            ObjString* s = (ObjString*)o;
            if (s->isSymbol) {
                Value* slot = envFind(env, s);
                if (!slot) { runtimeError("unbound variable"); return NIL_VAL; }
                Value v = *slot; retain(v);
                return v;
            }
            retain(node); return node;     /* 字符串字面量自求值 */
        }
        case OBJ_LIST:    return evalListNode(node, env);
        case OBJ_VECTOR: {
            ObjVector* vec = (ObjVector*)o;
            ObjVector* nv = (ObjVector*)allocObj(sizeof(ObjVector), OBJ_VECTOR);
            nv->count = vec->count; nv->capacity = vec->count;
            nv->items = (Value*)malloc(nv->count * sizeof(Value));
            for (int i = 0; i < vec->count; i++)
                nv->items[i] = evalValue(vec->items[i], env); /* 转移所有权 */
            return OBJ_VAL(nv);
        }
        case OBJ_FUNCTION:
        case OBJ_NATIVE:
        case OBJ_ENV:
            retain(node); return node;     /* 这些对象自求值 */
        default: break;
    }
    return node;
}

ObjFunction* makeClosure(ObjString* name, Value paramList, Value body, Env* env) {
    ObjFunction* f = (ObjFunction*)allocObj(sizeof(ObjFunction), OBJ_FUNCTION);
    f->name = name; retain(OBJ_VAL(name));
    int arity = 0;
    for (Value p = paramList; isList(p); p = listCdr(p)) arity++;
    f->arity = arity;
    f->params = (ObjString**)malloc(arity * sizeof(ObjString*));
    int i = 0;
    for (Value p = paramList; isList(p); p = listCdr(p)) {
        Value pv = listCar(p);
        if (!(IS_OBJ(pv) && AS_OBJ(pv)->type == OBJ_STRING)) {
            runtimeError("closure: parameter is not a symbol");
        }
        f->params[i] = (ObjString*)AS_OBJ(pv);
        retain(OBJ_VAL(f->params[i]));
        i++;
    }
    f->body = body; retain(body);
    f->env = env; retain(OBJ_VAL(env));
    return f;
}

/* 调用函数或原生函数 */
Value apply(Value callee, Value* argv, int argc, Env* env) {
    (void)env;
#ifdef MOCHI_DEBUG
    if (IS_OBJ(callee)) {
        Obj* co = AS_OBJ(callee);
        const char* nm = "?";
        if (co->type == OBJ_NATIVE) nm = ((ObjNative*)co)->name;
        else if (co->type == OBJ_FUNCTION) nm = ((ObjFunction*)co)->name ? ((ObjFunction*)co)->name->chars : "<fn>";
        fprintf(stderr, "  [apply] callee type=%d name=%s argc=%d\n", (int)co->type, nm, argc);
    } else {
        fprintf(stderr, "  [apply] callee scalar type=%d\n", (int)callee.type);
    }
#endif
    if (IS_OBJ(callee) && AS_OBJ(callee)->type == OBJ_NATIVE) {
        Value r = ((ObjNative*)AS_OBJ(callee))->fn(argc, argv, env);
#ifdef MOCHI_DEBUG
        fprintf(stderr, "  [apply] native returned objtype=%d (0=list)\n", IS_OBJ(r) ? (int)AS_OBJ(r)->type : -1);
#endif
        return r;
    }
    if (IS_OBJ(callee) && AS_OBJ(callee)->type == OBJ_FUNCTION) {
        ObjFunction* fn = (ObjFunction*)AS_OBJ(callee);
        if (argc != fn->arity) {
            runtimeError("arity mismatch"); return NIL_VAL;
        }
        g_dbg_phase = "apply-func-newEnv";
        Env* callEnv = newEnv(fn->env);
        g_dbg_phase = "apply-func-envDefine";
        for (int i = 0; i < argc; i++) envDefine(callEnv, fn->params[i], argv[i]);
        Value result = NIL_VAL;
        Value cur = fn->body;
        bool first = true;
        while (isList(cur)) {
            g_dbg_phase = "apply-func-evalBody";
            Value v = evalValue(listCar(cur), callEnv);
            if (!first) release(result);
            result = v;
            first = false;
            cur = listCdr(cur);
        }
        g_dbg_phase = "apply-func-release-env";
        release(OBJ_VAL(callEnv));
#ifdef MOCHI_DEBUG
        fprintf(stderr, "  [apply] func returned type=%d\n", (int)result.type);
#endif
        return result;
    }
    runtimeError("not a callable"); return NIL_VAL;
}
