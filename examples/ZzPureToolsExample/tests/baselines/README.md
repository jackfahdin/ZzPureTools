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

- 生成日期：2026-09-28。
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

2026-09-14 在同一 Linux Qt 6.11.1 参考环境同步选择指示条与内容间距变化，人工
检查了 100% 和 200% 的 Light、Dark、HighContrast 场景。Windows 与 macOS
视觉仍须在对应平台验证。

2026-09-28 统一侧面板内容外观：导航树去掉内层边框，背景使用跟随主题的 Window 角色。
同时纳入当前标题栏深浅切换及 Activity 强调色图标效果。已检查 100% 三主题与 200% Light，
保持原截图阈值；仅更新综合 Example 的 12 张基线，控件与工作区截图基线未改写。

2026-09-28 中央区域改为无标签堆叠页面，移除外层“组件示例 / 终端”标签和新建标签按钮。
页面直接从标题栏下方开始，窗口标题跟随当前页面。已检查 100% 三主题与 200% Light；
本次仍只更新 Example 的 12 张基线，不放宽截图比较阈值。

2026-09-28 导航树的实心三角展开标记改为细线折角，原位置、布局和操作保持不变。
已检查 100% 三主题与 200% Light，更新四档 DPR 的 12 张 Example 基线；比较阈值不变。

## 文件摘要

| 文件 | SHA-256 |
|---|---|
| `linux/dpr-100/dark.png` | `525d3f27055f4409f135712905215d3bc1f2af66796a2a6f7137ba55ceea5b95` |
| `linux/dpr-100/high-contrast.png` | `0095581c28cb1d65d9470efe2cb050ed828a6d9d09f27cef05d4fddf4c7ea5a5` |
| `linux/dpr-100/light.png` | `d41e8e462124119309e189f52b2411a93347d78f260a7508afb4b44a83b9758e` |
| `linux/dpr-125/dark.png` | `2d54771de355c910841dd9f49ec2893246d0e31c4a8bbaff5f2daae8ade27ef0` |
| `linux/dpr-125/high-contrast.png` | `b2f4a8bda4c879e6689a4c86bc29a1d9f2f3f23491b022836a37e0b3fdd736fd` |
| `linux/dpr-125/light.png` | `e26b6ad5768e445d621b478bff000384ae79157808d5968fe46d618153d080b3` |
| `linux/dpr-150/dark.png` | `9cf160968a8053ca88a6801ec8f1e6c915fe5be7b09aca18161ee729d056fda2` |
| `linux/dpr-150/high-contrast.png` | `38030ae62d8390d84dba3ad4ee3ffbb9bfe0a593438b1d50d83a2b7ea4a69e03` |
| `linux/dpr-150/light.png` | `97921b5190885762d108b3caacb7c71a5365ebd5b272b1e8ac006ab9fc7a33d2` |
| `linux/dpr-200/dark.png` | `887becbaf64ca7593c6220ce0d36994d6868aa3a168b9f5a5e4179ab719bc02a` |
| `linux/dpr-200/high-contrast.png` | `a0e577275d55cae6b956ebf5cd004798163e7500929fe40ca68476b2f8b233e5` |
| `linux/dpr-200/light.png` | `f3051a60938ead08f639283f6015ea47d3ca1a28ee1c73b20e6d60f9c66b4645` |
