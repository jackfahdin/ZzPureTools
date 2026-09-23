# ZzFluentUI 线性进度条重设计实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 把标准 `QProgressBar` 重设计为 3px 中性轨道、4px 强调色指示器和独立文字区域，并以每个 `ZzFluentStyle` 唯一共享动画驱动不确定进度。

**架构：** 保留 `QProgressBar + ZzFluentStyle` 的公开协议，确定性布局由 `ZzFluentStylePrivate.cpp` 内部纯函数计算，`ZzFluentStylePrivate` 只额外持有共享动画、当前相位和弱引用注册表。Gallery、最终 Example 和 Linux 四档 DPR 截图只消费标准 Qt API，不引入包装控件或示例专用绘制分支。

**技术栈：** C++20、Qt 6.8+ Core/Gui/Widgets/Test、CMake Presets、CTest、QPainter、QVariantAnimation、QPointer、ASan/UBSan、clang-tidy。

---

## 文件结构

- 修改 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`：声明进度条绘制入口、共享动画生命周期方法以及样式级动画状态。
- 修改 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`：实现纯几何计算、确定/不确定指示器绘制、标签分区和共享动画注册清理。
- 修改 `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：为 `CT_ProgressBar` 提供与文字分区一致的自然尺寸。
- 修改 `ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`：覆盖几何、方向、范围、颜色组、原生协议、共享对象预算和动画生命周期。
- 修改 `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`：展示确定、忙碌、禁用、无文字和纵向线性进度条。
- 修改 `examples/ZzPureToolsExample/ZzExampleGalleryPagePrivate.cpp`：在最终集成示例中展示同一组标准控件状态。
- 修改 `examples/ZzPureToolsExample/ZzExampleSmokeControllerPrivate.cpp`：通过对象名验证 Example 的进度条状态确实接入。
- 修改 `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`：扩展标准控件截图场景，固定 reduced motion 并覆盖纵向、禁用和忙碌状态。
- 更新 `ZzFluentUI/tests/baselines/linux/dpr-{100,125,150,200}/standard-breadth-{light,dark,high-contrast}.png`：接纳三主题、四档 DPR 的线性进度条视觉变化。
- 修改本计划：在最终门禁后记录真实命令、环境和未执行的平台边界。

不创建 `ZzProgressBar`、公开头文件、动态属性、样式枚举或新的安装目标。`ZzProgressRing` 及其截图不修改。

### 任务 1：实现确定进度、文字分区和自然尺寸

**文件：**
- 修改：`ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`
- 修改：`ZzFluentUI/widgets/src/ZzFluentStyle.cpp`

- [ ] **步骤 1：增加颜色包围盒测试工具和失败测试**

在测试匿名命名空间加入只统计接近目标颜色且非透明像素的工具：

```cpp
/** @brief 返回接近目标颜色的不透明像素包围盒。 */
[[nodiscard]] QRect zzColorBounds(
    const QImage &image,
    const QColor &expected)
{
    QRect bounds;
    constexpr int tolerance = 8;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor actual = image.pixelColor(x, y);
            const bool matches = actual.alpha() > 0
                && qAbs(actual.red() - expected.red()) <= tolerance
                && qAbs(actual.green() - expected.green()) <= tolerance
                && qAbs(actual.blue() - expected.blue()) <= tolerance;
            if (matches) {
                bounds = bounds.united(QRect(x, y, 1, 1));
            }
        }
    }
    return bounds;
}
```

增加 `laysOutThinProgressWithoutCoveringText()`，分别渲染 `160x36` 的 50% 水平进度条和 `36x160` 的 25% 纵向进度条。palette 使用互不相同的纯色；断言轨道/指示器包围盒横截面不超过 5px、指示器长轴约为可用轨道的 50%/25%、水平色条位于标签下方、纵向 LTR 色条位于标签右侧。增加 `sizesProgressForSeparatedText()`，断言文字可见时水平自然高度和纵向自然宽度至少为 `fontMetrics.height() + 8`，文字关闭时不强制增加同一文字预算。

增加 `respectsProgressRangeDirectionAndPalette()` 的数据行：水平 LTR、水平 RTL、两者各自的 `invertedAppearance`、纵向默认/反向，以及 `bottomToTop` 开关。覆盖 0%、中间值、100%、非零 minimum、`INT_MIN` 到 `INT_MAX` 和 `span <= 0`；断言起点和长度正确，切换 `bottomToTop` 不改变色条方向。给 Normal/Disabled 的 `Mid`、`Highlight` 分配不同纯色，证明禁用绘制只取 Disabled 颜色组；再遍历 Light/Dark/HighContrast，证明轨道和指示器来自各模式标准 palette。

增加 `keepsTinyProgressInsideOptionRect()`，渲染 `1x1`、`2x3`、`3x2` 的横纵控件并在图像外包一圈哨兵色，断言绘制不越出 option rect。增加 `preservesLinearProgressProtocol()`：用 `QSignalSpy` 验证 `valueChanged`，检查 format、alignment、orientation 和 range 未被 style 写回，并通过 `QAccessible::queryAccessibleInterface()` 验证 role 仍为 `QAccessible::ProgressBar`、value interface 返回当前数值。

- [ ] **步骤 2：运行定向测试并确认旧实现失败**

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --target ZzFluentStandardControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.standard-controls$' --output-on-failure
```

预期：FAIL；旧实现的轨道/指示器占满 36px 横截面，且 `CT_ProgressBar` 没有预留文字与色条的 8px 总间距。

- [ ] **步骤 3：加入确定性布局纯函数**

在 `ZzFluentStylePrivate.cpp` 的匿名命名空间定义内部结构与常量，不向头文件或 ABI 暴露：

```cpp
constexpr qreal zzProgressTrackThickness = 3.0;
constexpr qreal zzProgressIndicatorThickness = 4.0;
constexpr qreal zzProgressAxisInset = 2.0;
constexpr qreal zzProgressTextGap = 4.0;

struct ZzProgressBarLayout final
{
    QRectF trackRect;
    QRectF indicatorRect;
    QRect labelRect;
};

[[nodiscard]] ZzProgressBarLayout zzProgressBarLayout(
    const QStyleOptionProgressBar &option,
    qreal busyPhase,
    bool animateBusy) noexcept;
```

实现规则写死为：

- 横向由 `State_Horizontal` 判定，其余按纵向处理；
- 文字可见时，水平标签在上、4px 间距、色条在下；纵向 LTR 标签在左、色条在右，RTL 镜像；
- 文字不可见时 3px/4px 色条在横截面居中；
- 长轴不小于 8px 时两端各缩进 2px，更小时把缩进收敛到长轴四分之一；
- `span` 和 `progress - minimum` 先转 `qint64`，比例收敛到 `[0, 1]`；
- 水平 `fromMaximum = invertedAppearance != (direction == Qt::RightToLeft)`，纵向 `fromMaximum = !invertedAppearance`；
- 0% 返回空指示器，100% 覆盖完整轨道；圆角不超过短边一半和当前长度一半。

- [ ] **步骤 4：替换绘制并委托独立标签矩形**

`drawProgressBar()` 先绘制 `Mid` 轨道与 `Highlight` 指示器，再复制 option 并委托基础样式绘制标签：

```cpp
QStyleOptionProgressBar labelOption = *option;
labelOption.rect = layout.labelRect;
const QPalette::ColorGroup group = option->state.testFlag(
    QStyle::State_Enabled)
    ? option->palette.currentColorGroup()
    : QPalette::Disabled;
const QColor text = option->palette.color(group, QPalette::Text);
labelOption.palette.setColor(group, QPalette::Text, text);
labelOption.palette.setColor(group, QPalette::HighlightedText, text);
q_ptr->QProxyStyle::drawControl(
    QStyle::CE_ProgressBarLabel,
    &labelOption,
    painter,
    widget);
```

必须给 painter 的色条绘制设置 `clipRect(option->rect)`；空尺寸、空标签和空指示器直接跳过，不能构造负矩形。禁用时明确使用 `QPalette::Disabled`，启用时使用 palette 当前颜色组。

- [ ] **步骤 5：实现 `CT_ProgressBar` 自然尺寸**

在 `ZzFluentStyle::sizeFromContents()` 中转换 `QStyleOptionProgressBar`。文字可见时，水平执行：

```cpp
result.setHeight(qMax(
    result.height(),
    progress->fontMetrics.height() + 8));
```

纵向对宽度执行同一公式；文字不可见时只保证对应横截面至少 4px。始终保留基础样式更大的返回值。

- [ ] **步骤 6：运行确定进度测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStandardControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.standard-controls$' --output-on-failure
git diff --check
git add ZzFluentUI/widgets/src/ZzFluentStyle.cpp \
  ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp \
  ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp
git commit -m "重构：实现轻量线性进度条布局" \
  -m "使用3像素轨道和4像素指示器替换整块填充视觉。\n分离标签与进度线区域，并覆盖方向、范围和自然尺寸契约。"
```

预期：`fluent.standard-controls` PASS，提交只包含上述三个文件。

### 任务 2：实现样式级共享忙碌动画

**文件：**
- 修改：`ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`

- [ ] **步骤 1：编写共享动画失败测试**

增加 `sharesOneBusyProgressAnimation()`：创建同一 style 下两个已显示、启用、`range(0, 0)` 的 `QProgressBar`，处理事件并渲染；断言 `style.findChildren<QAbstractAnimation *>().size() == 1` 且动画处于 `Running`。等待不超过 600ms 后再次渲染，断言两帧不同。把两个控件依次改为确定范围、隐藏或禁用，使用 `QTRY_COMPARE(animation->state(), QAbstractAnimation::Stopped)`。

增加 `keepsBusyProgressStaticWhenMotionIsReduced()`：先 `controller.setReducedMotion(true)`，渲染忙碌控件两次并等待 100ms，断言图像相同且 style 下没有运行中的动画。再直接以 `widget == nullptr` 调用 `drawControl()`，断言固定短段居中且没有创建第二条动画。

扩展对象预算测试：1000 次在 busy/determinate、enabled/disabled、visible/hidden 和 reduced-motion 间切换，每轮处理事件；结束时断言 style 下最多一个 `QVariantAnimation`、没有 style 子级 `QTimer`，全部控件离开忙碌态后动画停止且注册不会造成 QObject 数量增长。

- [ ] **步骤 2：运行测试并确认当前固定短段行为失败**

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStandardControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.standard-controls$' --output-on-failure
```

预期：FAIL；当前没有样式级动画，100ms 前后图像相同，style 下找不到运行中的 `QAbstractAnimation`。

- [ ] **步骤 3：在样式私有类声明唯一动画状态**

在头文件前置声明 `QProgressBar`、`QVariantAnimation`，加入以下私有实现状态和中文 Doxygen 注释：

```cpp
void registerBusyProgressBar(QProgressBar *progressBar);
void removeIneligibleBusyProgressBars();
void stopBusyProgressAnimation(bool refreshWidgets);

QVariantAnimation *busyProgressAnimation = nullptr;
QList<QPointer<QProgressBar>> busyProgressBars;
qreal busyProgressPhase = 0.0;
```

同时显式包含 `<QtCore/QList>`。把 `drawProgressBar()` 改为非 const 私有方法。注册表只存 `QPointer<QProgressBar>`，重复 paint 通过线性查找保持幂等；常见窗口中忙碌进度条数量很小，避免额外哈希节点和逐控件连接对象。

- [ ] **步骤 4：懒创建并驱动 1800ms 循环动画**

仅在第一次注册符合全部条件的真实 `QProgressBar` 时创建：

```cpp
busyProgressAnimation = new QVariantAnimation(q_ptr);
busyProgressAnimation->setStartValue(0.0);
busyProgressAnimation->setEndValue(1.0);
busyProgressAnimation->setDuration(1800);
busyProgressAnimation->setLoopCount(-1);
```

`valueChanged` 先更新 `busyProgressPhase`，再删除空指针以及不再满足以下条件的条目：控件可见、启用、`minimum() == 0 && maximum() == 0`、`style() == q_ptr`。只对剩余项调用 `update()`；列表为空立即 `stop()` 并把相位复位为 0。动画对象停止后保留复用，不再创建第二个对象。

移动短段使用无分配公式：

```text
t = clamp(phase, 0, 1)
smooth = t * t * (3 - 2 * t)
length = 0.18 + 0.18 * 4 * t * (1 - t)
leading = -length + (1 + length) * smooth
```

把逻辑短段裁切到 `[0, 1]` 后映射到轨道；方向继续复用任务 1 的 `fromMaximum`，不增加 RTL/纵向的第二套公式。

- [ ] **步骤 5：处理 reduced motion、禁用和主题切换**

`drawProgressBar()` 只有在 widget 可转换为真实 `QProgressBar`、控件可见启用、快照未开启 reduced motion 时才注册动画。其他忙碌绘制传入固定模式，使用轨道 28% 的居中短段。

`applySnapshot()` 在 `Motion` 变化且新快照开启 reduced motion 时调用 `stopBusyProgressAnimation(true)`：先刷新仍有效的已注册控件，再清空弱引用并停止动画；恢复动效不主动启动，由下一次真实 paint 懒注册。

- [ ] **步骤 6：运行生命周期测试并提交**

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentStandardControlsTest --parallel 2
ctest --preset linux-gcc-debug -R '^fluent\.standard-controls$' --output-on-failure
git diff --check
git add ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h \
  ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp \
  ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp
git commit -m "功能：增加共享忙碌进度动画" \
  -m "每个Fluent样式仅懒创建一条循环动画，并用弱引用刷新可见忙碌进度条。\n在减少动效、禁用、隐藏、销毁和样式切换时及时停止后台更新。"
```

预期：定向测试 PASS；停止状态等待 250ms 后 CPU 不再收到该动画的 valueChanged 回调。

### 任务 3：扩展 Gallery 和最终 Example 展示

**文件：**
- 修改：`examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleGalleryPagePrivate.cpp`
- 修改：`examples/ZzPureToolsExample/ZzExampleSmokeControllerPrivate.cpp`

- [ ] **步骤 1：先收紧 Example 冒烟契约**

把 controls 路由对“存在任意进度条”的判断改为按对象名查找以下四个标准控件：

```text
zzExampleProgressDeterminate
zzExampleProgressBusy
zzExampleProgressDisabled
zzExampleProgressVertical
```

分别断言 range/value、忙碌范围、禁用状态和 `orientation() == Qt::Vertical`。先构建并运行：

```bash
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample --parallel 2
ctest --preset linux-gcc-debug -R '^example\.puretools-integration$' --output-on-failure
```

预期：FAIL；当前页面只有一个没有这些对象名的确定进度条。

- [ ] **步骤 2：扩展最终 Example 的进度区域**

保留 slider 与 68% 确定进度联动，给确定进度命名并设置 `"68% complete"`。同一无卡片嵌套的布局中加入：无文字 `range(0, 0)` 的 busy、禁用 42% 和高度 112px 的纵向 64% 标准 `QProgressBar`。继续并排展示现有 `ZzProgressRing`，证明环形组件未受影响；不在 Example 中添加绘制、timer 或主题逻辑。

同步修正 `ZzExampleSmokeControllerPrivate.cpp` 的进度条文字遮罩：水平文字矩形扣除底部 8px；纵向按 LTR/RTL 从色条所在尾侧扣除 8px。遮罩只忽略文字像素，不能覆盖轨道或指示器。

- [ ] **步骤 3：扩展 Controls Gallery**

在 Gallery 的标准 range 区域使用相同四种线性状态，并保留现有 slider 联动。每个控件设置可访问名称；纵向进度条放入有稳定高度的横向子布局，避免窗口拉伸时破坏页面宽度。

- [ ] **步骤 4：运行示例冒烟并提交**

```bash
cmake --build --preset linux-gcc-debug \
  --target ZzFluentControlsGallery ZzPureToolsExample --parallel 2
ctest --preset linux-gcc-debug \
  -R '^example\.(fluent-controls|puretools-integration)$' \
  --output-on-failure
git diff --check
git add examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp \
  examples/ZzPureToolsExample/ZzExampleGalleryPagePrivate.cpp \
  examples/ZzPureToolsExample/ZzExampleSmokeControllerPrivate.cpp
git commit -m "示例：展示线性进度条完整状态" \
  -m "在控件画廊和最终应用示例中加入确定、忙碌、禁用与纵向状态。\n通过对象名和原生QProgressBar属性收紧最终示例冒烟契约。"
```

预期：两个示例冒烟测试 PASS，且进程在 offscreen 自动关闭。

### 任务 4：更新三主题与四档 DPR 视觉基线

**文件：**
- 修改：`ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`
- 修改：`ZzFluentUI/tests/baselines/linux/dpr-{100,125,150,200}/standard-breadth-{light,dark,high-contrast}.png`

- [ ] **步骤 1：扩展固定截图场景**

在 `ZzStandardBreadthScreenshotSurface::buildRangeColumn()` 中保留确定和 busy，加入禁用 42% 与 112px 纵向 64% 的标准进度条。截图控制器已经在 `initTestCase()` 设置 `controller_->setReducedMotion(true)`，必须继续使用固定居中 busy 短段；不要在截图中等待动画相位。

把文字遮罩对进度条的矩形改为与生产分区一致：水平文字区域扣除底部 8px，纵向 LTR 扣除右侧 8px、RTL 扣除左侧 8px。这样截图比较忽略字体栅格差异，但仍比较完整轨道和指示器。

- [ ] **步骤 2：运行旧基线并确认只因进度视觉失败**

```bash
cmake --build --preset linux-gcc-debug --target ZzFluentScreenshotTest --parallel 2
ctest --preset linux-gcc-debug \
  -R '^fluent\.screenshot-(100|125|150|200)$' \
  --output-on-failure
```

预期：四个入口均在 `standard-breadth-*` 比较处失败，报告目录生成 actual/diff；不得出现崩溃、DPR 不匹配或其他截图场景失败。

- [ ] **步骤 3：在登记的 Linux Qt 6.11.1 环境更新基线**

```bash
ZZ_UPDATE_SCREENSHOTS=1 ctest --preset linux-gcc-debug \
  -R '^fluent\.screenshot-(100|125|150|200)$' \
  --output-on-failure
git status --short ZzFluentUI/tests/baselines/linux
```

预期：仅 12 个 `standard-breadth-{light,dark,high-contrast}.png` 发生变化。若其他 PNG 变化，恢复那些非进度条基线并调查环境漂移，不能一起接纳。

- [ ] **步骤 4：人工检查并关闭更新模式复跑**

逐档检查 actual/baseline：文字不压线、3px/4px 层次可见、忙碌短段居中、禁用色来自 Disabled palette、纵向 LTR 色条在右侧，高对比下轨道和强调色均可辨识。然后运行：

```bash
ctest --preset linux-gcc-debug \
  -R '^fluent\.screenshot-(100|125|150|200)$' \
  --output-on-failure
```

预期：4/4 PASS。

- [ ] **步骤 5：提交截图场景和基线**

```bash
git diff --check
git add ZzFluentUI/tests/ZzFluentScreenshotTest.cpp \
  ZzFluentUI/tests/baselines/linux/dpr-100/standard-breadth-*.png \
  ZzFluentUI/tests/baselines/linux/dpr-125/standard-breadth-*.png \
  ZzFluentUI/tests/baselines/linux/dpr-150/standard-breadth-*.png \
  ZzFluentUI/tests/baselines/linux/dpr-200/standard-breadth-*.png
git commit -m "测试：更新线性进度条视觉基线" \
  -m "固定减少动效状态并覆盖浅色、深色、高对比主题及四档DPR。\n接纳文字分区、细轨道、忙碌短段、禁用和纵向进度条的新视觉。"
```

### 任务 5：执行完整质量门禁并记录证据

**文件：**
- 修改：`docs/superpowers/plans/2026-09-23-zzfluentui-linear-progress-bar-redesign.md`

- [ ] **步骤 1：运行 Linux 本机最小开发门禁**

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
./scripts/ci/run-local-development-gate.sh \
  --preset linux-gcc-debug \
  --tests '^fluent\.standard-controls$|^fluent\.screenshot-(100|125|150|200)$|^example\.(fluent-controls|puretools-integration)$'
```

预期：完整普通测试集和指定定向测试均 PASS。

- [ ] **步骤 2：运行 ASan/UBSan 定向测试**

```bash
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
cmake --preset linux-clang-asan -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-clang-asan \
  --target ZzFluentStandardControlsTest ZzFluentControlsGallery ZzPureToolsExample \
  --parallel 2
ctest --preset linux-clang-asan \
  -R '^fluent\.standard-controls$|^example\.(fluent-controls|puretools-integration)$' \
  --output-on-failure
```

预期：无 sanitizer 报告，全部定向测试 PASS。

- [ ] **步骤 3：运行 shared/static clang-tidy**

```bash
for preset in linux-clang-tidy-release linux-clang-tidy-static; do
  cmake --preset "$preset" -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset "$preset" --parallel 2
  cmake --build --preset "$preset" --target ZzClangTidy --parallel 2
done
```

预期：两种链接形态均无 warning-as-error、clang-tidy 或链接失败。

- [ ] **步骤 4：运行完整 Linux 门禁**

在已有性能环境变量已按登记 profile 配置时运行：

```bash
./scripts/ci/run-linux-gates.sh
```

预期：GCC shared/static/LTO、Clang、ASan/UBSan、clang-tidy、截图、安装消费和三轮性能门禁通过。进度条没有新增每控件 QObject/Timer，现有性能报告不得出现无法归因的回退。

- [ ] **步骤 5：记录真实结果和跨平台边界**

在本计划末尾追加“实施结果”章节，写入提交号、Ubuntu/Qt/编译器版本、每条命令的通过数量、截图人工检查结论。Windows MSVC、Windows MinGW 和 macOS Clang 只记录 CI 的真实结果；未推送或未运行时明确写“待远端验证”，不得写成通过。

- [ ] **步骤 6：提交验证记录**

```bash
git diff --check
git add docs/superpowers/plans/2026-09-23-zzfluentui-linear-progress-bar-redesign.md
git commit -m "文档：记录线性进度条验收结果" \
  -m "汇总Linux功能、视觉、静态分析、sanitizer与性能门禁证据。\n如实记录Windows和macOS远端静态构建的已验证或待验证边界。"
```

最终检查 `git status --short`，只允许保留用户原有的 `logcat.log` 与 `multi-window-requirements.md` 未跟踪文件；不得暂存、读取或提交它们。
