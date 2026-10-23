// sync-bin.js —— 在 `npm publish` 前自动把仓库根目录最新编译好的
// 预编译二进制同步进 npm/bin/，避免 npm 包里的 exe 副本过期。
// 运行目录：npm/（npm 在包目录执行 prepublishOnly）
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const binDir = path.join(__dirname, 'bin');
fs.mkdirSync(binDir, { recursive: true });

const pairs = [
  ['dist/windows/nuomi.exe', 'nuomi.exe'],
];

let ok = true;
for (const [rel, name] of pairs) {
  const src = path.join(root, rel);
  const dest = path.join(binDir, name);
  if (!fs.existsSync(src)) {
    console.error('[sync-bin] 缺少源文件: ' + src);
    ok = false;
    continue;
  }
  fs.copyFileSync(src, dest);
  const sz = fs.statSync(dest).size;
  console.log('[sync-bin] ' + rel + ' -> bin/' + name + ' (' + sz + ' bytes)');
}

if (!ok) {
  console.error('[sync-bin] 同步失败：请先在仓库根目录用 build.bat 编译出 nuomi.exe');
  process.exit(1);
}
console.log('[sync-bin] 完成');
