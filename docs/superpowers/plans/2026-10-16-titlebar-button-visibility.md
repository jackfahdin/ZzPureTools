# 标题栏按钮独立显隐实现计划

> **面向 AI 代理的工作者：** 使用 executing-plans 在当前隔离工作区内依次执行。

**目标：** 按已确认方案提供 `setButtonVisible(Button::Minimize, false)`，五个按钮默认显示，移除旧接口，不保留兼容层。

**架构：** foundation 新增 `ZzTitleBarButton` 枚举，标题栏通过 `using Button = ZzTitleBarButton` 提供简短调用。标题栏保存五个独立展示状态，统一 setter、getter 和 `buttonVisibleChanged` 信号；窗口装配层继续负责平台策略和 QWindowKit 命中配置。

**技术栈：** C++20、Qt Widgets / QtTest、CMake linux-gcc-debug。

## 已确认的行为

- 枚举项为 Minimize、Maximize、Close、AlwaysOnTop、Theme，初值全部为 true。
- `isButtonVisible(button)` 返回配置值，不受父窗口尚未显示影响；无效枚举返回 false，设置无效枚举无效果。
- 隐藏按钮释放宽度，剩余按钮按现有顺序排列；支持 RTL、菜单自适应和最小宽度计算。
- 主题、置顶、最大化状态刷新不覆盖显隐配置；隐藏不改变置顶和主题状态，也不禁用窗口操作。
- 控件只控制框架自绘按钮。原生系统按钮平台继续由宿主平台策略处理；原生平台隐藏自绘系统按钮，其他平台不再覆盖初始化回调中的用户设置。
- 移除旧三布尔接口、整组 setter、整组查询、属性和信号，并迁移所有调用方。

## 任务 1：实现与迁移

**文件：** foundation/include/ZzFluentUI/ZzTitleBarButton.h；ZzFluentTitleBar 公开头、实现和 private 两文件；ZzApplicationWindowPrivate.cpp；ZzAuxiliaryWindowChrome.cpp；对应三个测试文件。

- [x] 在 ZzFluentTitleBarTest 增加五按钮默认显示、独立显隐与信号幂等测试，验证缺少新接口的编译红灯。
- [x] 实现枚举、别名、`void setButtonVisible(Button button, bool visible = true)`、`bool isButtonVisible(Button button) const noexcept` 和单按钮变化信号。
- [x] 将命令按钮组状态拆成 themeButtonVisible 和 alwaysOnTopButtonVisible；两处宽度预测及实际排列使用各自状态。
- [x] 迁移主窗口、辅助窗口和已有测试；主窗口仅在存在 NativeSystemButtons 时隐藏三个自绘按钮。
- [x] 增加全部隐藏、单个恢复、RTL、主题刷新保留状态及初始化回调保留设置测试。
- [x] 补齐禁止菜单折叠时宿主最小宽度释放的回归，先复现隐藏后仍为 472 而预期 372，再修正同步规则并验证外部约束保留。

## 任务 2：验证、审查和提交

- [x] `cmake --build --preset linux-gcc-debug --target ZzFluentTitleBarTest ZzAuxiliaryWindowChromeTest ZzApplicationBuilderTest --parallel 8`。
- [x] `ctest --test-dir build/linux-gcc-debug -R '^(fluent.title-bar$|puretools.auxiliary-window-chrome$|puretools.application-builder\.)' --output-on-failure`。
- [x] 运行本机最小门禁 `scripts/ci/run-local-development-gate.sh --preset linux-gcc-debug --tests '^(fluent.title-bar$|puretools.auxiliary-window-chrome$|puretools.application-builder\.)'`；包括完整构建、普通测试、逐头消费和架构契约。
- [x] 按 requesting-code-review 执行独立审查，修复重要问题；核对旧接口无残留。
- [x] 更新 README 的标题栏 API 示例，运行文档检查；将实现、测试和调用示例作为一个完整任务提交。记录其他平台和专项待验证项，不推送。

## 验证记录

- Ubuntu 26.04 / Qt 6.11.1 / GNU 15.2.0 / linux-gcc-debug：本机门禁完整构建、普通测试 244/244、定向 19/19 通过；新枚举公共头自动纳入逐头消费。
- Xvfb/xcb：初始化回调保持隐藏设置的实际窗口场景通过，辅助窗口四个测试函数全部通过。
- 关闭截图更新，四档 DPR 的工作区截图测试 4/4 通过，未改动基线。
- 独立只读审查未发现待修问题；README 文档审计通过。
- 其他平台 CI、Sanitizer、clang-tidy、性能、人工视觉和真机交互待验证。
