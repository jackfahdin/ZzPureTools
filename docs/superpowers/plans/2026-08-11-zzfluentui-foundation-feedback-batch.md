# ZzFluentUI 第 0、1 批详细实施计划

本文是 `2026-08-10-zzfluentui-expansion-master-plan.md` 的决策完整执行规格。实施者不得把本文中的接口、所有权、定位顺序或性能门限改成临时方案；发现平台 API 限制时先以测试记录事实，再更新本文。

## 1. 当前状态和交付边界

- Linux 截图基线和 Item View 视觉统一已由 `568ba21` 完成，不在本批重复处理。
- 第 0 批交付逐指标性能噪声门禁、反馈组件绘制原语、所需主题令牌和新增源码架构扫描。
- 第 1 批交付 `ZzInfoBadge`、`ZzContentDialog`、`ZzTeachingTip`。三者提供完整标题/正文/内容/操作能力、键鼠、焦点、无障碍、RTL 和高对比支持，但不复制 WinUI 全部装饰属性。
- UI 只展示状态和发送 intent，不读取 repository、网络、数据库、领域对象或业务模型。
- Linux 参考机执行完整测试、截图、Sanitizer 和性能门禁。Windows MSVC/MinGW 与 macOS AppleClang 只记录 shared/static 静态构建结果，不冒充真机验收。

## 2. 第 0 批

### 2.1 性能噪声工具和门限 schema

新增 `scripts/ci/ZzAnalyzePerformanceNoise.cmake`，输入为：

- `ZZ_RUN_DIRECTORIES`：分号分隔的至少三个目录，每个目录必须包含相同的 `benchmark.<scenario>.json` 集合。
- `ZZ_OUTPUT_JSON`：候选逐项策略报告。
- `ZZ_OUTPUT_MARKDOWN`：供人工审核的表格报告。

脚本校验 schema、场景、环境指纹、指标名和单位一致。每个 `scenario/metric/p95|max` 使用 `((max-min)/min)*100` 计算三轮最大正向相对波动；最小值为零且最大值非零时视为无限波动，全部为零时视为零波动。

正式配置固定为 `docs/performance/reference/linux/regression-thresholds.json`：

```json
{
  "schemaVersion": 1,
  "scenarios": {
    "startup": {
      "metrics": {
        "external-total": {
          "p95": { "mode": "gate", "percent": 10 },
          "max": { "mode": "observe", "percent": 21 }
        }
      }
    }
  }
}
```

波动不超过 10% 时建议 10%；大于 10% 且不超过 20% 时向上取整；大于 20% 时为 `observe`。分析脚本只生成候选文件，人工审核后才替换正式配置。`ZzComparePerformanceReport.cmake` 必须要求 `ZZ_THRESHOLDS`，按当前场景、metric 和 field 读取策略；缺少条目、类型错误或未知 mode 均失败关闭。`gate` 超限失败，`observe` 超限输出 `WARNING` 并继续。

为分析器和比较器各增加 CTest 契约夹具，覆盖有效报告、目录不足、指纹/指标不一致、零值、10%/20% 边界、缺失策略、gate 与 observe。`run-linux-gates.sh` 继续枚举十二个场景，但统一传正式配置文件，不再传 `ZZ_MAX_REGRESSION_PERCENT`。

### 2.2 绘制原语和令牌

`ZzFluentPainter` 增加四个公开静态函数：

- `drawRoundedSurface(QPainter *, const QRectF &, const ZzThemeSnapshot &, ZzColorToken fill, ZzColorToken stroke, qreal radius, qreal strokeWidth)`。
- `drawOverlayScrim(QPainter *, const QRectF &, const ZzThemeSnapshot &)`。
- `drawPopupSurface(QPainter *, const QRectF &, const ZzThemeSnapshot &)`。
- `drawBadgeSurface(QPainter *, const QRectF &, const ZzThemeSnapshot &, ZzColorToken fill)`。

四个函数都要求有效 painter，使用 `QPainter::save/restore`，按 DPR 对齐描边；不得分配 pixmap、timer、animation 或事件过滤器。Item View 继续使用私有 `ZzItemViewVisual`。

`ZzColorToken` 增加 `OverlayScrim`、`Information`、`Success`、`Warning`。`ZzMetricToken` 增加 `OverlayPadding`、`DialogMinWidth`、`DialogMaxWidth`、`BadgeMinDiameter`、`TeachingTipTargetGap`、`TeachingTipMaxWidth`。所有主题和高对比模式显式赋值，并补齐 snapshot 单元测试。

### 2.3 架构扫描

对 `ZzFluentUI/widgets/src` 增加规则：禁止 `setStyleSheet`、`QColor(...)`、`QRgb(...)` 和十六进制主题色。扫描前剥离注释与字符串。既有违规保存在固定 fixture，键为规则、相对路径和代码摘要；白名单只减不增，新文件禁止加入。

尺寸规则只扫描 `setFixedWidth/Height/Size`、`setMinimumWidth/Height/Size`、`setMaximumWidth/Height/Size`、`setContentsMargins`、`setSpacing`、`drawRoundedRect` 的数字实参，避免误伤循环次数和测试数据。具名 `constexpr` 和 token 解析结果允许。

## 3. 第 1 批接口

### 3.1 ZzInfoBadge

`ZzInfoBadge final : public QLabel`，公开枚举 `ZzInfoBadgeKind { Dot, Number, Icon }`，严重性复用 `ZzMessageSeverity`。属性为 `kind`、`value`、`maximumValue`、`severity`、`icon`，每个实际变化只发一次对应信号。

- `value < 0` 收敛为 0；`maximumValue < 1` 收敛为 1；默认最大值 99。
- Dot 不画文本，Number 超过上限显示 `<maximum>+`，Icon 使用 `QIcon::paint` 的 mode/state。
- 数字使用 Caption 字体；尺寸、颜色和背板只读 snapshot token。
- 未设置 accessibleName 时生成可翻译的严重性和数值名称；保留 QLabel 的 StaticText role。

### 3.2 ZzContentDialog

`ZzContentDialog final : public QDialog`。公开枚举：

- `ZzContentDialogButton { None, Primary, Secondary, Close }`
- `ZzContentDialogResult { None = -1, Close = 0, Primary = 1, Secondary = 2 }`

属性为 `title`、`text`、`contentWidget`、三个按钮的 text/visible/enabled、`defaultButton` 和只读 `dialogResult`。提供 `setContentWidget(QWidget *)` 与 `takeContentWidget()`：setter 把 widget 重挂到内部容器并接管所有权，替换时删除旧 widget；调用者需要保留旧对象必须先 take，take 后 parent 为空且所有权交回调用者。

Primary 调用 `done(1)`，Secondary 调用 `done(2)`，Close 与 Escape 调用 `reject()`。Enter 只触发当前可见且启用的 default button，None 不隐式选择。`open/exec/show`、窗口模态和 Qt 焦点链保持原生语义；仅模态显示时在所属 parent window 内容区创建遮罩。关闭或父窗口销毁时清理遮罩并恢复打开前焦点，多窗口间不得共享遮罩。

标题、正文、按钮分别使用 Title、Body、Body 字体。自定义内容只被布局承载，Dialog 不读取其模型。

### 3.3 ZzTeachingTip

`ZzTeachingTip final : public QWidget` 使用 `Qt::Tool | Qt::FramelessWindowHint`，不用 `Qt::Popup`。枚举 `ZzTeachingTipPlacement { Auto, Top, Bottom, Left, Right }`。

属性为 `title`、`text`、`contentWidget`、非拥有 `targetWidget`、`preferredPlacement`、只读 `effectivePlacement`、`lightDismissEnabled`、action text/enabled/visible 和 closeButtonVisible。内容所有权与 ContentDialog 相同。方法为 `showForTarget()`、`dismiss()`；信号为 `actionTriggered()`、`dismissed()`。

Auto 尝试 Bottom、Top、Right、Left。显式方向尝试首选、反向、另两个方向。取第一个完全位于 target 所在 screen `availableGeometry` 内的候选；均不满足时取溢出面积最小者并钳制到可用区域。箭头中心对准 target 中心并钳制到圆角安全区，`effectivePlacement` 反映最终方向。

监听 target 的移动、尺寸、显示、隐藏、parent、screen 和销毁变化；移动/尺寸变化重新定位，隐藏、销毁或无 screen 时 dismiss。light dismiss 仅在点击 tip 与 target 之外时生效，应用级 event filter 在 hide 和析构中注销；默认窗口失活不关闭。Action 只发 intent，不关闭；Close 和 Escape 关闭。

显示隐藏使用 Fast token 的透明度与 4 个逻辑像素位移，时长经 `ZzAnimationPolicy` 调整；结果不超过 50ms 时直接到终态。动画对象由 private 长期持有，快速反向不得增长对象数量。

## 4. 验收矩阵

每个新控件建立独立 Qt Test，覆盖属性幂等、信号次数、键盘、鼠标、焦点、LanguageChange、LTR/RTL、无障碍 role/name/value、parent/target 销毁、多窗口隔离和对象数量有界。TeachingTip 额外覆盖四向定位、边界 fallback、light dismiss 和 reduced motion；ContentDialog 覆盖 result 映射、遮罩与焦点恢复；InfoBadge 覆盖三种 kind 和数值钳制。

三控件加入 Gallery 与综合示例。截图覆盖 Light、Dark、HighContrast 和 DPR 100/125/150/200，源码与基线同提交。Linux 执行 reference 全套、Clang-Tidy、ASan/UBSan 和性能门禁；Windows MSVC/MinGW shared/static 与 macOS arm64/x86_64 shared/static 只做构建检查并记录真实结果。

## 5. 提交顺序

1. `文档：细化 FluentUI 第零批与第一批实施路线`
2. `性能：建立逐指标噪声阈值门禁`
3. `架构：禁止新增裸颜色与关键尺寸魔数`
4. `基础：补充反馈组件绘制原语与主题令牌`
5. `控件：新增信息徽章组件`
6. `控件：新增内容对话框组件`
7. `控件：新增教学提示组件`
8. `文档：完成第一批组件验收与路线状态同步`

每个提交必须先通过对应定向测试；影响共享行为后再跑完整 Linux reference。未执行的平台不得写成已验证。
