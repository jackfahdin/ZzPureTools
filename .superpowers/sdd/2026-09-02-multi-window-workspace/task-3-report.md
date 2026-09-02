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
