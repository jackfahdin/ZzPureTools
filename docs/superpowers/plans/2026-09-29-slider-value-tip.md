# 滑块调节提示实现计划

**目标：** 向任意原生 `QSlider` 附加可复用的调节提示，支持百分比、原始数值和关闭。Example 只调用公开接口。

**架构：** 新增四文件 QObject 组件 `ZzSliderValueTip`，由目标滑块拥有；通过 `attach()` 幂等安装，不替换滑块或截断原生事件。私有实现管理延迟创建并复用的单个提示窗口，以及仅在键盘/滚轮调节后短暂运行的单次隐藏计时器。绘制复用样式的 `PE_PanelTipLabel`。

**技术栈：** Qt 6.8+、C++20；本机 Qt 6.11.1 / GCC 15 / linux-gcc-debug。

## 接口与语义

```cpp
auto *tip = ZzFluentUI::ZzSliderValueTip::attach(slider);
tip->setMode(ZzFluentUI::ZzSliderValueTipMode::Percentage);
// 也可使用 Value（原始数值）或 Disabled（关闭）。
```

- 百分比按 `(position - minimum) / (maximum - minimum)` 四舍五入为整数，范围 20～80 的 50 显示 50%；零跨度显示 0%，完整 int 范围使用 64 位中间值避免溢出。
- 拖动显示实时 sliderPosition，兼容 tracking=false；松开隐藏。键盘方向键、Home/End、PageUp/PageDown 和滚轮操作显示 900ms，重复调节续期。
- 程序赋值不会主动打开提示；已打开的提示会同步最新值、范围、主题与字体。
- 水平滑块在滑柄上方显示，垂直在侧面显示；使用样式滑柄几何，支持 RTL、反向外观、屏幕边缘避让。窗口不抢焦点、不接收鼠标输入。
- 隐藏、禁用、失焦、窗口失活或销毁时清理；窗口或祖先移动时跟随。无常驻轮询，未使用时不创建提示窗口。

## 文件与步骤

- [x] 新增 `ZzFluentUI/tests/ZzSliderValueTipTest.cpp`，通过真实输入覆盖范围换算、原始数值、关闭、非 tracking 拖动、键盘/滚轮、生命周期和几何；先确认缺失提示时回归失败。
- [x] 新增公开头 `ZzSliderValueTip.h`、`ZzSliderValueTipMode.h`，公开实现 `widgets/src/ZzSliderValueTip.cpp`，私有 `widgets/src/private/ZzSliderValueTipPrivate.h/.cpp`；注册库源码、MOC 和测试目标。
- [x] 示例滑块页展示百分比及原始数值；README、中文接入文档说明组件归属与调用方式，编码规范明确跨项目复用约束。
- [x] 构建并运行本机开发门禁、定向回归；更新 Debug Example；进行代码审查，记录真实验证范围后中文提交，不自动推送。

```bash
cmake --build build/linux-gcc-debug --target ZzSliderValueTipTest ZzPureToolsExample --parallel 2
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^fluent.slider-value-tip$'
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 GCC_13=/usr/bin/gcc-15 GXX_13=/usr/bin/g++-15 CMAKE_BUILD_PARALLEL_LEVEL=2 ./scripts/ci/run-local-development-gate.sh --preset linux-gcc-debug --tests '^fluent.slider-value-tip$|^fluent.standard-controls$'
```

## 审查记录

定向审查发现 `sliderPressed` 也可由程序调用 `setSliderDown(true)` 触发。已先通过回归复现错误弹出，再移除该主动显示连接，所有提示开启均由观察到的输入事件驱动；信号只负责刷新与收尾。最终定向回归通过。

组件及示例均无外部数据模型依赖。公共头进入现有安装/逐头编译清单，示例通过链接 `Zz::FluentUI` 调用公开接口。

## 完成验证

- Linux Qt 6.11.1 / GCC 15.2.0 / Debug：217 项普通测试及 2 项定向 CTest 通过；新组件测试共 18 项（含初始化、清理）。
- 100%、125%、150%、200% 缩放下的水平/垂直、RTL、反向外观、非 tracking 拖动及主题/屏幕避让检查通过；目视检查浅色、深色、高对比度提示渲染。
- Debug Example 已重编译，公开接口独立测试和逐头编译通过；文档审计与差异检查通过。没有修改旧截图基线。
- 待验证：Windows/macOS 实机、Qt 6.8、真实 Wayland/跨屏窗口定位、完整性能门禁、ASan/UBSan 与 clang-tidy；本轮不把 offscreen 验证等同物理桌面验收。
