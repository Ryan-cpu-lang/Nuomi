# Mochi 语言 + NUC 工具链 Makefile（跨平台：Linux / Windows / macOS）
# 用 .RECIPEPREFIX 把配方前缀改成 ">"，避免 Makefile 必须的 Tab 缩进问题。
# 默认 gcc；novus-os 构建机可用 CROSS_COMPILE= 交叉编译（如 x86_64-elf-）。
.RECIPEPREFIX = >
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Iinclude

# 解释器 mochi 也内含编译后端(codegen.c)与 NUC 读写(nucfmt.c)，必须一起链接。
LANG_SRC := src/value.c src/reader.c src/env.c src/eval.c src/native.c \
            src/codegen.c src/nucfmt.c src/main.c
NUC_SRC  := src/nucfmt.c src/nucutil.c
SAMPLES  := $(wildcard samples/*.mochi)

.PHONY: all mochi nucutil test clean

all: mochi nucutil

mochi: $(LANG_SRC) include/mochi.h include/codegen.h include/nucfmt.h
> $(CC) $(CFLAGS) $(LANG_SRC) -o mochi

nucutil: $(NUC_SRC) include/nucfmt.h
> $(CC) $(CFLAGS) $(NUC_SRC) -o nucutil

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
