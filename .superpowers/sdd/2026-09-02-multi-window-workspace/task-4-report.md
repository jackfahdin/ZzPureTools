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

## 第1轮定向修复

补充跨工作区边缘提交信号顺序断言，要求成功通知按
`groupAdded -> layoutChanged -> activeGroupChanged -> tabTransferCommitted`；
中心迁移保持 `activeGroupChanged -> tabTransferCommitted`。边缘事务继续使用
私有静默 split/remove，迁移失败不发布成功结构信号，并增加第三方接管失败回滚覆盖。
实现 RTL 左边缘物理位置、64 组容量和 16 层深度边界测试；更新跨实例 API 文档以说明支持五个拖放区域，并在边缘通知阶段加入 QPointer 生命周期守卫。

验证：`ZzWorkspaceCrossTransferTest` 全量 `21/21`、`ZzSplitWorkspaceTest` 全量
`79/79` 通过（Qt 6.11 offscreen，配置对应 `LD_LIBRARY_PATH`）。

## 第2轮定向修复

修复静默边缘分割成功后未重建可视布局的问题：迁移与审计成功后调用一次
私有 `rebuildView()`，再按既定顺序发布结构/活动/转移信号；失败路径仍只删除
临时组或恢复树，不重建成功视图。新增断言验证四个物理边缘迁移后每个新组的
`ZzTabWidget` 具有非空几何并挂接在目标工作区层级内。

验证：`ZzWorkspaceCrossTransferTest` `21/21`、`ZzSplitWorkspaceTest` `79/79`
通过（Qt 6.11 offscreen，配置对应 `LD_LIBRARY_PATH`）。
