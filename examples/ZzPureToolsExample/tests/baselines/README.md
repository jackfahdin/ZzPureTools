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

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `a6cb198cb986478204b7c13e72906acb6ccd620a48966d6007aded186465d49c` |
| `linux/dpr-100/high-contrast.png` | `30d117499ac7a7b2111b1b7a17e5d885fd08c0a4cc02ed70f257698ae63f88f9` |
| `linux/dpr-100/light.png` | `e871314119957377e36bef1874fd4bb251f48c1179c4a4e4cb432e193398d61e` |
| `linux/dpr-125/dark.png` | `280be3e0ca71389a7ab32d0e7fa0425bb75841d8799dbf8e35fc4342d647970e` |
| `linux/dpr-125/high-contrast.png` | `4ee7d659b05b075acd261ffd1bcd7b06d2f53d92ed0a5902ffa5e0fd9d431d3b` |
| `linux/dpr-125/light.png` | `314a756192a2661748b00b7501ee7dfd46caf70c79581748e936e4350efc6a77` |
| `linux/dpr-150/dark.png` | `95e5ed95506b2d5c14dc79f954a23beeb2aceab6fae27b16b181784344fc49b4` |
| `linux/dpr-150/high-contrast.png` | `2dc1a6f31867170de2568c7600a88767b82ac0e928f55153f63fd73bcb64e6db` |
| `linux/dpr-150/light.png` | `b028243ca8ac62fff07162cd639e9a33d311a825580e761c2ca1879ee466b400` |
| `linux/dpr-200/dark.png` | `ef1e915ad3ebb40d167d2f6c83e9b5173bb8cb6f6d7faf4434b6256b50585125` |
| `linux/dpr-200/high-contrast.png` | `b0a3428767630be81c135d8af8be38830eccb8dd15894fc4889aa5e87673bb95` |
| `linux/dpr-200/light.png` | `27120b29382db756d68bd5fedb1884c9df368ddcc44ec7fceba47a40f796e13a` |
