# 标题栏系统按钮与双模式主题交互实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 逐任务实现本计划。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 为 `ZzFluentTitleBar` 增加可配置的菜单/浅深切换主题交互，并将标题栏系统按钮统一替换为缓存化 SVG 图标。

**架构：** 公开标题栏只暴露主题交互模式、菜单访问器和意图信号；私有类创建并复用 action 与按钮。标题栏不依赖主题控制器或窗口实现，宿主继续通过请求信号执行并回写确认状态。图标优先使用当前 `ZzFluentStyle::iconPixmap()` 的 SVG 形状缓存，无 Fluent 样式时使用同一内嵌 SVG 的轻量着色回退。

**技术栈：** Qt 6.8+ Widgets、C++20、Qt Resource SVG、QSignalSpy、CTest。

---

### 任务 1：建立主题交互模式契约与失败测试

**文件：**
- 创建：`ZzFluentUI/foundation/include/ZzFluentUI/ZzTitleBarThemeInteractionMode.h`
- 修改：`ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentTitleBar.h`
- 修改：`ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`

- [ ] **步骤 1：先在测试中声明目标行为**

在测试中包含 `ZzTitleBarThemeInteractionMode.h`，增加以下测试：

```cpp
void exposesConfigurableThemeInteractionMode()
{
    ZzFluentUI::ZzFluentTitleBar titleBar;
    QCOMPARE(
        titleBar.themeInteractionMode(),
        ZzFluentUI::ZzTitleBarThemeInteractionMode::Menu);
    QSignalSpy spy(
        &titleBar,
        &ZzFluentUI::ZzFluentTitleBar::themeInteractionModeChanged);
    titleBar.setThemeInteractionMode(
        ZzFluentUI::ZzTitleBarThemeInteractionMode::Toggle);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(
        spy.first().first().value<ZzFluentUI::ZzTitleBarThemeInteractionMode>(),
        ZzFluentUI::ZzTitleBarThemeInteractionMode::Toggle);
}

void emitsThemeIntentForSelectedInteractionMode()
{
    ZzFluentUI::ZzFluentTitleBar titleBar;
    auto *button = titleBar.findChild<QToolButton *>(
        QStringLiteral("zzTitleBarThemeButton"));
    QVERIFY(button != nullptr);
    QSignalSpy modeSpy(
        &titleBar,
        &ZzFluentUI::ZzFluentTitleBar::themeModeRequested);
    QSignalSpy toggleSpy(
        &titleBar,
        &ZzFluentUI::ZzFluentTitleBar::themeToggleRequested);
    titleBar.setThemeInteractionMode(
        ZzFluentUI::ZzTitleBarThemeInteractionMode::Toggle);
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(modeSpy.count(), 0);
    QCOMPARE(toggleSpy.count(), 1);
}
```

- [ ] **步骤 2：运行测试确认接口缺失导致失败**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentTitleBarTest --parallel 2
```

预期：编译失败，指出主题交互枚举、属性访问器或信号尚未定义。

- [ ] **步骤 3：提交测试与枚举文件**

创建带 `Q_DECLARE_METATYPE` 的 `ZzTitleBarThemeInteractionMode.h`，在标题栏头文件中加入属性、访问器声明和两个信号，使下一任务可以实现其行为。

- [ ] **步骤 4：提交**

```bash
git add ZzFluentUI/foundation/include/ZzFluentUI/ZzTitleBarThemeInteractionMode.h \
    ZzFluentUI/widgets/include/ZzFluentUI/ZzFluentTitleBar.h \
    ZzFluentUI/tests/ZzFluentTitleBarTest.cpp
git commit -m "test(标题栏): 增加主题交互模式契约测试" \
    -m "先记录菜单与浅深切换两种主题交互模式的公开契约，测试在实现前保持失败。"
```

### 任务 2：实现菜单与浅深切换主题交互

**文件：**
- 修改：`ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`

- [ ] **步骤 1：完成红灯测试覆盖菜单和 Toggle 分支**

测试通过 `themeMenu()` 获取长期存在的菜单，验证四个 action 的 `ZzThemeMode` 数据；在 `Menu` 模式触发 action 断言 `themeModeRequested()`，在 `Toggle` 模式断言 `themeToggleRequested()` 且不发出模式请求。

- [ ] **步骤 2：实现最少状态和信号转发**

在私有类中增加 `themeInteractionMode` 字段；构造时将默认值设为 `Menu`。主题按钮点击处理按模式分支：`Menu` 仅让 Qt 打开已有菜单，`Toggle` 发出 `themeToggleRequested()` 并恢复按钮的确认勾选状态。新增高对比度 action，并在 `refreshThemeActions()` 中同步四项。

- [ ] **步骤 3：运行标题栏测试确认通过**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentTitleBarTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R ZzFluentTitleBarTest --output-on-failure
```

预期：标题栏测试全部通过，系统按钮意图测试保持原有计数。

- [ ] **步骤 4：提交**

```bash
git add ZzFluentUI/widgets/src/ZzFluentTitleBar.cpp \
    ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.h \
    ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp \
    ZzFluentUI/tests/ZzFluentTitleBarTest.cpp
git commit -m "feat(标题栏): 支持菜单与浅深切换主题模式" \
    -m "增加可配置主题交互模式和高对比度菜单项。Toggle 模式只发出浅深切换意图，Menu 模式继续发出具体主题模式请求。"
```

### 任务 3：使用缓存化 SVG 图标覆盖标题栏命令

**文件：**
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp`
- 修改：`ZzFluentUI/tests/ZzFluentTitleBarTest.cpp`

- [ ] **步骤 1：增加 SVG 图标行为测试**

断言置顶状态在 `Pin.svg` 与 `PinFill.svg` 间变化，最大化状态在 `Maximize.svg` 与 `Restore.svg` 间变化；检查按钮图标非空，并在标题栏调色板改变后图标像素使用新的文字色。

- [ ] **步骤 2：实现 SVG 渲染 helper**

用 `ZzIconDescriptor::fromBundledSvg()` 创建描述；若 `q_ptr->style()` 是 `ZzFluentStyle`，调用 `iconPixmap(descriptor, QSize(16, 16), devicePixelRatioF(), palette().color(QPalette::ButtonText), layoutDirection())`。否则用 `QIcon` 加载相同 `:/zzfluent/icons/*.svg`，通过 `CompositionMode_SourceIn` 做一次回退着色。`refreshPresentation()` 只在状态、主题、调色板或 DPR 变化时调用，复用样式缓存，不在 `paintEvent()` 中解析 SVG。

- [ ] **步骤 3：运行测试与构建**

运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentTitleBarTest --parallel 2
ctest --test-dir build/linux-gcc-debug -R ZzFluentTitleBarTest --output-on-failure
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample --parallel 2
```

- [ ] **步骤 4：提交**

```bash
git add ZzFluentUI/widgets/src/private/ZzFluentTitleBarPrivate.cpp \
    ZzFluentUI/tests/ZzFluentTitleBarTest.cpp
git commit -m "feat(标题栏): 统一使用内嵌SVG系统按钮图标" \
    -m "系统按钮、主题和置顶状态改用现有SVG资源及ZzFluentStyle图标缓存，保留高DPI、RTL、调色板换色和无样式回退。"
```

### 任务 4：全量回归与交付检查

**文件：** 无新增代码文件；仅检查前述提交。

- [ ] **步骤 1：运行完整 Linux 回归**

```bash
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug --output-on-failure
```

- [ ] **步骤 2：检查静态平台契约**

确认新增接口只使用 Qt 6.8+ 公共 API、C++20 标准库和 Qt Resource，不引入 Linux、Windows 或 macOS 专用代码；确认 `temp_image/` 未被读取、添加或提交。

- [ ] **步骤 3：提交前验证工作树与提交记录**

```bash
git diff --check
git status --short --branch
git log -3 --oneline
```

预期：仅保留允许的未跟踪 `temp_image/`，所有测试命令退出码为 0，提交说明包含中文简述和中文详细说明。
