# RA3 类与资源处理器图谱

## 1. 范围与结论

本轮只静态读取 `ra3_1.0.game`，没有运行游戏，也没有修改游戏目录。函数分析严格
限制为 `function_ledger.tsv` 中 `section == .text` 且 `securom == clean` 的记录。
逐表结果见：

- `docs/analysis/ra3_1.0/classes.tsv`
- `docs/analysis/ra3_1.0/asset_handler_vtable.tsv`
- `docs/analysis/ra3_1.0/asset_handler_methods.tsv`
- `docs/analysis/ra3_1.0/vclass_annotations.tsv`
- `docs/analysis/ra3_1.0/class_recovery_stats.tsv`

最重要的结论是：**2,977 个既有虚表候选中没有一个具有有效的 MSVC Complete
Object Locator，因此不能用 RTTI 批量恢复 RA3 类名或继承图。** 文件字节中虽能搜到
83 个 `.?AV` 和 16 个 `.?AU` 字符串，但任一候选的 `vtable[-1]` 都不能按 x86
MSVC COL 结构验证到这些 TypeDescriptor；仅有 mangled 字符串不能证明某张虚表带
RTTI。[程序][全量]

## 2. 方法与再生成

`tools/analysis/recover_classes.py` 直接解析 PE32 节表、虚表清单、资源类型表和函数
台账，并用 Capstone 解码明文 `.text`。流程如下：

1. 对每个候选检查 `vtable[-1] -> COL -> TypeDescriptor(".?AV/.?AU")`，并同时验证
   COL 签名、地址范围和 ClassHierarchyDescriptor 可读性。
2. 把 164 个 `0x00417220` 注册描述符中的处理器地址作为可信虚表边界。既有
   `RecoverVtables.java` 记录的是“最长连续代码指针段”，相邻虚表间没有非代码
   指针时会合并；因此 164 张处理器表只有 15 张恰好也是原候选起点。
3. 在 clean 函数内跟踪 `mov reg, vtable; mov [this], reg` 和
   `mov [this], vtable`。同一函数中先写 A、后写 B 产生候选继承边 A → B，最后写入
   的表关联为该构造函数的派生表。
4. 资源处理器固定读取 5 槽；每个目标再次核对必须位于 `.text` 且台账为 clean。
   输出实现复用计数、分配大小、字段偏移和前 12 条指令形状。

从 `Ra3Source` 目录执行：

```powershell
python tools/analysis/recover_classes.py `
  --exe D:\Game\Ra3\Game\Data\ra3_1.0.game `
  --source-root D:\Game\Ra3\RevRa3-vclass\Ra3Source
```

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

## 5. 类图统计与可信度

| 指标 | 数量 |
|---|---:|
| 既有连续代码指针候选 | 2,977 |
| 已注册处理器可信边界 | 164 |
| 去重后的 `classes.tsv` 行 | 3,126 |
| RTTI COL | 0 |
| 已命名类（全部来自资源注册） | 164 |
| 合成名类 | 2,962 |
| 含构造函数候选的表 | 1,036 |
| 可关联 clean 首槽析构函数的表 | 2,380 |
| 有源文件关联的表 | 102 |
| 有序 vptr 写入产生的候选边 | 805 |

这里的 805 条边是**候选继承边**，不是已经证明的完整 C++ 层级。原因有二：既有
2,977 行是连续指针段起点，不一定是真实类边界；其次同一函数中的多次 vptr 写入
也可能来自成员子对象或多重继承。`classes.tsv` 对多入边只把出现次数最高者放进
`base_vtable`，同时在 evidence 保留全部候选，避免把启发式结果伪装成确定事实。

## 6. 未决问题

1. 用构造函数写入地址、析构函数重写地址和直接虚调用三种证据重新切分连续指针段，
   才能把 2,977 个“段”提升为精确的真实虚表集合。
2. 槽 4 的行为已明确，但原始接口名未知；`resolve_missing_prototype` 是描述性名称。
3. 槽 2 的第三个参数在已反编译代表实现中未使用；需要读调用点确定它是标志、
   allocator/context 还是为 ABI 统一保留。
4. 除 164 个资源处理器外，当前没有可靠类名来源。后续可把 clean 方法中的断言文本、
   源路径、全局对象初始化器与 Generals 祖先源码逐类交叉验证，但不能仅凭相似字符串
   宣称类名成立。
5. 文件中的 99 个 MSVC 风格 TypeDescriptor 字符串属于何种异常/第三方元数据仍未
   逐项归属；本轮结论只是否定它们作为这批虚表的 COL 链。
