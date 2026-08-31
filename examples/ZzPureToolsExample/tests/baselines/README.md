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
| `linux/dpr-100/dark.png` | `cc9fe2b4c422065deaf75f92f4c3ab7501047de8653e304751f7a774135107d1` |
| `linux/dpr-100/high-contrast.png` | `7f325abbcc5cf64b86ca5304ba5a1c982841e1b0842f6c0dd00db1b61334c5ad` |
| `linux/dpr-100/light.png` | `3e23d5cd361f633b6f237e07a6509d3cb5bdbd64d439dffd52d2f78564b6f97c` |
| `linux/dpr-125/dark.png` | `73f6caaee013e6cf079b23bdd7f914f6eab1c8b21d25511986024f87b35f27c7` |
| `linux/dpr-125/high-contrast.png` | `13e8e821cafcdb36b82cc64259f4828005351f7b6407f7a16a2052f7e889058c` |
| `linux/dpr-125/light.png` | `43aebd647175fe717e355a95d93598b78d55253b6a890f15f860757e8f4ad417` |
| `linux/dpr-150/dark.png` | `fd80d478ed3d1f162f6509fdeb8da46c3cf959f8c286b13ea5e0bc54f5a3b2e5` |
| `linux/dpr-150/high-contrast.png` | `539b975295234929fea0a6921cf4ce3b2e582c858a2eba74a6ab4e339c93c186` |
| `linux/dpr-150/light.png` | `4d42f6927f2060cfa930bf93d48c9a9fd84a7be45f6690642ad754cad7406ebc` |
| `linux/dpr-200/dark.png` | `9cedde08649ece93f52ea77f437932ffb0b8a6b27a9764124b5e1794fb70f258` |
| `linux/dpr-200/high-contrast.png` | `bc6e71066425f0a10d298ed20f2ae210d764e648fa01b5d06e0f9f81f7229bc9` |
| `linux/dpr-200/light.png` | `8ab46210229852f7d5b549d1e0855c5fd9ce78beb48c41bdee4fa4702c0af862` |
