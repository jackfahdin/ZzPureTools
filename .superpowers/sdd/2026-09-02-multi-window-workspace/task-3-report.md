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

## 第4轮修复

补齐来源页面移走后的即时 size 清理（size 校验来源索引），为页面/工作区 destroyed 连接建立显式句柄并在失效、消费及过期清理时断开，避免连接累积。新增 removeTab 后 size=0 断言。构建与注册表测试通过。

剩余疑虑：应用销毁清理和完整 split/tab 回归需在集成环境中继续验证。

## 第3轮修复

修复 Workspace DragEnter/DragMove 在注册表为空时的安全处理，并在有效 v2 payload inspect 后记录活动令牌；令牌切换时立即失效旧值，drop/leave/cancel 清理活动令牌。新增来源页面移走、伪造载荷、跨线程和高频失效安全断言均通过。

命令：`cmake --build --preset linux-gcc-debug --target ZzWorkspaceTransferRegistryPrivateTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2`，输出三个目标成功。

命令：`ctest --preset linux-gcc-debug -R '^fluent\\.workspace-transfer-registry-private$' --output-on-failure`，输出 `1/1 Passed`。
