# SpinBox 四种布局与 Fluent 外观实现计划

> 面向 AI 代理的工作者：按 executing-plans 逐项实现；延续用户指定的当前仓库、master 和 Debug 验收路径，完成验证后提交中文说明。

**目标：** ZzSpinBox、ZzDoubleSpinBox 共用四种可运行时切换的布局，匹配 FluentUIStyle 的布局与轻量圆角按钮外观。

**架构：** 公开 ZzSpinBoxButtonLayout 枚举及两个控件的 buttonLayout 属性；新增状态通过四文件 PIMPL 隔离，数值编辑继续使用 Qt。公共样式统一负责测量、几何、绘制和命中；非 Zz 数值控件保留原有竖排布局与编辑区域，避免改变日期时间和滚轮控件。

**技术栈：** Qt 6.8+、C++20；本机使用 Qt 6.11.1 / GCC 15。

| 枚举 | 中文语义（从左到右布局） |
| --- | --- |
| Vertical | 右侧竖排箭头：上方增大，下方减小。 |
| HorizontalSides | 两侧箭头：左侧减小，中间数值，右侧增大。 |
| HorizontalRight | 右侧横排箭头：左侧数值，右侧先减小、后增大；默认模式。 |
| PlusMinusHorizontalSides | 两侧加减：左侧减号，中间数值，右侧加号。 |

RTL 时按 Qt 规则镜像布局，增减含义不变。设置布局同步对应按钮符号；NoButtons 优先且切换布局不使其重新显示。调用者之后仍可显式使用 Qt 的 setButtonSymbols 覆盖符号。

## 文件与职责

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzSpinBoxButtonLayout.h`：枚举及中文 Doxygen 语义。
- `ZzSpinBox.h/.cpp`、`ZzDoubleSpinBox.h/.cpp`：属性、读取、切换、通知；切换刷新 Qt 编辑区域和尺寸缓存，不改数值。
- `widgets/src/private/ZzSpinBoxPrivate.h/.cpp`、`ZzDoubleSpinBoxPrivate.h/.cpp`：布局状态、枚举校验及原生布局刷新；由公开类唯一持有。
- `widgets/src/private/ZzFluentStylePrivate.h/.cpp`、`widgets/src/ZzFluentStyle.cpp`：统一四种几何、内容尺寸和点击区域；用完整圆角表面与局部按钮反馈，移除重复焦点底线。
- `ZzFluentUI/tests/ZzSpinBoxControlsTest.cpp`：布局位置、RTL、运行时切换、真实点击、NoButtons、长文字与大字体、只读状态回归。
- `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`：在已有矩阵展示所有布局。
- `examples/ZzPureToolsExample/ZzExampleControlPagePrivate.cpp`：两个独立示例页展示四种模式及中文描述。

## 实现与验证

- [x] 先补回归：现有默认按钮仍竖排、只读聚焦仍出现额外底线，测试应失败。
- [x] 增加类型化接口；共享尺寸计算中按字体计算按钮宽度，并为横排/两侧布局预留两个按钮的空间；微小尺寸不产生重叠或越界。
- [x] 绘制复用公共主题、图标缓存；按钮背景内缩，悬停不覆盖外边框或焦点线；只有一处输入底线。
- [x] 补齐四种模式真实点击、RTL、编辑器布局立即更新、NoButtons、范围边界、小数输入和尺寸测试。示例全部展示中文语义。
- [x] 构建 Debug Example，运行 `fluent.spin-box-controls` 及相关标准/日期控件测试，执行本机开发门禁。
- [x] 定向审查并更新受影响的四档 DPR 截图基线，关闭更新模式复验；记录未验证平台，中文提交，不自动推送。

验证命令：

```bash
cmake --build build/linux-gcc-debug --target ZzSpinBoxControlsTest ZzPureToolsExample --parallel 2
QT_QPA_PLATFORM=offscreen ./build/linux-gcc-debug/ZzFluentUI/tests/ZzSpinBoxControlsTest
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 GCC_13=/usr/bin/gcc-15 GXX_13=/usr/bin/g++-15 CMAKE_BUILD_PARALLEL_LEVEL=2 ./scripts/ci/run-local-development-gate.sh --preset linux-gcc-debug --tests '^fluent.spin-box-controls$|^fluent.standard-controls$|^fluent.calendar'
ctest --test-dir build/linux-gcc-debug --output-on-failure -R screenshot
```

## 完成记录

- 本机环境：Linux、Qt 6.11.1、GCC 15.2.0、CMake 4.3.3，`linux-gcc-debug`；已重新编译该路径下的 Example。
- 控件测试 35 项通过；本机开发门禁 216 项普通测试及 4 项定向测试通过，定向范围包含数值、标准、日期及滚轮控件。
- 四档 DPR 的 12 项截图测试在关闭更新模式后全部通过；仅更新数值输入及输入扩展场景的 24 张基线，未改 Qt 6.8 专用基线。
- 代码审查通过；布局状态采用四文件 PIMPL，共享几何调整仅用于两种 Zz 数值输入框，保留日期及滚轮控件原有几何。
- 本轮未验证：Windows/macOS 实机、Qt 6.8、完整性能门禁、ASan/UBSan、clang-tidy 及物理桌面人工交互。
