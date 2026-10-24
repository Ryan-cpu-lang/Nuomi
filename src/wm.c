// SPDX-License-Identifier: GPL-2.0
/*
 * wm.c — Nuomi 窗口原语（Novus-WM / IPC 客户端侧）
 *
 * 定位：liam 定调 Nuomi 为 novus 上的**用户级 GUI 应用语言**，窗口能力走
 *       Novus-WM 协议（见 include/novus_wm.h 与规范 V1.0）。
 *
 * 两套后端：
 *   ① 宿主（Windows/Linux，无 novus 内核）：内存"假 WM"顶替——记录窗口、
 *      把消息摘要打印到 stderr，event 恒返回 nil。用于在宿主上跑通 Nuomi
 *      窗口 API 与脚本写法（本里程碑目标）。
 *   ② 真机（novus）：走内核 IPC——ipc_lookup("nwm") 取端口，
 *      ipc_send(port, type|flags<<16, payload, len) 发请求，ipc_recv 收应答/事件。
 *      NIPC 头由内核填，客户端只构造 type + 载荷（见 novus_wm.h）。
 *      ⬜ 待接：真机 IPC 需 codegen/nucutil 支持 ipc syscall 发射，下一步做。
 *
 * 本文件先落地 ①（宿主原型），② 以注释/占位标出。
 */
#include "nuomi.h"
#include "novus_wm.h"
#include <string.h>

/* ====================== 宿主假 WM ====================== */
#define HOST_WM_MAX_WIN 16
typedef struct {
    int      used;
    uint32_t win;
    int      x, y, w, h;
    char     title[64];
} HostWin;
static HostWin   g_host_wm[HOST_WM_MAX_WIN];
static uint32_t  g_host_wm_next = 1;
static int       g_wm_trace = 1;   /* 默认把消息摘要打到 stderr，便于原型期观察 */

static HostWin* hostwm_find(uint32_t win) {
    for (int i = 0; i < HOST_WM_MAX_WIN; i++)
        if (g_host_wm[i].used && g_host_wm[i].win == win) return &g_host_wm[i];
    return NULL;
}
static int argInt(Value v, int dflt) { return IS_NUM(v) ? (int)AS_NUM(v) : dflt; }

/* ====================== 原语 ====================== */

/* (window-create x y w h [title]) -> 窗口号 */
static Value natWinCreate(int c, Value* a, Env* e) {
    (void)e;
    if (c < 4 || !IS_NUM(a[0]) || !IS_NUM(a[1]) || !IS_NUM(a[2]) || !IS_NUM(a[3])) {
        runtimeError("window-create expects (x y w h [title])");
        return NIL_VAL;
    }
    int slot = -1;
    for (int i = 0; i < HOST_WM_MAX_WIN; i++) if (!g_host_wm[i].used) { slot = i; break; }
    if (slot < 0) { runtimeError("window-create: too many windows"); return NIL_VAL; }
    uint32_t win = g_host_wm_next++;
    HostWin* hw = &g_host_wm[slot];
    hw->used = 1; hw->win = win;
    hw->x = (int)AS_NUM(a[0]); hw->y = (int)AS_NUM(a[1]);
    hw->w = (int)AS_NUM(a[2]); hw->h = (int)AS_NUM(a[3]);
    hw->title[0] = 0;
    if (c >= 5 && IS_OBJ(a[4])) {
        char* t = valueToCString(a[4]);
        if (t) { strncpy(hw->title, t, sizeof hw->title - 1); hw->title[sizeof hw->title - 1] = 0; free(t); }
    }
    if (g_wm_trace)
        fprintf(stderr, "[wm-host] ipc_lookup(\"nwm\"); CREATE win=%u %dx%d@(%d,%d) title=\"%s\"\n",
                win, hw->w, hw->h, hw->x, hw->y, hw->title);
    return NUM_VAL((double)win);
}

/* (window-shape win op x y w h color [radius]) */
static Value natWinShape(int c, Value* a, Env* e) {
    (void)e;
    if (c < 7 || !IS_NUM(a[0]) || !IS_NUM(a[1])) {
        runtimeError("window-shape expects (win op x y w h color [radius])");
        return NIL_VAL;
    }
    uint32_t win = (uint32_t)AS_NUM(a[0]);
    HostWin* hw = hostwm_find(win);
    if (!hw) { runtimeError("window-shape: unknown window"); return NIL_VAL; }
    int op = (int)AS_NUM(a[1]);
    int x = argInt(a[2], 0), y = argInt(a[3], 0);
    int w = argInt(a[4], 0), h = argInt(a[5], 0);
    uint32_t color = IS_NUM(a[6]) ? (uint32_t)AS_NUM(a[6]) : 0;
    int radius = (c >= 8 && IS_NUM(a[7])) ? (int)AS_NUM(a[7]) : 0;
    if (g_wm_trace)
        fprintf(stderr, "[wm-host] SHAPE win=%u op=%d %dx%d@(%d,%d) color=#%06X radius=%d\n",
                win, op, w, h, x, y, color, radius);
    /* 真机：构造 nwm_shape_t 载荷 -> ipc_send(WM_PORT, NWM_SHAPE, &payload, len) */
    return NUM_VAL((double)win);
}

/* (window-fill win x y w h color) = shape op=FILL 的便捷封装 */
static Value natWinFill(int c, Value* a, Env* e) {
    if (c < 5 || !IS_NUM(a[0])) { runtimeError("window-fill expects (win x y w h color)"); return NIL_VAL; }
    Value args[7];
    args[0] = a[0];
    args[1] = NUM_VAL(NWM_OP_FILL);
    args[2] = a[1]; args[3] = a[2]; args[4] = a[3];
    args[5] = (c >= 5) ? a[4] : NUM_VAL(0);
    args[6] = (c >= 6 && IS_NUM(a[5])) ? a[5] : NUM_VAL(0);
    return natWinShape(7, args, e);
}

/* (window-text win x y scale "text") */
static Value natWinText(int c, Value* a, Env* e) {
    (void)e;
    if (c < 5 || !IS_NUM(a[0]) || !IS_OBJ(a[4])) {
        runtimeError("window-text expects (win x y scale \"text\")");
        return NIL_VAL;
    }
    uint32_t win = (uint32_t)AS_NUM(a[0]);
    HostWin* hw = hostwm_find(win);
    if (!hw) { runtimeError("window-text: unknown window"); return NIL_VAL; }
    char* s = valueToCString(a[4]);
    if (!s) { runtimeError("window-text: bad string"); return NIL_VAL; }
    if (g_wm_trace)
        fprintf(stderr, "[wm-host] TEXT win=%u @(%d,%d) scale=%d \"%s\"\n",
                win, (int)AS_NUM(a[1]), (int)AS_NUM(a[2]), (int)AS_NUM(a[3]), s);
    free(s);
    return NUM_VAL((double)win);
}

/* (window-clear win) — 清空显示列表（开一新帧） */
static Value natWinClear(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("window-clear expects (win)"); return NIL_VAL; }
    if (!hostwm_find((uint32_t)AS_NUM(a[0]))) { runtimeError("window-clear: unknown window"); return NIL_VAL; }
    if (g_wm_trace) fprintf(stderr, "[wm-host] CLEAR win=%u\n", (uint32_t)AS_NUM(a[0]));
    return NIL_VAL;
}

/* (window-invalidate win) — 帧尾重绘 */
static Value natWinInvalidate(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("window-invalidate expects (win)"); return NIL_VAL; }
    if (!hostwm_find((uint32_t)AS_NUM(a[0]))) { runtimeError("window-invalidate: unknown window"); return NIL_VAL; }
    if (g_wm_trace) fprintf(stderr, "[wm-host] INVALIDATE win=%u (redraw frame)\n", (uint32_t)AS_NUM(a[0]));
    return NIL_VAL;
}

/* (window-move win x y) */
static Value natWinMove(int c, Value* a, Env* e) {
    (void)e;
    if (c < 3 || !IS_NUM(a[0])) { runtimeError("window-move expects (win x y)"); return NIL_VAL; }
    HostWin* hw = hostwm_find((uint32_t)AS_NUM(a[0]));
    if (!hw) { runtimeError("window-move: unknown window"); return NIL_VAL; }
    hw->x = (int)AS_NUM(a[1]); hw->y = (int)AS_NUM(a[2]);
    if (g_wm_trace) fprintf(stderr, "[wm-host] MOVE win=%u -> (%d,%d)\n", hw->win, hw->x, hw->y);
    return NIL_VAL;
}

/* (window-title win "title") */
static Value natWinTitle(int c, Value* a, Env* e) {
    (void)e;
    if (c < 2 || !IS_NUM(a[0]) || !IS_OBJ(a[1])) { runtimeError("window-title expects (win \"title\")"); return NIL_VAL; }
    HostWin* hw = hostwm_find((uint32_t)AS_NUM(a[0]));
    if (!hw) { runtimeError("window-title: unknown window"); return NIL_VAL; }
    char* t = valueToCString(a[1]);
    if (t) { strncpy(hw->title, t, sizeof hw->title - 1); hw->title[sizeof hw->title - 1] = 0; free(t); }
    if (g_wm_trace) fprintf(stderr, "[wm-host] TITLE win=%u \"%s\"\n", hw->win, hw->title);
    return NIL_VAL;
}

/* (window-close win) */
static Value natWinClose(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("window-close expects (win)"); return NIL_VAL; }
    HostWin* hw = hostwm_find((uint32_t)AS_NUM(a[0]));
    if (!hw) { runtimeError("window-close: unknown window"); return NIL_VAL; }
    if (g_wm_trace) fprintf(stderr, "[wm-host] DESTROY win=%u\n", hw->win);
    hw->used = 0;
    return NIL_VAL;
}

/* (window-event [block]) — 宿主无真实事件源，恒返回 nil。
 * 真机：ipc_recv 收 EV_MOUSE/EV_KEY/EV_CLOSE，解析成 Nuomi 关联表/向量。 */
static Value natWinEvent(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    /* TODO(真机): 解析 ipc_recv 得到的事件为 Nuomi 值 */
    return NIL_VAL;
}

/* (window-trace 0|1) — 开关宿主消息摘要打印 */
static Value natWinTrace(int c, Value* a, Env* e) {
    (void)e;
    if (c < 1 || !IS_NUM(a[0])) { runtimeError("window-trace expects (0|1)"); return NIL_VAL; }
    g_wm_trace = AS_NUM(a[0]) != 0;
    return NIL_VAL;
}

/* ====================== 注册 ====================== */
void registerWmNatives(Env* g) {
    defNative(g, "window-create",     natWinCreate);
    defNative(g, "window-shape",      natWinShape);
    defNative(g, "window-fill",       natWinFill);
    defNative(g, "window-text",       natWinText);
    defNative(g, "window-clear",      natWinClear);
    defNative(g, "window-invalidate", natWinInvalidate);
    defNative(g, "window-move",       natWinMove);
    defNative(g, "window-title",      natWinTitle);
    defNative(g, "window-close",      natWinClose);
    defNative(g, "window-event",      natWinEvent);
    defNative(g, "window-trace",      natWinTrace);
}
