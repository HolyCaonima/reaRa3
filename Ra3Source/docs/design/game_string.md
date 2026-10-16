# 引擎字符串类 StringBase

本文记录英文 1.0 `ra3_1.0.game` 引用计数字符串类的静态恢复、离线差分和实机结果。
地址以 `0x00400000` 为映像基址；只移植和 hook 台账中 `.text / clean` 的函数。
调试路径 `C:\BF_QABuild_RA3_EALA-BUILD108\Ra3\code\Libraries\Source\string\StringBase.cpp`
说明它是一个字符类型模板，有 `char` 与 `wchar_t` 两个实例。整体结构与 GPL
Generals 源码的 `AsciiString` 相同（单指针对象、引用计数、`ensureUniqueBufferOfSize`），
但 RA3 把块头移到字符之前、增加了复制源对象和类级锁，因此所有细节取自反汇编。

## 1. 内存模型

字符串对象只有一个指针 `data`，指向字符；空串为 `nullptr`。块头紧挨在字符之前：

| 偏移（相对 `data`） | 字段 |
|---:|---|
| `-8` | `int32 refCount` |
| `-4` | `uint16 length`（不含结尾 0） |
| `-2` | `uint16 capacity`（可容纳字符数，含结尾 0） |

块由全局分配器 `0x004165A0(size, 0, 0)` 分配，`0x004166B0(block)` 释放（均 cdecl）。
`size = ((请求 + 3) >> 2) << 2`，请求为 ASCII `needed + 9`、Unicode `2*needed + 10`；
`capacity` 为 ASCII `size - 8`、Unicode `(size - 8) >> 1`。

每个实例有一把类级锁：getter `0x004CA440`（ASCII）/`0x004CA490`（Unicode）首次调用时
分配 0x28 字节对象（vtable `0x00BD9004`，`+8` 为 `InitializeCriticalSection` 的
临界区）并缓存在 `0x00DEACF0`/`0x00DEACF8`。加锁调用 vtable 槽 0（thiscall，参数 -1），
解锁直接 `LeaveCriticalSection(lock + 8)`（IAT `0x00BBA1E8`）。锁只保护引用计数和
指针交换；独占块上的原地修改不加锁。

复制源是带 vtable 的对象，槽 3 `copyTo(dest)`（thiscall）写入字符并返回字符数。
`concat`/`set` 在栈上构造 `{vtable, text, length}`：ASCII vtable `0x00BDAB6C`
（`copyTo` = `0x004CB040`），Unicode `0x00BDAB7C`（`0x004CB110`），两者都是
`memcpy(dest, text, length * sizeof(Char))`。

## 2. 语义

`ensureUniqueBufferOfSize(needed, preserve, copySource, catSource)`
（`0x004CA710`/`0x004CABA0`，ret 0x10，`preserve` 只读低字节）：

1. 现有块 `capacity > needed`（有符号）且 `refCount == 1` 时原地处理：有 copy 源则
   `length = copyTo(data)`，有 cat 源则 `length += copyTo(data + length)`，再写结尾 0。
   不加锁，`preserve` 不影响该路径（无 copy 源时保留旧长度）。
2. 否则若 `capacity <= needed` 且有 cat 源，`needed = max(needed, cap + cap/2 - 1)`；
   共享块（`refCount != 1`）不增长。
3. 请求超过 0x7FFF 字节时原函数进入 inner-patched 的报告代码；移植返回 false。
4. 分配新块，`refCount = 1`；有旧块且 `preserve` 时复制旧字符与长度，否则长度 0；
   再按第 1 步处理 copy/cat 源并写结尾 0。
5. 加锁，旧块 `--refCount`（到 0 释放），`data = 0`，解锁，最后 `data = 新块`。

其余入口：

| VA | 实例 | 作用 |
|---|---|---|
| `0x004CA8F0` / `0x004CAD90` | char / wchar_t | 拷贝构造：加锁共享对方块（`++refCount`），返回 this |
| `0x004CA9B0` | char | 赋值：加锁；非自赋值时（再加锁）释放自身块、共享对方块 |
| `0x004CAB60` | wchar_t | 释放：加锁，`--refCount`（到 0 释放），`data = 0` |
| `0x004CAA30` / `0x004CAA80` | char | `ensure(length, 1, 0, 0)` 后逐字节 `tolower`/`toupper`（符号扩展） |
| `0x004CAF40` | wchar_t | 同上，`towlower`（零扩展） |
| `0x004CAF90` | wchar_t | `ensure(length, 1, 0, 0)`，长度减一；结果为空时尾跳到释放 |
| `0x004CB9B0` | wchar_t | `set(text, n)`：`text == data` 时不动；`n == 0` 时释放；否则 `ensure(n, 0, 源, 0)` |
| `0x004CB6E0` / `0x004CBA00` | char / wchar_t | `concat(text, n)`：`n == 0` 不动；空串时 ASCII 转 inner-patched 的 `0x004CB690`、Unicode 调 `set`；否则 `ensure(length + n, 1, 0, 源)` |

ASCII 释放 `0x004CA930`、Unicode 赋值 `0x004CAE10` 和 ASCII 设置 `0x004CB690` 为
inner-patched，没有分析，也没有 hook；`0x004CA660`（非空判断）的 stolen 前缀含 rel8
跳转，未接入。

## 3. 实现

`ra3/common/GameString.h` 声明布局与 `GameStringOps`（锁、分配、释放、文本源 vtable
由调用方注入），`GameString.cpp` 用 `char`/`char16_t` 模板实现上表全部语义。原函数会
转入 inner-patched 代码的情形在任何副作用之前返回 false，由 hook 改调原函数。

## 4. 验证

`tools/analysis/ports/verify_game_string.py` 生成随机场景（1–3 个块，任意引用计数、
容量、结尾 0 之后的垃圾字节，2–3 个可共享块的字符串对象），在 Unicorn 中执行 13 个
原函数（锁 getter、分配、释放、`memcpy`、CRT 大小写函数为桩），`ra3_stringcheck` 对
同一场景执行移植。两边输出有序的加锁/分配/释放轨迹、每个对象指向的块和每个存活块的
头与全部字节。每个函数 1000 例加 18 个边界例（最大可接受请求与第一个须拒绝的请求、
满块追加增长）共 13,018 例，0 不一致；人为改动增长公式或把分配移入锁内分别产生 46 和
869 个不一致。

实机见 `injection.md` §5.4 运行 29（shadow，13 个目标约 430 万次调用 0 不一致）和
运行 30（76 个目标全部 replace，对局 2 分钟正常）。
