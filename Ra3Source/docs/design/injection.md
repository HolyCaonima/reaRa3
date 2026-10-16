# 路线 A 注入与逐函数替换

## 1. 范围

本设计用于把已经重建并验证的函数逐个接入正在运行的 32 位
`ra3_1.0.game`。注入不是启动器，不修改磁盘上的游戏文件，也不处理保护代码。
当前唯一登记的游戏目标是一次性 RefPack 解码器
`ra3_1.0.game Decode (0x009F72E0)`。

`Ra3::hook` 在目标入口写入五字节 `jmp rel32`，直接跳到替换函数；32 位下 rel32
按 2^32 回绕，任意两地址之间都可达。可执行 trampoline 先执行完整的 stolen bytes，
再跳回原函数。trampoline 从共享的 64 KiB 可执行块里按 16 字节切槽：每次
`VirtualAlloc` 都要占 64 KiB 地址空间，32 位进程装不下成千上万个独立分配。

安装和卸载都执行以下事务：

1. 先在堆上准备好全部数据（trampoline、原字节、补丁字节、Detour 对象）。
2. 创建 Toolhelp32 线程快照，挂起当前进程的其他线程。
3. 读取每个线程的 EIP；EIP 落在待改写范围时恢复所有线程并重试，持续出现则失败。
4. 在所有线程保持挂起时比较内存中的 expected bytes。
5. 临时把代码页设为可写，写入补丁，刷新指令缓存并恢复原页保护。
6. 恢复所有线程。

第 2–6 步不得使用堆（线程句柄放在定长数组里，错误信息在恢复线程后才拼）：被挂起的
游戏线程可能正持有堆锁，此时再分配内存会让安装线程永久卡死。

expected bytes 必须覆盖全部 stolen bytes。RefPack 目标固定校验前 16 字节：
`51 8B 4C 24 0C 85 C9 56 8B 74 24 0C 75 07 33 C0`，只偷取前五字节的两条完整、
位置无关指令。任何字节不同都会拒绝安装。卸载也要求入口仍然等于本实例写入的
补丁，避免覆盖其他工具的改动。

## 2. SecuROM 边界

本方案不脱壳、不解密、不绕过检查，也不分析或改写 `Stext`、`Sdata`、
`.securom`。它不改入口点，不参与保护启动过程：

- 先由原版启动器正常启动游戏，等主窗口出现后才用 `CreateRemoteThread` 和
  `LoadLibraryW` 附加 DLL。
- 候选地址必须来自 `docs/analysis/ra3_1.0/function_ledger.tsv`，同时满足
  `section == .text` 和 `securom == clean`。其余状态一律不能作为 hook 目标。
- 每个可发布目标还要把地址、足够长的原字节和 stolen 指令边界写进受评审的
  hook 注册项，不能只从运行时配置任意指定替换函数。
- 内存字节校验是版本门槛，不把“地址相同”当作“版本相同”。

若游戏自身或保护系统拒绝带 hook 运行，立即停止实验，保存普通启动与注入后的
日志并记录现象；不得隐藏 DLL、禁用检查或尝试规避。

## 3. DLL 与配置

`ra3hook.dll` 的 `DllMain` 只关闭线程通知并启动 worker。读取配置、打开日志、
检查 PE 信息和安装 hook 都在 worker 上执行。配置路径优先取目标进程启动时已有的
`RA3_HOOK_CONFIG` 环境变量，否则读取 DLL 同目录的 `ra3hook.ini`。后者最适合首次
实验，因为不需要把任何文件放进游戏目录。

配置示例：

```
mode=shadow
target_rva=0x005F72E0
summary_interval_ms=1000
```

可选 `log=绝对路径`；默认写入
`%LOCALAPPDATA%\Ra3Source\logs\ra3hook.log`。日志记录主模块基址、PE 时间戳、
镜像大小、安装结果以及原子计数 `calls/matches/mismatches`。前五次 shadow
不一致另写明返回大小。`target=绝对地址` 只用于无游戏集成测试；
真实实验保持 `target_rva=0x005F72E0` 或省略该行。

注入器 `ra3inject` 接受 `--pid` 或 `--name`。名称匹配多个进程时拒绝猜测；
`--wait-window` 最多等 60 秒可见顶层窗口。它先确认目标是 32 位进程，再在目标中
运行一个小型 loader stub。stub 调用 `LoadLibraryW`，并把返回模块地址和目标进程
的 `GetLastError` 写回注入器。

## 4. shadow 到 replace

RefPack hook 的签名是
`int __stdcall Decode(uint8_t* dest, const uint8_t* src)`。

`shadow` 先通过 trampoline 调用原函数，返回值和 `dest` 对调用者保持不变。然后
用同一 `src` 调用 `ra3::io::decompressRefPackPrefix`，以上述返回大小为比较范围，
逐字节比较结果。比较代码的异常被截断为 mismatch，不能穿过游戏 ABI。

`replace` 不调用原函数。`src == nullptr` 时返回 0；否则读取 RefPack 声明大小，
用重建解码器写入 `dest` 并返回大小。无长度 ABI 意味着它和原函数一样信任输入是
可访问的完整 RefPack 流；当前只应用于游戏内部已验证的调用链。

逐函数收缩流程固定为：

1. 从台账选择 `.text/clean` 且已有等价重建实现的低风险叶函数。
2. 静态确认签名、调用约定、完整指令边界、原字节和所有已知调用方。
3. dummy 单测覆盖 expected-byte 拒绝、trampoline 和卸载。
4. 先跑 shadow；代表性场景中调用数充足且 mismatch 始终为零。
5. 新进程改跑 replace，重复相同场景并与未注入基线比较。
6. 任一阶段异常就撤回该目标，不扩大 hook 范围。

## 5. 首次真实游戏实验

以下命令只在主 agent 审核代码后执行。PowerShell 当前目录为
`D:\Game\Ra3\RevRa3-hook\Ra3Source`。配置和 DLL 都在构建目录，游戏安装目录保持
只读。

### 5.1 shadow

```
@'
mode=shadow
target_rva=0x005F72E0
summary_interval_ms=1000
'@ | Set-Content -Encoding ASCII build\win32\bin\Debug\ra3hook.ini

Remove-Item "$env:LOCALAPPDATA\Ra3Source\logs\ra3hook.log" -ErrorAction SilentlyContinue
```

用原版启动器正常启动 RA3。启动器 `RA3.exe` 本身没有 SecuROM 区段（只有
`.text/.rdata/.data/.rsrc`）；按它的字符串，它按语言挑一个 `*_*.SkuDef`，以
`-config "<SkuDef 路径>"`（可选 `-runver`）启动 SkuDef 里 `set-exe` 指定的
`Data\ra3_1.0.game`。

游戏进程出现后立即执行下面的注入命令：`--wait-window` 会在第一个可见顶层窗口出现时
注入。这时保护代码的启动阶段已经结束（窗口由游戏代码创建），而大部分 BIG 压缩成员
还没读，正好让 RefPack hook 覆盖整个加载过程。等到主菜单再注入，多数解码已经做完。

```
build\win32\bin\Debug\ra3inject.exe --name ra3_1.0.game --dll D:\Game\Ra3\RevRa3-hook\Ra3Source\build\win32\bin\Debug\ra3hook.dll --wait-window
Get-Content "$env:LOCALAPPDATA\Ra3Source\logs\ra3hook.log" -Wait
```

若同名进程不止一个，先运行 `Get-Process ra3_1.0` 查 PID，再把 `--name
ra3_1.0.game` 换成 `--pid <PID>`。注入器应报告非零 `LoadLibraryW result` 和
`GetLastError=0`。日志必须显示基址 `0x400000`、发行版时间戳和镜像大小，随后是
`installed mode=shadow`。进入一次会读取 BIG 压缩成员的正常流程；只接受
`calls` 增长且 `mismatches=0`。

### 5.2 replace

完全退出游戏。把配置第一行改成 `mode=replace`，清空日志，重新正常启动一个新
游戏进程，再执行同一条注入命令。重复 shadow 的场景，确认
`installed mode=replace`、`calls` 增长、`mismatches=0`，并检查加载、主菜单和
进入对局没有可见偏差。

### 5.3 撤销

补丁只存在于进程内。完全退出游戏即可撤销；不要在已注入进程中覆盖 DLL或重复
注入。删除构建目录旁的 `ra3hook.ini` 只会阻止下一次安装，不改变游戏文件。若
注入、保护或游戏行为出现任何异常，退出进程、保存日志并停止路线 A 实验。

## 6. 已知风险

- Toolhelp32 快照与新线程创建之间仍有理论竞态；当前事务覆盖快照中所有其他线程，
  并拒绝 EIP 位于改写范围的时刻。
- 无输入长度的原 ABI 无法在坏指针或截断流上提供内存安全保证。
- shadow 会额外分配输出缓冲并增加解码开销，不用于性能结论。
- 当前 DLL 面向“注入后随进程退出”的实验，不支持 `FreeLibrary` 热卸载。撤销方式
  是退出目标进程。进程退出时 worker 线程先被系统终止，可能正持有日志锁，所以
  `DLL_PROCESS_DETACH` 只在拿得到锁时写最后一次汇总。
- 代码完整性检查属于必须尊重的外部约束，不在本项目规避范围内。
