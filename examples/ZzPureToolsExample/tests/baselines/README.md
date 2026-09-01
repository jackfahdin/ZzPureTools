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

- 生成日期：2026-09-01。
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
| `linux/dpr-100/dark.png` | `14790aeab094b9eb3f8bde9cd23aa0763a2da43b600a1bacaa0c8040faf37853` |
| `linux/dpr-100/high-contrast.png` | `7d3ca7ff3740e6760f23d3e2f2589d5f1bf30f6f71ed44644488c0a078b06323` |
| `linux/dpr-100/light.png` | `2e6c96084d43aa3a790e89a23516a46b736c1045131d2b7e26db5ec2c9713cbb` |
| `linux/dpr-125/dark.png` | `0089bc47d1ca9b588203e510a23c01df7885a225b91074a39463e4a4eca17610` |
| `linux/dpr-125/high-contrast.png` | `87f52eaacf354dad7b37dfa97fec34b8fb7a59b54cf6d85600c74a8a1fb7df78` |
| `linux/dpr-125/light.png` | `d54327fc4b0d9c2634456c9d1d73eb464d1d018385a88c6c54b2572231baa24e` |
| `linux/dpr-150/dark.png` | `32f14eda76f36eaf477f01ca115a85e0bf998e5d8d8448ec89b501e836600052` |
| `linux/dpr-150/high-contrast.png` | `c97a4cb755ec34ebe2310b62d7685a17ae62510645c5790c5ca20b31f736400b` |
| `linux/dpr-150/light.png` | `5ae7288acc34d7c18acf3dd301b43857d731e3c9c11b9b3b032a029d1d06a284` |
| `linux/dpr-200/dark.png` | `c451828ce4a90e61fd7da76ce5e33e4afd0487113c165580feb5eea8acb19c1c` |
| `linux/dpr-200/high-contrast.png` | `e83308708b0811d809643971b27dad80e981f39499320d36aec3ba27664e41b8` |
| `linux/dpr-200/light.png` | `983ff49f2e11f90522a575a9cc88cabc5174f3dec2d3fe837e3fd687c6b360e9` |
