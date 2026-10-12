# CI 运行时故障修复计划

> 面向执行者：按系统化调试流程逐项复现、修复和验证。独立故障并行排查，构建与性能采样串行协调。

**目标：** 修复辅助窗口销毁次序、布局恢复分配开销及 Qt 默认事件循环的定时精度问题，先验证实际行为，再整理性能报告。

**边界：** 保留历史参考、20 倍布局增长限制和 16.7 ms 帧间隔预算；不修改已安装的 Qt SDK，不在生产进程注入私有 Qt 符号。`docs/research/` 与其他 worktree 不在范围内。

## 1. 辅助窗口生命周期

- [x] 在 `ZzAuxiliaryWindowChromeTest.cpp` 中观察真实原生窗口的 `SurfaceAboutToBeDestroyed`，验证代理在原生表面销毁前释放；覆盖宿主先销毁及 chrome 先销毁。
- [x] 在旧实现运行新断言，记录失败。
- [x] 修改 `ZzAuxiliaryWindowChrome.cpp`，将宿主销毁时的代理释放绑定到仍存活的 chrome 子对象，保留所有权安全。
- [x] 运行 ASan/UBSan 与窗口集成回归，提交独立修复 `88aedd0`。Linux offscreen/XCB 已验证；原 macOS CI 的故障仍需对应平台复核。

## 2. 布局恢复性能

- [x] 在 `ZzWorkspaceLayoutStatePrivateTest.cpp` 覆盖连续缺省面板与未锚定尾部顺序，保持已有增长断言。
- [x] 在 `ZzWorkspaceLayoutStatePrivate.cpp` 用连续索引存储替换逐锚点小列表分配，保持线性恢复语义。
- [x] 交错测量小/大输入，记录真实分配量与耗时；28 项布局检查通过，增长实测 9.27045 倍（原限制 20 倍），提交独立修复 `6f28ac6`。未声称已复现或验证 macOS 的 24.5536 倍失败。

## 3. 动画调度精度

- [x] 在 `build/` 下复制 Qt 6.11.1 源码并应用已验证的 timerWait 与 GLib ready_time 补丁；构建独立 QtBase、Svg、LinguistTools SDK。
- [x] 运行 Qt 原生定时器、动画和事件循环测试；7/7 CTest 通过，498 项检查通过，2 项 Windows 专用检查跳过。框架与 Example 完整构建通过，正常链接独立 SDK。
- [x] 在无并发构建的真实 XCB/Xvfb 环境串行重复动画基准，并执行原始 16.7 ms 绝对门限。三轮共 15/15 CTest 通过；仪表 P95 16.240016 / 16.263378 / 16.238440 ms，开关 16.541155 / 16.446506 / 16.450113 ms。
- [x] 外置 SDK 的非性能回归 263/263 通过；完整采集 51/51，三轮性能各 52/52，24 项原始绝对门禁全部通过。
- [x] 用户确认归档旧基线并启用新完整基线；历史文件逐字节保留，新 profile 与十六份采集报告同源。
- [x] 四轮噪声校准后，仅将 Tab 状态 P95 设为观察并补充独立 1 ms 绝对预算；三轮保存报告各通过 25 项绝对门禁与 16 场景相对比较。原比较失败日志保留。
- [x] 独立验证暴露启动探针焦点时序竞态，原实现复现 focus=0；增加有界异步就绪检查与受控回归，真实 XCB 启动/就绪检查各 30 轮通过。
- [x] 固定三轮完整复测各 54/54，原相对策略仅活动激活失败；同提交四轮校准后，为同步活动激活增加 1 ms 预算并记录 observe 24%，其他策略保留。
- [x] 锁定策略后，从空 reports 目录执行独立验收：`7fffaa1` 的 55/55 CTest、26 项绝对门禁、16/16 相对比较全部通过，所有 JSON 均为该提交。
- [x] 文档审计覆盖十六份同源报告、活动 profile 摘要与 26 项门禁数量；修正旧复现命令的 SDK、renderer 和依赖路径。

提交顺序：各代码修复独立提交，其后归档与启用基线、性能文档，根 README 最后单独提交。

```bash
env QT_QPA_PLATFORM=offscreen ctest --preset linux-gcc-benchmarks \
  -R 'puretools\.(auxiliary-window-chrome|workspace-layout-state-private)' --output-on-failure
```

最后汇总实测证据与平台验证限制。性能参考生成和 README 在修复验证之后处理；不自动恢复远端 CI 监控。
