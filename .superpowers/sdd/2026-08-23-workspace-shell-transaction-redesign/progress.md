# SDD ledger — plan: docs/superpowers/plans/2026-08-23-workspace-shell-transaction-redesign.md

Branch base: 09d5005
Worktree: /home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro/.worktrees/workspace-shell-transaction-redesign
Baseline: puretools.workspace-shell 1/1 passed (2026-08-23)

Task 1: fix round 1/5 (3 addressed, 0 open; commits 3d8de54..ff4af19)
Task 1: review checkpoint (commits 09d5005..ff4af19, reopened by downstream DTO audit)
Task 1: fix round 2/5 (3 addressed, 2 open — projection membership reconciliation; rightActive equality mutation coverage; commits ff4af19..7b81ff7)
Task 1: fix round 3/5 (2 addressed, 0 open; commits 7b81ff7..aa7f65d)
Task 1: complete (commits 09d5005..aa7f65d, review clean)
Task 2: minor (deferred): private codec tests do not yet assert InvalidArgument/InvalidState/Io error-code mapping
Task 2: fix round 1/5 (2 addressed, 0 open; commits 4d20bf9..c95684d)
Task 2: complete (commits aa7f65d..c95684d, review clean)
Task 3: minor (deferred): Activity move 事务门禁测试尚未逐项断言 take/show/badge/save/restore 在重入期间返回 InvalidState
Task 3: fix round 1/5 (1 addressed, 0 open — Activity move 审计复杂度由立方级收敛为线性；commits 41d0d23..e531c6d)
Task 3: complete (commits c95684d..e531c6d, review clean)
Task 4: fix round 1/5 (审查发现复杂度问题；性能 RED/GREEN 已完成，等待定向复审；commits 57c6b39..9c017e2)
Task 4: fix round 2/5 (rollback cleanup 复用 runtime.panelRows；Activity restore 专项性能测试经证据确认不可靠并删除；等待定向复审；commits 9c017e2..7c2a1d5)
Task 4: fix round 3/5 (多失效 panel rollback 按降序稳定 identity 清理并重建索引；等待定向复审；commits 7c2a1d5..47ed8bd)
Task 4: parked — 低 row 先 destroyed 与其他 panel 同回调第三方接管的复合重入未能由合法公开 Qt 信号稳定构造；ParentChange 中销毁正在换父内容会 SIGSEGV，属于计划明确不支持的页面自定义 ParentChange 删除行为；currentWidgetChanged 不产生目标序列。未改生产代码。
Task 4: complete (commits e531c6d..47ed8bd, 3 review rounds clean; 1 unsupported boundary parked)
Task 5: implementation complete with concerns (commit 8bbd750; waiting task review; GCC/ASan/gates pass, existing OrderedPage/screenshot/example environment and clang-tidy predecessor failures recorded)
Task 5: fix round 1/5 (3 addressed, 0 open — 补强真实 Activity/restore/registration 状态断言；commits 8bbd750..b387c83; waiting re-review)
Task 5: fix round 2/5 (registration/identity Bottom 与 Activity 映射覆盖缺口；waiting implementation)
Task 5: fix round 2/5 implementation (registration/identity Bottom 与 Activity 映射、污染恢复门禁；commits b387c83..ec23bf1; waiting re-review)
Task 5: fix round 3/5 (Bottom registry/ghost 与 Dock identity 压力覆盖缺口；waiting implementation)
Task 5: fix round 3/5 implementation (Bottom registry/title/content 与 Dock owner/identity 压力门禁；commits 3bfbd0f..5123176; waiting re-review)
Task 5: complete (commits 47ed8bd..5123176, 3 review rounds clean; existing environment/preceding-code concerns parked in report)
Task 9R order Important: complete (commits 85ad3ef..d5a4150; Secondary-first/交错注册立即 save/round trip、固定 owner 与第三方接管合同均由任务审查和 GCC 全量回归关闭)
Task 9R planner Important: complete (commits 1dfd34c..d8dddc8; planner 512/4096 从 58.03x 降至 8.00x，Activity 128/512 为 3.88x；锚点和 target-index 变异均被语义测试杀死)
Task 9R Clang fix: complete (commit 90f6e71; 本批 16 个 -Wshadow 诊断清零，定向复审无新破坏；第 677/753 行 optional::emplace 仍为前序阻塞)
Task 9R final gates: GCC Debug private 20/20、Shell 151/151；GCC Release 2/2；static 2/2；full Debug CTest 143/148，失败仅为 4 个既有 workspace screenshot 目标和既有 OrderedPage 架构命名
Task 9R independent review: 等待控制者分派精确 diff 91040df..HEAD 的独立审查；本代理未以自审冒充
Task 9R parked: 保留 codec error-code mapping Minor、ParentChange 销毁不支持边界、rollback-failed Secondary area 风险、Clang optional::emplace 前序编译阻塞、ASan 基线资源测试悬空 QWidget* 比较和既有 screenshot/OrderedPage 失败
