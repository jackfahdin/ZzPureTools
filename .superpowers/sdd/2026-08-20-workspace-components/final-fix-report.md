# 工作区组件最终修复报告

## 修复批次

- `5f389e9 fix(工作区): 防护面板同步析构`
  - SidePane add/take 的同步删除回归，以及 host 析构时 dock 防护。
- `42bab7e fix(工作区): 隔离模型切换状态`
  - Command Palette 旧模型连接断开，ActivityBar 清空索引通知。
- `1cab046 fix(工作区): 限制布局解码并报告回滚失败`
  - 4096 条侧栏 schema 上限、线性去重、回滚成功/失败错误语义。
  - 红灯：旧实现接受 4097 项且错误报告仍称回滚成功；绿灯：`puretools.workspace-shell`。
- `9569f6f fix(示例): 适配活动日志视图烟雾验收`
  - Smoke 查找新 `QTableView`/`zzExampleActivityLogView`，保留尾随、共享模型和关闭守卫验证。
- `f95af31 fix(架构): 迁移工作区截图到 PureTools`
  - 工作区截图从 Fluent 移至 PureTools，24 张基线原样迁移。
  - 红灯：`architecture.zzfluent-boundaries`；绿灯：架构门禁和 100/125/150/200 DPR 下六个工作区截图场景。
- `02b8751 fix(性能): 记录工作区结构与内存观测`
  - 每轮写入 object/timer/animation/result-view/style-cache/RSS 指标，不调整正式阈值。
- `ad66738 docs(性能): 更新工作区三轮观测证据`
  - 三轮原始 observe JSON：`docs/performance/evidence/workspace-components/2026-08-21/round-{1,2,3}.json`。
- `a591d62 fix(截图): 恢复 Fluent 截图命名空间边界`
  - Workspace 截图迁移时，旧场景的禁用块误包含匿名 namespace 闭合，导致
    Fluent 截图测试的 MOC 解析失败；已恢复 namespace 边界。
- `aa84a2a test(动效): 稳定折叠容器反向动画验收`
  - 用状态条件等待替换固定帧等待，避免并行 CTest 下首次动画帧尚未调度时
    偶发观察到零高度，同时保留动画推进和连续反向的断言。

## 验证

- `puretools.workspace-shell`：通过。
- Example integration、英文、三种 close guard、多窗口：通过（测试模式日志目录使用可写临时 HOME）。
- `architecture.zzfluent-boundaries`：通过。
- 新 PureTools 工作区截图：100/125/150/200 DPR 均通过 Light、Dark、HighContrast 的宽/窄场景。
- 基准目标以 Linux GCC 15/Qt 6.11.1 编译；三轮 JSON 包含全部新增指标。
- 使用 `GCC_13=/usr/bin/gcc-15`、`GXX_13=/usr/bin/g++-15` 与
  `QT_ROOT=/home/zz/Qt/6.11.1/gcc_64` 运行
  `cmake --fresh --preset linux-gcc-debug`，恢复标准 Debug 构建树。
- 随后运行 `cmake --build --preset linux-gcc-debug --parallel 2`，完整
  Debug 构建通过。
- 完整 CTest 以相同工具链和可写临时 `HOME` 执行。受单次执行窗口限制，
  按编号分组验收：第 1–64 项为 64/64 通过；第 65–127 项先通过 58/63，
  其余 `install.consumer`、`architecture.public-headers`、
  `platform.package-relocation`、`release.blocked-without-evidence` 与
  `release.complete-fixture` 也均通过。最终合计 127/127 通过。

## Example 基线补充验收

- 基线更新提交：`df59dc1`。
- 重采命令：

  ```bash
  HOME=/tmp/zzpuretools-ctest-home \
  GCC_13=/usr/bin/gcc-15 \
  GXX_13=/usr/bin/g++-15 \
  QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 \
  ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1 \
  ctest --preset linux-gcc-debug \
    -R '^example\.puretools-screenshot-(100|125|150|200)$' \
    --parallel 4 --output-on-failure
  ```

  四档 DPR 均成功写入，共更新 12 张 PNG。
- 关闭更新变量后的同一截图测试为 4/4 通过；`example.workspace-smoke` 为 1/1
  通过。
- 逐张校验 12 张 PNG 均为预期 RGBA 尺寸且非空，三主题 SHA-256 各不相同；
  人工查看 dpr-100 的 Light、Dark、HighContrast 与 dpr-200 Light，SFTP、
  会话、终端、属性、日志和任务工作区均完整渲染，未见空白帧、截图边界裁切、
  控件重叠或主题串色。

## 空侧边缘修复与基线更新

- `9a1cd18 fix(工作区): 隐藏空侧边缘`：空 Shell 的左右 SidePane 默认收起，
  ActivityBar 默认隐藏；注册首个面板、移除最后一个面板、外部销毁、注册回滚与
  布局恢复均按实际存活面板同步边缘可见性。
- 红灯：新增 `hidesEmptySideEdgesAndRestoresOnlyTheOccupiedEdge` 后，
  `puretools.workspace-shell` 在未修复实现上因左 SidePane 未收起失败。
- 绿灯：`puretools.workspace-shell` 通过；`puretools.workspace-screenshot`
  四档 DPR 为 4/4 通过；`example.workspace-smoke` 为 1/1 通过。
- Example 基线更新提交：`HEAD`（本提交）。以 Qt 6.11.1、GCC 15、
  `linux-gcc-debug` 和 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1` 重采 12 张 PNG；
  关闭变量后 `example.puretools-screenshot-*` 为 4/4 通过。
- 人工查看 dpr-100 三主题和 dpr-200 Light：右侧空 Shell rail 已释放，中心
  工作区获得额外宽度；无空白、截图边界裁切、重叠或主题串色。

## 基准身份与边界

- 代码采样 SHA：`02b87516ed73ec62c842fc617c9ffa8502c8fd8f`。
- runner digest：`sha256:f3b3982a44212a5f9b2c15c034290d920439fc3712b8361c5a11aecf19899e41`。
- GPU identity：`Qt offscreen raster`。
- RSS 仅在 Linux `/proc/self/statm` 可用时报告；其他平台不伪造该项。

## 残余风险

- 本轮只在 Linux offscreen raster 上执行 GUI/截图；Windows MSVC、Windows MinGW、macOS 未执行原生 GUI 验证。
- `platform.package-relocation` 通过，验证安装包在隔离前缀下可被消费者重定位使用；
  其运行时间为 79.36 秒，属于本轮最长的 CTest 门禁。
