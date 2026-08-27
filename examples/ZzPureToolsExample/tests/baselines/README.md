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

- 生成日期：2026-08-27。
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

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `654f7055bec66b4e37f8262ef85f3c72ff24228be9cef1ecdd48918ef48dacdc` |
| `linux/dpr-100/high-contrast.png` | `4a0bd19c9a93b3854526695f00ee5d5edd9c25dcb88daa88c44a0116b7963563` |
| `linux/dpr-100/light.png` | `9a91267b1420b72e960e8df1f67a61343a4bd84225525349a0af389bb0e31411` |
| `linux/dpr-125/dark.png` | `ab0715d9cb151e247269c4491a5435458cc7f04c015b187e1c939a65c8fee78f` |
| `linux/dpr-125/high-contrast.png` | `8f7f2cf465ab19239c12e9ef9f3361302dfb3057d1ababdfebd5ab6241657afe` |
| `linux/dpr-125/light.png` | `16583a54622b9ca226c7a43eeb5c0475509e7b92ce4676ac52c972477cf9457b` |
| `linux/dpr-150/dark.png` | `c04ae23be310e56863f81965b5c956f647d5e80ad4a5e9e6b792cee2fb5640a7` |
| `linux/dpr-150/high-contrast.png` | `5656817d0baf7df50b77c15d37208bdae179e2f11a43b17c3a71713e54698894` |
| `linux/dpr-150/light.png` | `0715655bc96556cb89a6e16c4bede89942be2c4f6c39f8d6a3fa3090bed9397b` |
| `linux/dpr-200/dark.png` | `36bdebcc83d4aa959ecdd7a864c4971402b1f99c11668dae8c9f2a39f7ea0806` |
| `linux/dpr-200/high-contrast.png` | `d8d252aaaf1922936bddb33706d77921f7246b4d1a3177ca8d197f7eac27bd4e` |
| `linux/dpr-200/light.png` | `4fde21d9e5dbe10e9788a9247a830ad6797b6d00387bd4deaf257dbca78f8782` |
