# 任务 3 性能调试报告：Example 首帧图标

## 状态

DONE_WITH_CONCERNS

- 交付：将 Example 四个 Activity 字体图标改为 Example 自有 SVG，并与既有翻译合并为单一 qrc。
- 根因：四个 Side 内容延迟后，Activity Bar 成为首帧第一次使用 `FontGlyph` 的位置，触发 `QFontDatabase::addApplicationFont()`；全局字体注册及首批 glyph 栅格化把 first paint 增加约 34 ms，并增加 idle RSS。
- 边界：没有修改 shared `ZzFluentStyle` 的 FontGlyph 渲染、历史 reference metric、10% threshold 或截图基线。

## 修改文件

- `examples/ZzPureToolsExample/CMakeLists.txt`
- `examples/ZzPureToolsExample/ZzExampleWindowShellPrivate.cpp`
- `examples/ZzPureToolsExample/tests/CMakeLists.txt`
- `examples/ZzPureToolsExample/tests/ZzExampleWorkspaceSmokeTest.cpp`
- `examples/ZzPureToolsExample/resources/Sessions.svg`
- `examples/ZzPureToolsExample/resources/Files.svg`
- `examples/ZzPureToolsExample/resources/Properties.svg`
- `examples/ZzPureToolsExample/resources/Tasks.svg`

Task 3 已有的 21 个文档、reference 和 evidence 修改未由本性能修复改写，也未纳入本提交。

## 根因证据

### 计时与对象边界

- `external-total` 从父进程调用 `QProcess::start()` 前计时到子进程退出。
- `first-paint` 从 Example `main()` 入口计时，在顶层窗口首次 Paint 后的下一事件轮次记录。
- 临时阶段埋点显示 Shell 装配约 14 ms，其中 Bottom 内容约 13 ms；首帧后裔对象为 322 个 QObject、180 个 QWidget。
- `zzExampleSessionPanel`、`zzExampleSftpPanel`、`zzExamplePropertiesPanel`、`zzExampleTasksPanel` 在首帧均不存在，证明四个 Side factory 没有被调用。

### 单变量消融

原 factory 候选三轮：

| 轮次 | external P95 | first-paint P95 |
|---|---:|---:|
| 1 | 118.891 ms | 102.283 ms |
| 2 | 117.125 ms | 100.664 ms |
| 3 | 118.740 ms | 101.444 ms |

只把四个 Activity descriptor 置空后，正式 30 样本为：

```text
external-total.p95 = 77.140 ms
first-paint.p95    = 67.841 ms
```

其余对象、factory 和显示路径不变，因而回归来自 Activity FontGlyph 首次注册，不是 Side 内容被错误 eager 创建。

### 被否定方案

1. 移除隐藏终端 tab 没有改善，实验已撤销。
2. shared `QRawFont` 单次解析让 startup/idle 通过，但 `QPainterPath` 与 `QGlyphRun` 都改变既有 Fluent 图标像素；该方案和测试已完整撤销。
3. 空 descriptor 只用于定位，不能作为产品修复。

## TDD 证据

### Red

真实 `ZzExampleWorkspaceSmokeTest` 遍历两个 Activity model 的 `Qt::DecorationRole`，要求每行 source 为 `SvgResource`。旧实现按预期失败：实际 source 为 `FontGlyph(1)`，期望 `SvgResource(0)`。

### Green

- 四个 descriptor 改为语义对应的 Example SVG。
- smoke 使用 `QFile::exists(resourceId)` 验证资源真实链接进目标，不只检查非空字符串。
- Example 与 smoke 分别打包资源；Example 把 QM 和四个 SVG 合并成一个 qrc，保持原资源路径不变。
- `fluent.icon-font`、`fluent.style`、`puretools.workspace-shell`、`example.workspace-smoke` 最终 4/4 通过。
- Debug/static 的 English integration 与 static smoke 通过；两个配置生成的 Example qrc 都保留 `:/translations/ZzPureToolsExample_en.qm` 和四个 workspace icon alias。

## 性能复采

环境固定为 `local-release-xvfb`：Xvfb CPU 8、benchmark CPU 10、`DISPLAY=:99`、`QT_QPA_PLATFORM=xcb`，profile digest 和 renderer identity 与 reference 一致。

固定相对上限：

| metric | P95 上限 | max 上限 |
|---|---:|---:|
| external-total | 80.294 ms | 80.330 ms |
| first-paint | 70.765 ms | 70.790 ms |

三轮通过结果保存在 `build/gate-evidence/task-15-deferred-side-panel/final-round-{2,3,5}/`：

| 轮次 | external P95 / max | first-paint P95 / max | idle RSS start / end | growth | CPU |
|---|---:|---:|---:|---:|---:|
| 2 | 76.183 / 77.064 ms | 67.256 / 68.280 ms | 67,825,664 / 67,956,736 B | 0.19325% | 0% |
| 3 | 76.469 / 77.035 ms | 67.555 / 68.068 ms | 67,682,304 / 67,813,376 B | 0.19366% | 0% |
| 5 | 76.163 / 76.230 ms | 67.456 / 67.529 ms | 67,698,688 / 67,829,760 B | 0.19361% | 0% |

每轮的 startup 与 idle 都分别通过 `cmake/ZzComparePerformanceReport.cmake`；未覆盖 `docs/performance/reference/linux/*.json` 的历史 metric。

### qrc 收口

独立 SVG qrc 初验为 `external-total.max=80.836 ms`，比固定上限高 0.507 ms；`first-paint.max=70.111 ms` 已通过。Example 的计时器从 `main()` 才开始，而 qrc 静态注册发生在 pre-main，因此将 SVG 并入既有 Example 翻译 qrc，消除了只进入 external 的额外初始化。合并后的单变量探针为：

```text
external-total.p95/max = 76.629/77.073 ms
first-paint.p95/max    = 67.673/67.910 ms
```

## 噪声与截图边界

- 另保留 `final-round-1`：长期运行的 Xvfb 下出现成对首帧尖峰，external/first-paint max 为 89.202/79.948 ms。
- 新启动 Xvfb 后 `final-round-4` 仍出现单个成对尖峰，max 为 81.751/72.678 ms；两轮 median 均维持约 75.7/66.9 ms。证据说明 strict max 对宿主调度或显示停顿敏感，失败样本没有被删除或改写。
- Workspace 四档截图通过，共享 Fluent 图标路径未变。
- Example 截图基线与上游 `c3d208c` 后“Side 首帧折叠”的新合同存在疑虑；本修复不刷新任何 PNG。统一门禁若仍按 eager Side 旧图比较，应由 Task 3 负责人单独裁定基线迁移，不能把它归因于本轮 SVG 或用刷新掩盖。

## Concerns

- 三轮连续无尖峰数据均有足够门禁余量，但本机另有两轮 strict max 被偶发成对停顿击中；统一 Linux 门禁仍应使用脚本创建的全新 Xvfb，并保留完整日志。
- 独立审查无 Critical/Important finding。非阻塞 Minor：smoke 自带资源 qrc，`QFile::exists()` 只证明测试资源表存在，未直接证明生产目标 alias 或 SVG 可解析；本轮以实际 Example English integration、Debug/static 生成 qrc 检查和四份 `xmllint` 验证补足证据，后续可把 production resource 自检放进实际 Example smoke。
- 本报告只收口性能根因与 Example 图标修复；Task 3 的统一 Linux 门禁、平台人工边界和既有文档/evidence 提交由任务负责人继续完成。
