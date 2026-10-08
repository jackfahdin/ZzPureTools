# 边框光束

## 目标与设计

参考 FluentUIStyle 的 ExBorderBeam、ExBorderBeamButton 和 PageBorderBeam，提供
`ZzBorderBeam` 容器及 `ZzBorderBeamButton` 按钮。保留原生布局、按钮点击和键盘语义。
二者共享私有绘制与动画实现，不引入 Qt 私有 API。

沿内缩圆角边框的真实周长均匀推进，按路径长度采样双色渐变和透明拖尾。
支持最多 8 束光、正反方向、初始位置、周期、线宽、长度、圆角和颜色覆盖，
浅深色默认值与参考一致。高对比度使用应用调色板，减少动态效果时静止显示。
暂停、隐藏或禁用时停止计时，恢复时从原位置继续；修改方向或周期保持光束头部位置。

## 实现顺序与验收

1. 先写生命周期、非法参数、暂停恢复与原生按钮交互测试，验证缺少实现时失败。
2. 添加公开控件和共享私有动画/绘制实现，运行控件测试。
3. 自定义控件导航中放在范围滑块之后，迁移三种示例、按钮及实时属性编辑，补英文翻译。
4. 固定动画位置，对照真实参考绘制；验证浅色、深色、高对比度及四档 DPR。
5. 运行示例集成测试和独立代码审查，不自动提交。

## 公共约定

- 长度限制 0–10000，线宽 0.5–32，圆角 0–1000，周期 100–600000 毫秒。
- 初始位置限制 0–1，非有限浮点输入忽略；无效 QColor 清除局部颜色覆盖。
- ThemeConfig 提供浅深色默认配置；配置颜色无效时回退到调色板。
- 按钮固定主题或覆盖背景时自动选择有对比度的默认文字色，尊重显式 ButtonText。
  容器只绘制表面，内部任意子控件的前景由调用方按 Qt palette 管理。
- `progress()` 返回当前光束头部的归一化位置，`restartAnimation()` 回到初始位置。
- `animationEnabled` 表示播放意图，`running` 表示实际运行状态。
- 本地验证环境为 Linux / GCC / Qt 6.11.1，Windows/MSVC 需在目标系统验证。

## 验证记录

- 构建：ZzBorderBeamTest、ZzFluentScreenshotTest、ZzPureToolsExample、ZzExampleWorkspaceSmokeTest。
- CTest 共 9 项通过：光束控件、范围滑块回归、四档截图、中文和英文示例集成、工作区烟测。
- 新增三主题 × 四档 DPR 静态截图；光束位置固定，覆盖双光束、反向、宽线、直角和禁用。
- 编译真实 FluentUIStyle 控件进行浅深色并排对照，人工检查圆角、渐变及拖尾。
  对照产物在 build/border-beam-reference，实际页面在 build/border-beam-preview/border-beam.png。
- 独立审查发现的按钮相反主题文字对比问题已修复；渲染回归先失败后通过，保留显式文字配色。
- 未改动参考仓库。
