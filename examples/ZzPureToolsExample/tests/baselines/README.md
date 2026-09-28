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

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `6d5bf92ab786651314ba438c123654d98e34d3c3a1e862ea200acb611b9605cb` |
| `linux/dpr-100/high-contrast.png` | `402501adca1cda041c525961667b958f3a5397d4407b01d3ade9d4a8d57c8267` |
| `linux/dpr-100/light.png` | `3354c5d677cff7233948caf075bbf3345482ce538cbd23627aa6ba624a3a4ca6` |
| `linux/dpr-125/dark.png` | `99871b107ee9a368a300f02613caa291c42abc3584c5ac4051853102d1bceaf2` |
| `linux/dpr-125/high-contrast.png` | `69dd1cbf3e392736b2b405eda7e95a5d0b9802d42605c41f35f8736a9051102b` |
| `linux/dpr-125/light.png` | `16af2a890d35ba0e46f84bb7e3de61adc046505bdff4664ddfdb17ea3af5a02b` |
| `linux/dpr-150/dark.png` | `ab5052f2637f3236b0da8c2ce134f17d2484180139641dcb3abe5b1e1cfbb2ce` |
| `linux/dpr-150/high-contrast.png` | `a55eade64686b9ba919140e6f77a3c32ed994ed3aedb17b58114f6bb64161e7c` |
| `linux/dpr-150/light.png` | `fb1e95c6073f77db80bc198d93730a4e73df31458e749a3944f86be04f9b5a23` |
| `linux/dpr-200/dark.png` | `89276a5635ed15a4f3c3c3bd742edd7b3a0a323d2aa414a4fb2a17da68e6101d` |
| `linux/dpr-200/high-contrast.png` | `362ee28d8d1feb5a92118afa0f70dabf139d7ea1a7f44538e8d33ffbf68a4d83` |
| `linux/dpr-200/light.png` | `bc52e25919433aad82f9cb53429d3c06fd80941a3130b4d962a62fa52b9ca735` |
