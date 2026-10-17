# 遭遇战 AI 基地建造（0x00890000 窗口）

48 个函数与 `ra3_1.0.game` 逐字节对照，由 `verify_skirmishai89.py` 对 Release 检查器做差分。所有权来自虚表槽或已锚定函数的直接调用，记录在 `work/skirmishai89_claims.txt`。

未进入计数的 8 个地址：`00884C40`、`008925B0`、`00892670`、`00893520`、`0089FEB0` 有调用链但没有差分用例；`0088AE10`、`0089AC90`、`0089F480` 没有落到“槽 2 本身是账本函数”的虚表或 `.cpp` 断言串。

主控复核：`0089E700`、`0089E750`（带/不带边界的插入排序）、`0089F140`（三数取中）、`0089F1D0`（按枢轴划分）是以 `0x00890600` 为比较器的 EASTL 排序算法实例，`0089DD90` 是 0x44 字节元素 vector 的区间 erase，均为 EASTL 实例化，按派单规则改为 rejected 行，计数 **43**；移植保留供 oracle 使用。手写逻辑外包内联容器操作的函数（滑动窗口裁剪、active 过滤迭代、嵌套 map 查找）保留。hook 键原为 `SkirmishAIBaseBuild.<方法名>`，主控改成与主分支一致的短小写键（下表）。

Oracle：`python Ra3Source\tools\analysis\ports\verify_skirmishai89.py` 对 48 个移植 194 例零差异；独立 mutant 检查器把加权请求比较 `>` 改为 `>=`，等优先级用例（`minPriority == 2`）检出 1 处差异。

## Hooks

六个目标都是 `explicitOnly = true`。窃取长度是整条指令、至少 5 字节，且不含会跳回被窃取区间的分支。比较在影子缓冲里完成，包装按 `__fastcall(this, edx, ...)` 转发 thiscall。`canAffordRequest` 的三个浮点参数走 XMM/栈，返回值是 AL，不经过 x87。

| Key | VA | Expected bytes | explicitOnly |
| --- | --- | --- | --- |
| skaientry20bit | 0x00882AE0 | 8B098B81100100008B910C010000568D | true |
| skaifindtemplate | 0x00887300 | 568B7424088D511C578B7A0433C98BFF | true |
| skaigrouphastemplate | 0x0088D120 | 518B410C53558B6914568B71103BF557 | true |
| skaicanafford | 0x008900B0 | F30F10442408D9EEF30F106C240C568D | true |
| skaibasebetter | 0x00890600 | 83EC088B44240C85C0568BF189742404 | true |
| skaibestbase | 0x008908C0 | 83EC2033C083C8048944240C8B442428 | true |
