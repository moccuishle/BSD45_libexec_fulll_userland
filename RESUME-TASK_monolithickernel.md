# 任务恢复文档

本文件只是入口：**真正的恢复/任务文档在 [`bsd45/RESUME-TASK.md`](bsd45/RESUME-TASK.md)**，
里面包含环境要求、构建方式、S0–S9 的验收记录、43+ 条已踩过的坑，以及
S10 的四个子任务（页/vnode 回收、信号与作业控制、`rm`/`cp` 等工具、
mount 表与 `chdir`）。

快速开始：

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

> 源码已合并进仓库：`bsd45/kernel/`（4.5BSD sys 树）+
> `bsd45/libc/bsd/`（`/bin/sh` 用的 4.5BSD 用户态源码）。
> 原来的 `bsd45/src/` 已删除，构建**不需要任何 tarball**。
