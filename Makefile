# Mochi 语言 + NUC 工具链 Makefile（跨平台：Linux / Windows / macOS）
# 用 .RECIPEPREFIX 把配方前缀改成 ">"，避免 Makefile 必须的 Tab 缩进问题。
# 默认 gcc；novus-os 构建机可用 CROSS_COMPILE= 交叉编译（如 x86_64-elf-）。
.RECIPEPREFIX = >
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Iinclude

# nucfmt.c 是共享的 NUC 格式库：编译器用它写、宿主工具用它读；两个二进制都链接它。
NUC_LIB  := src/nucfmt.c

# codegen.c 属于语言二进制（而非 nucutil）：它与 reader 配合解析，并由 main 的
# -c 编译模式调用，因此必须与 nucfmt.c 一起链接进 mochi（漏掉则链接失败）。
LANG_SRC := src/value.c src/reader.c src/env.c src/eval.c src/native.c \
            src/codegen.c src/main.c
NUC_SRC  := src/nucutil.c
SAMPLES  := $(wildcard samples/*.mochi)

# fmod 来自 C 数学库，多数主机(Linux)上是独立的，需要 -lm。
LDLIBS ?= -lm

.PHONY: all mochi nucutil test clean

all: mochi nucutil

mochi: $(LANG_SRC) $(NUC_LIB) include/mochi.h include/nucfmt.h include/codegen.h
> $(CC) $(CFLAGS) $(LANG_SRC) $(NUC_LIB) -o mochi $(LDLIBS)

nucutil: $(NUC_SRC) $(NUC_LIB) include/nucfmt.h
> $(CC) $(CFLAGS) $(NUC_SRC) $(NUC_LIB) -o nucutil $(LDLIBS)

# 冒烟测试：跑全部样例(解释器) + NUC 流水线(emit-demo -> validate -> run)
test: all
> @for f in $(SAMPLES); do echo "[run] $$f"; ./mochi "$$f" >/dev/null || { echo "FAIL: $$f"; exit 1; }; done
> ./nucutil emit-demo /tmp/mochi-demo.nuc >/dev/null && \
    ./nucutil validate /tmp/mochi-demo.nuc && \
    ./nucutil run /tmp/mochi-demo.nuc
> @rm -f /tmp/mochi-demo.nuc
> @echo "test: OK"

clean:
> rm -f mochi mochi.exe nucutil nucutil.exe
