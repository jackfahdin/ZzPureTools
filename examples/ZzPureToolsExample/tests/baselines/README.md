# ZzPureToolsExample 视觉基线来源记录

## 来源与授权

- 资源类型：自动渲染的 PNG 测试参考图，不进入安装包或运行时资源。
- 生成来源：本仓库 `ZzPureToolsExample` 的首页、ZzFluentUI 样式和 Qt Widgets
  offscreen 渲染结果；不包含旧版图片、下载素材或第三方摄影/插画像素。
- 作者与维护者：Jackfahdin。
- 许可证：随本项目采用 MIT License。
- 用途：验证综合应用在 Light、Dark、HighContrast 和四档 DPR 下的非文字像素、
  布局、边框、图标与主题颜色稳定性。

## 参考环境

- 生成日期：2026-09-28。
- Qt：6.11.1。
- CMake preset：`linux-gcc-debug`（GCC 15）。
- 平台插件：`offscreen`。
- 字体：DejaVu Sans 10pt。
- locale 与布局：`C.UTF-8`、LTR。
- 逻辑窗口：1280x800。
- DPR：1.0、1.25、1.5、2.0，`PassThrough` rounding policy。

更新时必须在已审 Linux 参考发布机执行：

```bash
GCC_13=/usr/bin/gcc-15 \
GXX_13=/usr/bin/g++-15 \
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 \
ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1 \
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^example\.puretools-screenshot-(100|125|150|200)$'
```

更新后必须关闭 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS` 重新运行测试、人工检查 100% 三主题
和 200% Light，并重新计算本文件的 SHA-256。普通 CI 只比较，不得更新参考图。

本轮基线覆盖 IDE 式六入口合同：左侧会话、文件、组件与设置，右侧属性、任务；
组件入口承载应用导航，设置使用逐窗口 `WindowModal` 窗口。这些布局变化不是 SVG
图标回归。

2026-09-14 在同一 Linux Qt 6.11.1 参考环境同步选择指示条与内容间距变化，人工
检查了 100% 和 200% 的 Light、Dark、HighContrast 场景。Windows 与 macOS
视觉仍须在对应平台验证。

2026-09-28 统一侧面板内容外观：导航树去掉内层边框，背景使用跟随主题的 Window 角色。
同时纳入当前标题栏深浅切换及 Activity 强调色图标效果。已检查 100% 三主题与 200% Light，
保持原截图阈值；仅更新综合 Example 的 12 张基线，控件与工作区截图基线未改写。

2026-09-28 中央区域改为无标签堆叠页面，移除外层“组件示例 / 终端”标签和新建标签按钮。
页面直接从标题栏下方开始，窗口标题跟随当前页面。已检查 100% 三主题与 200% Light；
本次仍只更新 Example 的 12 张基线，不放宽截图比较阈值。

2026-09-28 导航树的实心三角展开标记改为细线折角，原位置、布局和操作保持不变。
已检查 100% 三主题与 200% Light，更新四档 DPR 的 12 张 Example 基线；比较阈值不变。

2026-09-28 基础控件拆分为独立演示页面，中文导航显示中文名与英文控件名，首页总页数更新为 29。
已检查 100% 三主题与 200% Light，更新四档 DPR 的 12 张 Example 基线，关闭更新模式后复验通过；
继续使用原比较阈值。

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `2b3704f747ef4ed6c07b25aad102639410b1ca307f23fe11f4fcf39c07f59e4b` |
| `linux/dpr-100/high-contrast.png` | `d887d3dbfea00fdded86d9ebbaafb00a883d39092ba1c6162e846453543f1d09` |
| `linux/dpr-100/light.png` | `9ef92daf1a20ec8e95dcbe771029f9121fcf03ec5b6643fe98505acec3bbac9e` |
| `linux/dpr-125/dark.png` | `fbb102f4d281e7a5a375da496f39f313882a7c20f9292e9424fc7adfa7295af5` |
| `linux/dpr-125/high-contrast.png` | `92811894a2c795fd0cc8819bd31dc9acdb42941c914f4a6f014600fb392ad914` |
| `linux/dpr-125/light.png` | `ca5fe1cfc1219c130d09f71b3f32c300a6794a7bba7da515e64e8e30ede1bf18` |
| `linux/dpr-150/dark.png` | `bde1989b3f14d3f78d67c92745359fa46a886054bf2b2cf51c5abd0fd42d49bc` |
| `linux/dpr-150/high-contrast.png` | `e9996f794ce83ff0ab798da452a8a09f530c15312855939be85c1733b48fdf35` |
| `linux/dpr-150/light.png` | `c6f06c5cebb9d07d0f1cf02e9aaf8303417a12d17d4a09ff1fc8e6aa11620dca` |
| `linux/dpr-200/dark.png` | `ffa14d254cd8fb994c68e5f021f72de2f2c1087de0505d631c4507a5b9f308a0` |
| `linux/dpr-200/high-contrast.png` | `ab2545bb5dfb2ad9311657ce414dab072e26f796e5283dc0513279b472427567` |
| `linux/dpr-200/light.png` | `d6290be40ea393024b4831bbeced89d0a0ec1f1dcac7c0600b6f14b659de32cc` |
