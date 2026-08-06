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

- 生成日期：2026-08-06。
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
| `linux/dpr-100/dark.png` | `c690e5b4d09ddd194582e5c7a49c3f4a6da3d638d91ab968d0e87cfe9996e9f6` |
| `linux/dpr-100/high-contrast.png` | `95ab42983b24bf6694aa780516da11c40cd0abac5a59146c787f804fc3c8e1f3` |
| `linux/dpr-100/light.png` | `36140100100e15cd61f48a7ab8f2aa5b2e905bc60e704ce9f3d39d706fe3ecd1` |
| `linux/dpr-125/dark.png` | `6c819a27401bcc1666a75d0a453145fcf4330e04fb35103474fc5e748440eef7` |
| `linux/dpr-125/high-contrast.png` | `45bccbe6b0c92deb23249f36615189d1e925cbdc8bf7569171d67a48e5f5cdfd` |
| `linux/dpr-125/light.png` | `acec2dcbb281aa6cd3596f89b0d6d958f9cc3db760f80baaef098f2247236a01` |
| `linux/dpr-150/dark.png` | `577a87e83ca142f9a8d789f29dc6c8b15da7817b5d9a19f3ca3d6e8271dca116` |
| `linux/dpr-150/high-contrast.png` | `5527c2891454b32f62835abb5de97e4b9725029aeb3b1707a935830905036f69` |
| `linux/dpr-150/light.png` | `0b637407a6d2c21e39a95ded8d047a71da7be9d6a6511007dde89e598adfd8ec` |
| `linux/dpr-200/dark.png` | `36f406f8fbeee836b02923da1b5cd0fc0725481cb8d0a4af47de5d03f029d879` |
| `linux/dpr-200/high-contrast.png` | `3969f266038d5ce5d5961a80e8cd219b7408ca7d0bbc0b374b3c9b2bf6786e27` |
| `linux/dpr-200/light.png` | `a57cb6af3b82b53b130fa6267669280203fa142ee6073195ba342b567e51090e` |
