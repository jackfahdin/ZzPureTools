# SDD ledger — plan: docs/superpowers/plans/2026-09-02-multi-window-workspace.md

## 任务状态

- Task 1: complete
- Task 2: complete
- Task 3: complete
- Task 4: complete
- Task 5: complete
- Task 6: complete
- Task 7: complete
- Task 8: complete
- Task 9: complete
- Task 10: complete
- Task 11: complete
- Task 12: complete
- Task 13: complete
- Task 14: complete

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
Task 8: fix round 1/5 (7 addressed, 1 open — 关闭状态、顺序快照、精确抑制与审计已加固；静默迁移和最终通知仍有公开信号边界; commits 13e1678..94aa86c)
Task 8: fix round 2/5 (2 addressed, 1 open — 关闭前通知与后续过滤器拒绝已解决；windowAboutToClose 仍可销毁目标或让页面回流; commits 94aa86c..f138abd)
Task 8: fix round 3/5 (1 addressed, 1 open — 最终通知已移至迁移前并冻结工作区；ZzTabWidget 公开变更仍可绕过冻结且失败不恢复; commits f138abd..00a3e41)
Task 8: fix round 4/5 (0 addressed, 2 open — 标签可见性和独立 TabBar 条目仍可绕过冻结/快照；FluentUI 公共头反向依赖 PureTools 私有类; commits 00a3e41..ab91613)
Task 8: fix round 5/5 (2 addressed, 0 open — 补齐可见性与独立 TabBar 条目快照，并以 FluentUI 私有桥解除反向组件依赖; commits ab91613..012dd3c)
Task 8: complete (commits e341fe3..012dd3c, review clean after round 5)
Task 9: fix round 1/5 (1 addressed, 0 open — 普通 transferTab 事务冻结边界; commits b4afab2..396f336)
Task 9: fix round 2/5 (1 addressed, 0 open — 关闭通知与普通事务冻结边界; commits 396f336..3c2fb61)
Task 9: fix round 3/5 (1 addressed, 0 open — 关闭通知直接 TabWidget 转移回归测试; commit e81cc16)
Task 9: complete (commits b4afab2..e81cc16, review clean after recovery rounds 1-3)
Task 10: minor (deferred): 测试未充分覆盖多组稳定树序、groupAttentionChanged 幂等及跨工作区信号
Task 10: complete (commits e81cc16..1a1a3ed, review clean)
Task 11: minor (deferred): 修复报告声称补充窗口计数篡改，但测试实际未修改 windowCount 首部，也未独立覆盖总页面 4097 的解码篡改；最终审查时复核。
Task 11: fix round 1/5 (3 addressed, 0 open — 几何、标题长度、配置枚举门禁与失败解码无 QWidget/计数测试已补齐; commit 13104e2)
Task 11: complete (commits 1a1a3ed..13104e2, review clean after fix round 1)
Task 12: fix round 1/5 (4 addressed, 0 open — 稳定 pageId、raw/DTO 交叉校验、失败清理与几何收敛; commit 8f1eb13)
Task 12: fix round 2/5 (4 addressed, 0 open — 非法 resolver 对象、完整工厂句柄、历史来源放宽与最大化状态校验; commit c867a61)
Task 12: fix round 3/5 (1 Critical addressed, 0 open — 重复 resolver 指针、跨线程页面所有权与工厂注册拒绝测试; commit c88d004)
Task 12: minor (deferred): `layout.restore` 结构化审计仍待后续统一补齐；FluentUI `adoptPageId` 桥接超出 Task 12 原始 PureTools 文件范围，暂按稳定 pageId 的最小依赖保留，最终审查复核。
Task 12: complete (commits 7fbbaea..c88d004, review clean after fix round 3)
Task 12: final review resolution: `layout.restore` 审计已补齐，成功/失败恰好一条且不泄漏业务内容；`adoptPageId` 位于 FluentUI 自有私有命名空间，公共头和完整架构审计通过，保留为恢复稳定 pageId 的最小桥接。(commit ef9c715)
Task 13: complete (commit d25c0ba, Example smoke and full-suite verification passed)
Task 14: complete (commits e2f590d, 7ad046e, a21f43f, 414d938, 5e5cf82, 9f02735; three benchmark rounds, shared/static clang-tidy, install consumer, relocation, architecture and preset contracts passed)
Task 14: verification: Linux GCC Debug build passed; CTest 235/235 passed on ef9c715 in 485.05 s, including install.consumer and platform.package-relocation.
Task 14: performance: commit 9f02735, Qt 6.11.1, GNU 15.2.0, Release/shared/LTO, offscreen; transfer p95 0.074089/0.072904/0.083369 ms, max 0.122809/0.124812/0.131453 ms, object growth max 0/0/0.
Task 14: static analysis: shared/static full clang-tidy 294/294 passed at 9f02735; ef9c715 changed coordinator production/test translation units passed targeted clang-tidy in both shared and static configurations.
Task 14: platform boundary: Windows MSVC、Windows MinGW 与 macOS 本轮仅验证 preset、源码、公共头和打包契约；未声明对应平台原生构建通过。
