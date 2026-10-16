# 标题可用区域居中实现计划

**目标：** 修复窄窗口标题过早省略，使用左右控件之间的全部可用空间。

**架构：** 在现有 ZzFluentTitleBarPrivate 内计算安全矩形，保留 QLabel 的居中绘制和省略逻辑。菜单阈值与新布局规则同步。

**技术栈：** Qt Widgets、C++20、Qt Test、CMake。

用户已确认设计，本次在当前主线工作区执行。

## 单项交付：标题布局与回归

文件：`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp`、对应私有和公共头文件、`ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`、原工作区设计的标题布局章节。

- [x] 修改测试，无菜单的 500 像素窗口应完整展示 `ZzApkInfo-v1.0.0 (Jackfahdin)`；实际文本宽度不足时才省略。断言标题位于左右控件之间且两侧剩余间隔相等（允许 1 像素取整差）。覆盖菜单模式、全部按钮组合及 RTL，空标题和空间耗尽时隐藏。
- [x] 执行 `cmake --build build/linux-gcc-debug --target ZzFluentTitleBarTest --parallel 8` 及 `ctest --test-dir build/linux-gcc-debug -R '^fluent.title-bar$' --output-on-failure`，确认新测试在旧实现失败。
- [x] 以 `QRect(leftBoundary + spacing, 0, availableWidth, height)` 设置标签几何；右边界减去 spacing，宽度钳制为非负。删除全窗中心与奇数宽度限制，菜单阈值改用左右组宽度之和。
- [x] 更新注释和现有设计，运行标题栏测试及 `run-local-development-gate.sh --preset linux-gcc-debug --tests '^(fluent.title-bar|architecture.documentation-audit)$'`。
- [x] 生成 Qt 实际渲染预览并独立复审，保留修改供用户查看。

验证结果：Linux / GCC 15.2 / Qt 6.11.1 下 244 项普通测试及 2 项定向测试通过。
预览确认无菜单、五个按钮均可见时，500 像素窗口保留 238 像素标题区域并完整显示文本；300 像素窗口仅剩 38 像素，正常省略。复审无阻断问题，并补充了标签必须使用全部可用宽度的断言。Windows 真机效果尚未验证。
