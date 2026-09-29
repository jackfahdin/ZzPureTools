# 密码输入眼睛切换实施计划

**目标：** 基础控件的密码输入提供点击眼睛显示／隐藏，复用库组件与字体图标。

**架构：** 扩展已有四文件结构的 `ZzPasswordBox`，不新增重复输入控件。`QLineEdit::text` 仍是唯一文本状态；`Eye` 表示显示操作，`EyeSlash` 表示隐藏操作，图标继续走 `ZzIconButton` 的主题缓存。新增 `ZzPasswordRevealMode::Toggle`，原默认 Peek 及 Hidden/Visible 策略不变。

**交互：** Toggle 初始隐藏，有文本时显示查看按钮；点击或键盘空格切换，松开不立即隐藏。保持光标、选区和文本。清空、禁用、隐藏控件或窗口失活时终止临时查看，重新进入仍隐藏。仅焦点转移不终止 Toggle；Peek 的失焦终止保持原语义。基础输入页不为密码框同时启用原生清除按钮，避免与眼睛争用尾部区域。

**文件与职责：**

- `ZzFluentUI/widgets/include/ZzFluentUI/ZzPasswordRevealMode.h`：追加 Toggle 枚举。
- `ZzFluentUI/widgets/include/ZzFluentUI/ZzPasswordBox.h`、`widgets/src/ZzPasswordBox.cpp`、`widgets/src/private/ZzPasswordBoxPrivate.h/.cpp`：保持原接口，增加点击处理、状态收束和动作提示。
- `ZzFluentUI/tests/ZzPasswordBoxTest.cpp`：真实鼠标／键盘切换、选区／光标保留、清空／失活恢复、字体图标／主题刷新和固定对象数量。
- `examples/ZzPureToolsExample/ZzExampleControlPagePrivate.cpp`：密码示例改用 ZzPasswordBox，设置 Toggle。
- `examples/ZzPureToolsExample/tests/CMakeLists.txt`、`ZzExampleWorkspaceSmokeTest.cpp`：直接创建真实基础输入页，检查所用组件及点击行为。

## 实施顺序

- [x] 先增加 Toggle 枚举及失败回归。核心断言为 `mouseClick(button, Qt::LeftButton); QVERIFY(box.isPasswordVisible());`，第二次点击应隐藏且 `textChanged` 次数为零。通过反向选区验证切换前后 cursorPosition/selectionStart/selectedText 不变。
- [x] 扩展现有固定按钮与私有状态，不新增定时器、每次点击对象或密码副本；连接 clicked 处理 Toggle，pressed/released 仅处理 Peek；修改示例装配。
- [x] 重编译 Debug Example，运行 `ZzPasswordBoxTest` 和真实基础输入页回归、本机最小门禁及既有截图；检查实际眼睛图标。独立审查后用中文提交，不推送。记录未运行的平台及专项门禁。

使用示例：

```cpp
auto *password = new ZzFluentUI::ZzPasswordBox(parent);
password->setRevealMode(ZzFluentUI::ZzPasswordRevealMode::Toggle);
password->setPlaceholderText(tr("输入密码"));
```

## 验证记录

- 本机 Ubuntu 26.04.1 LTS、Qt 6.11.1、GCC 15.2.0、CMake 4.3.3，`linux-gcc-debug`。
- Toggle 状态回归先因按钮不可见、点击后未显示而失败；实际基础输入页先因没有使用 PasswordBox 而失败，替换后通过。
- PasswordBox 全部 13 项 QtTest 通过（含初始化与清理）；200% DPR 的图标及点击定向测试通过，保持原 Peek 测试。
- 本机完整构建、216 项普通 CTest、2 项定向 CTest、12 项截图 CTest 通过；截图基线及阈值未调整。
- Debug Example 已重编译；查看真实渲染的浅深主题眼睛／划线眼睛，确认字体图标随主题配色。独立审查未发现阻断问题，并补充同窗口焦点转移保持 Toggle 显示的断言。
- 待验证：Windows、macOS、Qt 6.8、远端 CI、ASan/UBSan、clang-tidy、专项性能及物理桌面交互。本机结果不替代发布门禁。
