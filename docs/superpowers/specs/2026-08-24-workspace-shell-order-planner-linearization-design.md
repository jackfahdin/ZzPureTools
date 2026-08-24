# WorkspaceShell 主次顺序与规划器线性化设计

## 1. 文档目的

本文定义 `ZzWorkspaceShell` 事务引擎最终审查遗留的两个承重问题及其修复合同：

1. 合法的 Secondary-first 注册顺序会令 SidePane 物理顺序与 Activity 分区顺序不一致，
   进而导致 `saveLayout()` 返回 `InvalidState`；
2. `ZzWorkspaceLayoutStatePrivate` 的纯值规划路径包含重复的线性查找，在 schema 允许的
   4096 个 Side entry 边界下会退化为 `O(n^2)`。

本设计以提交 `68a5b0e` 为基线，延续
`2026-08-23-workspace-shell-transaction-redesign-design.md` 的不可变投影、同步信号审计、
第三方 owner 保护和完整回滚合同。本文只收敛顺序语义与规划复杂度，不重写 codec、公开
API 或 schema。

## 2. 已确认决策

- 采用入口即规范化方案，不要求调用方按 Primary-first 顺序注册。
- 每侧唯一物理顺序为 `Primary rows + Secondary rows`。
- 同一 Activity area 内保持注册顺序，Activity move 后保持 move 目标顺序。
- Activity model 的全局注册顺序保持既有兼容行为；规范化只约束每侧 PanelStack 以及从
  Activity area 派生的逻辑分区顺序。
- `saveLayout()` 保持只读，不允许在保存时偷偷重排界面。
- planner 使用单次建立的局部索引，不向 Shell 增加长期缓存或缓存失效协议。
- 规划平均时间复杂度为 `O(n)`，临时空间复杂度为 `O(n)`。
- 输出顺序只由输入有序容器决定；`QHash` 和 `QSet` 只用于查询，不得以哈希迭代顺序生成
  可观察结果。

## 3. 范围

### 3.1 包含

- `registerSidePanel()` 在任意合法注册顺序下建立规范物理顺序；
- 注册新增的 `panelMoved` 同步信号边界审计与失败清理；
- `ZzWorkspaceLayoutStatePrivate` 中 Side、Bottom、Split、Activity、Dock identity 规划路径的
  重复 `contains()`、`indexOf()` 和嵌套 `find_if()`；
- Activity move 目标 model order 的一次性位置索引；
- Secondary-first、混合区域、保存恢复、同步第三方接管和性能增长回归；
- 更新任务 9R 账本与最终验证记录。

### 3.2 不包含

- 不修改 `ZzWorkspaceShell` 公开头、公开方法签名或信号；
- 不修改 schema 1/2 字节合同、Qt Dock state 版本或 Split codec 格式；
- 不为 codec 的每侧最多 32 个 visible entry 有界扫描做无收益重构；
- 不引入永久 ID 索引、后台线程、协程、timer、animation 或 `processEvents()`；
- 不处理 GitHub CI，不调用 GitHub CLI，不 push；
- 不读取、修改、暂存或提交 `temp_image/`；
- 不复制或提交主工作树中的任务 9 原型修改。

最终审查另行发现的 rollback-failed 路径 Secondary area 修复不借本批静默混入。实现完成后
必须继续作为独立审查项保留，除非另立设计和 TDD 任务处理。

## 4. 规范顺序合同

### 4.1 每侧物理顺序

左右 SidePane 分别满足：

```text
left PanelStack  = LeftPrimary rows  + LeftSecondary rows
right PanelStack = RightPrimary rows + RightSecondary rows
```

`rows` 表示 Activity model 中过滤到对应 area 后的稳定相对顺序。左右两侧互不影响。

### 4.2 注册语义

对某一侧已有 `P1, P2, S1, S2` 的情况：

- 新注册 Primary 得到 `P1, P2, P3, S1, S2`；
- 新注册 Secondary 得到 `P1, P2, S1, S2, S3`。

因此先注册 Secondary、后注册 Primary 是完全合法的公开调用顺序。两个调用成功后，Shell
必须立即处于可保存状态，不得要求额外 move、restore 或事件循环刷新。

### 4.3 Activity model 兼容性

Activity model 继续按公开注册发生顺序追加新 row。例如依次注册
`LeftPrimary(one)`、`RightPrimary(two)`、`LeftSecondary(three)` 后，全局 model row 仍为
`one, two, three`。ActivityBar 按 `ZzActivityArea` 过滤展示；SidePane 物理顺序按本节规范
从 area-local rows 派生。

Activity move 和 layout restore 已使用完整目标投影，它们必须输出同一规范物理顺序，禁止
注册、move、restore 各自维护不同排序定义。

## 5. 注册提交与同步审计

### 5.1 mutation 前规划

`registerSidePanel()` 在接管 content 前，从当前稳定 Activity rows 计算目标物理索引：

- Primary：本侧现有 Primary 数量；
- Secondary：本侧现有全部 Side panel 数量。

计算只读取 mutation 前状态。后续同步信号不得让外层注册从 observed 状态重新学习目标。

### 5.2 固定提交步骤

1. 完成参数、线程、parent、全局 ID 和 subsystem 校验；
2. 创建带唯一 registration generation 的保留记录并连接 content destroyed；
3. `ZzSidePane::addWidget()` 接管 content；
4. `setCurrentWidget()` 保持既有新注册面板激活行为；
5. 若追加位置不是目标位置，调用 `ZzPanelStack::movePanel()`；
6. Activity model 按既有全局注册顺序追加 row；
7. 同步 current、active、collapsed 和 edge visibility；
8. 清除 `registrationInProgress` 并返回成功。

### 5.3 每个同步边界后的身份审计

`addWidget()`、`setCurrentWidget()`、`movePanel()` 和 ActivityBar 状态更新都可能同步发射 Qt
信号。每个调用返回后至少校验：

- host、workspace root、目标 SidePane 和 PanelStack 的 `QPointer` 身份未替换或销毁；
- registry 中仍存在同 ID、同 content identity、同 registration generation 的记录；
- content 仍由目标 pane/stack 合法拥有，ancestry 与 membership 一致；
- PanelStack 中 content 的最终索引等于固定目标索引；
- 第三方没有接管 content。

任一审计失败，注册返回 `InvalidState` 并执行既有 registration rollback。若 content 已由第三方
接管，只清理 Shell 自身记录和连接，不得强制 reparent 或删除第三方对象。

## 6. 线性规划架构

### 6.1 局部索引

每次 `buildRestoreTarget()` 或 `buildActivityMoveTarget()` 调用按需要构造局部只读索引：

- `QSet<QString>`：unique、registered、claimed、visible 和 group membership；
- `QHash<QString, qsizetype>`：Side order、visible、Activity row 和 model order 位置；
- `QHash<QString, int>`：Side visible ID 对应的正尺寸；
- `QHash<QString, const ZzPanelIdentity *>`：稳定 ID 到 snapshot identity；
- `QHash<QString, ZzActivityArea>`：Side panel 的 snapshot area。

所有容器在可信的内部 snapshot 数量或 codec 已验证上限后 `reserve()`。索引只活到本次规划
结束，不跨 QObject mutation 保存。

### 6.2 确定性规则

算法始终扫描原始 `QStringList`、`QVector` 或 tree node 顺序来写入结果。哈希容器只回答
“是否存在”“位置是多少”“对应值是什么”，不得遍历哈希 key/value 生成 Side、Activity、
Bottom、Dock 或 Split 顺序，避免 Qt hash seed 改变序列化结果。

### 6.3 Side 过滤和恢复

- 去重使用 `QSet`，第一次出现的非空 ID 保留；
- requested order 通过 registered/claimed set 单次过滤；
- visible 与 sizes 同步扫描和重建，禁止二次 `indexOf()`；
- current 通过 visible set 判断，fallback 规则保持原规格不变；
- identity/content 通过 ID 索引匹配，不对 `snapshot.identities` 重复 `find_if()`。

### 6.4 遗漏面板锚点合并

旧 `zzSnapshotInsertionIndex()` 为每个遗漏项前后扫描 snapshot，并在不断增长的 target 中反复
`indexOf()`。新算法保持旧语义但分为三次线性扫描：

1. 为 requested target 中的 ID 建立位置索引；
2. 扫描 snapshot order，把遗漏项挂到最近后继 target anchor 的 before bucket；若不存在后继，
   挂到最近前驱 anchor 的 after bucket；
3. 按 requested target 顺序依次输出 before bucket、anchor、after bucket。

同一 bucket 内保持 snapshot 顺序。visible order 使用同一算法，sizes 与 ID 同步移动。没有
合法前后锚点的遗漏项按 snapshot 顺序追加，结果与当前逐项插入语义一致。

### 6.5 Bottom、Split、Activity 与 Dock

- Bottom order/visible/current 使用 set 和 size map 一次归一化；
- Split tree 单次收集 group ID set，groupOrder 单次过滤和补全；
- Activity 四区单次过滤，snapshot area 通过 area map 查询；
- Dock placement 和 panel identity 通过 ID map 匹配；
- Activity move 目标 model order 为去除 moved ID 后的稳定 order 建立位置表，再扫描目标区一次
  找到前后 anchor；不重复扫描完整 model order。

### 6.6 复杂度

在 Qt salted hash 的平均 `O(1)` 查询模型下，codec 已验证输入的规划时间为 `O(n)`，局部额外
空间为 `O(n)`。算法不承诺恶意构造 hash collision 下的理论最坏常数，但不得存在由源码结构
直接形成的嵌套全列表扫描。

## 7. TDD 与验收

### 7.1 顺序 RED

新增真实公开行为测试，至少覆盖：

- 左右两侧分别 Secondary-first 后 Primary；
- 多个 Primary/Secondary 交错注册，同 area 内相对顺序稳定；
- PanelStack 规范顺序、Activity model 全局注册顺序和 area role；
- 注册成功后立即 `saveLayout()`；
- round trip 后 content identity、owner、current、visible 和 sizes；
- `panelMoved` 同步回调由第三方接管新 content，注册失败、Shell 记录清理、第三方 owner 保留。

当前基线应至少因 Secondary-first 保存失败或物理顺序错误而 RED。失败原因必须来自目标合同，
不能来自 fixture、权限或平台环境。

### 7.2 planner 性能 RED

`ZzWorkspaceLayoutStatePrivateTest` 构造 512 和 4096 项的有效内部 snapshot/request。requested
采用交替保留/遗漏顺序，使约一半 snapshot ID 进入遗漏恢复，并迫使旧实现反复扫描长 target。

测试要求：

- 大小两组都生成成功并逐项核对投影语义；
- 预热后采集多轮同批次耗时，比较中位数；
- 输入数量放大 8 倍时，耗时增长小于 20 倍；
- 不使用宽松固定 1 秒阈值替代增长约束；
- 当前 `O(n^2)` 实现应接近 64 倍增长并 RED；新实现预期接近 8 倍并 GREEN。

若本机噪声令 RED 不稳定，只能增加每个 sample 的规划重复次数或调整数据构造以稳定命中最差
路径，不能放宽到允许二次增长。

### 7.3 Activity move 增长门禁

既有 `activityMoveAuditAvoidsCubicGrowth()` 从 2 倍输入、4.25 倍上限收紧为 4 倍输入、10 倍
增长上限，使用预热和中位数。该门禁区分线性与二次增长，并继续断言 move 后目标 row、Area、
PanelStack 顺序和 identity。

### 7.4 回归与资源

- `ZzWorkspaceLayoutStatePrivateTest` 完整通过；
- `ZzWorkspaceShellTest` 完整通过；
- 相关 CTest 通过；
- 1000 次成功/失败事务资源基线保持稳定；
- Linux GCC 15 Debug/Release、static 和 ASan 定向验证；
- Linux Clang 20/clang-tidy 尽可能复跑；既有前序阻塞必须记录真实日志，不得伪报；
- Windows MSVC/MinGW 与 macOS 只记录静态可移植性检查，不伪造运行证据。

## 8. 错误与兼容性

- 公开 API、ABI 和 schema 2 字节保持不变；
- 任意合法 area 注册顺序都必须成功并可保存；
- 参数、线程、parent、重复 ID 和 subsystem 销毁的错误码保持不变；
- 同步污染、identity replacement 或第三方接管返回 `InvalidState`；
- 注册失败不得留下 ghost registry、重复 Activity row、悬空连接或错误 owner；
- planner 优化前后，相同输入必须产生完全相等的 `ZzWorkspaceProjection`；
- 不通过缩小 4096 上限或提高既有性能阈值完成优化。

## 9. 文件与提交边界

预计修改：

- `ZzPureTools/widgets/src/private/ZzWorkspaceShellPrivate.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceLayoutStatePrivate.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceActivityMoveTransactionPrivate.cpp`
- `ZzPureTools/tests/ZzWorkspaceShellTest.cpp`
- `ZzPureTools/tests/ZzWorkspaceLayoutStatePrivateTest.cpp`
- `.superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/progress.md`
- 任务 9R 最终验证报告。

代码提交固定为：

1. `修复：统一侧栏主次面板注册顺序`
2. `性能：线性化工作区布局规划器`
3. `文档：记录工作区顺序与性能验收结果`

第 1、2 个提交分别包含自己的 RED/GREEN 测试与生产实现。提交正文必须使用中文详细说明：

- Primary 在前、Secondary 在后、同 area 稳定的公开顺序合同；
- Activity model 全局注册顺序保持不变；
- 同步信号身份审计和第三方 owner 处理；
- 被替换的重复扫描、最终复杂度和临时空间；
- 实际运行的构建、测试、sanitizer 与性能命令及结果。

## 10. 完成标准

只有同时满足以下条件，本批两个 Important 才能关闭：

1. Secondary-first 和所有交错注册排列在成功后立即满足规范物理顺序并可保存；
2. Activity model 的既有全局注册顺序兼容测试保持通过；
3. 同步第三方接管测试证明失败清理不强取 owner；
4. planner 的交替遗漏 512/4096 增长门禁先 RED 后 GREEN；
5. 源码审查不再发现对 panel/group 数量形成 `O(n^2)` 的嵌套全列表扫描；
6. 相同 snapshot/request 的优化前后投影语义完全一致；
7. WorkspaceShell 完整测试、定向 CTest、资源门禁和 Linux sanitizer 通过；
8. 每项修改均进入边界清晰的中文提交，主工作树原型和 `temp_image/` 未被触碰。
