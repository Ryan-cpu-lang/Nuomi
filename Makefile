# Mochi 语言 + NUC 工具链 Makefile
# 用 .RECIPEPREFIX 把配方前缀改成 ">"，避免 Makefile 必须的 Tab 缩进问题。
# 跨平台：默认 gcc；novus-os 构建机可用 CROSS_COMPILE= 交叉编译（如 x86_64-elf-）。
.RECIPEPREFIX = >
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -Iinclude

# nucfmt.c is the shared NUC format library: the compiler writes with it,
# the host tool reads with it. Both binaries link it.
NUC_LIB  := src/nucfmt.c

# codegen.c belongs to the language binary, not to nucutil: it parses with
# the reader and is reached from main's -c mode, so leaving it out builds a
# mochi that links without the one function that produces a .nuc - and it
# needs nucfmt.c next to it for the same reason.
LANG_SRC := src/value.c src/reader.c src/env.c src/eval.c src/native.c \
            src/codegen.c src/main.c
NUC_SRC  := src/nucutil.c

# fmod comes out of the C math library, which is separate on most hosts.
LDLIBS ?= -lm

all: mochi nucutil

mochi: $(LANG_SRC) $(NUC_LIB) include/mochi.h include/nucfmt.h include/codegen.h
> $(CC) $(CFLAGS) $(LANG_SRC) $(NUC_LIB) -o mochi $(LDLIBS)

nucutil: $(NUC_SRC) $(NUC_LIB) include/nucfmt.h
> $(CC) $(CFLAGS) $(NUC_SRC) $(NUC_LIB) -o nucutil $(LDLIBS)

clean:
> rm -f mochi mochi.exe nucutil nucutil.exe
