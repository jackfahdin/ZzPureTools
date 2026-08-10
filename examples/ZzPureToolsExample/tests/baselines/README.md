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
| `linux/dpr-100/dark.png` | `4f5c7b32a8990cd5b439d964e04c8114550d106338a47851e3f55c0263afed52` |
| `linux/dpr-100/high-contrast.png` | `bb150c192072f7d145db7510c7012e8108d39742dcb414ba8732983d53aa7f3e` |
| `linux/dpr-100/light.png` | `66c32e59ae5ebae1877e0920201fe74723171b19736462e855a622e2a0225f11` |
| `linux/dpr-125/dark.png` | `5ce16234bccd5f95ffbfa4773fc6bc6425ab49a8411ac75e508b8f049e5b5a10` |
| `linux/dpr-125/high-contrast.png` | `5dd83a307504bbda86e5f9205b33de3a238d99af94f9538c7642e932ccd28a75` |
| `linux/dpr-125/light.png` | `9d8cc38c93153c1d70d26b9a85b6795bad5d143591c144a76b9dbf382d4e571e` |
| `linux/dpr-150/dark.png` | `819be05f817b6cff21eb91d61beb21e53964c7a93f2613b1712172bd211f2977` |
| `linux/dpr-150/high-contrast.png` | `61770ebe741c78f12bbc543f13e82a3ecbc53db80a50d8bb6659cf362b98c814` |
| `linux/dpr-150/light.png` | `942562b2b37cb06e1fd6791407c54e54b45373de21924fec6e81cc57cd74e4aa` |
| `linux/dpr-200/dark.png` | `fc2e57b49388db2b45be142926be3850241c7270d72210d0e1af9f33a25081cf` |
| `linux/dpr-200/high-contrast.png` | `5ef575b724b226b5ece5115723df7cc8701a053102105c586445d021d93e81a6` |
| `linux/dpr-200/light.png` | `0a8e7eb4f8e65d4ae07e8a059b92af3e95be5e020d7eee013e62cd6a7f7aa7d4` |
