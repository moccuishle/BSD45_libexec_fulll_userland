# 内核边界盘点（S100.1）

> **目的**：在把 `bsd45` 分叉成 `microkernel/` 与 `monolithickernel/` 之前，先把
> 「内核态保留什么、什么可以上移到用户态服务、两边通过哪些原语说话」固定成一份
> 可检查的契约。本文只描述现状与规则，不改代码。
>
> 基线：`c89e4fd`（2026-11-15，Windows 与 Ubuntu 双端 15 套验收全绿）。

---

## 0. 一句话结论

| 类别 | 结论 |
|---|---|
| **必须留 ring0** | 调度器/进程表、地址空间与页表、trap/IDT、时钟与 RTC、端口 I/O、IPC 内核侧 |
| **可以上移** | tmpfs、ISO9660、tty 行规程、块设备协议栈（ATAPI 的端口操作留内核代理） |
| **不该动** | 进程/信号/管道/fd 表（微内核里也留在核内） |
| **当前真实耦合点** | 只有 6 类原语，见第 3 节；FS/驱动**没有**直接碰进程表或调度器 |

---

## 1. 内核里到底编了什么

`build_kernel.ps1` / `build_kernel.sh` 实际编译的文件（不含生成的 `embed.c`、
`vector.s`、`assym.s`）：

| 文件 | 行数 | 作用 | 可上移性 |
|---|---:|---|---|
| `kernel/arch/i386/isa/locore.s` | 1555 | 启动、trap 入口、`inb/outb/insw/outsw`、`rtcin` | **留**（特权指令） |
| `kernel/arch/i386/isa/pmap.c` | 1552 | 页表 / pmap | **留** |
| `kernel/arch/i386/isa/machdep.c` | 1017 | 机器相关启动、控制台探测 | **留** |
| `kernel/arch/i386/i386/cons.c` | 181 | ROM console 桩（实际用 pccon） | **留** |
| `kernel/other/usermode.c` | 2827 | 进程/调度/信号/fd/系统调用/ELF 装载/**控制台行规程**/**tmpfs 页原语** | **拆**（见第 4 节） |
| `kernel/other/usermode_asm.s` | 76 | `resume_user` 等 | **留** |
| `kernel/other/glue.c` | 259 | 内核运行时 glue（printf、panic…） | **留** |
| `kernel/other/mon.c` | 285 | 板载 monitor（`bsd45>`） | **留** |
| `kernel/other/stubs.c` | 82 | 4.5BSD 桩 | **留** |
| `kernel/other/strings.c` | 50 | 内核字符串 | **留** |
| `kernel/driver/pccon.c` | 229 | VGA 文本 + COM1 + 8042 键盘 | **一半**（字节 I/O 留，行规程走） |
| `kernel/driver/atapi.c` | 227 | ATA PIO / ATAPI 包命令 | **协议栈可上移**，端口操作留 |
| `kernel/fs/isofs.c` | 261 | ISO9660 目录记录解析 | **可上移** |
| `kernel/fs/vfs.c` | 709 | vnode 层 + tmpfs + ISO vnodeops | **转发层留，后端上移** |

**合计约 9.3k 行。**

> ⚠️ `kernel/sys/`、`kernel/vm/`、`kernel/net/` 等 4.5BSD 目录**没有编进内核**
> （只有 `arch/i386/isa/{pmap,machdep}.c` 和 `i386/cons.c` 被选中）。它们是
> 参考树，不要拿它们当「内核现状」来估算工作量。

---

## 2. 判定原则

1. **只有必须接触特权指令 / 中断 / 页表 / 调度的代码才留内核。**
2. 只处理「字节和名字」的（目录解析、文件内容、行规程）→ 可以上移。
3. 需要「让别人等我」的代码 → 必须走阻塞原语，**禁止直接改进程表**。
4. 单次操作比一次消息往返还便宜时（如 fstat 缓存命中），允许留在内核，
   但必须在本文登记为「性能例外」。

---

## 3. 内核原语契约（KP）

「原语」= 内核向 FS/驱动/服务暴露的唯一入口。服务端代码只许调这些。

| 编号 | 原语 | 当前实现 | 状态 | 微内核里谁用 |
|---|---|---|---|---|
| KP-1 | 用户内存范围检查 | `user_ok(p, len)`（usermode.c:1547） | ✅ 有 | 内核 syscall 层 |
| KP-2 | 内核窗口映射/解除 | `pmap_map()` / `pmap_remove()` | ✅ 有 | fs_server 搬运数据 |
| KP-3 | 页分配/回收/统计 | `vfs_page_alloc/free/unmap/stats`（usermode.c:257-289） | ✅ 有 | fs_server 的 tmpfs 数据页 |
| KP-4 | 阻塞/唤醒 | **无通用原语**：现在直接 `cur->state = P_BLOCKED; proc_sched();`，唤醒方 `p->state = P_RUN` | ❌ 缺 | IPC、bdev、tty 都要 |
| KP-5 | 端口 I/O | `inb/outb/insw/outsw`（locore.s） | ✅ 有 | 只给内核内驱动，不暴露给用户态 |
| KP-6 | 同步消息 send/recv | — | ❌ 缺（S100.3 新增） | 全部服务 |
| KP-7 | 共享页映射 | — | ❌ 缺 | 大块 read/write |
| KP-8 | 时间 | `gettimeofday(2)`（S16） | ✅ 有 | 日志/超时 |
| KP-9 | 控制台字节输出 | `pccnputc()`（pccon.c:145） | ✅ 有 | tty_server 的输出半部 |

**关键缺口是 KP-4 和 KP-6/7。** 现在内核里的「阻塞」是 4 处手写状态机
（usermode.c:1200、1502、1953、2111），没有 `sleep(chan)/wakeup(chan)`。
做 IPC 之前必须先把它抽出来，否则服务端没法等消息。

---

## 4. 当前违规清单（2026-11-15 审计）

### 4.1 硬违规（必须为 0，`tools/check-boundary.sh` 会卡）

| 位置 | 现状 | 问题 |
|---|---|---|
| — | FS/驱动代码没有直接引用 `cur`、`procs`、`proc_*`、`P_BLOCKED`、`page_alloc`、`zero_page`、`pmap_enter` | **无违规** ✅ |

这条是好消息：`vfs.c` 到目前为止只通过 `vfs_page_*` 和 `pmap_map` 说话，没有
偷偷摸进程表。

### 4.2 已知层次债（不阻断，登记在案）

| 位置 | 现状 | 应该改为 | 计划 |
|---|---|---|---|
| `kernel/fs/vfs.c:73` | tmpfs 数据页直接 `pmap_map()` | KP-2 的 `vfs_page_map()` 包装 | S100.2a |
| `kernel/fs/vfs.c:30` | 声明了 `extern int user_ok()` 但全文没用到 | 删掉这个声明 | S100.2a |
| `kernel/fs/vfs.c:473` | ISO vnode 的 read 里直接 `atapi_read()` | 块设备接口 `bdev_read()` | S100.4 |
| `kernel/fs/isofs.c:79,110,236,291` | 直接 `atapi_read()` / `atapi_present()` | 同上 | S100.4 |
| `kernel/other/usermode.c:1778` | 控制台行规程 `console_read()` 和进程/fd 混在一个文件 | 搬到 `tty_server` | S100.5 |
| `kernel/other/usermode.c:1200,1502,1953,2111` | 手写 `P_BLOCKED` 状态机 | KP-4 `block_on()/wakeup()` | S100.3 前置 |
| `kernel/driver/atapi.c` | `inb/outb/insw` + 忙等延时 | 留内核，但包成 bdev 服务（端口代理） | S100.6 |

> `pmap_map()` 本身不算违规（它就是 KP-2），要修的是「FS 直接调它」这个方向；
> 短期用包装函数隔离，长期由 IPC 服务自己映射。

---

## 5. 迁移顺序与判定点

每一步都必须：**15 套验收全绿（Windows + Ubuntu）+ 至少一条故障注入**。

| 阶段 | 内容 | 改动面 | 判定点 |
|---|---|---|---|
| S100.2a | 把 tmpfs 从 `vfs.c` 搬到 `kernel/fs/tmpfs.c`，`vnodeops` 不变 | 纯搬移，0 行为变化 | 15 套全绿 |
| S100.3 | tmpfs → `fs_server`（KP-4/6/7 落地） | 新增 ipc.c、fs_server、vfs 转发 | `/tmp` 读写全走 IPC 且 15 套全绿 |
| S100.4 | ISO9660 → `fs_server`，先落 `bdev_read()` | fs + 块设备接口 | 从 ISO 跑 `/bin/sh` 全绿 |
| S100.5 | tty 行规程 → `tty_server` | usermode.c 瘦身 | 交互 shell + Ctrl-C 全绿 |
| S100.6 | ATAPI 留内核，bdev 作为「内核内服务」暴露 | 驱动不动 | 同上 |

**判定点**：S100.3 通过后，再决定要不要继续上移 ISO9660 / tty。如果 IPC 的
往返开销已经吃掉交互体验，就停在「FS 在用户态、tty 在内核」的混合态，并在
`docs/microkernel-design.md` 里记录理由。

---

## 6. 机器检查

`tools/check-boundary.sh` 是这条边界的「体检脚本」：

- **硬规则**：`kernel/fs/*.c`、`kernel/driver/*.c` 里出现
  `cur` / `procs` / `proc_*` / `P_BLOCKED` / `page_alloc` / `page_free` /
  `zero_page` / `user_map` / `pmap_enter` / `pmap_remove` → 失败（exit 1）。
- **软规则**：直接调 `atapi_*`、`pmap_map` → 打印为「已知层次债」，不失败，
  但每次迁移都应让数量下降。

它不进 15 套验收，作为开发时的自检；等 S100.3 落地后再考虑接进 CI。

---

## 7. 变更记录

| 日期 | 内容 |
|---|---|
| 2026-11-15 | 初稿（S100.1），基于 `c89e4fd` 的符号审计 |
