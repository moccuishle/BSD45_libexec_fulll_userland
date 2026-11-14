# 微内核设计（S100.2）

> 配套文档：[`kernel-boundary.md`](kernel-boundary.md)（S100.1 的边界与原语清单）。
> 本文给出 `bsd45` 微内核化的方案、IPC 契约、服务布局、构建开关与度量方式。
> 基线：`c89e4fd`，单 CPU、64 MB、协作式调度、无时钟中断。

---

## 1. 目标与非目标

**目标**

1. 内核只保留：调度/进程表、地址空间与页表、trap/IDT、时钟/RTC、**IPC**、
   端口 I/O 代理。
2. 文件系统（tmpfs、ISO9660）与控制台行规程跑在用户态服务里。
3. 现有 **15 套验收在 micro 变体下同样全绿**；`-Variant mono` 随时可回退。
4. 单个服务崩溃不拖死整机：内核不 panic，在飞请求返回 `EIO`，`init` 可重启服务。

**非目标**

- 不追求 POSIX 全覆盖、不做网络协议栈、不做 SMP、不追求性能超过宏内核。
- 不把 `ATAPI` 的端口操作搬到用户态（没有 IOPL/IO 端口权限模型，代价大于收益）。

---

## 2. 两个方案

| 维度 | 方案 A：宏内核基线（当前 `bsd45`） | 方案 B：微内核化 |
|---|---|---|
| 内核规模 | 约 10.4k 行全在 ring0 | 约 7k 行在 ring0 + 服务约 2k 行用户态 |
| 一次 `read()` | 1 次 syscall + 直接调用 `atapi_read` | 1 次 syscall + 2 次 IPC 往返（VFS↔fs_server）+ 1 次 bdev |
| 崩溃影响 | 任一 FS/驱动 bug = 整机 TRAP | 服务崩溃 = 该文件系统不可用，其他照常 |
| 调试 | 内核断点，符号齐全 | 服务可用普通用户态调试，但 IPC 协议要一起看 |
| 改动风险 | 低（已稳定） | 高（新增 IPC、共享页、服务生命周期） |
| 回退 | — | `-Variant mono` 保留全部旧路径 |

**结论**：方案 B 值得做，但必须先有 KP-4（阻塞原语）和 KP-6/7（IPC、共享页），
并且**一次只上移一个服务**。第一个实验对象是 tmpfs（无块设备依赖）。

---

## 3. 内核保留物（micro 变体）

| 模块 | 为什么留 | 备注 |
|---|---|---|
| `locore.s` / `pmap.c` / `machdep.c` | 特权指令、页表、trap | 原样 |
| 进程表、调度器、信号、fd 表、管道 | 微内核也放核内 | 现有 `usermode.c` 主体 |
| `syscall_dispatch` | 用户态唯一入口 | 新增 IPC 相关 syscall |
| `pccon.c` 的字节 I/O | 端口 I/O | 行规程上移到 tty_server |
| `atapi.c` | 端口 I/O + PIO 时序 | 作为内核内 **bdev 服务** |
| IPC 内核侧（新） | 端口表、消息队列、阻塞/唤醒 | 新增 `ipc.c` |

---

## 4. IPC 设计

### 4.1 端口与消息

```c
struct msg {
	unsigned	from;		/* 发送方 pid（内核填） */
	unsigned	type;		/* FS_LOOKUP / FS_READ / ... */
	unsigned	len;		/* inline 有效字节数，≤ 64 */
	unsigned	pa;		/* 大块数据：共享页物理地址（0 = 无） */
	char		inline_data[64];
};
```

- 每个服务一个 **port**（0 = fs，1 = tty，2 = bdev，…），端口表固定 16 项。
- **小消息内联**（路径、名字、少量元数据），**大块数据走共享页**（文件内容）。
- 端口只有「创建/注册」由内核做，服务退出时端口关闭并唤醒所有等待者。

### 4.2 语义（先做最简单的会合）

| 操作 | 语义 |
|---|---|
| `send(port, msg)` | 接收方在等 → 直接交付并唤醒；否则发送方阻塞（排在该端口的发送队列） |
| `recv(port, msg)` | 队列非空 → 取一条；否则阻塞 |
| `reply` | 不单独做：请求消息里带 `reply_port`，服务端 `send` 回去 |

会合语义（而不是异步队列）是为了避开「队列满」的背压问题；协作式调度下，
阻塞就是 `block_on(port)`，唤醒由 `send` 完成。

### 4.3 共享页（KP-7）

```
fs_server: vfs_page_alloc() → 填数据 → map_shared(pa, len) 发给内核
kernel:    把 pa 映射进目标进程 → copyout → 通知 fs_server 释放
```

生命周期用「谁分配谁释放」：内核收到页后**拷完立即回一条 ACK**，fs_server 收
ACK 才 `vfs_page_free()`。这样不需要跨进程引用计数。

### 4.4 阻塞原语（KP-4）

新增内核内 API（不暴露给用户态，服务通过 syscall 间接触发）：

```c
void block_on(void *chan);   /* 置 P_BLOCKED，记 chan，proc_sched() */
void wakeup(void *chan);     /* 所有停在 chan 上的进程置 P_RUN */
```

现有 4 处手写状态机（`usermode.c:1200/1502/1953/2111`）逐步改成 `block_on`；
`wait4`、pipe、console 读都受益。

### 4.5 一次 `read(/tmp/a)` 的完整路径（micro）

```
user: read(fd, buf, n)
  → syscall_dispatch (case 3)
  → vfs 转发层：vnode->ops->read == tmpfs_read_ipc
  → ipc_send(FS_PORT, {type=FS_READ, fd_id, off, len, reply=KERNEL_PORT})
  → block_on(reply)
fs_server: recv → 查自己的 vnode 表 → 数据放共享页 → send(KERNEL_PORT, {...pa})
kernel: wakeup → copyout 到用户 buf → send(fs_server, ACK) → 返回 n
```

ISO 路径多一跳：`fs_server` 调 `bdev_read(lba, nsec, buf)`（内核内服务，
用户态不可见）。

---

## 5. 服务布局与启动

| 程序 | 路径 | 职责 |
|---|---|---|
| `fs_server` | `/sbin/fs_server` | tmpfs 全部操作；ISO9660 只读；对外一个端口 |
| `tty_server` | `/sbin/tty_server` | 行规程（回显、退格、`^C/^Z` 字符→信号）、termios 状态 |
| `bdev`（内核内） | — | `atapi_read/capacity`，只给 fs_server 用 |

启动序列：

```
kernel: vfs_init() 建占位 rootvnode（type=VREG, ops=dead_ops）
init:   挂根、启动 fs_server（fork/exec），等待端口注册（最多 10 s）
fs_server: recv 循环；注册成功 → 内核把 placeholder rootvnode 替换成 fs 的根
init:   exec /bin/sh
```

超时兜底：10 秒没注册 → 回退到内核内只读 ramfs，打印
`fs_server: not responding, using built-in ramfs`，系统仍可进 shell（只是 `/tmp` 不可写）。

---

## 6. VFS 转发层

`vnodeops` 保持现有 11 个操作不变，micro 变体下：

| 层 | 文件 | 角色 |
|---|---|---|
| `kernel/fs/vfs.c` | 保留 | 路径解析、vnode 池、挂载表；**不再实现 tmpfs/iso** |
| `kernel/fs/tmpfs.c` | 宏内核变体 | 现有 `vfs.c` 里的 tmpfs 代码原样搬出（**S100.2a 已落地**） |
| `kernel/fs/ipc_vfs.c`（新） | 微内核变体 | 把每个 vnodeops 调用翻译成 IPC |
| `servers/fs_server.c`（新，用户态） | 微内核变体 | 真正的 tmpfs/ISO 实现 |

关键约束：**转发层不允许缓存文件内容**，只做消息打包；否则「服务崩溃后一致性」
会变复杂。

---

## 7. 故障模型

| 事件 | 行为 |
|---|---|
| fs_server 崩溃/退出 | 端口关闭 → 内核唤醒所有等待者，返回 `-EIO`；`/` 变成 deadfs；init 重启服务 |
| fs_server 卡死 | 没有时钟中断，无法超时；只能靠 init 的看门狗（等 10 s 后 kill 重启）。**这是已知限制** |
| 共享页泄漏 | 服务退出时内核回收该进程全部映射页（已有 `free_address_space`） |
| 内核 panic | 不引入（服务只通过 syscall 进内核，和现在一样） |

---

## 8. 构建变体

```powershell
# Windows
powershell -File build_all.ps1 -Variant mono     # 默认，和现在完全一致
powershell -File build_all.ps1 -Variant micro    # 内核 + fs_server
```
```bash
# Linux
bash build_all.sh --variant mono|micro
```

- 差异集中在文件级：`ipc.c`、`ipc_vfs.c`、`servers/*.c`，其余文件共用。
- `build_iso` 把 `fs_server`/`tty_server` 装进 `/sbin`。
- 验收：`tools/all_tests.sh -Variant micro` 跑同一套 15 个用例。
- **落地顺序**：先加 `-Variant` 参数但只认 `mono`（零行为变化），S100.3 再让
  `micro` 真正生效。

---

## 9. 度量与判据（S100.4）

| 指标 | 采集方式 | 目标 |
|---|---|---|
| 每次 `open/read` 的 IPC 往返数 | 内核计数器 `ipc_sends/ipc_recvs` | 记录即可，不设上限 |
| 上下文切换次数 | `proc_sched` 计数 | 对比 mono/micro |
| 内核镜像大小 | `bsd45.elf` 字节数 | micro 应小于 mono |
| 内存占用 | `vstat` 的 `pages inuse` | 同场景对比 |
| 15 套验收耗时 | `all_tests.sh` 计时 | micro 允许慢，但必须全绿 |
| 故障隔离 | `kill fs_server` 后跑 `ls /tmp` | 内核不 TRAP，返回错误 |

---

## 10. 风险与回退

| 风险 | 缓解 |
|---|---|
| IPC 调试成本高 | 先「共享内存 + 自旋」跑通，再换会合语义 |
| 协作式调度下服务忙等卡死整机 | 服务端处理完一条消息必须立刻 `recv`；禁止在服务里忙等 |
| 共享页生命周期出错 | 谁分配谁释放 + ACK 释放协议 + 退出时统一回收 |
| 64 MB 内存不够（双份页） | 先只做 tmpfs；共享页复用同一块 |
| 改动太大不可回退 | `-Variant mono` 永远保留；主线（`bsd45/`）只合入不影响 mono 的改动 |

**硬约束（沿用 S100）**：任何方案都必须让 15 套全绿；先文档后代码；实验代码
默认在 `microkernel/` 里做，验证通过再回主线。

---

## 11. 变更记录

| 日期 | 内容 |
|---|---|
| 2026-11-15 | 初稿（S100.2）：IPC 契约、服务布局、构建变体、故障模型 |
| 2026-11-15 | S100.2a 落地：tmpfs 已拆到 `kernel/fs/tmpfs.c`（15 套全绿），`vfs.c` 只剩转发层 + ISO 后端 |
