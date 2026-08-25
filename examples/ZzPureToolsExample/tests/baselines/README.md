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

- 生成日期：2026-08-25。
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

本轮因延迟 Side 合同把首帧从 eager 展开迁移为“Activity 入口可见、Side Pane 折叠”，
四个 Side 内容不再 eager 创建，中央内容因此扩展；这不是 SVG 图标回归。

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `5f2672d1228f02ea539786e8cad580bbcc7422e3474d891b2fbfc21f820bd912` |
| `linux/dpr-100/high-contrast.png` | `4bbdf60830b66655fbb3ae67db513b00b1ae42c5e9c88434fb4c2dccff6e970b` |
| `linux/dpr-100/light.png` | `8c8d85419a166f9022a490462d946a654ec54d12439a481059396fc782a3754d` |
| `linux/dpr-125/dark.png` | `a28a2ee03cc99698fae57a95d354442f0272a1ef892c1363bdf6e47dbb457d31` |
| `linux/dpr-125/high-contrast.png` | `1319fcb4af84f618d544094a2fb759efbc2b0255946a40d03449275511aecaaa` |
| `linux/dpr-125/light.png` | `a43164bce043cf2af44ce2ce5a8ce8ab2a8dd06878c9e633ae3a1749c157120a` |
| `linux/dpr-150/dark.png` | `ac0ffe90a68fcf4fa2c4b901a7f9035c94fadc5d4bab1a22a1407e41a6301f78` |
| `linux/dpr-150/high-contrast.png` | `349991c7093096f3cf63100a6a5a8016430edc6abce365448c71ceacc1f73db2` |
| `linux/dpr-150/light.png` | `88204285d50ca7a8eb6d76afb8b3da249fa60695789d21a24c8294d962048fe6` |
| `linux/dpr-200/dark.png` | `fef1dca20e0bbd914cb5654422a537eb5820e20982200d39c9cfd509e45978ac` |
| `linux/dpr-200/high-contrast.png` | `103022f562ce5c57ebec37ca0a809f0dfb32d700e34b54863e17c644b0668d47` |
| `linux/dpr-200/light.png` | `769c5803588aa44b590e468d3aa5ba1eab89d2b02ba20bf3dec5e98dcfc3e3b6` |
