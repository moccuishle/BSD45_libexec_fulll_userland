# microkernel/（预留目录，当前为空）

这个目录预留给**微内核变体**。现在是空的，是有意为之：

按 `docs/kernel-boundary.md` 的「晚分叉 ≠ 晚设计」原则，微内核化是
**在单基线里重构、最后才分叉**。在 S100.3（tmpfs → `fs_server`）跑通之前，
开发只发生在仓库根的 `bsd45/`，不维护第二份代码——两份 2301 文件的副本已经
证明过一次：任何 bug 都要改两遍、验两遍。

将来这里会有：

```
microkernel/
└── bsd45/                  # 从主线打出的微内核快照（或最终交付目录）
    └── kernel/other/ipc.c        # 新增：端口/消息/共享页
    └── kernel/fs/ipc_vfs.c       # 新增：vnodeops → IPC 的转发
    └── servers/fs_server.c       # 新增：用户态文件服务
```

与主线的差异应当只有上面这几个文件，其余全部共用。

现在该看什么：

- [`../docs/kernel-boundary.md`](../docs/kernel-boundary.md)：KP-1..KP-9 原语契约、模块可上移性、违规审计
- [`../docs/microkernel-design.md`](../docs/microkernel-design.md)：IPC 消息/端口/会合语义、服务启动、故障模型
- [`../bsd45/RESUME-TASK.md`](../bsd45/RESUME-TASK.md)：当前进度（0.13 / 0.14 节）
