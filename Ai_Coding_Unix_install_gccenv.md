# Windows 开发环境安装指南（MSYS2 + gcc + qemu）

> 来源：2026-11-13 在本机的实战记录。新机器装环境时，按本文顺序执行即可，文末附踩坑记录。

## 环境清单

| 组件 | 版本参考 | 路径 |
|------|---------|------|
| MSYS2 | 最新版 | `C:\msys64` |
| GCC | 16.x | `C:\msys64\ucrt64\bin\gcc.exe` |
| QEMU | 11.x | `C:\msys64\ucrt64\bin\qemu-system-x86_64.exe` |
| binutils（objcopy/ld/ar/as/nm/objdump 等） | 2.47 | 随 gcc 自动作为依赖安装 |

## 一键复现步骤

### 1. 下载并安装 MSYS2（静默安装）

```bash
# 下载（约 81MB，国内推荐 TUNA）
curl -L -o "$USERPROFILE/Downloads/msys2-x86_64-latest.exe" \
  https://mirrors.tuna.tsinghua.edu.cn/msys2/distrib/msys2-x86_64-latest.exe

# 静默安装到 C:\msys64
"$USERPROFILE/Downloads/msys2-x86_64-latest.exe" install --root C:/msys64 --confirm-command
```

⚠️ 安装器可能会在最后初始化阶段卡住或被中断，**先验证**再继续：

```bash
# 验证核心文件存在
ls C:/msys64/usr/bin/pacman.exe C:/msys64/usr/bin/bash.exe
```

文件在就算安装成功，初始化问题后面有绕过办法。

### 2. 配置镜像源（清华 TUNA，实测完整可靠）

编辑 `C:\msys64\etc\pacman.d\mirrorlist.mingw`：

```
Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/mingw/ucrt64/
```

编辑 `C:\msys64\etc\pacman.d\mirrorlist.msys`：

```
Server = https://mirrors.tuna.tsinghua.edu.cn/msys2/msys/x86_64/
```

### 3. 绕过签名校验（关键！）

编辑 `C:\msys64\etc\pacman.conf`，找到 `SigLevel`，改为：

```
#SigLevel    = Required
SigLevel    = Never
```

同时把用不到的仓库注释掉，只保留（本环境实测可行）：

```
[ucrt64]
Include = /etc/pacman.d/mirrorlist.mingw

[msys]
Include = /etc/pacman.d/mirrorlist.msys
```

### 4. 同步数据库并安装 gcc + qemu

```bash
C:/msys64/usr/bin/pacman.exe -Sy
C:/msys64/usr/bin/pacman.exe -S --noconfirm --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-qemu
```

约 2~3 分钟（含依赖共数百 MB）。binutils（objcopy、ld 等）会自动装上，无需单独装。

### 5. 验证

```bash
C:/msys64/ucrt64/bin/gcc.exe --version
C:/msys64/ucrt64/bin/qemu-system-x86_64.exe --version
C:/msys64/ucrt64/bin/objcopy.exe --version
C:/msys64/ucrt64/bin/ld.exe --version
```

## 日常使用

- 打开 `C:\msys64\ucrt64.exe` 终端直接用 `gcc` / `qemu-system-x86_64` / `objcopy`
- 或把 `C:\msys64\ucrt64\bin` 加入系统 PATH，任意终端可用
- 装其它工具：`pacman -S --noconfirm mingw-w64-ucrt-x86_64-<包名>`（如 gdb、make、ninja）

---

## 踩坑记录（新机器遇到同样问题时直接对号入座）

### 坑 1：pacman 报 "Public keyring not found" 后卡住
- **原因**：安装器最后一步 bash 初始化被中断，`pacman-key --init` 没跑过
- **对策**：不要尝试修 keyring，直接按上面第 3 步设 `SigLevel = Never` 绕过

### 坑 2：pacman-key --init 死循环刷 "removing stale lockfile"
- **原因**：gpg 在本机环境生成主密钥会卡死（bash -lc、msys2.exe 两种方式都复现）
- **对策**：同上，`SigLevel = Never`。这是选择绕过的根本原因，**不要再浪费时间试 pacman-key**

### 坑 3：南大镜像（mirror.nju.edu.cn/msys2）大量 404
- **现象**：.db 和部分包文件 404；它家 mingw 仓库还不支持 `$arch` 变量（要用固定路径）
- **对策**：直接用 TUNA 镜像（实测完整）。备选：官方 `repo.msys2.org` 也验证过可用；中科大 403 不可用
- **排查手法**：用 curl 直接探测具体包文件 URL（返回 200 才算有），不要信镜像名气

### 坑 4：长任务用 shell `&` 放后台会随命令结束被杀
- **对策**：用持久后台任务机制跑 pacman，输出保留下来便于检查

## 排查卡住命令的通用流程

1. **输出落盘**：`命令 > log 2>&1 &`，然后 `tail` 看它在等什么
2. **限时换路**：一个方法卡超过 1 分钟就换（先想"能不能绕过"，再想"怎么修"）
3. **直接验证**：curl 探文件、`--debug` 看实际 URL、`pacman -Q` 查已装包
4. **横向对比**：一个源坏了同时测多个备选，选实测能用的
