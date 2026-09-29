# 线性进度条外观改进计划

## 目标与边界

参考本机 FluentUIStyle 的 Thin／Thick 视觉，在组件库提供可复用的两种线性进度外观。
默认 Thin：1px 轨道、3px 强调色填充；Thick：轨道与填充均为 4px，使用圆角。
继续使用 QProgressBar 的范围、方向、文字、无障碍和现有样式级共享忙碌动画。
颜色来自主题 palette，支持局部强调色和禁用态；不照搬参考项目的固定灰色和私有动画。
环形进度继续由 ZzProgressRing 提供。

本次取代 2026-09-23 线性进度规格中的“唯一 3px／4px 外观”和“不增加样式枚举”约束；
其余范围安全、生命周期及分区文字约束仍然适用。两种外观共用 4px 布局槽，切换时文字不跳动。

## 文件与执行顺序

- [x] `ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`：先添加默认细轨道真实像素回归，
  在修改前运行，确认旧版轨道过厚导致失败。再覆盖运行时 Thin／Thick 切换、控件之间隔离、
  横向／纵向、RTL／反向、禁用及共享动画。
- [x] `ZzFluentUI/widgets/include/ZzFluentUI/ZzProgressBarAppearance.h`：提供 Thin／Thick 枚举；
  `ZzControlAppearance.h/.cpp` 增加类型安全的设置和查询。复用该无状态外观工具的私有属性机制，
  setter 通知 update；空指针忽略，未知枚举收敛为 Thin。
- [x] `ZzFluentUI/widgets/src/private/ZzControlAppearancePrivate.h` 保存内部属性名；
  `ZzFluentStylePrivate.cpp` 在同一几何函数里选择轨道及填充厚度，比例、裁切和方向算法共用。
- [x] `examples/ZzPureToolsExample/ZzExampleControlPagePrivate.cpp` 展示细线和粗线的确定、忙碌、
  禁用状态，进度滑块同步调节两种确定值。示例不承担绘制或动画。
- [x] `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp` 加入粗线场景，审阅并更新受影响的 Linux
  四档 DPR 基线；关闭更新模式后复验。
- [x] `docs/development/PROGRESS_BAR_APPEARANCE_ZH.md` 记录使用方法、默认值、主题及性能边界。

## 验证与交付

先执行定向测试：

```bash
cmake --build build/linux-gcc-debug --target ZzFluentStandardControlsTest --parallel 2
QT_QPA_PLATFORM=offscreen ./build/linux-gcc-debug/ZzFluentUI/tests/ZzFluentStandardControlsTest
```

随后执行 `scripts/ci/run-local-development-gate.sh --preset linux-gcc-debug
--tests '^fluent.standard-controls$|^fluent.progress-ring$'`，完成普通回归及 Debug Example 构建。
截图覆盖浅色、深色、高对比与 DPR 1、1.25、1.5、2。检查 diff 后中文提交，不推送。
Windows/macOS、Qt 6.8 运行、物理桌面效果和性能基准不冒充本机已验证结果。

## 本次结果

- 环境：Linux x86_64、Qt 6.11.1、GCC 15.2.0、CMake 4.3.3、`linux-gcc-debug`。
- 默认轨道像素测试在修改前因厚度过大失败，修改后通过。
- 本机门禁 217 项普通测试及 2 项定向测试通过；补充粗线边界覆盖后，标准控件测试
  36 项通过（包含初始化／清理）。
- 更新 24 张受影响截图；关闭更新模式后四档完整截图测试通过，检查了浅色、深色、
  高对比与常用 DPR 的渲染。未更新 Qt 6.8 专用基线。
- 独立代码审查无阻塞问题，粗线极小尺寸、极端范围及禁用态覆盖建议已落实。
- Debug Example 已重编译。Windows/macOS、Qt 6.8、ASan/UBSan、clang-tidy、性能基准及
  物理桌面人工交互仍待验证。
