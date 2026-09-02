# 任务 2 报告：跨实例工作区页面转移事务

## 变更

- 为 `ZzSplitWorkspace` 增加 `pageId`、`pageForId` 和 `transferTabToWorkspace` 公共 API。
- 在私有工作区中登记页面 UUID、反向映射和销毁清理连接，并用 `transactionDepth` 拒绝事务重入。
- 新增跨工作区中心转移私有事务：校验工作区/组/索引/线程、布局键冲突，复用 `transferToDirect` 保留完整标签元数据，成功后同步页面身份、布局键、目标活动组并发出一次审计信号。
- 新增 `ZzWorkspaceCrossTransferTest`，覆盖身份迁移、键冲突、全部标签元数据和活动组更新。

## TDD 证据

### RED

命令：

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest --parallel 2
```

实际结果：编译失败，明确提示 `ZzSplitWorkspace` 没有 `pageId`、`pageForId`、`transferTabToWorkspace`。

### GREEN

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest --parallel 2
```

成功完成库和测试目标构建。首次运行发现页面销毁连接在工作区析构后访问登记哈希并触发 SIGSEGV，随后补充析构断连修复。

## 回归验证

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest ZzSplitWorkspaceTest ZzTabControlsTest --parallel 2
```

构建成功。

由于当前 preset 未注入动态库 RPATH，CTest 直接执行时报 `libZzFluentUI.so.0` 找不到；使用等价环境显式设置 Qt 6.11.1 和项目动态库路径运行：

```text
LD_LIBRARY_PATH=/home/zz/Qt/6.11.1/gcc_64/lib:build/linux-gcc-debug/ZzFluentUI:build/linux-gcc-debug/ZzCore:build/linux-gcc-debug/ZzThirdParty/ZzLog QT_QPA_PLATFORM=offscreen ./build/linux-gcc-debug/ZzFluentUI/tests/ZzWorkspaceCrossTransferTest -o -,txt
```

结果：跨实例测试 `5 passed, 0 failed`；`ZzSplitWorkspaceTest` `74 passed, 0 failed`；`ZzTabControlsTest` `28 passed, 0 failed`。

另行执行 `git diff --check`，无输出且成功。

## 疑虑

- 本次实现和测试聚焦简报要求的中心跨实例转移；边缘 DropZone 仍由既有单实例事务处理。
- 线程拒绝通过当前线程比较覆盖；未引入跨线程排队。
- CTest preset 的动态库环境仍需上游构建配置补充 RPATH/LD_LIBRARY_PATH。
