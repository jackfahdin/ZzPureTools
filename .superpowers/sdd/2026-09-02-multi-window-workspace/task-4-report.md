# 任务4报告

## RED

新增跨工作区 Left/Top/Right/Bottom 与工作区撕出信号测试，初次运行边缘事务因仅支持 Center 失败。

## GREEN

扩展跨工作区事务：边缘区域先在目标树创建临时组，再复用 Center 迁移；失败删除临时组。新增 `tabTearOffRequested` 信号，`prepareTabs()` 映射来源组、索引、pageId、全局位置与稳定推荐尺寸。

## 验证

`cmake --build --preset linux-gcc-debug --target ZzWorkspaceCrossTransferTest ZzSplitWorkspaceTest --parallel 2`：成功。

`ZzWorkspaceCrossTransferTest transfersAcrossPhysicalEdgeZones mapsTabTearOffToWorkspaceSignal`：4 passed, 0 failed。

疑虑：完整容量/深度边界和信号顺序回归仍需集成环境进一步覆盖。

补充 RTL 物理 Right 边缘、pageId/layoutKey/固定标签元数据保持测试，定向执行 5 个跨工作区测试，全部通过。
