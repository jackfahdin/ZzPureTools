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
| `linux/dpr-100/dark.png` | `ce235f41f44d143ff946279ed36b24711201510ded486573d0ed22050e68c160` |
| `linux/dpr-100/high-contrast.png` | `d82a316a85deeafa1c73f86dc83ab99a099448a99459c9a583ec3fc3c1c312f7` |
| `linux/dpr-100/light.png` | `e01a368d1d242e063889411faa4168ef66433e1a0691dc7fb97dfffe4c5ebcca` |
| `linux/dpr-125/dark.png` | `08d9c067d85398435eef47e4392d4e0ce0f1cdcc5faecc74be43a0792bc2d614` |
| `linux/dpr-125/high-contrast.png` | `687ca381f9bef9e24d971d4606b71f64513465ed06879e4389c2a083c1178f0d` |
| `linux/dpr-125/light.png` | `f339ba29d945b57f487643777d1aa16ba101d8ffbb877f21cabde4c5d76ec672` |
| `linux/dpr-150/dark.png` | `e764817521b4c4d50c1b34a845fe600b34b5a4569f5442b94d4601c0fd663f9e` |
| `linux/dpr-150/high-contrast.png` | `97760643a952cf217599ad8f384e18e17c5a11700810853a587fa8f3f3f8c2e1` |
| `linux/dpr-150/light.png` | `45c78fab83b4ab5990f85d282a2009637ce1f2e9913ce2a31262af834dbb21e0` |
| `linux/dpr-200/dark.png` | `38ebcf83b83b9e51d9c14e82f00c78d2acc92a3fbbb63304f3e0548f157bee2e` |
| `linux/dpr-200/high-contrast.png` | `8aaab3678f9da33f604355d4179c60d80f0a144a9e998bc2485ac6cde5ec622d` |
| `linux/dpr-200/light.png` | `fe4504129144acfdf8939c637ee16a337620bbb936e6427634b537c36e8fe82f` |
