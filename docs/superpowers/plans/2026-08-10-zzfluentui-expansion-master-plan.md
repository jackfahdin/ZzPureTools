# ZzFluentUI 广度扩张总实施计划（第 0 批还债 + 四个控件批次）

**目标：** 在保持现有质量深度（五纪律、六阶段门禁）不变的前提下，把 ZzFluentUI 从 26 个控件扩展到约 40 个，并先修复评审发现的三个短板：截图基线欠债、性能阈值无噪声依据、Typography/Motion/AnimationPolicy 死 API。

**执行环境：** 全部验证步骤必须在 Linux 参考机执行（截图与性能基线只在该机有效）。固定环境：i7-14700 / Xvfb 1920×1080 DPR 1.0 / Mesa llvmpipe / Ubuntu 26.04 / Qt 6.11.1 / GCC 15.2.0 / preset `linux-gcc-reference`，详见 `docs/performance/PERFORMANCE_BASELINE_ZH.md`。

**技术栈：** Qt 6.8+ Widgets、C++20、CMake Presets、Qt Test、Clang-Tidy、ASan/UBSan。

**执行方式（重要）：** 本文档是总计划（路线图粒度）。每个批次动工前，先把该批次展开为一份独立的详细实施计划，放在本目录（`docs/superpowers/plans/2026-08-XX-zzfluentui-<batch>.md`），粒度对齐既有 28 份计划（逐文件、逐绘制函数、旧代码审计、红绿命令、Expected），确认后再写代码。本文档中标注"实施时定/选简单者"的决策点，必须在批次详细计划里给出定论和理由，不允许带着未定决策动工。

**只读参考（禁止修改）：**
- `temp/ElaWidgetTools/`（MIT，55 控件零测试，仅作视觉与交互参考；其实现含 paint 建动画等反模式，只看不抄）
- `temp/WinUI-Gallery/`（WinUI 官方设计规范参考）

---

## 0. 执行约束（每批次退出条件，不可协商）

### 0.1 六阶段门禁与真实命令

每个批次（含第 0 批）退出前必须全部满足。以下命令均在参考机执行，截图与性能为 Linux-only 机制（Windows preset 全部 `ZZ_BUILD_BENCHMARKS: false`，截图测试仅 Linux 注册）：

| 门禁 | 命令 | 红线 |
|---|---|---|
| 配置+构建 | `cmake --preset linux-gcc-reference && cmake --build --preset linux-gcc-reference` | 警告即错误（`zz_enable_project_warnings`） |
| 真断言测试 | `ctest --preset linux-gcc-reference --output-on-failure` | 每个新控件一个独立 `fluent.xxx` 目标，禁止无断言冒烟 |
| 截图基线 | `ctest -R 'fluent.screenshot-' --output-on-failure`（4 个 DPR 档：100/125/150/200） | 视觉变更必须同批用 `ZZ_UPDATE_SCREENSHOTS=1` 重采并逐张人工确认 |
| 架构边界扫描 | `ctest -R Architecture` | 新增规则对新控件零容忍 |
| 性能门禁 | `scripts/ci/run-linux-gates.sh`（内含 Xvfb + taskset + 12 场景相对回归比较） | 回归超阈值必须解释或修复 |
| 文档同步 | README 控件清单、`docs/` 对应章节 | 未验证的能力不得写进文档 |

**性能门禁机制现状（执行者必读，第 0 批要改的就是它）：**
- 每个 benchmark 输出 `build/<preset>/reports/benchmark.<scenario>.json`（`benchmarks/common/ZzPerformanceReporter.cpp` 聚合样本，含 warmup/p95/max 与环境指纹）。
- 两层校验：①绝对阈值门 `benchmark.reference-gate.*`（仅 `ZZ_PERFORMANCE_REFERENCE=ON` 的 `linux-gcc-reference` preset 注册，阈值硬编码在 `benchmarks/CMakeLists.txt:195-215`，如 startup p95≤300ms、frame-time p95≤16.7ms、idle cpu≤0.5%）；②相对回归门 `cmake/ZzComparePerformanceReport.cmake`，逐 metric 比 p95 和 max，先校验 14 项环境/构建指纹一致，回归上限由 `-DZZ_MAX_REGRESSION_PERCENT=10` 传入（默认 10，**不是硬编码**）。
- CI 序列在 `scripts/ci/run-linux-gates.sh:60-130`：`linux-gcc-benchmarks` preset 构建 → `taskset -c 10 ctest --preset linux-gcc-benchmarks -j1` → 对 12 个场景循环调 `ZzComparePerformanceReport.cmake` → 追加 `linux-clang-asan-benchmarks` 下 2 个 ASAN 用例。
- 基线重采：`linux-gcc-reference` preset 跑完后把 `build/linux-gcc-reference/reports/benchmark.<s>.json` 逐字复制到 `docs/performance/reference/linux/<s>.json`（映射表见 `PERFORMANCE_BASELINE_ZH.md:127-138`）。

**截图机制现状（执行者必读）：**
- 单文件 QTest：`ZzFluentUI/tests/ZzFluentScreenshotTest.cpp`（约 7350 行），可执行 `ZzFluentScreenshotTest`。
- 基线目录 `ZzFluentUI/tests/baselines/linux/dpr-{100,125,150,200}/`（宏 `ZZ_FLUENT_SCREENSHOT_BASELINE_DIR`，`ZzFluentUI/tests/CMakeLists.txt:459-496`），按场景×主题命名（如 `cards-dark.png`、`high-contrast.png`）。
- 比对：文字区域遮罩剔除后逐像素比 RGBA，单通道容差 3，差异像素比例上限 0.005（参考 Qt 6.11）/ 0.02（其他 Qt minor）；失败时把 `*-actual.png`/`*-diff.png` 写到 `build/.../reports/fluent-screenshots/dpr-*/`。
- 环境钉死：Qt 6.11、DejaVu Sans 10pt、Fusion 基样式、`QT_QPA_PLATFORM=offscreen`、`QT_SCALE_FACTOR_ROUNDING_POLICY=PassThrough`。
- **重采 = 带 `ZZ_UPDATE_SCREENSHOTS=1` 重跑同一测试覆写基线 PNG，无独立脚本。**
- 第二套：`examples/ZzPureToolsExample` 的 example 截图烟测，基线在 `examples/ZzPureToolsExample/tests/baselines/linux`，走 `ZZ_PURETOOLS_EXAMPLE_*` 环境变量。

### 0.2 反模式清单（历史计划提炼，新控件审查逐条对照）

- 禁止每控件独立 `QProxyStyle`；样式集中在应用级 `ZzFluentStyle`（gallery 用法见 `examples/ZzFluentControlsGallery/main.cpp`：`ZzThemeController` + `new ZzFluentStyle(&controller, fusion)`）。
- 禁止 paint 中分配 `QPixmap`、`render()` 自身、创建动画/定时器/event filter。
- 禁止硬编码色值与像素尺寸；颜色走 `snapshot->color(ZzColorToken::*)`，尺寸走 `snapshot->metric(ZzMetricToken::*)`，字号走 `snapshot->font(ZzTypographyToken::*)`，时长走 `snapshot->duration(ZzMotionToken::*)`。
- 禁止 `findChild` 查 Qt 内部 objectName；自家子控件 objectName（如 `zzMessageBarCloseButton`）供测试定位是既定模式，允许。
- 禁止 mutable 成员保存跨控件绘制状态。
- 禁止动画 lambda 捕获裸 `this/d` 而无生命周期保护。
- 禁止绕过 `QAction`/`QIcon` mode/state、助记键、elide、RTL 语义手绘文本。

---

## 1. 第 0 批：还债与加固（先于一切新控件，独立提交）

### 1.1 截图基线重采

**背景：** 2026-08-09 在 Windows 本机完成 4 次视觉变更，Linux 截图基线未同步，当前 `fluent.screenshot-*` 在参考机必然失败：

- `d20fad9` item delegate 选中态改为圆角背板 + accent 指示条（`ZzFluentItemDelegatePrivate.cpp` 重写）
- `57ed806` 树形分支区绘制接管（`ZzFluentStylePrivate::drawItemViewRow` 新增，保留斑马纹 Alternate 分支）
- `ce5ce1a` 指示条移至标题内容左缘 + 背板底色填充消接缝
- `2348ecc` 树形 item 内容统一内移 10px（`ZzFluentItemDelegatePrivate.cpp:60` 附近）

**任务：**
1. 参考机构建后在**重采前先跑一次现状**，确认失败场景集中在 delegate/tree 相关截图（`ZzFluentScreenshotTest.cpp` 中含 item view/tree 场景），若出现无关场景失败先排查环境问题。
2. `ZZ_UPDATE_SCREENSHOTS=1 ctest -R 'fluent.screenshot-' --output-on-failure` 重采 4 个 DPR 档全部基线。
3. 逐张人工确认（重点 dpr-100）：选中指示条为 3px 圆角竖条、位于标题内容左缘（不随树层级内移）；背板圆角连续无接缝、无前后高度差；斑马纹在 Alternate 行保留且不被背板露边；指示条与文字间距叶子行约 17px、组行约 6px。
4. `examples/ZzPureToolsExample` 截图烟测如涉及同区域一并重采。
5. 提交基线 PNG，commit message 记录覆盖的源码 commit 范围（`d20fad9..2348ecc`）。
6. 立规写入 `docs/development/CODING_STANDARD_ZH.md`：视觉变更提交必须同批包含基线重采，否则视为未完成。

### 1.2 性能阈值噪声治理

**背景：** 相对回归门统一传 10%，但 llvmpipe 软渲染下部分场景（尤其 frame-time 类）跨轮噪声可能逼近或超过 10%，阈值无实测依据，存在误报/漏报双重风险。注意 reporter 已在单轮内聚合样本给 p95/max，本任务治理的是**跨轮**方差。

**任务：**
1. 在参考机用 `linux-gcc-benchmarks` preset 连续跑 3 轮全量 benchmark（每轮 `taskset -c 10 ctest --preset linux-gcc-benchmarks -j1`），保留每轮 12 份报告。
2. 写一个小工具（Python 或 CMake 脚本，放 `benchmarks/common/` 或 `scripts/ci/`）统计每场景每 metric 的跨轮变异系数，输出噪声带表。
3. 决策阈值：每场景 `ZZ_MAX_REGRESSION_PERCENT = max(10, ceil(P95 噪声比))`；把逐场景阈值写进 `run-linux-gates.sh:103-123` 的循环（场景→阈值映射表），不再全局一刀切。
4. 噪声带长期 >20% 的场景标记"仅供参考不做门禁"，在该循环中跳过并打印说明。
5. 方法与实测噪声数据写入 `docs/performance/PERFORMANCE_BASELINE_ZH.md` 新章节，作为阈值依据存档。

### 1.3 死 API 处置

**现状（grep 证实）：** `ZzAnimationPolicy`、`ZzMotionToken`、`ZzTypographyToken` 只被 foundation 自身与 tests 引用，无生产消费。

**接口现状（激活时直接用，不要改签名）：**
- `ZzThemeSnapshot::font(ZzTypographyToken) → QFont`（5 档：Caption/Body/BodyStrong/Subtitle/Title）
- `ZzThemeSnapshot::duration(ZzMotionToken) → int ms`（4 档：Fast/Normal/Slow/PageTransition）
- `ZzAnimationPolicy::adjustedDuration(int ms, bool reducedMotion, bool essential) → int`（reducedMotion 时非必要动画 ≤50ms；snapshot 自带 `reducedMotion()`）

**激活方案：**
- 批次 2 导航指示条动画与 Expander 展开动画：`duration(ZzMotionToken::Normal/Fast)` 取时长，`ZzAnimationPolicy::adjustedDuration(..., snapshot->reducedMotion(), /*essential=*/false)` 做降级。
- 批次 1 的 ContentDialog 标题/正文、InfoBadge 数字、TeachingTip 标题：`font(ZzTypographyToken::Title/Body/Caption)`。
- 执行者若发现某令牌确实无合理消费场景，走 deprecated 流程（文档 + `QT_DEPRECATED` 标记）并在批次总结记录决策；不允许继续空转。

### 1.4 架构扫描新增规则

**任务：** 扩展 `tests/Architecture/ZzArchitectureAudit.cmake`：
1. 新增检查：`ZzFluentUI/widgets/src/` 下禁止裸色值字面量（`QColor(`、`QRgb(`、`0xFF[0-9A-Fa-f]{6}`、`setStyleSheet` 任意出现）与裸圆角/尺寸魔数（匹配模式在 cmake 注释中说明）；必须走 token/metric。
2. 对现有源码生成白名单文件（`tests/Architecture/fixtures/` 下新增，记录当前违例点，只减不增）。
3. 批次 1-4 新文件不在白名单，违例即门禁失败。
4. 接入 `RunArchitectureChecks.cmake` 总入口。

### 1.5 第 0 批提交边界

4 个独立 commit：①截图基线重采（含 example 基线）②性能噪声治理（工具 + run-linux-gates.sh + 文档）③架构扫描规则 + 白名单 ④`CODING_STANDARD_ZH.md` 立规。不含任何新控件；死 API 激活随批次 1/2 各自提交。

---

## 2. 前置投资（随第 0 批末或批次 1 开头完成）

### 2.1 绘制原语沉淀到 ZzFluentPainter

**现状：** `ZzFluentPainter.h`（52 行静态工具类）只有 2 个 API：`drawControlBackground(...)`、`drawFocusRing(...)`。而"圆角背板 + accent 指示条"目前在 `ZzFluentStylePrivate::drawItemViewRow`（widgets/src/private/ZzFluentStylePrivate.cpp，约 1871 行文件内）和 `ZzFluentItemDelegatePrivate` 中各手写了一份——这正是要消除的重复。

**任务（纯内聚重构，不改任何视觉输出，先跑截图测试确认零差异）：**
1. 新增原语（签名形式，实现者自定细节）：
   - `static void drawRoundedBackdrop(QPainter*, const QRectF&, const ZzThemeSnapshot&, ZzColorToken fill, qreal radius)` —— 圆角背板
   - `static void drawAccentIndicator(QPainter*, const QRectF& barRect, const ZzThemeSnapshot&)` —— accent 指示条（3px 圆角竖条，调用方算好 rect）
   - `static void drawPopupSurface(QPainter*, const QRectF&, const ZzThemeSnapshot&)` —— 弹出表面（SurfaceSecondary + ControlStroke + CornerRadiusMedium，从 drawMenuPanel 提取）
2. `drawItemViewRow` 与 `ZzFluentItemDelegatePrivate::paint` 改为调用原语，删除重复画法；保留"突出 radius 由对方覆盖"的拼接技巧与 RTL visualRect 处理，注释里说明。
3. 新控件只允许调用原语，不重复手写。

### 2.2 令牌分层与新令牌流程

- 现状分层已成立：`ZzColorToken`（13 值）、`ZzMetricToken`（9 值）、`ZzTypographyToken`（5 值）、`ZzMotionToken`（4 值）+ `ZzThemeSnapshot` 不可变快照 O(1) 解析（std::array 按 Count 定长）。
- 新控件需要新令牌时：在对应枚举 `Count` 前插入新值 + `ZzThemeSnapshot.cpp` 填充数组，调用方零改动。禁止绕过令牌直接引用原始色（由 1.4 的扫描规则强制）。
- 不追求 WinUI 的 ~130 语义刷子全量，只为批次 1-4 实际需要补令牌（预计：Dialog 遮罩色、InfoBadge 三态色、RatingControl 星级色、Drawer 遮罩色等，实施时逐项评审）。

### 2.3 新控件文件清单模板（实测现状，照此执行）

新增控件 `ZzXxx` 要动的文件（以 ZzMessageBar/ZzNavigationPane 为参照）：

1. 新建 `ZzFluentUI/widgets/include/ZzFluentUI/ZzXxx.h`：`final : QWidget`（或合适基类），`Q_OBJECT` + `Q_DISABLE_COPY_MOVE`，Q_PROPERTY + 信号，`std::unique_ptr<ZzXxxPrivate> d_ptr`（PIMPL，非 Qt d-pointer 宏）；配套枚举单独成头（参照 `ZzMessageSeverity.h`，带 `Q_DECLARE_METATYPE`）。
2. 新建 `ZzFluentUI/widgets/src/ZzXxx.cpp` + `widgets/src/private/ZzXxxPrivate.h/.cpp`；子控件设 objectName 供测试定位；组合型控件（不自绘）参照 MessageBar：`setAutoFillBackground(true)`，视觉交给应用级 style。
3. `ZzFluentUI/CMakeLists.txt` 改 2 处：`zz_fluent_ui_sources`（42–98 行区）加 2 个 cpp；含 Q_OBJECT 则 `zz_fluent_ui_moc_headers`（99–127 行区）加头。公共头无需手动登记（`cmake/ZzLibraryTarget.cmake:64-70` GLOB 自动收集，`tests/PublicHeaderConsumer` 自动校验）。
4. 需要标准控件绘制时：`ZzFluentStylePrivate.{h,cpp}` 加 `drawXxx()`，`ZzFluentStyle.cpp` 的 drawPrimitive(283 行起)/drawControl(396 行起)/drawComplexControl(487 行起) 分派链加分支；`pixelMetric`（72–129 行）按需加 case。纯组合控件跳过此步。
5. `ZzFluentUI/tests/ZzXxxTest.cpp`（QTEST_MAIN 模式）+ `ZzFluentUI/tests/CMakeLists.txt` 复制 5 段式注册块（参照 301–314 行 ZzMessageBarTest：`add_executable` → 链 `Qt6::Test Qt6::Widgets Zz::FluentUI` → AUTOMOC → `zz_enable_project_warnings`/`zz_enable_sanitizers` → `add_test(NAME fluent.xxx ...)` + LABELS/ENVIRONMENT `QT_QPA_PLATFORM=offscreen`）。
6. 断言覆盖 6 维度（项目既定模式）：①状态/信号幂等（重复 set 信号只发一次，QSignalSpy）②无障碍（accessibleName/toolTip）③交互意图（如 Esc 发信号但控件不自隐藏——"不拥有业务状态"约定）④计时/生命周期（内部 QTimer/动画对象数有界，`findChildren<QTimer*>().size()` 断言）⑤国际化（手动发 `QEvent::LanguageChange` 验证刷新）⑥几何/对象数有界（参照 NavigationPaneTest：尺寸断言 + descendant 数不增长）。
7. `examples/ZzFluentControlsGallery/ZzFluentControlsGalleryPrivate.{h,cpp}` 在对应 `buildXxxColumn` 加 `zzSectionTitle` 分区示例；新分类才加 build 方法。
8. 截图场景：`ZzFluentScreenshotTest.cpp` 加新控件场景（Light/Dark/高对比），纳入基线重采流程。

---

## 3. 批次 1：反馈与对话层（ContentDialog / InfoBadge / TeachingTip）

**设计参照：** WinUI-Gallery 对应页面的规范（尺寸、状态、行为）；ElaWidgetTools 只看视觉。三者均为组合型控件：视觉尽量走令牌 + `ZzFluentPainter` 原语 + palette，不为它们扩展 `ZzFluentStyle` 分派（确有必要时按 §2.3 第 4 步走）。

### 3.1 ZzContentDialog

- **API 草图：** `final : QDialog`；Q_PROPERTY：`title`、`text`、`primaryButtonText`、`secondaryButtonText`、`closeButtonText`、`primaryButtonEnabled`、`defaultButton`（枚举 Primary/Secondary/Close/None）；信号：`primaryClicked()`、`secondaryClicked()`。按钮区复用 `ZzPushButton`；不接管 QDialog 的模态、焦点链、Esc 语义。
- **Typography 激活点：** 标题 `font(ZzTypographyToken::Title)`、正文 `Body`、按钮 `Body`；测试断言实际 font 与 snapshot 一致。
- **遮罩：** 需要新增遮罩色令牌（§2.2 流程），modal 遮罩由 private 在半透明顶层事件过滤中绘制或独立 overlay widget，二选一，实施时选简单者并写注释；禁止每帧重绘全屏 pixmap。
- **测试清单：** 按钮信号路由与幂等、Esc 触发 close 而非 primary、默认按钮焦点、LanguageChange 刷新三处文案、字体令牌断言、对象数有界。
- **截图场景：** Light/Dark/高对比 × 两按钮/三按钮布局。

### 3.2 ZzInfoBadge

- **API 草图：** `final : QWidget`；配套枚举头 `ZzInfoBadgeKind.h`（Dot/Number/Icon）；Q_PROPERTY：`kind`、`value(int)`、`maximumValue(默认 99)`、`severity`（复用 `ZzMessageSeverity` 或中性 accent，实施时定）；99+ 截断显示；RTL 布局。
- **绘制：** 自绘圆/圆角胶囊（`drawAccentIndicator` 思路的填充变体），尺寸走 metric token；数字字号 `font(ZzTypographyToken::Caption)`。
- **测试清单：** value 越界钳制与显示文本（"99+"）、kind 切换时 minimumSizeHint 变化、信号幂等、无障碍 accessibleName 含数值。
- **截图场景：** 三 kind × 三主题。

### 3.3 ZzTeachingTip

- **API 草图：** `final : QWidget`（工具窗 popup）；Q_PROPERTY：`title`、`text`、`target`(QWidget* 非拥有)、`placement`（枚举 Top/Bottom/Left/Right/Auto）；`showForTarget()`/`dismiss()`；信号 `dismissed()`。
- **定位：** Auto 时按屏幕可用空间翻转；target 移动/析构时跟随或自动 dismiss（`QPointer` 守护）；屏幕边界钳制。这些是 WinUI 规范的硬行为，逐条写测试。
- **与 QToolTip 的边界：** ToolTip 是被动态（已有 style 支持），TeachingTip 是持久/交互态；禁止复刻 Ela 的反模式（每次显示新建 opacity 动画、不可取消 singleShot）；出现/消失动画若做，时长走 `duration(ZzMotionToken::Fast)` + `ZzAnimationPolicy::adjustedDuration`。
- **测试清单：** 四向 placement 几何断言、边界翻转、target 析构后 dismiss、Esc/点击外部 dismiss、动画 policy 降级（reducedMotion 时直接终态）。
- **截图场景：** 终态四向 × Light/Dark。

**批次 1 提交边界：** 每控件一个 commit（实现+测试+截图基线+gallery 示例），遮罩/令牌变更并入对应控件 commit。

---

## 4. 批次 2：导航补全 + 指示条动画（死 API 主激活批）

### 4.1 导航指示条两段式动画（本批重点）

- **参考实现：** `temp/ElaWidgetTools/ElaWidgetTools/DeveloperComponents/ElaNavigationStyle.cpp:22-63`——选中切换时旧指示条缩没、新指示条长出，`QPropertyAnimation` 300ms `InOutSine`。
- **落地位置：** `ZzNavigationView`/`ZzNavigationPane` 的选中指示条与 item delegate 树形指示条（视觉已与 2026-08-09 定稿一致：3px 圆角竖条、标题左缘；动画只改几何不改样式）。
- **死 API 激活：** 时长 `snapshot->duration(ZzMotionToken::Normal)`（若 300ms 与 Normal 档不符，按 §2.2 流程调整档位值并更新 MotionToken 测试）；降级 `ZzAnimationPolicy::adjustedDuration(ms, snapshot->reducedMotion(), false)`——reducedMotion 时缩到 ≤50ms 或直接跳变（测试断言终态正确即可）。
- **实现约束：** 动画对象由控件/private 持有（成员 `QVariantAnimation` 或 `QPropertyAnimation`），`DeleteWhenStopped` 不能替代生命周期管理；旧条/新条两段可用一个动画驱动两个高度值；主题切换/快速连续选中时动画可中断并从当前值反向，测试覆盖中断路径。
- **测试清单：** policy 禁用时 `findChildren<QAbstractAnimation*>().size()==0` 且直接终态（呼应 NavigationPaneTest 既有"无动画约束"模式，该测试需改为"policy 关闭时无动画"）；动画开启时 valueChanged 单调、finished 后指示条 rect 与静态绘制一致；快速切换 10 次后对象数有界。
- **截图：** 仍在动画终态采集，基线不变。

### 4.2 ZzExpander

- **API 草图：** Q_PROPERTY：`headerText`、`expanded(bool)`、`contentWidget`(QWidget*)；信号 `expandedChanged(bool)`；展开/收起走高度动画（QVariantAnimation 驱动 maximumHeight 或直接几何，禁止重建子控件），时长 `duration(ZzMotionToken::Normal)` + policy 降级。
- **测试清单：** 展开态 sizeHint 变化、contentWidget 所有权不转移、动画关闭时瞬时终态、LanguageChange。

### 4.3 ZzPivot

- **API 草图：** 顶部标签导航；Q_PROPERTY：`currentIndex`、`count`；`addItem(text)`/`itemText()`；信号 `currentChanged(int)`；选中下划线用与指示条同一原语（横条变体），移动动画复用 4.1 的机制。
- **与 ZzTabBar/ZzTabWidget 的边界：** Pivot 是页面级导航（无 close/detach），TabView 是文档级；实施前写一段对比注释进头文件。

### 4.4 ZzDrawer

- **API 草图：** 边缘滑入模态面板；Q_PROPERTY：`edge`（Left/Right）、`modal`、`widthHint`；`open()`/`close()`；遮罩复用批次 1 ContentDialog 的遮罩令牌与机制；滑入动画 `duration(ZzMotionToken::Normal)` + policy。
- **边界：** 与 `ZzNavigationPane` 折叠态职责区分（Drawer 是临时覆盖层，Pane 是常驻导航），注释写明。

### 4.5 TabView 评估（先做决策再动手）

现有 `ZzTabWidget`/`ZzTabBar` 已有 tear-off 能力（见 `2026-08-05-zzfluentui-tear-off-tabs.md`）。任务：评估 WinUI TabView 相对现有实现的新增价值（拖动重排/新建按钮/关闭中键等），输出决策（增强现有 or 新控件）写入批次总结；若增强，走既有文件；若新建，按 §2.3 清单。

**批次 2 提交边界：** 指示条动画单独 commit（含 policy/令牌激活）；每控件各一个 commit。

---

## 5. 批次 3：输入补全

### 5.1 ZzPasswordBox

- **API 草图：** 基于 `QLineEdit` 子类化；Q_PROPERTY：`password`（或沿用 text）、`revealEnabled(bool)`；眼睛按钮为子 QToolButton（objectName `zzPasswordRevealButton`），切换 `echoMode`；不接管输入法与平台密码管理语义。
- **测试清单：** reveal 切换 echoMode 断言、按钮可聚焦性（默认 `Qt::TabFocus` 排除还是包含，按 WinUI 行为定并写注释）、LanguageChange 刷新按钮 tooltip、无障碍。

### 5.2 ZzSplitButton

- **API 草图：** 主按钮 + 下拉箭头两区；Q_PROPERTY：`text`、`icon`、`menu`(QMenu*)；信号 `clicked()`（主区）；两区 hover/pressed 状态独立（自绘或 style 分支，实施时选简单者）；弹出复用既有菜单 surface（drawMenuPanel 已 Fluent 化）。
- **测试清单：** 两区几何命中（`subControlRect` 或私有 hitTest 断言）、menu 弹出归属、键盘（Down 开菜单）、状态独立重绘。

### 5.3 ZzRatingControl

- **API 草图：** Q_PROPERTY：`value(qreal)`、`maximum(int 默认 5)`、`precision`(Full/Half)、`readOnly`；信号 `valueChanged(qreal)`；星级自绘（填充比例裁剪），accent 色走 token（如需新增星级令牌走 §2.2）；键盘左右键步进、Home/End。
- **测试清单：** Half 精度取整、readOnly 拒交互、键盘可达、值钳制、无障碍值描述。

### 5.4 ZzKeyBinder

- **API 草图：** Q_PROPERTY：`keySequence`(QKeySequence)、`recording(bool)`；信号 `keySequenceChanged`、`conflictHint(QString)`（冲突判定职责在使用方，控件只发提示）；录制态捕获 keyPressEvent 显示键帽样式文本；Esc 取消录制、Backspace 清空。
- **测试清单：** 修饰键组合捕获序列、Esc/Backspace 语义、录制态进出信号幂等。

### 5.5 ZzColorDialog（先评估后实现）

- 第一步评估：标准 `QColorDialog` 走 style 级 Fluent 化的可达性（遵循 `2026-08-06-zzfluentui-popup-surfaces.md`"标准对话框优先 style 路线"的架构原则）。评估结论写入批次总结。
- 若 style 路线不可行，新建 `ZzColorPicker`（色板网格 + 自定义区），按 §2.3 清单执行；不追求 WinUI ColorPicker 全光谱轮盘，先网格+RGB 输入。

**批次 3 提交边界：** 每控件一个 commit；ColorDialog 评估结论单独 commit（文档），实现另行提交。

---

## 6. 批次 4：软件 Mica/Acrylic（窗口特效）

**现状（实施起点）：** `ZzWindowKit` 公开枚举 `ZzWindowBackdrop`（None/Blur/Acrylic/Mica/MicaAlt/Automatic，include/ZzWindowKit/ZzWindowBackdrop.h），入口 `ZzWindowAgent::setBackdrop()`，平台声明与支持矩阵全在 `src/private/ZzQWindowKitBackend.cpp`（Win11≥22000→Mica、22H2≥22621→MicaAlt；Linux 仅 None）；测试替身 `tests/private/ZzFakeWindowBackend.*`。

**设计：**
- 新增软件路径组件（建议 `ZzWindowKit/src/private/ZzSoftwareBackdrop.{h,cpp}`）：对窗口背景做采样 + 指数模糊 + 噪声/色层叠加，参考 ElaWidgetTools 软件 Mica 方案，但修正其反模式：模糊结果按背景变化失效缓存，禁止每帧重算；eventFilter 注册/注销严格配对；DPR 变化时缓存重建。
- **降级链（写进文档与代码注释）：** `Automatic` 解析为：硬件 Mica（Win11 22H2+）→ 软件 Mica（其余 Windows/Linux）→ `None` 纯色（reducedMotion/高对比/性能策略禁用时）。Linux 当前 `Automatic→None` 的行为变为 `Automatic→软件 Mica`，属于行为变更，需要在 `docs/development/PLATFORM_SUPPORT_ZH.md` 同步声明。
- **性能红线：** 窗口移动/缩放时模糊不得造成可感知掉帧；新增 benchmark 场景（如 `benchmark.backdrop`）纳入 `benchmarks/CMakeLists.txt` 的绝对阈值门与 `run-linux-gates.sh` 的 12 场景循环（注意参考机 llvmpipe 下软件模糊很慢，阈值按第 0 批噪声带方法设定，过慢则该场景标记"仅供参考"）。
- **测试：** 三路径切换（fake backend 注入能力位）、缓存失效（背景变化后重算次数有界）、policy 禁用时回退 None、高对比主题下不启用。

**批次 4 提交边界：** 软件背景组件 + agent 集成一个 commit；benchmark 与文档一个 commit。

---

## 7. 批次顺序与依赖

```
第 0 批（截图基线 / 性能噪声治理 / 架构扫描规则 / 立规）
   └─ 前置投资（ZzFluentPainter 原语、令牌流程、§2.3 清单）
        ├─ 批次 1（反馈对话层；Typography 激活；遮罩机制产出）
        ├─ 批次 2（导航补全 + 指示条动画；Motion/AnimationPolicy 激活）
        ├─ 批次 3（输入补全）
        └─ 批次 4（软件 Mica；依赖批次 1 遮罩机制与第 0 批性能门禁）
```

批次 1、2、3 之间无硬依赖，可按参考机时间灵活调整；批次 4 建议最后。

## 8. 总验收红线

- 每批次：`ctest --preset linux-gcc-reference` 全绿；`fluent.screenshot-*` 同批重采并人工确认；`ctest -R Architecture` 过（白名单只减不增）；`run-linux-gates.sh` 绿；README/docs 同步且只写已验证能力。
- 全部完成后：控件数 26 → 约 40；无新增死 API（每个公开令牌/策略至少一个生产消费者）；`docs/performance/PERFORMANCE_BASELINE_ZH.md` 含噪声带章节；`PLATFORM_SUPPORT_ZH.md` 含软件 Mica 降级链声明。
- 任何批次发现既有体系新短板，按第 0 批方式先记录再修，不允许带病扩张。
