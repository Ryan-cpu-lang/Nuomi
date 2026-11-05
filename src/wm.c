// SPDX-License-Identifier: GPL-2.0
/*
 * wm.c — Nuomi 窗口原语（Novus-WM / IPC 客户端侧）
 *
 * 定位：Nuomi 为 novus 上的用户级 GUI 应用语言，窗口能力最终走 Novus-WM 协议
 *       （include/novus_wm.h + 规范 V1.0）。
 *
 * 三套后端：
 *   ① Windows（#ifdef _WIN32）：**Win32 真窗口**——CreateWindowExW 建窗、GDI 双缓冲
 *      绘制（fill/frame/circle/round/text）、消息泵嵌进 window-event、方向键映射符号。
 *      窗口主体文字走 TextOutW，标题栏走 CreateWindowExW/SetWindowTextW，
 *      统一用 utf8_to_wcs() 把 UTF-8 转 wchar_t，中文可正常显示。
 *      消息泵仍是 PeekMessageA/DispatchMessageA，WM_CHAR 的字节语义不变。
 *   ② 其它宿主（#else）：内存"假 WM"，消息摘要打 stderr，event 恒 nil。
 *   ③ 真机 novus（⬜ 待接）：内核 ipc_lookup("nwm") + ipc_send/ipc_recv，NIPC 头由
 *      内核填，客户端只构造 type+载荷。需 codegen/nucutil 发射 ipc syscall。
 */
#include "nuomi.h"
#include "novus_wm.h"
#include <string.h>
#include <stdlib.h>

static int g_wm_trace = 0;

#ifdef _WIN32
/* ====================== Win32 真窗口后端（A 版 API） ====================== */
#include <windows.h>

#define W32_MAX   16
#define W32_KQ    256
#define W32_CQ    64
#define W32_DL0   64

/* TCC 自带 winnls.h 不全，没有 CP_UTF8 / MultiByteToWideChar 声明，
   这里自带纯算法版 UTF-8 <-> UTF-16 转换，避免依赖系统函数。BMP 内字符直接映射；
   超出 BMP（emoji 等）用 '?' 占位（启动器界面用不到代理对）。 */
static int utf8_to_wcs(const char* s, wchar_t* out, int outcap) {
    int wi = 0;
    while (*s && wi < outcap - 1) {
        unsigned char c = (unsigned char)*s; unsigned int cp;
        if (c < 0x80) { cp = c; s += 1; }
        else if ((c & 0xE0) == 0xC0) { cp = ((c & 0x1F) << 6) | ((unsigned char)s[1] & 0x3F); s += 2; }
        else if ((c & 0xF0) == 0xE0) { cp = ((c & 0x0F) << 12) | (((unsigned char)s[1] & 0x3F) << 6) | ((unsigned char)s[2] & 0x3F); s += 3; }
        else if ((c & 0xF8) == 0xF0) { cp = ((c & 0x07) << 18) | (((unsigned char)s[1] & 0x3F) << 12) | (((unsigned char)s[2] & 0x3F) << 6) | ((unsigned char)s[3] & 0x3F); s += 4; }
        else { s += 1; continue; }
        out[wi++] = (cp <= 0xFFFF) ? (wchar_t)cp : (wchar_t)'?';
    }
    out[wi] = 0; return wi;
}
static int wunit_to_utf8(wchar_t ch, char* out) {
    unsigned int cp = (unsigned int)ch;
    if (cp < 0x80) { out[0] = (char)cp; return 1; }
    else if (cp < 0x800) { out[0] = (char)(0xC0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 0x3F)); return 2; }
    else { out[0] = (char)(0xE0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); out[2] = (char)(0x80 | (cp & 0x3F)); return 3; }
}

typedef struct {
    int op, x, y, w, h; uint32_t color; int radius;
    int is_text; int tx, ty, scale; char text[96];
} W32Cmd;

typedef struct {
    int used; uint32_t id; HWND hwnd; int w, h;
    W32Cmd* dl; int dn, dcap;
    int keyq[W32_KQ]; int kqh, kqt;
    int cxq[W32_CQ]; int cyq[W32_CQ]; int cqh, cqt;   /* 鼠标左键点击队列(x,y) */
    wchar_t charq[W32_CQ]; int hqh, hqt;             /* 字符输入队列(UTF-16) */
    int want_close;
} W32Win;

static W32Win   g_w32[W32_MAX];
static uint32_t g_w32_next = 1;
static uint32_t g_w32_cur  = 0;
static int      g_w32_class = 0;

static void w32_pump(void) {
    MSG m;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
}
static W32Win* w32_by_hwnd(HWND h) { for (int i=0;i<W32_MAX;i++) if (g_w32[i].used && g_w32[i].hwnd==h) return &g_w32[i]; return NULL; }
static W32Win* w32_by_id(uint32_t id){ for (int i=0;i<W32_MAX;i++) if (g_w32[i].used && g_w32[i].id==id) return &g_w32[i]; return NULL; }
static void w32_pushkey(W32Win* W, int vk){ int n=(W->kqt+1)%W32_KQ; if(n!=W->kqh){ W->keyq[W->kqt]=vk; W->kqt=n; } }
static int  w32_popkey(W32Win* W){ if(W->kqh==W->kqt) return 0; int v=W->keyq[W->kqh]; W->kqh=(W->kqh+1)%W32_KQ; return v; }

static LRESULT CALLBACK w32_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        LPCREATESTRUCT cs = (LPCREATESTRUCT)lp;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_KEYDOWN: {
        W32Win* W = w32_by_hwnd(hwnd);
        if (W) w32_pushkey(W, (int)wp);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        W32Win* W = w32_by_hwnd(hwnd);
        if (W) {
            int x = (int)(short)LOWORD(lp);
            int y = (int)(short)HIWORD(lp);
            int n = (W->cqt + 1) % W32_CQ;
            if (n != W->cqh) { W->cxq[W->cqt] = x; W->cyq[W->cqt] = y; W->cqt = n; }
        }
        return 0;
    }
    case WM_CHAR: {
        W32Win* W = w32_by_hwnd(hwnd);
        if (W && wp >= 0x20) {   /* 跳过控制字符；退格由 WM_KEYDOWN→window-event 的 "back" 处理 */
            int n = (W->hqt + 1) % W32_CQ;
            if (n != W->hqh) { W->charq[W->hqt] = (wchar_t)wp; W->hqt = n; }
        }
        return 0;
    }
    case WM_CLOSE: {
        W32Win* W = w32_by_hwnd(hwnd);
        if (W) W->want_close = 1;
        DestroyWindow(hwnd);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    case WM_PAINT: {
        W32Win* W = w32_by_hwnd(hwnd);
        if (W && W->hwnd) {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, W->w, W->h);
            HGDIOBJ oldbmp = SelectObject(mem, bmp);
            RECT rc = {0, 0, W->w, W->h};
            HBRUSH bg = CreateSolidBrush(RGB(16,16,20));
            FillRect(mem, &rc, bg); DeleteObject(bg);
            for (int i = 0; i < W->dn; i++) {
                W32Cmd* c = &W->dl[i];
                COLORREF col = RGB((c->color>>16)&0xFF, (c->color>>8)&0xFF, c->color&0xFF);
                if (c->is_text) {
                    SetBkMode(mem, TRANSPARENT);
                    SetTextColor(mem, col);
                    int fs = (c->scale >= 2) ? 22 : 13;
                    HFONT f = CreateFontA(fs,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
                                          OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,
                                          FIXED_PITCH|FF_DONTCARE,"Consolas");
                    HGDIOBJ of = SelectObject(mem, f);
                    /* 源码字符串是 UTF-8；UTF-8 → UTF-16 后用 TextOutW 绘制，中文正常 */
                    wchar_t wbuf[256];
                    int wn = utf8_to_wcs(c->text, wbuf, 256);
                    if (wn > 0) TextOutW(mem, c->tx, c->ty, wbuf, wn);
                    SelectObject(mem, of); DeleteObject(f);
                } else {
                    RECT r = { c->x, c->y, c->x + c->w, c->y + c->h };
                    switch (c->op) {
                    case 1: { HBRUSH b=CreateSolidBrush(col); FillRect(mem,&r,b); DeleteObject(b);} break;
                    case 2: { HBRUSH b=CreateSolidBrush(col); FrameRect(mem,&r,b); DeleteObject(b);} break;
                    case 5: {
                        HPEN p=CreatePen(PS_SOLID,1,col); HGDIOBJ op=SelectObject(mem,p);
                        HGDIOBJ ob=SelectObject(mem,GetStockObject(NULL_BRUSH));
                        int cx=c->x+c->w/2, cy=c->y+c->h/2;
                        Ellipse(mem, cx-c->w/2, cy-c->h/2, cx+c->w/2, cy+c->h/2);
                        SelectObject(mem,ob); SelectObject(mem,op); DeleteObject(p);
                    } break;
                    case 6: case 7: {
                        int rad = c->radius < 0 ? 0 : c->radius;
                        HPEN p = CreatePen(PS_SOLID, (c->op==7)?1:0, col); HGDIOBJ op=SelectObject(mem,p);
                        HBRUSH b = (c->op==6) ? CreateSolidBrush(col) : (HBRUSH)GetStockObject(NULL_BRUSH);
                        HGDIOBJ ob = SelectObject(mem, b);
                        RoundRect(mem, r.left, r.top, r.right, r.bottom, rad, rad);
                        SelectObject(mem,ob); SelectObject(mem,op); DeleteObject(p);
                        if (c->op==6) DeleteObject(b);
                    } break;
                    default: break;
                    }
                }
            }
            BitBlt(hdc, 0, 0, W->w, W->h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldbmp); DeleteObject(bmp); DeleteObject(mem);
            EndPaint(hwnd, &ps);
        }
        return 0;
    }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static W32Win* w32_alloc(void) {
    for (int i=0;i<W32_MAX;i++) if (!g_w32[i].used) {
        W32Win* W = &g_w32[i];
        W->used = 1; W->id = g_w32_next++; W->hwnd = NULL; W->want_close = 0;
        W->dcap = W32_DL0; W->dn = 0; W->dl = (W32Cmd*)malloc(sizeof(W32Cmd)*W->dcap);
        W->kqh = W->kqt = 0; W->cqh = W->cqt = 0; W->hqh = W->hqt = 0;
        return W;
    }
    return NULL;
}
static void w32_dl_push(W32Win* W, W32Cmd c) {
    if (W->dn >= W->dcap) { W->dcap *= 2; W->dl = (W32Cmd*)realloc(W->dl, sizeof(W32Cmd)*W->dcap); }
    if (W->dl) W->dl[W->dn++] = c;
}

/* (window-create x y w h [title]) -> 窗口号 */
static Value w32Create(int c, Value* a, Env* e) {
    (void)e;
    if (c < 4 || !IS_NUM(a[0])||!IS_NUM(a[1])||!IS_NUM(a[2])||!IS_NUM(a[3])) {
        runtimeError("window-create expects (x y w h [title])"); return NIL_VAL;
    }
    W32Win* W = w32_alloc();
    if (!W) { runtimeError("window-create: too many windows"); return NIL_VAL; }
    W->w = (int)AS_NUM(a[2]); W->h = (int)AS_NUM(a[3]);
    char title[128] = "Nuomi";
    if (c >= 5 && IS_OBJ(a[4])) { char* t = valueToCString(a[4]); if (t){ strncpy(title,t,127); title[127]=0; free(t);} }
    /* 标题栏走宽字符：A 版 API 会把 UTF-8 字节按 GBK 解释，中文标题必乱码。
       窗口主体文字一直用 TextOutW，本来就是正常的。消息泵仍是 PeekMessageA，
       WM_CHAR 的字节语义不变。 */
    wchar_t wtitle[128]; utf8_to_wcs(title, wtitle, 128);
    if (!g_w32_class) {
        WNDCLASSEXW wc; memset(&wc,0,sizeof wc);
        wc.cbSize = sizeof wc; wc.lpfnWndProc = w32_wndproc;
        wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"NuomiWMClass";
        wc.hCursor = LoadCursorA(NULL, IDC_ARROW);  /* 光标句柄与 ANSI/Unicode 无关，
                                                      TCC 头文件里 W 版声明缺失会报警告 */
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassExW(&wc); g_w32_class = 1;
    }
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX; /* 固定大小 */
    RECT rc = {0, 0, W->w, W->h};
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    HWND hwnd = CreateWindowExW(0, L"NuomiWMClass", wtitle, style,
                                (int)AS_NUM(a[0]), (int)AS_NUM(a[1]), ww, wh,
                                NULL, NULL, GetModuleHandleW(NULL), W);
    if (!hwnd) { W->used = 0; free(W->dl); W->dl = NULL; runtimeError("window-create: CreateWindow failed"); return NIL_VAL; }
    W->hwnd = hwnd; g_w32_cur = W->id;
    ShowWindow(hwnd, SW_SHOW); UpdateWindow(hwnd); w32_pump();
    if (g_wm_trace) fprintf(stderr, "[wm-win] CREATE id=%u %dx%d@(%d,%d)\n", W->id, W->w, W->h, (int)AS_NUM(a[0]),(int)AS_NUM(a[1]));
    return NUM_VAL((double)W->id);
}

static W32Win* w32_win(int c, Value* a) {
    if (c < 1 || !IS_NUM(a[0])) return NULL;
    W32Win* W = w32_by_id((uint32_t)AS_NUM(a[0]));
    return W ? W : (g_w32_cur ? w32_by_id(g_w32_cur) : NULL);
}

static Value w32Shape(int c, Value* a, Env* e) {
    (void)e;
    if (c < 7 || !IS_NUM(a[0]) || !IS_NUM(a[1])) { runtimeError("window-shape expects (win op x y w h color [radius])"); return NIL_VAL; }
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-shape: unknown window"); return NIL_VAL; }
    W32Cmd cmd; memset(&cmd, 0, sizeof cmd);
    cmd.op = (int)AS_NUM(a[1]);
    cmd.x = IS_NUM(a[2])?(int)AS_NUM(a[2]):0;
    cmd.y = IS_NUM(a[3])?(int)AS_NUM(a[3]):0;
    cmd.w = IS_NUM(a[4])?(int)AS_NUM(a[4]):0;
    cmd.h = IS_NUM(a[5])?(int)AS_NUM(a[5]):0;
    cmd.color = IS_NUM(a[6])?(uint32_t)AS_NUM(a[6]):0;
    cmd.radius = (c>=8 && IS_NUM(a[7]))?(int)AS_NUM(a[7]):0;
    cmd.is_text = 0;
    w32_dl_push(W, cmd);
    return NUM_VAL((double)W->id);
}

static Value w32Fill(int c, Value* a, Env* e) {
    if (c < 5 || !IS_NUM(a[0])) { runtimeError("window-fill expects (win x y w h color)"); return NIL_VAL; }
    Value args[7];
    args[0]=a[0]; args[1]=NUM_VAL(NWM_OP_FILL);
    args[2]=a[1]; args[3]=a[2]; args[4]=a[3];
    args[5]=(c>=5)?a[4]:NUM_VAL(0);
    args[6]=(c>=6 && IS_NUM(a[5]))?a[5]:NUM_VAL(0);
    return w32Shape(7, args, e);
}

static Value w32Text(int c, Value* a, Env* e) {
    (void)e;
    if (c < 5 || !IS_NUM(a[0]) || !IS_OBJ(a[4])) { runtimeError("window-text expects (win x y scale \"text\")"); return NIL_VAL; }
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-text: unknown window"); return NIL_VAL; }
    char* s = valueToCString(a[4]);
    if (!s) { runtimeError("window-text: bad string"); return NIL_VAL; }
    W32Cmd cmd; memset(&cmd, 0, sizeof cmd);
    cmd.is_text = 1;
    cmd.tx = IS_NUM(a[1])?(int)AS_NUM(a[1]):0;
    cmd.ty = IS_NUM(a[2])?(int)AS_NUM(a[2]):0;
    cmd.scale = IS_NUM(a[3])?(int)AS_NUM(a[3]):1;
    cmd.color = (c >= 6 && IS_NUM(a[5])) ? (uint32_t)AS_NUM(a[5]) : 0xFFFFFFu; /* 第6参颜色，默认白 */
    strncpy(cmd.text, s, sizeof cmd.text - 1);
    free(s);
    w32_dl_push(W, cmd);
    return NUM_VAL((double)W->id);
}

static Value w32Clear(int c, Value* a, Env* e) {
    (void)e;
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-clear: unknown window"); return NIL_VAL; }
    W->dn = 0;
    return NIL_VAL;
}

static Value w32Invalidate(int c, Value* a, Env* e) {
    (void)e;
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-invalidate: unknown window"); return NIL_VAL; }
    InvalidateRect(W->hwnd, NULL, FALSE);
    UpdateWindow(W->hwnd);
    return NIL_VAL;
}

static Value w32Move(int c, Value* a, Env* e) {
    (void)e;
    if (c < 3 || !IS_NUM(a[0])) { runtimeError("window-move expects (win x y)"); return NIL_VAL; }
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-move: unknown window"); return NIL_VAL; }
    SetWindowPos(W->hwnd, NULL, (int)AS_NUM(a[1]), (int)AS_NUM(a[2]), 0, 0, SWP_NOSIZE|SWP_NOZORDER);
    return NIL_VAL;
}

static Value w32Title(int c, Value* a, Env* e) {
    (void)e;
    if (c < 2 || !IS_NUM(a[0]) || !IS_OBJ(a[1])) { runtimeError("window-title expects (win \"title\")"); return NIL_VAL; }
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-title: unknown window"); return NIL_VAL; }
    char* t = valueToCString(a[1]);
    if (t) { wchar_t wt[256]; utf8_to_wcs(t, wt, 256); SetWindowTextW(W->hwnd, wt); free(t); }
    return NIL_VAL;
}

static Value w32Close(int c, Value* a, Env* e) {
    (void)e;
    W32Win* W = w32_win(c, a);
    if (!W) { runtimeError("window-close: unknown window"); return NIL_VAL; }
    if (W->hwnd) DestroyWindow(W->hwnd);
    W->hwnd = NULL; W->used = 0; free(W->dl); W->dl = NULL;
    w32_pump();
    return NIL_VAL;
}

static Value w32_key_sym(int vk) {
    const char* s = NULL; int len = 0;
    switch (vk) {
        case VK_UP: s="up"; len=2; break;
        case VK_DOWN: s="down"; len=4; break;
        case VK_LEFT: s="left"; len=4; break;
        case VK_RIGHT: s="right"; len=5; break;
        case VK_BACK: s="back"; len=4; break;
        case VK_ESCAPE: s="esc"; len=3; break;
        case VK_SPACE: s="space"; len=5; break;
        case VK_RETURN: s="enter"; len=5; break;
        default: break;
    }
    if (s) {
        ObjString* o = internString(s, len, true);
        retain(OBJ_VAL(o));
        return OBJ_VAL(o);
    }
    /* 字母键 A-Z → 单字符小写字符串（供 key-dir 的 "w"/"a"/"s"/"d" 与 wait-restart 的 "r" 比较） */
    if (vk >= 'A' && vk <= 'Z') {
        char ch = (char)('a' + (vk - 'A'));
        ObjString* o = internString(&ch, 1, false);
        retain(OBJ_VAL(o));
        return OBJ_VAL(o);
    }
    return NIL_VAL;
}

/* (window-click [win]) -> nil 或整数 (x*100000 + y)；同时泵消息让窗口保持响应 */
static Value w32Click(int c, Value* a, Env* e) {
    (void)e;
    W32Win* W = (c>=1 && IS_NUM(a[0])) ? w32_by_id((uint32_t)AS_NUM(a[0])) : (g_w32_cur?w32_by_id(g_w32_cur):NULL);
    if (!W) return NIL_VAL;
    w32_pump();
    if (W->cqh == W->cqt) return NIL_VAL;
    int x = W->cxq[W->cqh]; int y = W->cyq[W->cqh];
    W->cqh = (W->cqh + 1) % W32_CQ;
    return NUM_VAL((double)(x * 100000 + y));
}

/* (window-char [win]) -> nil 或单个字符的 UTF-8 字符串（字母/数字/中文输入法字符） */
static Value w32Char(int c, Value* a, Env* e) {
    (void)e;
    W32Win* W = (c>=1 && IS_NUM(a[0])) ? w32_by_id((uint32_t)AS_NUM(a[0])) : (g_w32_cur?w32_by_id(g_w32_cur):NULL);
    if (!W) return NIL_VAL;
    w32_pump();
    if (W->hqh == W->hqt) return NIL_VAL;
    wchar_t ch = W->charq[W->hqh];
    W->hqh = (W->hqh + 1) % W32_CQ;
    char buf[8]; int n = wunit_to_utf8(ch, buf);
    if (n <= 0) return NIL_VAL;
    ObjString* o = internString(buf, n, true); retain(OBJ_VAL(o));
    return OBJ_VAL(o);
}

/* (window-event [block]) -> 按键符号或 nil；同时泵消息让窗口保持响应 */
static Value w32Event(int c, Value* a, Env* e) {
    (void)c; (void)a; (void)e;
    w32_pump();
    if (!g_w32_cur) return NIL_VAL;
    W32Win* W = w32_by_id(g_w32_cur);
    if (!W) return NIL_VAL;
    if (W->want_close) { ObjString* o = internString("close", 5, true); retain(OBJ_VAL(o)); return OBJ_VAL(o); }
    int vk = w32_popkey(W);
    return vk ? w32_key_sym(vk) : NIL_VAL;
}

static Value w32Trace(int c, Value* a, Env* e) {
    (void)e;
    if (c>=1 && IS_NUM(a[0])) g_wm_trace = AS_NUM(a[0]) != 0;
    return NIL_VAL;
}

void registerWmNatives(Env* g) {
    defNative(g, "window-create",     w32Create);
    defNative(g, "window-shape",      w32Shape);
    defNative(g, "window-fill",       w32Fill);
    defNative(g, "window-text",       w32Text);
    defNative(g, "window-clear",      w32Clear);
    defNative(g, "window-invalidate", w32Invalidate);
    defNative(g, "window-move",       w32Move);
    defNative(g, "window-title",      w32Title);
    defNative(g, "window-close",      w32Close);
    defNative(g, "window-event",      w32Event);
    defNative(g, "window-click",      w32Click);
    defNative(g, "window-char",       w32Char);
    defNative(g, "window-trace",      w32Trace);
}

#else
/* ====================== 非 Windows：宿主假 WM ====================== */
#define HOST_WM_MAX_WIN 16
typedef struct { int used; uint32_t win; int x, y, w, h; char title[64]; } HostWin;
static HostWin   g_host_wm[HOST_WM_MAX_WIN];
static uint32_t  g_host_wm_next = 1;

static HostWin* hostwm_find(uint32_t win) {
    for (int i = 0; i < HOST_WM_MAX_WIN; i++)
        if (g_host_wm[i].used && g_host_wm[i].win == win) return &g_host_wm[i];
    return NULL;
}
static int argInt(Value v, int dflt) { return IS_NUM(v) ? (int)AS_NUM(v) : dflt; }

static Value natWinCreate(int c, Value* a, Env* e) {
    (void)e;
    if (c < 4 || !IS_NUM(a[0])||!IS_NUM(a[1])||!IS_NUM(a[2])||!IS_NUM(a[3])) { runtimeError("window-create expects (x y w h [title])"); return NIL_VAL; }
    int slot=-1; for (int i=0;i<HOST_WM_MAX_WIN;i++) if(!g_host_wm[i].used){slot=i;break;}
    if (slot<0){ runtimeError("window-create: too many windows"); return NIL_VAL; }
    uint32_t win=g_host_wm_next++; HostWin* hw=&g_host_wm[slot];
    hw->used=1; hw->win=win; hw->x=(int)AS_NUM(a[0]); hw->y=(int)AS_NUM(a[1]); hw->w=(int)AS_NUM(a[2]); hw->h=(int)AS_NUM(a[3]); hw->title[0]=0;
    if (c>=5 && IS_OBJ(a[4])){ char* t=valueToCString(a[4]); if(t){ strncpy(hw->title,t,sizeof hw->title-1); hw->title[sizeof hw->title-1]=0; free(t);} }
    if (g_wm_trace) fprintf(stderr,"[wm-host] CREATE win=%u %dx%d@(%d,%d) title=\"%s\"\n", win, hw->w, hw->h, hw->x, hw->y, hw->title);
    return NUM_VAL((double)win);
}
static Value natWinShape(int c, Value* a, Env* e) {
    (void)e;
    if (c<7 || !IS_NUM(a[0])||!IS_NUM(a[1])){ runtimeError("window-shape expects (win op x y w h color [radius])"); return NIL_VAL; }
    if (!hostwm_find((uint32_t)AS_NUM(a[0]))){ runtimeError("window-shape: unknown window"); return NIL_VAL; }
    if (g_wm_trace) fprintf(stderr,"[wm-host] SHAPE win=%u op=%d %dx%d@(%d,%d) color=#%06X radius=%d\n",
        (uint32_t)AS_NUM(a[0]),(int)AS_NUM(a[1]),argInt(a[4],0),argInt(a[5],0),argInt(a[2],0),argInt(a[3],0),
        IS_NUM(a[6])?(uint32_t)AS_NUM(a[6]):0, (c>=8&&IS_NUM(a[7]))?(int)AS_NUM(a[7]):0);
    return NUM_VAL(AS_NUM(a[0]));
}
static Value natWinFill(int c, Value* a, Env* e) {
    if (c<5||!IS_NUM(a[0])){ runtimeError("window-fill expects (win x y w h color)"); return NIL_VAL; }
    Value args[7]; args[0]=a[0]; args[1]=NUM_VAL(NWM_OP_FILL); args[2]=a[1]; args[3]=a[2]; args[4]=a[3];
    args[5]=(c>=5)?a[4]:NUM_VAL(0); args[6]=(c>=6&&IS_NUM(a[5]))?a[5]:NUM_VAL(0);
    return natWinShape(7, args, e);
}
static Value natWinText(int c, Value* a, Env* e) {
    (void)e;
    if (c<5||!IS_NUM(a[0])||!IS_OBJ(a[4])){ runtimeError("window-text expects (win x y scale \"text\")"); return NIL_VAL; }
    if (!hostwm_find((uint32_t)AS_NUM(a[0]))){ runtimeError("window-text: unknown window"); return NIL_VAL; }
    char* s=valueToCString(a[4]); if(!s){ runtimeError("window-text: bad string"); return NIL_VAL; }
    if (g_wm_trace) fprintf(stderr,"[wm-host] TEXT win=%u @(%d,%d) scale=%d \"%s\"\n",(uint32_t)AS_NUM(a[0]),(int)AS_NUM(a[1]),(int)AS_NUM(a[2]),(int)AS_NUM(a[3]),s);
    free(s); return NUM_VAL(AS_NUM(a[0]));
}
static Value natWinClear(int c, Value* a, Env* e){ (void)e; if(c<1||!IS_NUM(a[0])||!hostwm_find((uint32_t)AS_NUM(a[0]))){runtimeError("window-clear: unknown window");return NIL_VAL;} if(g_wm_trace)fprintf(stderr,"[wm-host] CLEAR win=%u\n",(uint32_t)AS_NUM(a[0])); return NIL_VAL; }
static Value natWinInvalidate(int c, Value* a, Env* e){ (void)e; if(c<1||!IS_NUM(a[0])||!hostwm_find((uint32_t)AS_NUM(a[0]))){runtimeError("window-invalidate: unknown window");return NIL_VAL;} if(g_wm_trace)fprintf(stderr,"[wm-host] INVALIDATE win=%u\n",(uint32_t)AS_NUM(a[0])); return NIL_VAL; }
static Value natWinMove(int c, Value* a, Env* e){ (void)e; if(c<3||!IS_NUM(a[0])||!hostwm_find((uint32_t)AS_NUM(a[0]))){runtimeError("window-move: unknown window");return NIL_VAL;} return NIL_VAL; }
static Value natWinTitle(int c, Value* a, Env* e){ (void)e; if(c<2||!IS_NUM(a[0])||!IS_OBJ(a[1])||!hostwm_find((uint32_t)AS_NUM(a[0]))){runtimeError("window-title: unknown window");return NIL_VAL;} return NIL_VAL; }
static Value natWinClose(int c, Value* a, Env* e){ (void)e; if(c<1||!IS_NUM(a[0])){runtimeError("window-close: unknown window");return NIL_VAL;} HostWin* hw=hostwm_find((uint32_t)AS_NUM(a[0])); if(hw)hw->used=0; return NIL_VAL; }
static Value natWinEvent(int c, Value* a, Env* e){ (void)c;(void)a;(void)e; return NIL_VAL; }
static Value natWinClick(int c, Value* a, Env* e){ (void)e; if(c<1||!IS_NUM(a[0])||!hostwm_find((uint32_t)AS_NUM(a[0]))){return NIL_VAL;} return NIL_VAL; }
static Value natWinChar(int c, Value* a, Env* e){ (void)e; if(c<1||!IS_NUM(a[0])||!hostwm_find((uint32_t)AS_NUM(a[0]))){return NIL_VAL;} return NIL_VAL; }
static Value natWinTrace(int c, Value* a, Env* e){ (void)e; if(c>=1&&IS_NUM(a[0])) g_wm_trace=AS_NUM(a[0])!=0; return NIL_VAL; }

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
    defNative(g, "window-click",      natWinClick);
    defNative(g, "window-char",       natWinChar);
    defNative(g, "window-trace",      natWinTrace);
}
#endif
