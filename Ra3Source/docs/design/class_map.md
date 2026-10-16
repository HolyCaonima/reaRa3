# RA3 类与资源处理器图谱

## 1. 范围与结论

本轮只静态读取 `ra3_1.0.game`，没有运行游戏，也没有修改游戏目录。函数分析严格
限制为 `function_ledger.tsv` 中 `section == .text` 且 `securom == clean` 的记录。
逐表结果见：

- `docs/analysis/ra3_1.0/vtables_split.tsv`
- `docs/analysis/ra3_1.0/vtable_split_stats.tsv`
- `docs/analysis/ra3_1.0/classes.tsv`
- `docs/analysis/ra3_1.0/asset_handler_vtable.tsv`
- `docs/analysis/ra3_1.0/asset_handler_methods.tsv`
- `docs/analysis/ra3_1.0/vsplit_annotations.tsv`
- `docs/analysis/ra3_1.0/class_recovery_stats.tsv`

原来的 2,977 行是连续代码指针段，不是 2,977 张已确认虚表。clean `.text` 中的
立即数引用/vptr 写入以及 `.rdata/.data` 指针把其中 955 段切成 7,442 个
证据边界片段；未被引用的长片段仍可能包含未发现的边界，故 7,442 是当前候选数，
不是已经证明的 C++ 类数。

另一个重要结论是：**这些候选中没有一个具有有效的 MSVC Complete
Object Locator，因此不能用 RTTI 批量恢复 RA3 类名或继承图。** 文件字节中虽能搜到
83 个 `.?AV` 和 16 个 `.?AU` 字符串，但任一候选的 `vtable[-1]` 都不能按 x86
MSVC COL 结构验证到这些 TypeDescriptor；仅有 mangled 字符串不能证明某张虚表带
RTTI。[程序][全量]

## 2. 方法与再生成

先运行 `tools/analysis/split_vtables.py`，再运行
`tools/analysis/recover_classes.py`。两者直接解析 PE32 节表、虚表清单、资源类型表和
函数台账，并用 Capstone 解码明文 `.text`。流程如下：

1. 把原始行解释为 `[vtable, vtable + slots*4)` 连续段；只解码
   `function_ledger.tsv` 中 `.text / clean` 的 40,333 个函数，跳过 1,212 个非 clean
   函数。段内立即数引用、对象内存写入、`.rdata/.data` 对齐指针、资源处理器注册地址
   都成为边界。`direct_slot_ref` 单独标出，因为它可能只是访问表中槽而不是真实起点。
2. 对每个切分候选检查 `vtable[-1] -> COL -> TypeDescriptor(".?AV/.?AU")`，并同时验证
   COL 签名、地址范围和 ClassHierarchyDescriptor 可读性。
3. 把 164 个 `0x00417220` 注册描述符中的处理器地址作为可信虚表边界。既有
   `RecoverVtables.java` 记录的是“最长连续代码指针段”，相邻虚表间没有非代码
   指针时会合并；因此 164 张处理器表只有 15 张恰好也是原候选起点。
4. 在 clean 函数内跟踪 `mov reg, vtable; mov [this], reg` 和
   `mov [this], vtable`。同一函数中先写 A、后写 B 产生候选继承边 A → B，最后写入
   的表关联为该构造函数的派生表。直接调用另一个 vptr 写入函数和首槽析构链只用于
   佐证已有边，不独立创建继承边；共享槽前缀记录在 evidence 中。
5. 资源处理器固定读取 5 槽；每个目标再次核对必须位于 `.text` 且台账为 clean。
   输出实现复用计数、分配大小、字段偏移和前 12 条指令形状。

从 `Ra3Source` 目录执行：

```powershell
python tools/analysis/split_vtables.py `
  --exe D:\Game\Ra3\Game\Data\ra3_1.0.game `
  --source-root D:\Game\Ra3\RevRa3\Ra3Source

python tools/analysis/recover_classes.py `
  --exe D:\Game\Ra3\Game\Data\ra3_1.0.game `
  --source-root D:\Game\Ra3\RevRa3\Ra3Source
```

也可用 `--vtables PATH` 显式选择输入；默认优先读取 `vtables_split.tsv`。

代表实现使用 Ghidra 12.1.4 的私有工程反编译；辅助脚本会先读台账并拒绝非 clean
函数。命令示例：

```powershell
$env:GHIDRA_INSTALL_DIR='D:\Game\Ra3\RevRa3\ghidra_12.1.4_PUBLIC'
$env:JAVA_HOME='C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot'
.\tools\ghidra\run_headless.ps1 `
  -ProjectDir D:\Game\Ra3\RevRa3\work\ghidra_vclass `
  -LogDir D:\Game\Ra3\RevRa3\work\vclass\logs `
  -Script DecompileCleanFunctions.java `
  -ScriptArgs D:\Game\Ra3\RevRa3\work\vclass\handler_decompilations.txt,`
    D:\Game\Ra3\RevRa3-vclass\Ra3Source\docs\analysis\ra3_1.0\function_ledger.tsv,`
    00AAC690,004D5C10,00522910,007E2E40,004A6560
```

## 3. 资源处理器接口

全部 164 张已注册处理器虚表都是 5 槽。下面的“覆盖数”是相对该槽最常见实现的
处理器数量，不是唯一函数数量。

| 槽 | 偏移 | 推断方法 | 最常见实现 | 共享数 | 覆盖数 | 证据 |
|---:|---:|---|---:|---:|---:|---|
| 0 | `0x00` | `scalar_deleting_destructor` | `0x00AAC690` | 162 | 2 | Ghidra：调用清理函数，按 `flags & 1` 释放 `this`，`ret 4` |
| 1 | `0x04` | `create_instance` | `0x004D5C10` | 119 | 45 | 尾调被包装对象虚槽 `+0x14`；其他族直接分配/构造 |
| 2 | `0x08` | `copy_construct_in_place` | `0x00522910` | 119 | 45 | 尾调被包装对象虚槽 `+0x1C`；4 字节族把源写入非空目标；DamageFX 特化以目标为 `this` 调复制构造 |
| 3 | `0x0C` | `destroy_instance` | `0x007E2E40` | 119 | 45 | 尾调被包装对象虚槽 `+0x18`；其他族清空持有槽、释放内存或调用特化析构 |
| 4 | `0x10` | `resolve_missing_prototype` | `0x004A6560` | 164 | 0 | 计算 `MissingPrototype` 名称哈希并从 `this+4` 的表查询；确切公开 API 名仍是推断 |

槽 1–3 的参数语义来自反编译而非名称猜测：`0x00A79730` 分配 4 字节并复制输入，
`0x00688BB0` 在目标非空时复制同一值，`0x00531250` 释放实例，三者构成完整的
create / in-place copy / destroy 族。`0x005AC8C0` 分配 `0x1C8` 字节，
`0x005AC8E0` 对非空目标调用复制构造，是类型特化的独立佐证。[程序]

## 4. 处理器模板族

164 个处理器不是 164 套独立代码，而是少数模板策略加类型特化：

- **119 个多态资源处理器**复用 `0x004D5C10 / 0x00522910 / 0x007E2E40`。这三个
  函数分别尾调被包装对象的虚槽 5、7、6，说明处理器把生命周期操作转交给资源类。
  该族包括 `GameObject`、`WeaponTemplate`、`Texture`、`FXList`、`W3DMesh` 等。
- **20 个单例/设置处理器**复用 `0x008D69B0 / 0x009AB7C0 / 0x0054A1C0`。
  create 只在 `this+0x10` 指向的槽为空时登记输入，copy 是空操作，destroy 清空槽。
  `DamageFXSettings`、`CameraSettings`、`NetworkSettings` 等都在此族。
- **9 个 4 字节值处理器**复用 `0x00A79730 / 0x00688BB0`：create 分配 4 字节，
  copy 写一个 4 字节值；destroy 多数走通用释放函数。该族包括 `ArmorTemplate`、
  `ObjectFilterAsset`、`W3DHierarchy`、`PlayerTechRank` 等。
- 其余槽实现是少量类型特化。例如 `DamageFX` create 的分配大小为 `0x1C8`；
  `StringHashTable` 为 36 字节；`AssetReportTable` 为 28 字节。完整常量和字段偏移
  在 `asset_handler_methods.tsv`，不能把分配大小等同于 manifest 中每个实例的
  序列化块长。

`vclass_annotations.tsv`（生成器输出；合并时已并入 `ledger_annotations.tsv` 并删除，
重新生成后需同样并入）对 49 个不同处理器槽函数给出建议符号。只有本轮实际由
Ghidra 反编译并理解语义的 14 个函数标为 `reversed`；其余 35 个仅有槽归类和建议
名称，按协议保持空状态。

## 5. 切分与类图统计

| 指标 | 数量 |
|---|---:|
| 原连续代码指针段 | 2,977 |
| 被切分的原段 | 955 |
| 切分后边界片段 / `classes.tsv` 行 | 7,442 |
| 仅有原段起点/处理器结束证据的片段 | 1,092 |
| primary / secondary vptr 写入边界 | 2,905 / 1,576 |
| text immediate / data pointer / direct-slot 边界 | 5,688 / 644 / 41 |
| 已注册处理器可信边界 | 164 |
| 处理器恰为 5 槽 | 164 / 164 |
| BigHandler base / derived 边界 | 2 / 2（各到下一边界 19 槽） |
| RTTI COL | 0 |
| 已命名类（全部来自资源注册） | 164 |
| 合成名类 | 7,278 |
| 含 vptr 写入的 clean 函数 | 3,763 |
| 有序 vptr 写入产生的候选边 | 3,721 |
| 另有 base-ctor 调用佐证的边 | 34 |
| 另有析构链佐证的边 | 1 |

槽数最高频为 1: 1,285、2: 848、3: 927、4: 964、5: 524、6: 338、7: 215、
8: 392；完整直方图见 `vtable_split_stats.tsv`。较大的剩余片段主要是没有更多引用的
连续段，不能据此断言它们是一张大虚表。

3,721 条边仍是**候选继承边**，不是已经证明的完整 C++ 层级。同一函数中的多次
vptr 写入也可能来自成员子对象、多重继承或析构过程。边数相对旧表的 805 增长主要
因为原先合并在段中间的写入地址现在成为可关联的表起点。`classes.tsv` 对多入边只把
出现次数最高者放进 `base_vtable`，并在 evidence 保留全部候选及佐证类型。

## 6. 未决问题

1. 1,092 个片段没有独立起点引用；特别是较长片段仍可能继续合并相邻虚表。需要更多
   构造/析构调用点或逐对象的数据流才能确认。
2. 槽 4 的行为已明确，但原始接口名未知；`resolve_missing_prototype` 是描述性名称。
3. 槽 2 的第三个参数在已反编译代表实现中未使用；需要读调用点确定它是标志、
   allocator/context 还是为 ABI 统一保留。
4. 除 164 个资源处理器外，当前没有可靠类名来源。后续可把 clean 方法中的断言文本、
   源路径、全局对象初始化器与 Generals 祖先源码逐类交叉验证，但不能仅凭相似字符串
   宣称类名成立。
5. 文件中的 99 个 MSVC 风格 TypeDescriptor 字符串属于何种异常/第三方元数据仍未
   逐项归属；本轮结论只是否定它们作为这批虚表的 COL 链。
6. `direct_slot_ref` 的 41 个边界可能只是绝对地址访问某个槽；输出有意保留独立证据
   标签，使用者不应把这类边界视为与 vptr 写入同等强度。
