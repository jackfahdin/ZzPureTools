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
Task 9R independent review: complete（初始独立审查范围 91040df..8deed7a：0 Critical、2 Important、1 Minor；两个 Important 分别为 addWidget 同步 owner 误学习与 panelMoved 第三方接管后的 rollback ghost。修复链 756c392、3e6c21d、a421cc1、2844f7f、4309507、bee5e5b；4 轮定向复审后，第 4 轮原始 DeferredDelete TOCTOU 为 ADDRESSED，ChildRemoved-only 缺口亦补齐。最终范围 91040df..bee5e5b，无未解决 Critical/Important/Minor）
Task 9R final owner verification: ownership/rollback/take 15/15、fluent panel-stack/side-pane 2/2、完整 WorkspaceShell 157/157、puretools.workspace-shell 1/1；静态扫描与 show --check clean。上述为实现者验证，独立审查结论另见上一行。
Task 9R parked: 保留初始独立审查的 Activity 性能门禁 Minor（计时后未逐次断言 current/active），不能表述为全部历史 Minor 已修；另保留 codec error-code mapping Minor、ParentChange 销毁不支持边界、rollback-failed Secondary area 风险、Clang optional::emplace 前序编译阻塞、ASan 基线资源测试悬空 QWidget* 比较和既有 screenshot/OrderedPage 失败。Windows/MSVC、MinGW、macOS 未运行，仅有源码静态可移植性检查。
Task 9R final branch review wave 3: complete（`ff06fdf` 关闭 `rowsRemoved` 精确重入缺口；复审随后发现 registry 在 visibility/current/active 同步前擦除的 Important）
Task 9R final branch review wave 4: complete（`9535a8c` 以逻辑待删、每轮单 setter、setter 后全局重审和最终无 signal 擦除关闭 collapsed/current 重入；原终审者独立判定 erase-after-sync 为 ADDRESSED，24 轮耗尽 fail-closed，不擦除。Final Important 全部关闭，无第 5 波代码修复）
Task 9R controller final verification (2026-08-25): Debug Shell 169/169（25.406 s）、Debug Shell/state/codec CTest 3/3（26.02 s）、GCC Release 2/2（21.47 s）、static Release 2/2（21.39 s）；全量 Debug CTest 127/132（172.63 s），仅既有 4 个 screenshot DPR 与 OrderedPage 命名失败。Clang Release 单独 Shell 构建/运行 169/169（21.319 s）；联合 Shell/state/codec 被既有 optional::emplace 两处阻塞。Windows/MSVC、MinGW、macOS、ASan、clang-tidy 未运行。
Task 9R final compatibility closure: complete（`083c79d` 关闭 `OrderedPage` 命名阻塞；`c637204` 关闭 Clang 20 的两处 `optional::emplace()` 阻塞；`47d29ff` 与 `1ab81bf` 修复窄宿主布局并同步 24 张截图基线；`9eaed9f` 修复 Dock 析构期未定义行为；`8077bff` 完成全量静态分析收口）
Task 9R final verification at `8077bff`: GCC Debug、Clang ASan/UBSan、GCC Release、GCC static Release、Clang Release 的工作区定向门禁均为 7/7；GCC Debug 全量 CTest 148/148；完整 `ZzClangTidy` 检查 264 个一方源文件通过，提交涉及的 12 个源文件定向检查 12/12；preset matrix 通过，架构/平台合同 4/4。shared CTest 显式使用 Qt 6.11.1 与构建树 `LD_LIBRARY_PATH`，未改变项目链接策略。
Task 9R final independent review at `8077bff`: complete（审查范围 `9535a8c..8077bff`；0 Critical、0 Important；唯一 Minor 为本账本和终审报告未覆盖最新 HEAD，已由最终文档提交关闭。审查者 fresh GCC Debug 验证 SidePane 12/12、SplitWorkspace 74/74、LayoutState 21/21、Codec 41/41、WorkspaceShell 169/169）
Task 9R platform boundary: Windows MSVC、Windows MinGW 和 macOS 仍未在物理机运行；仅 preset matrix、gate 脚本、架构合同与源码可移植性静态检查通过，不宣称三平台运行通过。
