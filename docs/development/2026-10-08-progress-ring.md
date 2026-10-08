# 环形进度条实现计划

> 使用 writing-plans、subagent-driven-development、test-driven-development 和 verification-before-completion。当前任务不提交。

**目标：** 扩展已有 ZzProgressRing，迁移 FluentUIStyle 的 ExProgressRing 及完整示例。
**架构：** 保留 QProgressBar 继承、PIMPL、现有整数 ringWidth API 和单一忙碌动画，补齐标题、数值样式与中心控件。复用稳定 progress-ring 路由，将导航移动到 liquid-gauge 后，总数保持 37。
**技术栈：** C++20 / Qt 6.8+ 公共 API / QtTest / CMake。

## 全局约束

- 仓库 /home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame；只读参考 /home/zz/Jackfahdin/github/FluentUIStyle。
- 不访问 docs/research，不提交，不切换工作树；使用 apply_patch 编辑。
- 遵循参考的环几何、双层中央标题/值布局及默认字体、配色；显式字体和颜色覆盖须保持。
- 公共 API 使用显式 Q_PROPERTY、中文 Doxygen、导出类与 PIMPL；不引入 Qt 私有依赖。

### Task 1: 扩展控件与真实行为测试

文件：ZzFluentUI/widgets/include/ZzFluentUI/ZzProgressRing.h，widgets/src/ZzProgressRing.cpp，widgets/src/private/ZzProgressRingPrivate.h/.cpp；必要时拆分私有绘制/中心所有权文件并登记 ZzFluentUI/CMakeLists.txt；tests/ZzProgressRingTest.cpp。

- [x] 阅读 ExWidgets/gauges/exprogressring.h/.cpp 及 fluentui3style/fluentui3style.cpp 的 drawProgressRing，先写测试证实缺少行为，再实现。
- [x] 增加 QString title、QFont titleFont/valueFont、QColor titleColor/valueColor、int textSpacing [0,100] 默认4、QWidget* centerWidget 属性及 setCenterWidget/takeCenterWidget；空标题默认。默认字体按参考尺寸自动调整，同时保留原 setFont() 显式字体支持。
- [x] 增加 qreal thickness（默认6，[1,64]，非有限输入忽略）及 QColor ringColor/trackColor（无效恢复调色板 Accent/Mid），支持示例的0.5px步长。保留 int ringWidth()/setRingWidth(int) 信号和调用兼容：ringWidth 返回 thickness 四舍五入，setRingWidth 写入整数厚度。更改先完成状态，安全发出相关通知。
- [x] 原 indeterminateDuration 默认800，保留现有范围[200,60000]；在页面设置200～5000，说明与参考100下限的兼容取舍。不改动画业务语义、invertedAppearance 和继承范围/可访问性。
- [x] 中心控件由 ring 接管，替换删除原控件；take 解除父对象返回所有权；拒绝自身、祖先、将被删除旧中心的后代。外部删除/重设父对象时清理指针，信号槽删除/重入不能访问释放对象或破坏新中心。不要在 QObject ChildRemoved 栈内发送导致再次 setParent 的外部通知。
- [x] 中心区域及文本遵循参考厚度+6内缩，文字隐藏同时隐藏中心；尺寸/字体/厚度/样式改变立即更新；中心控件存在时取代内置文本。防止小尺寸/过大字体压住环。
- [x] 测试验证实际标题/值像素、自定义颜色及恢复、fractional thickness、中心几何/隐藏/替换/取出/外部删除及重入、既有单动画与范围语义；使用现有测试目标做 RED/GREEN。
- [x] 运行 cmake --build build/linux-gcc-debug --target ZzProgressRingTest，然后 QT_QPA_PLATFORM=offscreen build/linux-gcc-debug/ZzFluentUI/tests/ZzProgressRingTest。主代理负责截图与页面。

### Task 2: 完整示例与验收

文件：examples/ZzPureToolsExample/ZzExampleProgressRingPage.cpp，页面枚举/分发/工厂/路由/CMake/烟测/翻译，ZzFluentUI/tests/ZzFluentScreenshotTest.cpp 及环相关基线。

- [x] 路由 progress-ring 转到 showcase 组合页并移动到 liquid-gauge 后，显示“环形进度条(ProgressRing)”，总数37；测试断言真实路由分类/顺序先失败。
- [x] 参考80px的0/25/50/75/100与不确定六样例；属性区156px预览默认65、标题“已完成”、标题12px、值24px、间距4。
- [x] 完整进度、忙碌周期、半像素厚度、环/轨道色、标题、格式、字体大小、文字间距、标题/值色、忙碌/文字/禁用、恢复默认和代码示例；颜色自动/手动与主题同步；提供可交互的自定义中心示例。
- [x] 烟测驱动真实编辑器验证属性和重置及中心切换，英文翻译完整。
- [x] 用真实 ExProgressRing、匹配字体和配色做浅深色静态比较；新增扩展环截图三主题×四DPR，只有旧环外观实际改变才更新旧环基线。
- [x] 构建示例、行为、截图；运行相关 CTests 和公共头/架构检查；独立审查并交付预览，未提交。

## 验收记录

- 已完成控件、页面、导航、翻译与文档；当前未提交。
- 控件测试 26/26 通过，包含外部析构重入及小数厚度递归通知的真实 RED/GREEN。
- 最终构建成功；核心、中英文集成、workspace、公共头、架构边界共 6 项 CTests 通过。
- 三主题 × 四种 DPR 新截图基线验证通过，原环形进度基线未变化；中英文整页预览已检查。
- 控件规格/质量及页面集成独立审查通过；Qt 6.11.1 / Linux GCC 验证，尚未验证 Windows/MSVC。
