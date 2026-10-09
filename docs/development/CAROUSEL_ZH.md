# 轮播图

项目原有 `ZzCarouselView` 已支持模型驱动的图片、标题、说明、前后导航、键盘、滚轮和首尾循环，示例位于“卡片与媒体”。本次从 FluentUIStyle 的 `ExCarousel` 参考沉浸外观和交互，新增“自定义控件 → 轮播图(Carousel)”页面，紧接信息栏。

## 新增能力

- 可选沉浸模式：图片覆盖视口，底部渐变遮罩与标题说明，整个过渡共用圆角裁剪。
- 圆形导航按钮：常驻或鼠标悬停、键盘聚焦时淡入；复用 Segoe Fluent Icons 原始 TTF 箭头。
- 可点击分页点：当前页高亮、RTL 镜像；最多显示七个点，跟随当前页移动。
- 显示/隐藏导航和分页、可调圆角，以及等比裁剪、完整显示、拉伸三种图片适配。
- 示例播放控制器：自动播放、可调间隔、悬停暂停、手动切换后重新计时；隐藏、禁用、键盘聚焦或减少动效时暂停。

默认仍是原来的卡片外观。现有 Model/View、外部模型所有权、无障碍和有界绘制约定保留。视图本身不读取文件、不做网络请求、不内置自动播放定时器；示例模型加载图片，页面控制器负责推进。

```cpp
auto *view = new ZzFluentUI::ZzCarouselView(parent);
view->setModel(model);
view->setImmersive(true);
view->setWrapAroundEnabled(true);
view->setNavigationButtonTrigger(ZzFluentUI::ZzCarouselView::OnHover);
view->setImageAspectRatioMode(Qt::KeepAspectRatioByExpanding);
view->setBorderRadius(6.0);
```

## 图片和图标素材

**不缺图标。** 导航直接使用已内嵌的 `ZzFluentUI/resources/fonts/SegoeFluentIcons.ttf`，无需下载 SVG、PNG 箭头或安装系统字体。

**当前可直接使用参考项目的五张照片。** 源目录为 `FluentUIStyle/Examples/Gallery/resources/images/`，参考页面标注来自 Microsoft WinUI 3 Gallery SampleMedia。本机已将它们复制到下述忽略目录用于对照预览；这些本地 JPG 不随代码提交。没有素材的构建使用内置生成的色彩预览，不影响运行。

如果希望更清晰，可以提供下表五张替换图。建议统一为 **JPG（真正 JPEG 编码）、sRGB、横向 16:9、至少 1600 × 900，推荐 1920 × 1080，每张约 200 KB–1 MB**。使用无水印、不含标题文字的图片，主体尽量居中，四周允许裁剪；标题由控件绘制。不要只把 PNG 扩展名改成 JPG。

| 文件名 | 图片内容 | 参考原图尺寸 |
| --- | --- | --- |
| `cliff.jpg` | 海岸、悬崖、海浪 | 400 × 266 |
| `grapes.jpg` | 葡萄或果实特写 | 400 × 266 |
| `rainier.jpg` | 雪山、山峰、蓝天 | 1024 × 681 |
| `sunset.jpg` | 金色晚霞、日落天空 | 400 × 266 |
| `valley.jpg` | 群山与峡谷、自然风景 | 400 × 268 |

存放位置相对于**包含 CMakePresets.json 的仓库根目录**：

```text
build/local-assets/ZzPureToolsExample/carousel/
  cliff.jpg
  grapes.jpg
  rainier.jpg
  sunset.jpg
  valley.jpg
```

本机完整路径：`/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/build/local-assets/ZzPureToolsExample/carousel/`。

如果 Windows 仓库仍位于 `D:\File\github\ZzPureTools`，则完整路径为 `D:\File\github\ZzPureTools\build\local-assets\ZzPureToolsExample\carousel\`。在仓库根目录执行：

```powershell
cmake --preset windows-msvc2022-release -DZZ_BUILD_EXAMPLES=ON -DZZ_EXAMPLE_CAROUSEL_ASSET_DIR="D:/File/github/ZzPureTools/build/local-assets/ZzPureToolsExample/carousel"
cmake --build --preset windows-msvc2022-release --target ZzPureToolsExample
```

Linux 对应配置（先按项目构建说明设置 `GCC_13`、`GXX_13`、`QT_ROOT`；Windows 使用已初始化的 MSVC 环境和 `QT_MSVC_ROOT`）：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON -DZZ_EXAMPLE_CAROUSEL_ASSET_DIR="$PWD/build/local-assets/ZzPureToolsExample/carousel"
cmake --build --preset linux-gcc-debug --target ZzPureToolsExample
```

这个独立选项不要求首页和卡片的旧四张 PNG。缺少任意一张时只对该张使用色彩预览；增加新文件后重新配置，替换已有文件后重新构建。图片在构建时嵌入示例，运行时无需保持目录。

## 验证入口

控件：`fluent.carousel-view`、`fluent.icon-font`；示例：`example.carousel-playback`、`example.workspace-smoke`、中英文集成烟测。截图分别检查原 `carousel-view-*` 基线和新增 `carousel-immersive-*` 基线，覆盖三种主题及四档 DPR。参考对照图输出在 `build/carousel-reference/`。

当前验证环境为 Linux GCC 15.2 / Qt 6.11.1。Windows/MSVC 和 Qt 6.8 最低版本需在相应环境验证；上述局部验证不替代全仓平台与发布检查。
