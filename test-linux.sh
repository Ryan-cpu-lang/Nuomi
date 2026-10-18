#!/bin/sh
# Mochi + NUC toolchain - Linux smoke test (needs gcc/cc + make).
# Verifies: full build via Makefile, all interpreter samples, NUC pipeline,
# and Route-A compile round-trip (mochi -c -> nucutil run) which now uses mmap on Linux.
set -e
cd "$(dirname "$0")"

echo "[build] make"
make

echo "[1] interpreter samples"
for f in samples/*.mochi; do
  if ./mochi "$f" >/dev/null; then echo "  ok   $f"; else echo "  FAIL $f"; exit 1; fi
done

echo "[2] NUC emit-demo / validate / run (expect entry 0x2A=42)"
./nucutil emit-demo /tmp/mochi-demo.nuc >/dev/null
./nucutil validate /tmp/mochi-demo.nuc
./nucutil run /tmp/mochi-demo.nuc

echo "[3] Route-A compile round-trip (mochi -c -> nucutil run, expect 142)"
./mochi -c samples/compile-demo.mochi -o /tmp/cd.nuc && ./nucutil run /tmp/cd.nuc

rm -f /tmp/mochi-demo.nuc /tmp/cd.nuc
echo "LINUX TEST: ALL OK"
