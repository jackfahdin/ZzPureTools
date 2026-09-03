# SDD ledger — plan: docs/superpowers/plans/2026-09-02-multi-window-workspace.md

## 任务状态

- Task 1: pending
- Task 2: pending
- Task 3: pending
- Task 4: pending
- Task 5: pending
- Task 6: pending
- Task 7: pending
- Task 8: pending
- Task 9: pending
- Task 10: pending
- Task 11: pending
- Task 12: pending
- Task 13: pending
- Task 14: pending

Task 1: minor (deferred): `ZzWorkspacePageIdTest.cpp` 未直接断言公开默认构造的 ID 无效；最终审查时复核。
Task 1: complete (commits 34e5780..f2d9918, review clean)
Task 2: fix round 1/5 (3 addressed, 5 open — 生命周期、双边回滚、完整审计、Doxygen 与故障测试; commits 6a1b95d..c1a949f)
Task 2: fix round 2/5 (2 addressed, 3 open — 工作区级回滚、完整审计与故障测试; commits c1a949f..cc06428)
Task 2: fix round 3/5 (0 addressed, 3 open — 实现者只留下未提交测试，未完成事务实现; no commit)
Task 2: fix round 4/5 (3 addressed, 3 open — 非末尾插入、第三方登记清理与回滚结果; commits cc06428..20fab9a)
Task 2: fix round 5/5 (3 addressed, 0 open — 目标插槽、接管清理和回滚结果已补齐; commits 20fab9a..44682c9)
Task 2: complete (commits f2d9918..44682c9, review clean after round 5)
Task 3: fix round 1/5 (部分修复了立即失效、过期清理、pageId 校验和旧 v1 路径; commits c635f44..d7fb174)
Task 3: fix round 2/5 (补齐时钟边界、伪造载荷、来源变化、跨线程和高频失效测试; commits d7fb174..0773c09)
Task 3: fix round 3/5 (修复工作区活动令牌记录、切换失效和空注册表保护; commits 0773c09..e153db7)
Task 3: fix round 4/5 (增加来源移走检测并回收连接句柄; commits e153db7..8459354)
Task 3: fix round 5/5 (统一过期/来源清理路径并检查 Workspace consume 返回值; commits 8459354..bc861b6)
Task 3: BLOCKED — 最终复审仍确认来源页面移走的即时失效没有可靠事件依据；TabBar/Workspace 均在页面迁移后消费令牌，消费失败不能原子回滚且 TabBar 仍忽略结果。应用销毁与 v2 MIME 集成测试属于未完成但非当前承重原因。审查报告：task-3-rereview3.md。
Task 3: fix round 1/5 (1 addressed, 1 open — TabBar 预留后失败重试已补齐；应用析构生命周期仍需独立归因; commits 571b67b..781910f)
Task 3: fix round 2/5 (1 addressed, 0 open — 应用析构前保持工作区和页面存活，注册表由 QApplication 析构清理; commit 48e8edf)
Task 3: complete (commits 44682c9..48e8edf, review clean after recovery rounds)
Task 4: fix round 1/5 (覆盖信号顺序、RTL 物理方向、失败静默和容量/深度边界；1 open — 静默边缘分割后未重建可视布局; commits cc5b0c8..0249377)
Task 4: fix round 2/5 (1 addressed, 0 open — 成功边缘事务重建目标可视布局并补充层级/几何断言; commit 567e91e)
Task 4: complete (commits 48e8edf..567e91e, review clean after round 2)
Task 5: complete (commits c4b18c7..6ae675a, review clean)
Task 6: minor (deferred): `destroyedWindowUnregistersItsRecordBeforeApplicationErase` 在窗口析构后把悬空地址作为只比较、不解引用的身份传回公共查询；最终审查时评估是否需改为更可移植的可观察证据。
Task 6: minor (deferred): 跨线程测试覆盖了异线程调用早退，但未独立命中“协调器线程调用、Window/Shell 对象亲和性不匹配”分支；最终审查时复核覆盖必要性。
Task 6: fix round 1/5 (1 addressed, 0 open — 为 Builder、Coordinator 和 MultiWindow 三类 CTest 增加跨平台构建树动态库环境; commit 1b4c76b)
Task 6: complete (commits 2681ab1..1b4c76b, review clean after round 1)
Task 7: fix round 1/5 (4 addressed, 1 open — 修复 tearOff 线程/关闭态、配置失败原子性、工厂异常和 Fluent CTest 环境；仍缺有效的配置失败表面测试; commit 0cfbeca)
Task 7: fix round 2/5 (0 addressed, 1 open — 新测试覆盖窗口表面和快照，但 Shell 标题/模式未真实写入，无法证明回滚; commit 3cdc712)
Task 7: fix round 3/5 (1 addressed, 0 open — 将唯一可失败的置顶操作前置预检，以零表面修改保证失败原子性并补充生产变异测试; commit 916e30f)
Task 7: complete (commits c86dcb5..916e30f, review clean after round 3)
