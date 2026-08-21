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

- 生成日期：2026-08-21。
- Qt：6.11.1。
- CMake preset：`linux-gcc-debug`（GCC 15）。
- 平台插件：`offscreen`。
- 字体：DejaVu Sans 10pt。
- locale 与布局：`C.UTF-8`、LTR。
- 逻辑窗口：1280x800。
- DPR：1.0、1.25、1.5、2.0，`PassThrough` rounding policy。

更新时必须在已审 Linux 参考发布机执行：

```bash
HOME=/tmp/zzpuretools-ctest-home \
GCC_13=/usr/bin/gcc-15 \
GXX_13=/usr/bin/g++-15 \
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 \
ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1 \
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^example\.puretools-screenshot-(100|125|150|200)$'
```

更新后必须关闭 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS` 重新运行测试、人工检查 100% 三主题
和 200% Light，并重新计算本文件的 SHA-256。普通 CI 只比较，不得更新参考图。

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `df094f360a08a85f3561cdfb593aa1cd7b30b5329a56bddf58c33fd3bf774e3a` |
| `linux/dpr-100/high-contrast.png` | `f35efe158acaadc2b2d614f4ff2515a4159e57d3001c43999c6bd290f050ebb5` |
| `linux/dpr-100/light.png` | `9725edbe50165fe2bb8804def37685826df92b03b86752d1e880b5304f768cb1` |
| `linux/dpr-125/dark.png` | `9d6978a2a6332096edac880146bddab660f2f12412369fc267c60e8e0e941819` |
| `linux/dpr-125/high-contrast.png` | `274be1a2ea27c0b5de36aa594bb4b80a7146ad531441d190f366299dbc2859b2` |
| `linux/dpr-125/light.png` | `1ad14a3df77942aa57a3e3158ee31e32d982fbb87f765f170d0f7fab05c7df42` |
| `linux/dpr-150/dark.png` | `7c2e5556a1c3b29035a7624ceb171c55f85adfdbb07fad6b6ab01086cfaaec27` |
| `linux/dpr-150/high-contrast.png` | `3fff28a00804ab312f98336d750ce5d3020f8f8bf8d27757658fc3bcf91d5f85` |
| `linux/dpr-150/light.png` | `6d0301bfdf963194965d9d5de0556513bc87f92a5675bfc3b5c3b7c35dec3da7` |
| `linux/dpr-200/dark.png` | `61445c271427cbeb50e0e804d6798fdcfc9f4a5060116fb71150cc35e7d316b5` |
| `linux/dpr-200/high-contrast.png` | `03ef03f2e4fc526191d7fe04b3c72bd4ad2282ee0a2e52304ee438b3cf020f80` |
| `linux/dpr-200/light.png` | `32bf6d07fddb18e0f7eec1596f71a1dffa07d4dbc2206f8efc2f439f4f13d7b2` |
