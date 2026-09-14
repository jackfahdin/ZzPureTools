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

- 生成日期：2026-09-14。
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

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `e6a18b584616ac511ccf8fa39c1d4660043956c288cb9d05f9d33016d3f68bb2` |
| `linux/dpr-100/high-contrast.png` | `666abe95e21a73bc4412262e13242efb8aefbc43ab444bc6625919ac0e8bc2ce` |
| `linux/dpr-100/light.png` | `f88ed6cff341d3bee636d76457c260fbf196f40b537ea042c05c46e77cd2a80b` |
| `linux/dpr-125/dark.png` | `2aba15c923d8581c699607bd4f0601e3a3ab65ad63d09accd3350de9610650b0` |
| `linux/dpr-125/high-contrast.png` | `45f4a04a18c6040e838863ef0caac2da68babea7045c57b6480f0f55ed3fae73` |
| `linux/dpr-125/light.png` | `697c7af1a932bbb7fbb4f38b368984ff7ba959e909e31b5ca454268131a520be` |
| `linux/dpr-150/dark.png` | `95905d06f17999b553a1e9a7161f190639f1f96ae31fcc89965463c9070ef652` |
| `linux/dpr-150/high-contrast.png` | `34125983b64bdf5d78bb6b21e20a1b2c85d2291fcf98d01f1345cc91fa27e3a6` |
| `linux/dpr-150/light.png` | `6cfa5bc1bd119077e08371fdb0ef61f5d1477341578824ac1bd0f604753f0db4` |
| `linux/dpr-200/dark.png` | `e00baf655a5e2d8b0c57993d970ce1259db80fc65a43d1df98fbe1472ca65262` |
| `linux/dpr-200/high-contrast.png` | `7f650d6cb1699cd8084c90dcb1b753437767b91060150737f64155f59d313915` |
| `linux/dpr-200/light.png` | `87b270c01a24bfbf0a4a998c973293a9fd4d35a9aafc62ecd1d0c529c35c78db` |
