# Mochi 语言 + NUC 工具链 Makefile
# 用 .RECIPEPREFIX 把配方前缀改成 ">"，避免 Makefile 必须的 Tab 缩进问题。
# 跨平台：默认 gcc；novus-os 构建机可用 CROSS_COMPILE= 交叉编译（如 x86_64-elf-）。
.RECIPEPREFIX = >
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Iinclude

LANG_SRC := src/value.c src/reader.c src/env.c src/eval.c src/native.c src/main.c
NUC_SRC  := src/nucfmt.c src/nucutil.c

all: mochi nucutil

mochi: $(LANG_SRC) include/mochi.h
> $(CC) $(CFLAGS) $(LANG_SRC) -o mochi

nucutil: $(NUC_SRC) include/nucfmt.h
> $(CC) $(CFLAGS) $(NUC_SRC) -o nucutil

clean:
> rm -f mochi mochi.exe nucutil nucutil.exe
