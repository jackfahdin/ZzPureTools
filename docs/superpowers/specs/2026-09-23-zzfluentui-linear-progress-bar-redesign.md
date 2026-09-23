# ZzFluentUI 线性进度条重设计

## 1. 背景

当前 `ZzFluentStylePrivate::drawProgressBar()` 直接把
`QStyleOptionProgressBar::rect` 的几乎整个高度作为轨道和进度填充区域。
在常见的 24 个逻辑像素高度下，确定进度会显示为一整块高饱和色条，文字又
直接压在色块上；不确定进度只是固定在中间的三分之一色块，没有形成明确的
忙碌反馈。该实现保留了 Qt 行为，但视觉重量、文字层次和 Fluent 风格均不理想。

本设计参考本地 `FluentUIStyle` 项目中 `QProgressBar` 的以下思路：

- 使用细中性轨道和略粗的强调色指示器，而不是填满控件高度；
- 确定进度和不确定进度使用相同的横截面几何；
- 不确定进度使用移动短段表达持续工作；
- 横向、纵向、RTL 和反向外观共享一套方向规则。

参考项目使用 MIT 许可证。本项目只吸收视觉和状态建模思路，不复制其实现，
不引入它的属性系统、主题管理器、Qt 私有动画类型或运行时依赖。

## 2. 已确认决策

1. 采用视觉方案 B“跨平台平衡”：3px 中性轨道和 4px 强调色指示器。
2. 不兼容旧版整块填充视觉，直接替换现有线性进度条绘制和尺寸规则。
3. 继续由标准 `QProgressBar + ZzFluentStyle` 提供线性进度条，不新增
   `ZzProgressBar` 包装类、动态属性或新的公开状态。
4. 保留 `QProgressBar` 的范围、值、格式、方向、对齐、信号和无障碍协议。
5. 不确定进度使用移动短段；开启 `reducedMotion` 时退化为固定居中短段。
6. 现有 `ZzProgressRing` 是独立的环形组件，不受本次设计影响。

## 3. 目标与非目标

### 3.1 目标

- 让标准 `QProgressBar` 在浅色、深色和高对比主题下具有稳定、轻量的
  Fluent 视觉。
- 让文字与色条拥有独立布局区域，不再发生文字压在进度色块上的情况。
- 使用一条样式级共享动画驱动全部可见忙碌进度条，避免每控件计时器。
- 完整支持水平、垂直、RTL、`invertedAppearance`、非零 minimum、禁用和
  极小尺寸。
- 在 Qt 6.8+ 的 Linux、Windows 和 macOS 上只依赖公开 Qt API。
- 保持绘制路径无每帧堆分配，动画停止后不产生后台唤醒。

### 3.2 非目标

- 不保留旧版粗色块模式或提供兼容开关。
- 不增加 thin、balanced、thick 等公开样式枚举；方案 B 是唯一默认视觉。
- 不为确定进度增加数值插值动画；调用 `setValue()` 后仍立即显示新值。
- 不修改 `QProgressBar` 的键盘、范围收敛、格式化或无障碍实现。
- 不把参考项目的 Ring 模式合并进标准进度条；环形需求继续使用
  `ZzProgressRing`。
- 不在 Example 中加入只对示例生效的绘制逻辑。

## 4. 架构边界

线性进度条仍由 `ZzFluentStyle::drawControl(CE_ProgressBar)` 分派到
`ZzFluentStylePrivate::drawProgressBar()`。公开类和应用代码不持有新的主题或
动画状态。

`ZzFluentStylePrivate` 增加两类私有职责：

1. 根据 style option、当前动画相位和主题快照计算轨道、指示器与文字矩形；
2. 懒创建并管理一条共享 `QVariantAnimation`，记录当前需要刷新的可见忙碌
   `QProgressBar` 弱引用。

共享动画只属于当前 `ZzFluentStyle` 实例。它不进入 foundation，不成为全局
单例，也不向业务代码暴露。绘制几何保持为可独立测试的私有纯计算逻辑；只有
动画注册、弱引用清理和控件 `update()` 涉及 QObject 生命周期。

## 5. 几何与视觉契约

### 5.1 基础尺寸

- 轨道厚度固定为 3 个设备无关逻辑像素。
- 指示器厚度固定为 4 个设备无关逻辑像素。
- 两者使用各自厚度的一半作为最大圆角半径，形成完整圆头。
- 进度轴两端各保留 2 个逻辑像素，避免圆头紧贴控件边界。
- 所有绘制矩形由浮点 `QRectF` 计算，最终由 QPainter 按当前 DPR 栅格化；
  不按物理像素写死坐标。

当可用横截面小于 4px 时，轨道和指示器按可用空间等比收缩。宽度或高度无效
时不绘制，不构造负尺寸矩形。

### 5.2 文字不可见

`textVisible == false` 时，轨道和指示器共同位于控件横截面中心。水平进度条
沿水平轴扩展，垂直进度条沿垂直轴扩展。

### 5.3 文字可见

文字与进度线必须分区，不允许叠加：

- 水平进度条：文字位于上方，进度线位于下方；
- 垂直进度条：文字保留 Qt 的旋转与 `bottomToTop` 语义，进度线位于逻辑
  尾侧的独立窄带，即 LTR 时位于右侧、RTL 时位于左侧；
- 文字和进度线之间保留 4 个逻辑像素间距；
- 文字继续使用 option 的 `textAlignment`、字体、format 和 palette，不强制
  改为居中或右对齐；
- 绘制标签时复制 option、把副本的 rect 收窄到文字区域，再委托
  `CE_ProgressBarLabel`；文字区域不足时按 Qt 原有行为裁切，不能侵占进度线
  区域，也不额外引入省略规则。

`ZzFluentStyle::sizeFromContents(CT_ProgressBar)` 必须为当前字体高度、4px 间距
和 4px 指示器预留横截面自然尺寸：水平进度条的自然高度至少为
`fontMetrics.height() + 4 + 4`，垂直进度条的自然宽度至少为同一数值，并且均不
缩小基础样式返回的尺寸。调用方显式给出更小尺寸时使用前述安全收缩规则。

### 5.4 颜色

- 轨道使用当前颜色组的 `QPalette::Mid`；
- 指示器使用当前颜色组的 `QPalette::Highlight`；
- 文字使用当前颜色组的 `QPalette::Text`；
- 禁用控件显式选择 `QPalette::Disabled`，不通过固定透明度模拟；
- 高对比主题继续由 `ZzThemeSnapshot` 生成的 palette 提供颜色，不硬编码浅色或
  深色常量。

因此绘制逻辑不需要判断操作系统主题，也不从 QApplication 之外读取全局颜色。

## 6. 确定进度

比例计算使用 64 位整数差值，避免 `maximum - minimum` 的有符号溢出：

```text
span = int64(maximum) - int64(minimum)
ratio = clamp((int64(progress) - int64(minimum)) / span, 0, 1)
```

`span <= 0` 的非忙碌异常输入按空进度处理。比例为 0 时不绘制强调色；比例为
1 时完整覆盖轨道。介于两者之间时只沿进度轴缩短指示器，圆角半径不超过当前
指示器短边和进度长度的一半，避免极小进度生成超出数学长度的圆点。

方向规则如下：

- 水平 LTR 默认从左开始，RTL 默认从右开始；
- `invertedAppearance` 对上述方向取反；
- 垂直默认从底部开始，反向外观从顶部开始；
- `bottomToTop` 只影响垂直文字方向，不能意外反转数值进度。

## 7. 不确定进度与共享动画

### 7.1 视觉运动

忙碌状态仍由 Qt 标准约定 `minimum == 0 && maximum == 0` 表示。短段从逻辑
起点外进入，在中段逐渐拉长，再缩短并从逻辑终点离开。相位范围为 `[0, 1]`，
一个循环约 1800ms；位置和长度由确定性函数计算，不依赖系统墙上时间。

方向复用确定进度规则。水平 RTL、水平反向和垂直反向都通过逻辑矩形变换实现，
不能维护第二套坐标公式。

开启 `reducedMotion`、控件禁用，或 `widget == nullptr`、widget 不是实际
`QProgressBar` 时，不注册动画，而是在轨道中心绘制约 28% 长度的固定短段。
这也保证直接调用 style 绘制的离屏渲染和截图测试稳定。

### 7.2 生命周期

- 第一个可见、启用且处于忙碌范围的真实 `QProgressBar` 绘制时，懒创建并启动
  唯一共享动画；
- 注册表只保存 `QPointer<QProgressBar>`，不拥有控件；
- 每个动画帧先清理已销毁、隐藏、禁用、离开忙碌范围或不再使用当前样式的
  条目，再对剩余控件调用 `update()`；
- 注册同一控件是幂等操作，重复 paint 不增加条目；
- 注册表为空时停止动画；后续再次出现忙碌控件时从确定起点重新启动；
- 主题切换为 reduced motion 时停止动画并刷新已注册控件；恢复动效后由下一次
  实际绘制按需重新注册；
- `ZzFluentStyle` 销毁时动画由 QObject 父子关系回收，注册表弱引用不执行外部
  对象删除。

每个 style 最多存在一条 `QAbstractAnimation`。确定进度条、隐藏忙碌进度条和
reduced-motion 状态均不保持运行中的动画。

## 8. 数据流

```text
QProgressBar 状态
  -> QStyleOptionProgressBar
  -> ZzFluentStyle::drawControl(CE_ProgressBar)
  -> 私有几何计算
  -> 轨道 / 指示器 / 可选文字绘制

忙碌且允许动效
  -> 注册真实可见控件弱引用
  -> 共享 QVariantAnimation 更新 phase
  -> 仅 update() 当前有效的忙碌控件
  -> 下一次 paint 使用新 phase
```

样式不写入 minimum、maximum、value、format 或 alignment，不发送业务信号，
也不访问应用数据模型。

## 9. 实现范围

预期修改集中在以下位置：

- `ZzFluentUI/widgets/src/ZzFluentStyle.cpp`：补充 `CT_ProgressBar` 自然尺寸；
- `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.h/.cpp`：新几何、绘制、
  共享动画注册和生命周期；
- `ZzFluentUI/tests/ZzFluentStandardControlsTest.cpp`：行为、方向、颜色、动画和
  对象预算回归；
- `ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`：确定、忙碌、禁用、纵向和文字
  分区视觉场景；
- `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.cpp`：可交互
  展示；
- `examples/ZzPureToolsExample/ZzExampleGalleryPagePrivate.cpp`：最终应用集成展示；
- Linux 四档 DPR 的受影响视觉基线。

若实现阶段发现 `ZzFluentStylePrivate` 的动画字段和纯几何函数已经无法保持单一
职责，可以提取一个仅在 widgets 私有目录使用的 `ZzProgressBarVisual` 两文件
辅助类型；不得因此增加公开头文件、安装目标或运行时库。

## 10. 测试策略

### 10.1 TDD 顺序

1. 先加入会在当前整块填充实现上失败的几何与像素断言；
2. 验证失败原因是轨道厚度、文字重叠或忙碌段静止，而不是测试环境；
3. 只实现足以通过当前行为的最小绘制和动画逻辑；
4. 每一组绿灯后再清理重复几何计算；
5. 最后更新人工确认后的截图基线。

### 10.2 行为与绘制测试

- 3px 轨道和 4px 指示器位于约定横截面，控件其余区域保持透明；
- `textVisible` 的文字矩形与进度线矩形不相交；
- 0%、中间值、100%、非零 minimum 和超界 option 均按约定收敛；
- 水平 LTR、RTL、反向，以及垂直默认和反向的起点正确；
- 浅色、深色、高对比和禁用状态只使用正确 palette 颜色组；
- 极小矩形不崩溃、不绘制负尺寸，也不污染 option rect 外像素；
- Qt 的 valueChanged、format、alignment、orientation 和 accessible role/value
  保持不变。

### 10.3 动画与对象预算测试

- 动效开启时，忙碌短段在限定时间内至少产生两个不同位置的真实帧；
- reduced motion、禁用或无 widget 上下文的帧保持确定；
- 多个忙碌控件同时显示时，style 下最多一条共享动画处于运行状态；
- 控件隐藏、销毁或切回确定范围后，注册条目被清理；最后一个条目消失后动画
  停止；
- 重复 1000 次范围、可见性、启用状态和主题切换后，QObject、
  `QAbstractAnimation` 和 `QTimer` 数量不增长。

### 10.4 视觉与平台验收

- Light、Dark、HighContrast 在 DPR 1.0、1.25、1.5、2.0 下更新并复跑截图；
- 截图固定 reduced motion，忙碌短段必须稳定居中；
- Linux GCC 完整本地开发门禁通过；
- Linux Clang ASan/UBSan 定向测试通过；
- clang-tidy 覆盖所有新改 C++ 文件；
- Windows MSVC、Windows MinGW 和 macOS Clang 至少完成 CI 静态构建检查；
- Linux 物理桌面人工确认文字、细轨道、动画平滑度、RTL 和主题切换效果。

## 11. 完成标准

同时满足以下条件后，本次重设计才算完成：

1. 项目中的标准线性进度条全部使用方案 B，不再出现旧版整块填充视觉；
2. 文字与进度线不重叠，方向、范围和无障碍语义无回归；
3. 忙碌动画只有一条共享驱动，隐藏或 reduced motion 时不持续运行；
4. Gallery 和 Example 能展示确定、忙碌、禁用和纵向状态；
5. 自动测试、四档截图、静态分析和本地开发门禁均有新鲜通过证据；
6. 实现没有引入第三方运行时依赖、Qt 私有 API、公开兼容开关或 Example 专用
   绘制分支。
