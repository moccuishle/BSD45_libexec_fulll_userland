# BSD45_rootfs_overlay — 4.5BSD i386 ELF 内核 / VFS / rootfs / shell

这个仓库是 **BSD45-ELF-x86** 的完整工程：在 4.5BSD-Lite2 i386 内核上实现了
VFS（ISO9660 + tmpfs）、`init(8)`、`execve`、fork/wait4、控制台 tty、管道和
目录遍历，并把 4.5BSD 的 libc / `/bin/sh` 交叉编译成 i386 ELF 放进 ISO。
启动后进入用户态就是真正的 BSD `/bin/sh`（`# ` 提示符）。

## 从哪看起

| 内容 | 位置 |
|---|---|
| 工程主目录（构建脚本、boot、kernel、libc、vmx） | [`bsd45/`](bsd45/) |
| 工程说明（构建、boot 链、VFS 设计） | [`bsd45/README.md`](bsd45/README.md) |
| **任务恢复文档（最重要，先读这个）** | [`bsd45/RESUME-TASK.md`](bsd45/RESUME-TASK.md) |
| Windows 环境安装记录（MSYS2 + qemu） | [`Ai_Coding_Unix_install_gccenv.md`](Ai_Coding_Unix_install_gccenv.md) |

## 快速构建

```powershell
cd bsd45
powershell -NoProfile -ExecutionPolicy Bypass -File .\build_all.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\test_s9.ps1
```

```bash
cd bsd45
bash build_all.sh
bash tools/test_s9.sh
```

产物：`bsd45/out/BSD45-ELF-x86.iso`（约 4.4 MB，El Torito 引导）和
`bsd45/build/bsd45.elf`。

启动：

```powershell
& "C:\msys64\ucrt64\bin\qemu-system-i386.exe" `
  -cdrom bsd45\out\BSD45-ELF-x86.iso -boot d -m 64 -serial stdio
```

## 目录结构（顶层）

```
bsd45/
  boot/            每架构引导代码：i386/（stage1、stage2、kernel.ld）+ arm/ amd64/ mips/ riscv/
  kernel/          完整 4.5BSD sys 树 + 本项目的 VFS/驱动/用户态装载代码
  libc/            用户态 C 库、工具源码，以及 libc/bsd/（4.5BSD 用户态源码）
  rootfs_overlay/  ISO 根目录覆盖层（/etc、/readme.txt 等）
  tools/           测试与辅助脚本
  vms/             VMware 虚拟机定义
  src/             已删除（已合并进 kernel/ + libc/bsd/，构建不再需要 tarball）
```

## 状态

| 阶段 | 内容 | 状态 |
|---|---|---|
| S0–S2 | 交叉编译链、ring3、ELF 装载、ramfs、execve | ✅ |
| S3 | 4.5BSD libc 子集 + 真正的 BSD `/bin/sh` | ✅ |
| S4 | ATAPI + ISO9660 只读 root | ✅ |
| S5 | fork/vfork/wait4、进程表与调度 | ✅ |
| S6 | 控制台 tty（termios/行规程/isatty） | ✅ |
| S7 | 目录遍历 + 管道 | ✅ |
| S8 | tmpfs（`/tmp` 可写）+ 重定向 | ✅ |
| S9 | vnode/VFS 统一层（ISO9660 与 tmpfs 共用接口） | ✅ |
| S10 | 页/vnode 回收、信号与作业控制、更多工具、mount/chdir | 下一步 |

7 套验收（S3/S4/S5/S6/S7/S8/S9）在 Windows（ucrt64 GCC `-m32`）和 Linux 上
均已通过；细节与踩过的坑见 [`bsd45/RESUME-TASK.md`](bsd45/RESUME-TASK.md)。

## 许可

`bsd45/kernel/` 与 `bsd45/libc/bsd/` 中的 4.5BSD 代码遵循其原始 BSD 版权
许可与 UC Berkeley advertising 条款；本项目新增的脚本、引导器与 glue 代码
可视为同一 BSD 风格许可。
