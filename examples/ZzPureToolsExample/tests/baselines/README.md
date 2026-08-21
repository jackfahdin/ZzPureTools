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
| `linux/dpr-100/dark.png` | `3d75baa8075a512f8cf4f0a806fdcf18d99300e3a741ede0781d63c0becb55c6` |
| `linux/dpr-100/high-contrast.png` | `df99049449f0441ad03dde340d77d7efbcf6a7e16f63a7881b5b2af8b86fac65` |
| `linux/dpr-100/light.png` | `7c8c42fbc1a7762508f2d453b0a6bca30b7fbb1ae881000a6ff910255edd599d` |
| `linux/dpr-125/dark.png` | `60fc4f6c95e46034ef69bb931de000a62c674eeb94ffe5f7162f712ab62fbcec` |
| `linux/dpr-125/high-contrast.png` | `d8abdc14b687e2babd8cf6bb3b30b9cc904ee97c3dc22d5e4afc9cf41c8e5310` |
| `linux/dpr-125/light.png` | `bb83ba31e8fc68e7c6c898bfd8880d324763301bf26711d34b45c1ac24217808` |
| `linux/dpr-150/dark.png` | `f2ffe7823348af5e8112e2e19591323441ef118eeb6397ab7d30cbdd3da2bca9` |
| `linux/dpr-150/high-contrast.png` | `355d2a3ca7ba7a374bed597ef86494be660610808ba383523b3dcf2a67ef22d7` |
| `linux/dpr-150/light.png` | `f83c181d5cfdbf9983bd1da3fc89e6ac17de6ea1c552bae10bd36d4aafc6489a` |
| `linux/dpr-200/dark.png` | `00421374c0915d9e83d5d95c0e9ad147681446f9551c7a8efa60a515392c0d1a` |
| `linux/dpr-200/high-contrast.png` | `6b5e28a0fafe6d7312d87fe9d8240b6440ea6eb9b771ea1b31bed8265847e95a` |
| `linux/dpr-200/light.png` | `ff7357af9e50805fe59ae22b85a907ba7cb1a89d75108586a0cfc4f01309b60d` |
