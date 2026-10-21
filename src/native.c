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

/* ============================================================
 * ---------- 终端 / 游戏原语 ----------
 * 贪吃蛇这类实时游戏需要四样东西：清屏、延时、不阻塞的键盘、随机数。
 * 糯米原先只有行式输入（input），所以补上这一组。
 * Windows 走 Win32 控制台 API + conio；其他平台用 ANSI 序列兜底。
 * ============================================================ */
#if defined(_WIN32)
#include <windows.h>
#include <conio.h>
#include <time.h>

static HANDLE g_conOut = NULL;
static void conEnsure(void) {
    if (!g_conOut) g_conOut = GetStdHandle(STD_OUTPUT_HANDLE);
}

/* cls：清屏 + 光标归位 + 隐藏光标（隐藏是为防游戏画面闪烁） */
static Value natCls(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    conEnsure();
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(g_conOut, &csbi)) {
        DWORD cells = (DWORD)csbi.dwSize.X * (DWORD)csbi.dwSize.Y;
        COORD home = { 0, 0 };
        DWORD written;
        FillConsoleOutputCharacterA(g_conOut, ' ', cells, home, &written);
        FillConsoleOutputAttribute(g_conOut, csbi.wAttributes, cells, home, &written);
        SetConsoleCursorPosition(g_conOut, home);
    }
    CONSOLE_CURSOR_INFO ci;
    if (GetConsoleCursorInfo(g_conOut, &ci)) { ci.bVisible = FALSE; SetConsoleCursorInfo(g_conOut, &ci); }
    return NIL_VAL;
}

/* show-cursor：恢复光标（游戏退出时必须调，不然光标一直藏着） */
static Value natShowCursor(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    conEnsure();
    CONSOLE_CURSOR_INFO ci;
    if (GetConsoleCursorInfo(g_conOut, &ci)) { ci.bVisible = TRUE; SetConsoleCursorInfo(g_conOut, &ci); }
    return NIL_VAL;
}

/* sleep 毫秒：游戏帧延时 */
static Value natSleep(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("sleep expects milliseconds"); return NIL_VAL; }
    if (AS_NUM(a[0]) < 0) return NIL_VAL;
    Sleep((DWORD)AS_NUM(a[0]));
    return NIL_VAL;
}

/* key：非阻塞读键盘。无按键 → nil。
 * 方向键 → 符号 'up / 'down / 'left / 'right；Esc → 'esc；
 * 普通键 → 该字符的字符串（如 "w"）。方向键在 Windows 控制台是
 * 0xE0/0x00 前缀 + 扫描码两段式，这里合成成符号返回。 */
static Value natKey(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    if (!_kbhit()) return NIL_VAL;
    int ch = _getch();
    if (ch == 0 || ch == 0xE0) {
        int code = _getch();
        const char* nm = NULL;
        switch (code) {
            case 72: nm = "up";    break;
            case 80: nm = "down";  break;
            case 75: nm = "left";  break;
            case 77: nm = "right"; break;
            default: return NIL_VAL;
        }
        ObjString* s = internString(nm, (int)strlen(nm), true);
        retain(OBJ_VAL(s));
        return OBJ_VAL(s);
    }
    if (ch == 27) {
        ObjString* s = internString("esc", 3, true);
        retain(OBJ_VAL(s));
        return OBJ_VAL(s);
    }
    if (ch == '\r' || ch == '\n') ch = ' ';
    char buf[2] = { (char)ch, '\0' };
    ObjString* s = internString(buf, 1, false);
    retain(OBJ_VAL(s));
    return OBJ_VAL(s);
}

/* random n：[0, n) 均匀随机整数。LCG + 时间种子，n<=0 时返回 0。 */
static unsigned long long g_rng = 0;
static Value natRandom(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("random expects a positive integer"); return NIL_VAL; }
    if (!g_rng) {
        g_rng = (unsigned long long)time(NULL);
        g_rng ^= (unsigned long long)(size_t)&g_rng;   /* 地址扰动，避免同秒同种子 */
        if (!g_rng) g_rng = 0x9E3779B97F4A7C15ull;
    }
    g_rng = g_rng * 6364136223846793005ull + 1442695040888963407ull;
    double n = AS_NUM(a[0]);
    if (n <= 0) return NUM_VAL(0);
    return NUM_VAL((double)((g_rng >> 33) % (unsigned long long)n));
}
#else  /* ---- 非 Windows：ANSI 清屏 + nanosleep；无键盘支持（key 恒返回 nil） ---- */
#include <time.h>
static Value natCls(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    fputs("\x1B[2J\x1B[H\x1B[?25l", stdout);
    fflush(stdout);
    return NIL_VAL;
}
static Value natShowCursor(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    fputs("\x1B[?25h", stdout);
    fflush(stdout);
    return NIL_VAL;
}
static Value natSleep(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("sleep expects milliseconds"); return NIL_VAL; }
    struct timespec ts;
    ts.tv_sec = (time_t)(AS_NUM(a[0]) / 1000);
    ts.tv_nsec = (long)((AS_NUM(a[0]) - ts.tv_sec * 1000) * 1000000);
    nanosleep(&ts, NULL);
    return NIL_VAL;
}
static Value natKey(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    return NIL_VAL;   /* 非 Windows 平台暂无非阻塞键盘 */
}
static Value natRandom(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("random expects a positive integer"); return NIL_VAL; }
    if (AS_NUM(a[0]) <= 0) return NUM_VAL(0);
    return NUM_VAL((double)(rand() % (int)AS_NUM(a[0])));
}
#endif

/* ---------- 高阶函数与列表工具 ---------- */
/* 遍历集合（列表或向量），对每个元素调用 fn。
 * listSeqNext 返回第 idx 个元素并把它拷出；集合耗尽时返回 false。 */
static bool collAt(Value coll, int idx, Value* out) {
    if (isList(coll)) {
        Value cur = listRef(coll, idx);
        if (!isList(cur)) return false;
        *out = listCar(cur);
        retain(*out);
        return true;
    }
    if (IS_OBJ(coll) && AS_OBJ(coll)->type == OBJ_VECTOR) {
        ObjVector* v = (ObjVector*)AS_OBJ(coll);
        if (idx < 0 || idx >= v->count) return false;
        *out = v->items[idx];
        retain(*out);
        return true;
    }
    return false;
}
static int collCount(Value coll) {
    if (isList(coll)) return listLen(coll);
    if (IS_OBJ(coll) && AS_OBJ(coll)->type == OBJ_VECTOR) return ((ObjVector*)AS_OBJ(coll))->count;
    return -1;
}
/* 在链表头压入一个元素，返回新的头。
 * 所有权约定：新 cell 自身持有一份（refcount=1，由返回值转交给调用方）；
 * 对 item 和 head 各 retain 一次，使它们同时被新 cell 与调用方各自持有。
 * 调用方随后应release 自己那份 item/head。
 */
static Value prependCell(Value head, Value item) {
    ObjList* cell = (ObjList*)allocObj(sizeof(ObjList), OBJ_LIST);
    cell->car = item; retain(item);
    cell->cdr = head; retain(head);      /* 新 cell 也要持有后继，缺这行会 use-after-free */
    return OBJ_VAL(cell);
}

/* (map fn coll) —— 逐元素映射，返回同形状的新列表 */
static Value natMap(int c, Value* a, Env* e) {
    if (c != 2) { runtimeError("map expects (fn coll)"); return NIL_VAL; }
    int n = collCount(a[1]);
    if (n < 0) { runtimeError("map: second arg must be a list or vector"); return NIL_VAL; }
    Value out = NIL_VAL;
    for (int i = n - 1; i >= 0; i--) {
        Value item;
        if (!collAt(a[1], i, &item)) continue;
        Value arg = item;
        Value r = apply(a[0], &arg, 1, e);
        release(arg);
        Value nh = prependCell(out, r);
        release(out);
        out = nh;
        release(r);
    }
    return out;
}

/* (filter pred coll) —— 保留 pred 为真的元素 */
static Value natFilter(int c, Value* a, Env* e) {
    if (c != 2) { runtimeError("filter expects (pred coll)"); return NIL_VAL; }
    int n = collCount(a[1]);
    if (n < 0) { runtimeError("filter: second arg must be a list or vector"); return NIL_VAL; }
    /* 先正序收集命中的元素，再逆序建链 */
    Value* keep = (Value*)malloc((n > 0 ? n : 1) * sizeof(Value));
    int nk = 0;
    for (int i = 0; i < n; i++) {
        Value item;
        if (!collAt(a[1], i, &item)) continue;
        Value arg = item;
        Value p = apply(a[0], &arg, 1, e);
        release(arg);
        bool yes = !isFalsey(p);
        release(p);
        if (yes) keep[nk++] = item;   /* item 已被 collAt retain */
        release(item);
    }
    Value out = NIL_VAL;
    for (int i = nk - 1; i >= 0; i--) {
        Value nh = prependCell(out, keep[i]);
        release(out);
        out = nh;
        release(keep[i]);
    }
    free(keep);
    return out;
}

/* (reduce fn coll) / (reduce fn init coll) / (reduce fn coll init)
 * 三参时按「哪个参数是集合」来区分 (fn init coll) 与 (fn coll init)。 */
static Value natReduce(int c, Value* a, Env* e) {
    if (c < 2) { runtimeError("reduce expects (fn coll) or (fn init coll)"); return NIL_VAL; }
    if (c > 3) { runtimeError("reduce: too many args"); return NIL_VAL; }
    Value fn = a[0];
    int n;
    Value coll, acc;
    bool haveAcc = false;

    if (c == 2) {
        coll = a[1];
    } else {
        int n1 = collCount(a[1]);
        int n2 = collCount(a[2]);
        /* 恰有一个是集合时，按位置决定：第二个是集合 → (fn coll init)；
         * 否则默认 (fn init coll)。 */
        bool a1isColl = (n1 >= 0);
        bool a2isColl = (n2 >= 0);
        if (a1isColl && !a2isColl)      { coll = a[1]; acc = a[2]; haveAcc = true; }
        else if (!a1isColl && a2isColl) { coll = a[2]; acc = a[1]; haveAcc = true; }
        else {
            runtimeError("reduce: one of the two args must be a list or vector");
            return NIL_VAL;
        }
    }
    n = collCount(coll);
    if (n < 0) { runtimeError("reduce: not a list or vector"); return NIL_VAL; }

    Value cur = acc; retain(cur);
    int start = 0;
    if (!haveAcc) {
        if (n == 0) { release(cur); return NIL_VAL; }
        Value first;
        if (!collAt(coll, 0, &first)) { release(cur); return NIL_VAL; }
        release(cur);
        cur = first;              /* first 已 retain，作为新初值 */
        start = 1;
    }
    for (int i = start; i < n; i++) {
        Value item;
        if (!collAt(coll, i, &item)) break;
        Value args[2];
        args[0] = cur; args[1] = item;
        Value r = apply(fn, args, 2, e);
        release(cur);
        release(item);
        cur = r;                  /* 转移所有权 */
    }
    return cur;
}

/* (range start [end [step]]) —— 返回数字列表
 * (range 5)      => (0 1 2 3 4)
 * (range 2 5)    => (2 3 4)
 * (range 0 10 2) => (0 2 4 6 8) */
static Value natRange(int c, Value* a, Env* e) {
    (void)e;
    double start = 0, end = 0, step = 1;
    if (c == 1) { if (!IS_NUM(a[0])) { runtimeError("range expects numbers"); return NIL_VAL; } end = AS_NUM(a[0]); }
    else if (c == 2) {
        if (!IS_NUM(a[0]) || !IS_NUM(a[1])) { runtimeError("range expects numbers"); return NIL_VAL; }
        start = AS_NUM(a[0]); end = AS_NUM(a[1]);
    } else if (c == 3) {
        if (!IS_NUM(a[0]) || !IS_NUM(a[1]) || !IS_NUM(a[2])) { runtimeError("range expects numbers"); return NIL_VAL; }
        start = AS_NUM(a[0]); end = AS_NUM(a[1]); step = AS_NUM(a[2]);
    } else { runtimeError("range expects 1-3 args"); return NIL_VAL; }
    if (step == 0) { runtimeError("range: step must not be zero"); return NIL_VAL; }
    /* 先正向算出元素个数，再逆序建链 */
    int n = 0;
    if (step > 0) { for (double v = start; v < end; v += step) n++; }
    else          { for (double v = start; v > end; v += step) n++; }
    if (n > 10000000) { runtimeError("range: too many elements"); return NIL_VAL; }
    Value out = NIL_VAL;
    for (int i = n - 1; i >= 0; i--) {
        Value num = NUM_VAL(start + step * (double)i);
        Value nh = prependCell(out, num);
        release(out);
        out = nh;
        release(num);
    }
    return out;
}

/* (null? x) —— 列表/向量是否为空 */
static Value natNull(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1) { runtimeError("null? expects 1 arg"); return NIL_VAL; }
    return BOOL_VAL(IS_NIL(a[0]) || collCount(a[0]) == 0);
}

/* (assoc lst key val) —— 在关联列表中替换/新增
 * 关联表元素形如 (key value)。 */
static Value natAssoc(int c, Value* a, Env* e) {
    if (c != 2 && c != 3) { runtimeError("assoc expects (lst key val)"); return NIL_VAL; }
    if (c == 2) {
        /* 两参形式assoc(lst key)：只查不建，返回找到的那条pair 或 nil */
        for (Value cur = a[0]; isList(cur); cur = listCdr(cur)) {
            Value pair = listCar(cur);
            if (!isList(pair)) continue;              /* 跳过非法条目，不解引用 */
            Value k = listCar(pair);
            if (valueEqual(k, a[1])) { retain(pair); return pair; }
        }
        return NIL_VAL;
    }
    Value head = NIL_VAL;
    bool replaced = false;
    for (Value cur = a[0]; isList(cur); cur = listCdr(cur)) {
        Value pair = listCar(cur);
        if (!isList(pair)) continue;                  /* 非法条目直接跳过 */
        Value k = listCar(pair);
        bool same = valueEqual(k, a[1]);
        if (same && !replaced) {
            Value kv[2]; kv[0] = k; kv[1] = a[2];
            Value np = natList(2, kv, e);
            Value nh = prependCell(head, np);
            release(head);
            head = nh;
            release(np);
            replaced = true;
        } else {
            retain(pair);
            Value nh = prependCell(head, pair);
            release(head);
            head = nh;
            release(pair);
        }
    }
    if (!replaced) {
        Value kv[2]; kv[0] = a[1]; kv[1] = a[2];
        Value np = natList(2, kv, e);
        Value nh = prependCell(head, np);
        release(head);
        head = nh;
        release(np);
    }
    return head;
}

/* (member? x coll) —— x 是否在 coll 中（结构相等） */
static Value natMember(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2) { runtimeError("member? expects (x coll)"); return NIL_VAL; }
    int n = collCount(a[1]);
    if (n < 0) return BOOL_VAL(false);
    for (int i = 0; i < n; i++) {
        Value item;
        if (!collAt(a[1], i, &item)) break;
        bool eq = valueEqual(a[0], item);
        release(item);
        if (eq) return BOOL_VAL(true);
    }
    return BOOL_VAL(false);
}

/* (number? x) / (string? x) / (list? x) / (vector? x) / (function? x) / (bool? x) */
static Value natNumberP(int c, Value* a, Env* e) { (void)e; (void)c; return BOOL_VAL(IS_NUM(a[0])); }
static Value natStringP(int c, Value* a, Env* e) {
    (void)e; (void)c;
    return BOOL_VAL(IS_OBJ(a[0]) && AS_OBJ(a[0])->type == OBJ_STRING && !((ObjString*)AS_OBJ(a[0]))->isSymbol);
}
static Value natListP(int c, Value* a, Env* e) { (void)e; (void)c; return BOOL_VAL(isList(a[0]) || IS_NIL(a[0])); }
static Value natVectorP(int c, Value* a, Env* e) {
    (void)e; (void)c;
    return BOOL_VAL(IS_OBJ(a[0]) && AS_OBJ(a[0])->type == OBJ_VECTOR);
}
static Value natFunctionP(int c, Value* a, Env* e) {
    (void)e; (void)c;
    if (!IS_OBJ(a[0])) return BOOL_VAL(false);
    ObjType t = AS_OBJ(a[0])->type;
    return BOOL_VAL(t == OBJ_FUNCTION || t == OBJ_NATIVE);
}
static Value natBoolP(int c, Value* a, Env* e) { (void)e; (void)c; return BOOL_VAL(IS_BOOL(a[0])); }

/* (abs x) / (floor x) / (ceil x) / (round x) / (sqrt x) / (min ...) / (max ...)
 * (pow a b) / (mod-floor a b) —— 补齐常用数学函数 */
static Value natAbs(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !IS_NUM(a[0])) { runtimeError("abs expects a number"); return NIL_VAL; }
    return NUM_VAL(fabs(AS_NUM(a[0])));
}
static Value natFloor(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !IS_NUM(a[0])) { runtimeError("floor expects a number"); return NIL_VAL; }
    return NUM_VAL(floor(AS_NUM(a[0])));
}
static Value natCeil(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !IS_NUM(a[0])) { runtimeError("ceil expects a number"); return NIL_VAL; }
    return NUM_VAL(ceil(AS_NUM(a[0])));
}
static Value natRound(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !IS_NUM(a[0])) { runtimeError("round expects a number"); return NIL_VAL; }
    return NUM_VAL(round(AS_NUM(a[0])));
}
static Value natSqrt(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1 || !IS_NUM(a[0])) { runtimeError("sqrt expects a number"); return NIL_VAL; }
    if (AS_NUM(a[0]) < 0) { runtimeError("sqrt: negative argument"); return NIL_VAL; }
    return NUM_VAL(sqrt(AS_NUM(a[0])));
}
static Value natPow(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2 || !IS_NUM(a[0]) || !IS_NUM(a[1])) { runtimeError("pow expects two numbers"); return NIL_VAL; }
    return NUM_VAL(pow(AS_NUM(a[0]), AS_NUM(a[1])));
}
static Value natMin(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1) { runtimeError("min expects at least 1 arg"); return NIL_VAL; }
    double m = AS_NUM(a[0]);
    for (int i = 1; i < c; i++) { if (!IS_NUM(a[i])) { runtimeError("min expects numbers"); return NIL_VAL; } if (AS_NUM(a[i]) < m) m = AS_NUM(a[i]); }
    return NUM_VAL(m);
}
static Value natMax(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1) { runtimeError("max expects at least 1 arg"); return NIL_VAL; }
    double m = AS_NUM(a[0]);
    for (int i = 1; i < c; i++) { if (!IS_NUM(a[i])) { runtimeError("max expects numbers"); return NIL_VAL; } if (AS_NUM(a[i]) > m) m = AS_NUM(a[i]); }
    return NUM_VAL(m);
}

/* (last coll) / (butlast coll) / (append a b) / (reverse coll) */
static Value natLast(int c, Value* a, Env* e) {
    (void)e;
    if (c != 1) { runtimeError("last expects 1 arg"); return NIL_VAL; }
    int n = collCount(a[0]);
    if (n <= 0) { runtimeError("last expects a non-empty list"); return NIL_VAL; }
    Value item;
    if (!collAt(a[0], n - 1, &item)) {
        runtimeError("last: bad list");
        return NIL_VAL;
    }
    return item;      /* collAt 已 retain，转移所有权给调用方 */
}
static Value natReverse(int c, Value* a, Env* e) {
    (void)e;
    int n = collCount(a[0]);
    if (n < 0) { runtimeError("reverse expects a list or vector"); return NIL_VAL; }
    Value out = NIL_VAL;
    for (int i = n - 1; i >= 0; i--) {
        Value item;
        if (!collAt(a[0], i, &item)) continue;
        Value nh = prependCell(out, item);
        release(out);
        out = nh;
        release(item);
    }
    return out;
}
static Value natAppend(int c, Value* a, Env* e) {
    (void)e;
    if (c != 2) { runtimeError("append expects (a b)"); return NIL_VAL; }
    int na = collCount(a[0]), nb = collCount(a[1]);
    if (na < 0 || nb < 0) { runtimeError("append expects lists or vectors"); return NIL_VAL; }
    Value out = NIL_VAL;
    for (int i = nb - 1; i >= 0; i--) {
        Value item;
        if (collAt(a[1], i, &item)) {
            Value nh = prependCell(out, item);
            release(out); out = nh; release(item);
        }
    }
    for (int i = na - 1; i >= 0; i--) {
        Value item;
        if (collAt(a[0], i, &item)) {
            Value nh = prependCell(out, item);
            release(out); out = nh; release(item);
        }
    }
    return out;
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

    /* 终端 / 游戏原语（cls / show-cursor / sleep / key / random） */
    defNative(g, "cls", natCls);
    defNative(g, "show-cursor", natShowCursor);
    defNative(g, "sleep", natSleep);
    defNative(g, "key", natKey);
    defNative(g, "random", natRandom);

    /* 高阶函数与列表工具 */
    defNative(g, "map", natMap);
    defNative(g, "filter", natFilter);
    defNative(g, "reduce", natReduce);
    defNative(g, "range", natRange);
    defNative(g, "null?", natNull);
    defNative(g, "assoc", natAssoc);
    defNative(g, "member?", natMember);

    /* 类型判断 */
    defNative(g, "number?", natNumberP);
    defNative(g, "string?", natStringP);
    defNative(g, "list?", natListP);
    defNative(g, "vector?", natVectorP);
    defNative(g, "function?", natFunctionP);
    defNative(g, "bool?", natBoolP);

    /* 数学函数 */
    defNative(g, "abs", natAbs);
    defNative(g, "floor", natFloor);
    defNative(g, "ceil", natCeil);
    defNative(g, "round", natRound);
    defNative(g, "sqrt", natSqrt);
    defNative(g, "pow", natPow);
    defNative(g, "min", natMin);
    defNative(g, "max", natMax);

    /* 序列工具 */
    defNative(g, "last", natLast);
    defNative(g, "reverse", natReverse);
    defNative(g, "append", natAppend);
}
