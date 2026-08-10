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

- 生成日期：2026-08-10。
- Qt：6.11.1。
- 平台插件：`offscreen`。
- 字体：DejaVu Sans 10pt。
- locale 与布局：`C.UTF-8`、LTR。
- 逻辑窗口：1280x800。
- DPR：1.0、1.25、1.5、2.0，`PassThrough` rounding policy。

更新时必须在已审 Linux 参考发布机执行：

```bash
ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1 \
ctest --preset linux-static-release --output-on-failure \
  -R '^example\.puretools-screenshot-'
```

更新后必须关闭 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS` 重新运行测试、人工检查 100% 三主题
和 200% Light，并重新计算本文件的 SHA-256。普通 CI 只比较，不得更新参考图。

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `fb80aa50622c7ca370b024614811321852f15e4f4e7cb30eafca9bec2f3a2b23` |
| `linux/dpr-100/high-contrast.png` | `ae1ad638df2d5aa0b80e6997d9f6db4bed6fd901bb3638d831806e35fca38188` |
| `linux/dpr-100/light.png` | `4513a110b19ef3aecc85d86d4d2a09b75d0f496c9802efac89fb4ebd17ded9e7` |
| `linux/dpr-125/dark.png` | `7af8f70d88563574239da388ca40751344047aaea94dad525720494729cc68c1` |
| `linux/dpr-125/high-contrast.png` | `6041bac759a1d43e881d2b4ea18e4c5a82e21c3dbde069b1cf086af62a884f55` |
| `linux/dpr-125/light.png` | `38fa23fce9aa1f5f322a2669467bdc5974d691a11cdf9e713050dacd358a3008` |
| `linux/dpr-150/dark.png` | `91aff9dd0b2155e635b7a2768f48397635cc308b63a2e46d934b97e370e8f6f2` |
| `linux/dpr-150/high-contrast.png` | `6eb9a9644b18047531602c32ac50ebb5082772970032d6f0310d00ef10e27854` |
| `linux/dpr-150/light.png` | `d99a489cd67d39935f5f21a090101ed81fc023ce0054c106762cbe533ab833e5` |
| `linux/dpr-200/dark.png` | `7b5097fae0fca3b70b29b5a1e42aadf8ffc3b1d21892067798708a908168d55e` |
| `linux/dpr-200/high-contrast.png` | `ad1fd7c42f0190dd7f3f20894f122b2548727c6531a39972a920345efdffe787` |
| `linux/dpr-200/light.png` | `ea2d888de5a411a5b21a3a0f4dd11eb4fa057911674786e433fda93694b72333` |
