# monolithickernel/（预留目录，当前为空）

这个目录预留给**宏内核变体**（方案 A：在现有 `bsd45/` 基线上加固）。

现在为空，是因为主线本身就是宏内核：仓库根的 `bsd45/` 就是方案 A 的代码，
15 套验收全绿。再复制一份只会带来双份维护成本，没有任何收益。

将来这里会有：

```
monolithickernel/
└── bsd45/                  # 宏内核最终交付快照（或加固后的分支）
```

与根 `bsd45/` 的关系：

- 只要 `bsd45/` 仍然是宏内核，这个目录就保持为空；
- 当 S100.3 的 `-Variant micro` 跑通、两套内核需要并行交付时，
  这里放宏内核那一侧的发布快照（`-Variant mono` 构建出的 ISO + ELF）。

现在该看什么：

- [`../docs/kernel-boundary.md`](../docs/kernel-boundary.md)：哪些模块必须留 ring0（本目录对应这一侧）
- [`../docs/microkernel-design.md`](../docs/microkernel-design.md)：方案 A 与方案 B 的成本对照
