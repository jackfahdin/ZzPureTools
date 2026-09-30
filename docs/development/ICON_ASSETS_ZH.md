# 图标资源所有权与维护记录

## 授权结论

项目所有者 Jackfahdin 于 2026-08-07 确认：下列 `ZzAwesome.ttf` 和 SVG
图标为其拥有完整授权的项目资源，批准在 ZzPureToolsFrame 的源码、测试、示例、
静态或动态二进制以及发布包中使用、修改和分发。它们属于项目资源，不是新增的
第三方运行时依赖，也不需要独立安装到系统字体目录。

资源随仓库根 `LICENSE` 和项目二进制分发。该结论明确替代早期综合示例计划中
“授权状态未知，因此不迁移字体图标和旧素材”的临时限制。该授权只覆盖下表固定
的资源字节；以后从其他来源增加图标时仍须单独记录来源和授权。

## 固定资源

| 路径 | SHA-256 |
|---|---|
| `ZzFluentUI/resources/fonts/ZzAwesome.ttf` | `a59cfd57797dcf169dcd03d6ce246326ca2a90f0abcf462d89939d14cb201618` |
| `ZzFluentUI/resources/icons/Close.svg` | `fa70ea1cf1025ae51600477c769e6c9bd15b09577196b0d7133faacf70103c0c` |
| `ZzFluentUI/resources/icons/ComputerSystem.svg` | `a0d1b2eedaafdcd5894545a70a2b2cca4bafa855cbc318d2ff00f6e2b7de51a2` |
| `ZzFluentUI/resources/icons/FullScreen.svg` | `f8c8487149258137f056c71ed943668ff11409e55a5db95eebab885b27be6d41` |
| `ZzFluentUI/resources/icons/Maximize.svg` | `4cb159c417deeba6ac3b26306b6f83754d95aa57650ab0019c537324e054c028` |
| `ZzFluentUI/resources/icons/Minimize.svg` | `b60517074afe3173945b6c644e7f0857d1179f118c7b20ea6ca566f36d3fa8d4` |
| `ZzFluentUI/resources/icons/Moon.svg` | `b92dcee889ac6df90b52ce858dffe2a608337d7d4f9708c9e162fd0d1c3f5d31` |
| `ZzFluentUI/resources/icons/MoreLine.svg` | `ae7bbc05d515e3e3f34d4eb0d27a9f1bb936d319fdcd40ba2d779bb7a1418b30` |
| `ZzFluentUI/resources/icons/Pin.svg` | `603785e3f36953a62f25eb22746b8d10368629fa01df5861e630391f50b4b13a` |
| `ZzFluentUI/resources/icons/PinFill.svg` | `14d2db8e83edbbc111d2fa5d5f2c91bfa220da685a5402b35736158b262bfdff` |
| `ZzFluentUI/resources/icons/Restore.svg` | `a8e38e47aad92b7ef70a90fbc1725c52bee0ba80a180d6aeaef511e6e0c9a640` |
| `ZzFluentUI/resources/icons/Sun.svg` | `0b97336032d8d1315f679a461c89fcc2e01e5a661c2f91fd6c65d072683f1f59` |

## Segoe Fluent Icons 来源记录

TabBar 图标直接复用参考项目 `FluentUIStyle/fluentui3style/resource/Segoe Fluent Icons.ttf`，
保持原始字体字节及内嵌版权信息；该字体不适用上文针对 ZzAwesome 和 SVG 的授权结论，
也不因导入而改为仓库根许可证。用户于 2026-09-30 明确要求沿用参考项目实际使用的
TTF 图标资源；此记录描述来源与导入范围，不构成新的字体许可授权。

| 路径 | SHA-256 |
|---|---|
| `ZzFluentUI/resources/fonts/SegoeFluentIcons.ttf` | `82f5dc0e0cb9f41efad49e5423c76768ae0fc96e062a0893b6c729b863033013` |

`ZzSegoeIconFont::icon(ZzSegoeIcon::Home)` 创建按目标尺寸和 DPR 绘制的 `QIcon`。
默认颜色随应用调色板更新；在 Fluent TabBar 中随标签的选中、禁用、自定义前景色更新。
传入显式 `QColor` 时保持该颜色。字体随 Qt Resource 内嵌，不要求 Windows 或安装系统字体。
TabBar 示例使用 `galleryIcon()` 保留原版字形与画布比例：Pivot 为 22/22 px，
其余图标先按 25/30 px 绘制再缩到 16 px，关闭叉号为 27/30 px（U+E894）。
Gallery 图标通常保持应用调色板的黑/白文字色，不随标签选中状态变灰；
高对比度标签会显式使用可读前景色。画布按 DPR 生成，不依赖预置的低分辨率位图。

## 构建与运行边界

- CMake 把全部资源编译进 `ZzFluentFoundation`，安装包不依赖源码目录或宿主字体。
- `ZzIconAssets` 显式初始化资源，保证共享库和静态库消费路径一致。
- 字体只在应用 GUI 线程注册一次，不写入系统字体数据库。
- SVG 只接受 Qt Resource 路径；绘制热路径不得访问文件系统或重新解析已缓存轮廓。
- 最终位图与颜色无关轮廓共享 4 MiB 有界预算；连续生成无限颜色不是支持的动画路径。

## 变更要求

修改任何资源后必须同步更新本表 SHA-256，并运行字体字形、SVG 原色/着色、
共享/静态安装消费、四档 DPR 截图和图标性能基准。新增外部资源不能沿用本记录，
必须提供独立的来源、版本、许可证和审核结论。
