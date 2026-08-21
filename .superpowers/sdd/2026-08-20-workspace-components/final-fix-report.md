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
- `d717428 fix(活动栏): 绘制统一图标描述`
  - ActivityBar 专用 delegate 通过 `ZzFluentStyle::iconPixmap()` 绘制字体与 SVG
    描述符；空描述符使用标题首字符后备。
  - 内部视图与虚拟行统一为 48 逻辑像素，避免标题 `sizeHint()` 把图标中心推到
    可视轨道外；不增加逐行 QWidget。
- `ea9ece8 feat(示例): 为会话入口配置服务器图标`
  - Example 通过公开 `registerSidePanel()` 接口传入 `ZzFontIcon::Server`，并同步
    四档 DPR、三种主题共 12 张截图基线。
- `3e1cb44` 至 `510b81f` 静态检查收口
  - 清理工作区实现与测试中的 57 条 Clang-Tidy 诊断，涵盖重入索引、等价分支、
    字符串复制、空控件失败路径和 `unique_ptr` 所有权转交表达。
- `76e0a3e fix(工作区): 避免宿主析构期类型失效`
  - 修复 UBSan 稳定捕获的宿主析构期无效 `QMainWindow` 向下转换；改用当前动态类型
    与 Dock 身份判断，保留外部销毁内容时的同步注册清理。

## 验证

- `puretools.workspace-shell`：通过。
- Example integration、英文、三种 close guard、多窗口、四档截图和 workspace smoke：
  全部通过。
- `architecture.zzfluent-boundaries`：通过。
- 新 PureTools 工作区截图：100/125/150/200 DPR 均通过 Light、Dark、HighContrast 的宽/窄场景。
- 基准目标以 Linux GCC 15/Qt 6.11.1 编译；三轮 JSON 包含全部新增指标。
- 使用 `GCC_13=/usr/bin/gcc-15`、`GXX_13=/usr/bin/g++-15` 与
  `QT_ROOT=/home/zz/Qt/6.11.1/gcc_64` 运行
  `cmake --fresh --preset linux-gcc-debug`，恢复标准 Debug 构建树。
- 随后运行 `cmake --build --preset linux-gcc-debug --parallel 2`，完整
  Debug 构建通过。
- 在 `76e0a3e` 上运行 `ctest --preset linux-gcc-debug --output-on-failure`，
  单次完整执行为 143/143 通过，总耗时 141.35 秒。该结果包含 12 项截图、
  `install.consumer`、`architecture.public-headers`、
  `platform.package-relocation`、全部平台/发布合同及 16 项 Example，不再沿用
  早期漏配 Example 的 127 项结果。

## Example 基线补充验收

- 初次工作区集成基线提交：`df59dc1`；当前 Server 图标基线提交：`ea9ece8`。
- 重采命令：

  ```bash
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

- `b25dfec fix(工作区): 隐藏空侧边缘`：空 Shell 的左右 SidePane 默认收起，
  ActivityBar 默认隐藏；注册首个面板、移除最后一个面板、外部销毁、注册回滚与
  布局恢复均按实际存活面板同步边缘可见性。
- 红灯：新增 `hidesEmptySideEdgesAndRestoresOnlyTheOccupiedEdge` 后，
  `puretools.workspace-shell` 在未修复实现上因左 SidePane 未收起失败。
- 绿灯：`puretools.workspace-shell` 通过；`puretools.workspace-screenshot`
  四档 DPR 为 4/4 通过；`example.workspace-smoke` 为 1/1 通过。
- 截图基线更新提交：`629b4a6`。以 Qt 6.11.1、GCC 15、
  `linux-gcc-debug` 和 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1` 重采 12 张 PNG；
  关闭变量后 `example.puretools-screenshot-*` 为 4/4 通过。
- 人工查看 dpr-100 三主题和 dpr-200 Light：右侧空 Shell rail 已释放，中心
  工作区获得额外宽度；无空白、截图边界裁切、重叠或主题串色。

## ActivityBar 固定轨道修复与基线更新

- 组件修复提交：`620f92a fix(活动栏): 固定入口轨道宽度`。
- 红灯：新增 `keepsFixedWidthInsideExpandingHorizontalLayout` 后，未修复实现的
  `fluent.activity-bar` 因 `bar->minimumWidth()` 为 `0` 而失败，期望为 `48`。
- 实现：在 `ZzActivityBarPrivate` 构造路径调用 `setFixedWidth(48)`，不改变公开
  API、模型、拖拽、item 高度或 SidePane。
- 绿灯：`fluent.activity-bar`、`puretools.workspace-shell` 与
  `example.workspace-smoke` 均通过；工作区截图和 Example 截图在关闭更新变量后，
  四档 DPR 均为 4/4 通过。
- PureTools 工作区基线随组件提交 `620f92a` 更新，Example 基线提交为
  `6c92f06`。以 Qt 6.11.1、GCC 15、`linux-gcc-debug` 重采 24 张
  PureTools workspace PNG 与 12 张 Example PNG。
  README 的 SHA-256 已逐项匹配，所有 Example 图像分别为 1280x800、1600x1000、
  1920x1200 和 2560x1600，且每档 DPR 的三主题哈希不同。
- 人工查看 dpr-100 的 Light、Dark、HighContrast 与 dpr-200 Light：ActivityBar
  为 48 px 窄轨道，中心标签页宽度增大；未见空白帧、截图裁切、控件重叠或主题串色。

## ActivityBar 图标描述符与内部几何

- 红灯：`rendersFontAndSvgDescriptors` 在旧 delegate 上的字体与 SVG 两个数据行均
  因目标颜色像素为零失败；诊断同时确认外层栏宽为 48px 时，标题测量仍把内部
  虚拟行扩展到 107px，图标中心落在可视区域之外。
- 绿灯：字体 `House` 与内嵌 `PinFill.svg` 均通过自定义颜色像素断言，且
  `rowRect.width()` 等于 viewport 宽度；`fluent.activity-bar` 通过。
- 关闭更新变量后，ActivityBar、WorkspaceShell、四档工作区截图及四项架构/文档
  门禁为 10/10 通过；Example 四档截图与 workspace smoke 为 5/5 通过。
- 人工查看工作区和 Example 的 dpr-100 三主题及 dpr-200 Light：后备字符与 Server
  图标均居中，选中背板和 leading 强调条不遮挡内容，高对比主题可辨识。

## 基准身份与边界

- 代码采样 SHA：`76e0a3e38cfbe08bb209ef9aa8d611492301c4c3`。
- runner digest：`sha256:f3b3982a44212a5f9b2c15c034290d920439fc3712b8361c5a11aecf19899e41`。
- GPU identity：`Qt offscreen raster`。
- RSS 仅在 Linux `/proc/self/statm` 可用时报告；其他平台不伪造该项。

三轮原始证据 SHA-256 依次为
`b57c0fd9e06bd07efcaf1bde5be51f104f1702dc42423a7a65486fd5366d8105`、
`83c8b8fda5787b2d0cd3a90ee1abe2b85ad3556f336fe1a0e96e49fea7b7de34` 和
`ffcff1c38de27bf0fc625cbd2d84368c69b832bdb15845994f68305b8f947ea4`。

## 最终质量门禁

- `linux-clang-tidy-release`：最终 HEAD 完整扫描 248/248，退出码 0，无实际
  `warning:` 或 `error:` 诊断。
- `linux-clang-tidy-static`：最终 HEAD 完整扫描 248/248，退出码 0，无实际
  `warning:` 或 `error:` 诊断。
- `linux-clang-asan`：完整构建通过；ASan/UBSan CTest 143/143 通过，总耗时
  181.55 秒。当前工具进程由 `ptrace` 管理，LSan 会统一报不受支持，因此本地运行
  使用 `detect_leaks=0`；未修改 preset，CI 与正常主机仍保持泄漏检测。
- `linux-gcc-debug`：最终 HEAD 增量构建通过；CTest 143/143 通过，总耗时
  141.35 秒。
- `benchmark.workspace-components`：三轮均为 1/1 通过，单轮分别耗时 4.85、
  4.83 和 4.85 秒；全部耗时指标保持 `observe`，正式阈值未调整。

## 残余风险

- 本轮只在 Linux offscreen raster 上执行 GUI/截图；Windows MSVC、Windows MinGW、macOS 未执行原生 GUI 验证。
- `platform.package-relocation` 通过，验证安装包在隔离前缀下可被消费者重定位使用；
  本次完整验收中其运行时间为 49.65 秒，属于本轮最长的 CTest 门禁。
