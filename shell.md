# 在这台 Windows 机器上用 bash 干活（PowerShell → bash 的固化写法）

> 目的：对话里所有命令都用 **bash** 写（人看得懂、和 `build_*.sh` 一致），
> 但实际执行器仍是 Windows 上的 MSYS2 bash。本文记录可用的调用方式、
> 必设的 PATH、以及踩过的坑。
>
> 环境：MSYS2 在 `C:\msys64`；工具链是 **ucrt64 GCC 16.2.0 + `-m32`**
> （本机没有 mingw32/i686 工具链）；构建脚本 Windows 侧仍是 `*.ps1`。

---

## 1. 基本调用形式

执行器是 PowerShell，所以外层是 `pwsh` 工具，**内层一条 `bash.exe` 命令**：

```powershell
C:\msys64\usr\bin\bash.exe --noprofile --norc /c/Users/Administrator/Desktop/work/bsd45/build/xxx.sh
```

要点：

- 用 `--noprofile --norc`：避免每次加载 profile（也会避免首次运行触发 MSYS2 初始化）。
- `--norc` 下 bash **不会**自动带 MSYS2 的 PATH，见第 2 节。
- 复杂逻辑**不要**写成 `bash -c '...'`，写成 `.sh` 文件再执行（见第 4 节）。

## 2. 必设 PATH（两条都要）

```bash
export PATH=/c/Windows/System32/WindowsPowerShell/v1.0:/usr/bin:/c/msys64/usr/bin
```

| 加进去的目录 | 为什么 |
|---|---|
| `/usr/bin`、`/c/msys64/usr/bin` | `grep/sed/head/tail/ls` 等 MSYS2 coreutils，`--norc` 下默认没有 |
| `/c/Windows/System32/WindowsPowerShell/v1.0` | `build_all.ps1` 内部用裸名调 `powershell` 跑子步骤，不加就报 `'powershell' is not recognized` |

## 3. 路径写法

| Windows | bash |
|---|---|
| `C:\Users\Administrator\Desktop\work` | `/c/Users/Administrator/Desktop/work` |
| `C:\msys64\usr\bin\bash.exe` | `/c/msys64/usr/bin/bash.exe`（外层调用仍写 Windows 路径） |

## 4. 最大的坑：PowerShell 会吃掉引号、`|`、`$`

把带引号/管道/`$` 的命令塞进 `bash -c '...'` 时，PowerShell 会先解析一遍，
常见症状：

```
.*redefin /tmp/build.log | head -12: line 1: FAIL: command not found
p->p_cwd kernel/other/usermode.c || sed -i s|^tp->brk_mapped: -c: line 2: syntax error
```

**规则：任何带 `"`、`|`、`$`、`(`、`)` 的 bash 逻辑，一律写成 `.sh` 文件**，
再用第 1 节的方式执行。

## 5. 调用 `.ps1` 脚本（构建、测试驱动）

```bash
PS=powershell.exe
cd /c/Users/Administrator/Desktop/work/bsd45
"$PS" -NoProfile -ExecutionPolicy Bypass -File build_all.ps1 > /tmp/build.log 2>&1
echo "build exit=$?"
grep -E 'ISO:' /tmp/build.log | tail -1
```

**数组参数要特殊处理**：从 bash 传 `-Keys "a","b"` 会被当成一个字符串。
`tools/run_qemu_test.ps1` 已经支持和 `.sh` 版相同的 `|` 分隔写法：

```bash
"$PS" -NoProfile -ExecutionPolicy Bypass -File tools/run_qemu_test.ps1 \
    -Keys "shell|cd /tmp|pwd|echo hi > a|cat a|exit 0" \
    -WaitFor 'user exit' -TimeoutSec 120 > /tmp/s104.log 2>&1
```

（实现：脚本开头 `if ($Keys.Count -eq 1 -and $Keys[0].Contains('|')) { $Keys = $Keys[0].Split('|') }`）

## 6. 现成的辅助脚本（`bsd45/build/`，已被 .gitignore 忽略）

| 脚本 | 用途 |
|---|---|
| `build/s104_verify.sh` | 构建 + 跑一段 shell 场景（可当模板复制改） |
| `build/showlog.sh <log> [n]` | 只挑 `error:`/`FAIL`/`undefined reference` 等错误行 |
| `build/tail.sh <file> [n]` | 看日志最后 n 行 |

模板：

```bash
#!/usr/bin/env bash
export PATH=/c/Windows/System32/WindowsPowerShell/v1.0:/usr/bin:/c/msys64/usr/bin
PS=powershell.exe
cd /c/Users/Administrator/Desktop/work/bsd45 || exit 1
"$PS" -NoProfile -ExecutionPolicy Bypass -File build_all.ps1 > /tmp/build.log 2>&1
rc=$?; echo "build exit=$rc"
[ "$rc" -ne 0 ] && { tail -n 12 /tmp/build.log; exit 1; }
```

## 7. 常用命令速查

```bash
# 构建（Windows 侧）
"$PS" -NoProfile -ExecutionPolicy Bypass -File build_all.ps1

# 单个验收（10 套：s3 s4 proc tty s7 s8 s9 s10 sig s10_3）
"$PS" -NoProfile -ExecutionPolicy Bypass -File tools/test_s10_3.ps1

# git（MSYS2 自带）
export PATH=/usr/bin:/c/msys64/usr/bin:$PATH
git -C /c/Users/Administrator/Desktop/work status --short
git -C /c/Users/Administrator/Desktop/work add -A
GIT_SSH_COMMAND='C:/Windows/System32/OpenSSH/ssh.exe -o StrictHostKeyChecking=accept-new -i C:/Users/Administrator/.ssh/id_rsa' \
  git -C /c/Users/Administrator/Desktop/work push origin main
```

注意：`git push` 必须在 PATH 里带 `/c/msys64/usr/bin`，否则
`git pack-objects` 找不到 msys DLL，报
`error while loading shared libraries: ?`（退出码 3328）。

## 8. 排查顺序（照这个来，别乱试）

1. 命令没输出/只输出一半 → 先查是不是 PATH 少了 `/usr/bin`
2. `'powershell' is not recognized` → PATH 少 WindowsPowerShell 目录
3. `syntax error near unexpected token` → 别再拼 `bash -c`，落成 `.sh` 文件
4. 构建报错 → 用 `build/showlog.sh /tmp/build.log`
5. `git push` 失败且无输出 → PATH 缺 msys 目录，或 ISO/目录被 VMware 占用
   （`out\BSD45-ELF-x86.iso` 被挂到某台虚拟机光驱时 `Remove-Item` 会失败，
   ISO 只剩 1~1.5 MB，验收全红）
