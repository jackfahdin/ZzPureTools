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
| `linux/dpr-100/dark.png` | `065845ccc8c4aa36463a93d370ce9c922d8c9f3c26658fc9842aaf7a7ef7c59f` |
| `linux/dpr-100/high-contrast.png` | `459e79ba92834ca7ea4b0cb5a111f0e59d11d17bf4b077b4dd0ebc13233fc22d` |
| `linux/dpr-100/light.png` | `a6adc6d2a285a2f2a332a523f305eb624e509004f65635eeb844c1ee2e44e344` |
| `linux/dpr-125/dark.png` | `b4b885c2eb371894fdd55fcfbee9a0fa29f663ac0357cadd2aadf2ac8848f8be` |
| `linux/dpr-125/high-contrast.png` | `ed4086977c02213f1c2998421c588915b3c125b9a4814b0e7767c74e1ba72199` |
| `linux/dpr-125/light.png` | `13657fe04df9f8b0a32d427f85229e5b9465dfd0d33f3c6ccf0308ab75f641e3` |
| `linux/dpr-150/dark.png` | `2787618923e0b50991db25cd85c790098f94b79c37c7818d9ae73a88be7026f0` |
| `linux/dpr-150/high-contrast.png` | `64407e3bee9f23c79ca9fee1e3db2de1f567d811e8b416117a506f68e628a384` |
| `linux/dpr-150/light.png` | `507dd93fc536559df1353b282ab29c957459e80ed441f5d97698b5d5152bbb33` |
| `linux/dpr-200/dark.png` | `30fd6ef5918fdc6377739d14ebe7a763b32a253f48f2af1f27cec751292e0b38` |
| `linux/dpr-200/high-contrast.png` | `dd15bf3f4b51201bc91b3557ee3b3e59f7967f3cc048cbbc28db1504d8e4d5e1` |
| `linux/dpr-200/light.png` | `bc0596cd4d41b62caf0a98dce31f61e4861ca13c611972be98d8edc024e4c2bb` |
