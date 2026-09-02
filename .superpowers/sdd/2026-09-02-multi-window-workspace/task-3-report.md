# 任务 3 报告：应用级一次性拖放令牌

## RED

按简报先加入 `ZzWorkspaceTransferRegistryPrivateTest`，初次构建暴露测试目标未链接注册表实现；随后补齐实现并修复随机数 API 与令牌长度校验。

## GREEN

已实现 GUI 线程惰性创建的 `QCoreApplication` 子对象注册表，令牌为版本前缀 + 随机 128-bit 字节，支持 inspect（不消费）、consume（一次性移除）、惰性过期清理、来源工作区析构失效、来源页面存活/索引验证和目标线程验证。TabBar MIME 不再保存 QObject 指针，工作区 overlay 使用同一注册表并在 drop 提交点消费。

## 命令与实际输出

`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest --parallel 2`

输出：链接成功。

`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-private$' --output-on-failure`

输出：`1/1 Test #14 ... Passed`，`100% tests passed`。

`cmake --build --preset linux-gcc-debug --target ZzFluentUI ZzWorkspaceTransferRegistryPrivateTest --parallel 2`

输出：目标构建成功。

## 疑虑

当前新增测试覆盖基础发布/检查/消费和重放拒绝；简报列出的完整时间边界、跨线程、4096 字节及 1000 次增长测试尚未全部补齐。工作区旧 `dragRecord` 兼容函数仍保留类型壳，但数据源已切换至应用级注册表。

## 第1轮修复

提交 `d7fb174`。补充来源页面/工作区销毁连接、取消和离开失效，修复 size 过期清理及 pageId 精确匹配；Workspace/TabBar 改为 inspect 后在事务成功边界 consume；删除旧 v1 MIME 常量及 ensureDragToken/dragRecord 入口；随机字节使用逐字 memcpy，移除 lookup 的 noexcept。新增注入时钟的 `<5 秒有效、>=5 秒失效` 与伪造 token 测试。

命令：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2`，实际输出：三个目标均成功链接。

命令：`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-private$' --output-on-failure`，实际输出：测试通过。

剩余疑虑：尚未在本轮补齐跨线程、4096 字节和 1000 次 QObject/QTimer 增长的独立断言；完整 split/tab 回归受当前环境动态库路径限制未执行。

## 第2轮修复

提交 `0773c09`。新增跨线程拒绝、伪造及错误长度、来源页面移走、注入时钟边界、1000 次发布/失效无对象增长测试；修正测试字节常量。注册表构建与安全测试通过。

命令：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest --parallel 2`，输出成功。

命令：`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-private$' --output-on-failure`，输出 `1/1 Passed`。

## v2 集成测试收紧

移除 TabBar/Workspace v2 drop 测试在事件未接受时的手工迁移 fallback，改为严格断言真实 QDropEvent 成功、重放被拒绝；补充 DragMove 检查及 Workspace 无效目标后的 release/retry 真实路径。registry 生命周期与工作区销毁测试保留。

构建：`cmake --build --preset linux-gcc-debug --target ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2` 成功。

测试：registry 私有测试此前通过；split/tab CTest 在当前环境因 `libZzFluentUI.so.0` 未进入运行时库路径而无法启动，需在集成环境设置正确 `LD_LIBRARY_PATH` 后执行。

## 第4轮修复

补齐来源页面移走后的即时 size 清理（size 校验来源索引），为页面/工作区 destroyed 连接建立显式句柄并在失效、消费及过期清理时断开，避免连接累积。新增 removeTab 后 size=0 断言。构建与注册表测试通过。

## 第5轮修复

统一 size/lookup 过期和来源无效清理路径，均通过 invalidate 断开连接；Workspace drop 现在检查 consume 返回值，失败时显式失效令牌并拒绝事件，避免忽略消费结果。

命令：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2`，三个目标成功。

命令：`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-private$' --output-on-failure`，输出 `1/1 Passed`。

## 恢复协议修复

新增 Published/Reserved 状态机及 `reserve/commit/release` API，TabBar 与 Workspace 均按 inspect/预检、reserve、迁移、commit 顺序处理，失败 release。`ZzTabWidget::tabRemoved` 通知注册表，移除页面立即失效并同步其他记录索引；连接句柄在所有清理路径断开。

验证：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2` 成功；注册表 ctest `1/1 Passed`。

## 生命周期补充

新增 `registryParentIsApplication`：断言注册表父对象为 qApp，并通过 deleteLater/DeferredDelete 验证可销毁且 QPointer 清空。测试构建及 ctest 通过。

## v2 拖放集成补充

接管并稳定 `ZzSplitWorkspaceTest` 中 TabBar/Workspace 的 QDropEvent 用例，覆盖成功提交后的重放拒绝及失败后 reserve/release 可重试；补充测试目标链接私有注册表与 MIME 实现。offscreen 下通过显示控件、处理事件并使用明确中心位置，直接命中真实事件处理器。

验证：`cmake --build --preset linux-gcc-debug --target ZzSplitWorkspaceTest --parallel 2` 成功；注册表测试此前 `1/1 Passed`。


## 恢复周期补充

新增状态机测试覆盖 reserve/commit/release、Reserved 页面移除、Published 页面移除及同源索引更新。修复聚合初始化编译错误、TabBar/Workspace 失败路径 release、活动令牌切换顺序，并增加 `removeTab`/`tabRemoved` 注册表通知。最终构建三目标成功，注册表测试通过。

## 定向复审修复

提交 `fa02293`、`55c655b`。增加移除通知去重标记，避免 removeTab 包装与 tabRemoved 双重处理；最后一个来源令牌清理时对称卸载 eventFilter。注册表目标构建及 ctest 均通过。

## 恢复周期测试补充

新增 v2 MIME 正向（格式及 18 字节）与工作区销毁后令牌失效测试；测试目标额外链接 TabBar 私有实现以验证真实 MIME 编码。应用销毁通过 QCoreApplication 所有权模型间接保证，独立子进程生命周期测试未加入以避免破坏 QTest 主应用。

验证：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2` 成功；注册表 ctest `1/1 Passed`。

## 恢复协议最终修复

提交 `a922cf1` 及后续补丁。`removeTab(int)` 在移除前通知注册表并校验索引；Reserved 令牌在来源移除后保留至 commit/release，release 重新验证来源页面/pageId，失效时拒绝恢复。构建三目标成功，注册表测试通过。

剩余疑虑：应用销毁清理和完整 split/tab 回归需在集成环境中继续验证。

## 拖放集成稳定性修复

修复测试目标重复编译私有实现导致的 DSO 分裂：将 `ZzTabMimeData` 与
`ZzWorkspaceTransferRegistryPrivate` 导出为库符号，并从注册表、工作区测试目标移除重复的私有 `.cpp`。
TabBar/Workspace 拖放改为从版本化 MIME 字节载荷提取 18 字节令牌，不依赖跨 DSO `dynamic_cast`，令牌安全性仍由应用级注册表校验。Workspace 跨工作区中心拖放提交改用来源工作区的 `transferTabToWorkspace()`。

验证：四个 v2 拖放用例全部通过；`ZzSplitWorkspaceTest` 全量 78 项、
`ZzWorkspaceTransferRegistryPrivateTest` 12 项、`ZzTabControlsTest` 28 项均通过。

## 第3轮修复

修复 Workspace DragEnter/DragMove 在注册表为空时的安全处理，并在有效 v2 payload inspect 后记录活动令牌；令牌切换时立即失效旧值，drop/leave/cancel 清理活动令牌。新增来源页面移走、伪造载荷、跨线程和高频失效安全断言均通过。

命令：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2`，输出三个目标成功。

命令：`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-private$' --output-on-failure`，输出 `1/1 Passed`。

## 第1轮定向修复

新增 `tabBarV2DropReleasesAfterMigrationFailureAndCanRetry`：在真实
`QDragEnterEvent`/`QDropEvent` 路径中由来源回调销毁目标工作区，迫使
`reserve` 后迁移事务失败并回滚；断言令牌仍可 `inspect`，随后使用同一 MIME
在新目标重试成功。新增独立 `ZzWorkspaceTransferRegistryLifetimeTest` 可执行目标，
在单独进程中创建并销毁 `QApplication`，验证应用子对象注册表的
`QPointer` 在应用析构后为空，同时 workspace/page 仍存活到随后显式销毁。

验证命令：

`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryLifetimeTest ZzSplitWorkspaceTest ZzWorkspaceTransferRegistryPrivateTest ZzTabControlsTest --parallel 2`

使用 Qt 6.11 offscreen 与对应 `LD_LIBRARY_PATH` 直接运行：生命周期程序返回 `0`；
迁移失败重试用例通过；注册表 `12/12`、标签控件 `28/28`、工作区全量 `79/79` 通过。

## 第2轮定向修复

重写生命周期 helper 为堆分配 `QApplication`：应用析构前保留
workspace/page，调用 `delete app` 后仅通过 `QPointer` 验证 registry 已被应用
销毁而 workspace/page 仍存活，随后再销毁 workspace。这样令牌记录的清理由
应用析构触发，不依赖来源工作区析构；应用销毁后不再调用 registry 业务 API。

验证：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryLifetimeTest --parallel 2` 成功；
`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-lifetime$' --output-on-failure`
输出 `1/1 Test ... Passed`，生命周期程序直接运行返回 `0`。
