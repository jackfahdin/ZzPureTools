# 任务 2 第 4 轮修复报告

## 本轮修复

- 跨工作区事务捕获来源/目标每个标签容器的页面顺序、当前页和完整标签元数据，以及 `activeId`、`pageIds`、`pagesById`、`pageKeys`。
- 使用 RAII 深度守卫；direct 转移后在每个同步边界检查工作区、标签容器和页面 `QPointer`。
- 失败时仅在页面仍由来源/目标拥有的情况下回搬页面，恢复顺序、元数据、活动组、登记映射和布局键；第三方已接管页面时不抢回。
- 提交通知前后校验双边页面登记反向映射、布局键唯一性和标签元数据；跨宿主 `ZzTabWidget::transferTabTo` 继续委托公共事务，事务内部使用无信号 direct 原语。
- 新增故障销毁、第三方接管、状态回滚、非法参数/线程、审计一次和跨宿主委托测试；修正线程测试的 worker affinity，避免 BlockingQueuedConnection 自死锁。

## 验证

- `ZzWorkspaceCrossTransferTest`: 11 passed, 0 failed。
- `ZzSplitWorkspaceTest`: 74 passed, 0 failed。
- `ZzTabControlsTest`: 28 passed, 0 failed。
- `git diff --check`: 通过。

测试使用 Qt 6.11.1、`QT_QPA_PLATFORM=offscreen`，并通过显式 `LD_LIBRARY_PATH` 运行；未修改 `multi-window-requirements.md` 或 `temp_image`。
