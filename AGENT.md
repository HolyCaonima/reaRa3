这是一个对ra3进行反编译的工程。
rea是逆向mcp工具服务
ra3的本体放置在D:\Game\Ra3\Game
在反编译过程中，不应该也不允许对游戏本体进行修改或者改动里面文件夹的任何内容，它应该对你来说是只读的。
在任何编码过程中都应该考虑框架设计，即使是在逆向过程中。
Ra3Source里应该放目前逆向出来的各种进度都放在里面，包括cmake以及各种内容
逆向过程中不移动ra3的data部分，应该使用配置文件，指引data目录，在当前项目里，data应该直接引用D:\Game\Ra3\Game里的data，不做另行拷贝。
少测试，多推进，测试很浪费时间。
速度优先：游戏运行时验证能少就少，单批靠离线 oracle，累计几百到几千个函数后再跑一次实机 shadow + replace。
子 agent 用 gpt-5.6-sol-medium，保持 3–5 个并行。
受保护代码（游戏自己的 inner-patched / entry-lost / entry-recoverable 函数）也要重建：以运行中进程里已解密的原生代码和动态输入输出为准；被虚拟化的只做行为等价重建，不逆向 SecuROM 自身的 VM/DRM；破坏可运行性或触发完整性检查就停下记录，不绕过。
新 agent 先读 Ra3Source/docs/HANDOFF.md。