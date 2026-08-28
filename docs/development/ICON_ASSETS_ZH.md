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
