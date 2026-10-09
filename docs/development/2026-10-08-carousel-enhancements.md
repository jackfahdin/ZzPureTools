# 轮播图增强实现计划

> 使用 writing-plans、subagent-driven-development、test-driven-development 和 verification-before-completion，在已授权范围内实现，用户说提交后才提交。

**目标：** 在已有轮播图中加入参考FluentUIStyle的沉浸呈现与交互，补齐可演示的自动播放并说明素材要求。
**架构：** 继续使用ZzCarouselView与外部模型，新增可选呈现属性；播放和图片加载在示例层。相比新建一套QWidget轮播，复用Model/View既可保留性能和无障碍，也可保持旧页面兼容。
**技术栈：** C++20 / Qt6.8+公共API；Linux Qt6.11.1验证。

## 全局约束
- 在 /home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame 当前工作树实施；参考 /home/zz/Jackfahdin/github/FluentUIStyle 只读；不访问 docs/research，不提交。
- 用户已授权从参考项目补功能；保持已有 ZzCarouselView Model/View 契约：不拥有模型、不加载路径、无内部自动播放timer、每帧至多两个item和七个指示点、不创建每项QWidget。
- Qt6.8+公共API，C++20，PIMPL，中文Doxygen。旧默认Card外观/ModelView和无障碍行为保持；新增沉浸外观可选开启。
- 图标复用内置 SegoeFluentIcons.ttf；参考ChevronLeft E76B、ChevronRight E76C（读取原图标定义确认）。不引入自画不同箭头或另套图标。
- 动画为固定持久对象，隐藏/禁用/SH_Widget_Animate关闭时停止到终态；UI线程、信号删除/重入安全；非法枚举忽略、非有限半径忽略、非负参数归一。
- 参考图仅本地预览，使用忽略的 build/local-assets/ZzPureToolsExample/carousel 下cliff.jpg、grapes.jpg、rainier.jpg、sunset.jpg、valley.jpg。不复制图片到提交资源；缺失图片用现有palette确定性图片降级。新独立CMake选项 ZZ_EXAMPLE_CAROUSEL_ASSET_DIR 不要求旧4张图。
- shared build/linux-gcc-debug 只允许一个实现者构建；先主线程路由RED，随后core独占，交回后主线程集成。文件编辑apply_patch；图片复制为二进制资产复制。

### Task 1: 增强已有轮播视图
负责：ZzFluentUI/widgets/include/ZzFluentUI/ZzCarouselView.h、widgets/src/ZzCarouselView.cpp、private/ZzCarouselViewPrivate.h/.cpp；可新增private/ZzCarouselVisuals.h/.cpp分离按钮/默认delegate/指标绘图，更新ZzFluentUI/CMakeLists.txt；foundation/include/ZzFluentUI/ZzSegoeIconFont.h仅新增Chevron枚举；tests/ZzCarouselViewTest.cpp。
- [x] 阅读当前控件和参考ExWidgets/controls/excarousel.h/.cpp；迁移其有用呈现/导航功能，保留模型设计。
- [x] 公共新增 bool immersive()/setImmersive 默认false；qreal borderRadius()/setBorderRadius 默认6；bool showNavigationButtons()/setShowNavigationButtons 默认true；bool showIndicators()/setShowIndicators 默认true；enum ZzNavigationButtonTrigger {AlwaysVisible,OnHover} 与 navigationButtonTrigger()/setNavigationButtonTrigger 默认AlwaysVisible；Qt::AspectRatioMode imageAspectRatioMode()/setImageAspectRatioMode 默认KeepAspectRatioByExpanding。所有属性显式Q_PROPERTY与同名Changed通知；有效变化一次通知。
- [x] 沉浸式全viewport圆角裁剪，滑动全程同一外框裁剪，默认delegate图片cover/contain/stretch，高DPI保持清晰；文字原参考底部黑色透明渐变、白标题16px bold/说明12px，左右20，56/76高。无文字不绘制遮罩。旧非沉浸默认外观不变，圆角和适配也可配置；高对比强调可辨识与焦点。
- [x] 沉浸导航36px圆形/边距12/图标16；用原TTF与参考按钮alpha按normal190/hover230/pressed245。AlwaysVisible常驻，OnHover在鼠标或内部键盘焦点进入后淡入，退出淡出；隐藏状态不能偷点击/键盘焦点，键盘导航仍可用。至少键盘焦点进入view可显现按钮。信号触发后删除view不得继续访问。
- [x] 显示分页时，点可点击跳转；鼠标按下释放同一点且左键才导航，不误发item activated；禁用项不跳转，最多7点，围绕当前项窗口，绘图/点击/RTL映射相同。沉浸点对齐参考slot16/直径6,hover7,selected8，指示点槽底边距底10（槽高20、中心距底20）；隐藏时无占位（Card也收回底部空间），点击不截取卡片其他事件。
- [x] 两个持久动画以内，数据规模不改变对象数；旧对象稳定性测试按新的固定总数更新，不弱化1000次更新断言。新增功能行为测试先RED再GREEN（允许最小可编译空setter骨架，不以编译错误当RED）；测试分页点击与RTL/有界窗口/禁用、导航显隐与hover/焦点、图片适配&圆角、减少动效，旧ModelView全部回归。
- [x] 构建ZzCarouselViewTest、ZzIconFontTest并运行ctest筛选；报告RED/GREEN命令输出与文件清单。新增文件沿本控件2空格风格，其他不全文件格式化。交回build。不提交。
示例契约：
```cpp
view.setImmersive(true);
view.setNavigationButtonTrigger(ZzCarouselView::OnHover);
view.setImageAspectRatioMode(Qt::KeepAspectRatio);
view.setShowIndicators(false);
QVERIFY(view.visualRect(view.currentIndex()).bottom() > oldBottom);
```

### Task 2: 轮播示例、播放控制器、素材与验收
负责：新examples/ZzPureToolsExample/ZzExampleCarouselPage.cpp、ZzExampleCarouselPlayback.h/.cpp、ZzExampleCarouselSmoke.h/.cpp、tests/ZzExampleCarouselPlaybackTest.cpp；修改示例CMake、RouteCatalog、PageFactory、ShowcasePage.h/Private.h/.cpp、SmokeControllerPrivate.cpp、workspace测试与英文翻译；ZzFluentUI/tests/ZzFluentScreenshotTest.cpp及仅新carousel-immersive截图基线；docs/development/CAROUSEL_ZH.md。
- [x] 路由carousel紧接info-bar，总数40。先workspace测试carouselRouteFollowsInfoBar RED，旧cards页保留。
- [x] 用外部QStandardItemModel加载5张本地图片或palette降级，模型parent页面；图片标题/说明对应海岸峭壁、葡萄、雪山、晚霞、峡谷。图片demo高280默认immersive=true、wrap=true、duration550、OnHover、autoplay=true、interval3500、pauseOnHover=true。
- [x] 新ZzExampleCarouselPlayback QObject页面控制器，QPointer view+QTimer子对象，setEnabled/isEnabled、setInterval/interval（最低100ms）、setPauseOnHover/pauseOnHover；隐藏/禁用/hover（可选）/内部焦点/减少动效暂停；恢复重新开始完整间隔。定时触发showNext，非循环最后一页停住，0/1页不空转；手动换页重置间隔；超时回调可删除页面安全。单次定时加动画时长避免重叠。对事件过滤器返回值遵守watched删除契约。
- [x] 真实交互面板：播放、循环、暂停悬停、分页、导航、按钮显隐、间隔、动效、圆角、图片适配、禁用、RTL；当前项/总数状态、上一张下一张、重置、公开API示例；文字卡片模型第二例展示同一组件用法，不伪称任意QWidget。
- [x] 自动播放真实timer测试先RED后GREEN：隐藏不推进、恢复推进、hover/禁用暂停、非循环边界；不做来源文本测试。烟测验证5行、导航、属性编辑和重置；截图烟测暂停自动播放并确定当前行，输出浅深高对比页面。
- [x] CMake独立可选图片目录，嵌入资源前缀/ZzPureToolsExample/carousel；缺失单张不致配置失败。先复制参考5张JPG到忽略目录做预览，记录来源/分辨率；无需用户下载。说明可选高清JPG/sRGB/横图1600x900以上、每张建议<=1MB、无水印文字、沿用五个英文文件名、完整Linux及Windows相对目录与configure参数。图标已内置TTF无需素材。
- [x] 构建示例、公共头与测试；运行控件/播放/workspace/中英文集成/相关性能，四DPR仅新沉浸截图和旧carousel基线比对；参考同照片/同字体对比，保存预览。检查diff，独立任务审查及最终审查，不提交。
示例timer回归（骨架启用不启动timer应RED）：
```cpp
playback.setInterval(100);
view.show();
playback.setEnabled(true);
QTRY_VERIFY_WITH_TIMEOUT(view.currentRow() != 0, 700);
```

## 验收记录

- Linux GCC 15.2 / Qt 6.11.1：示例、公共头、控件、字体、播放测试构建通过；相关 CTest 7/7 通过。
- 原 Card 与新增沉浸模式在 100%、125%、150%、200% 下三主题截图比对通过；仅新增 12 张沉浸基线，旧基线未修改。
- 十万行轮播两项性能/复杂度用例通过；固定两个动画、零内部定时器，5000 次导航最多每帧 3 次模型数据读取。
- 中英文示例截图烟测通过；同照片参考对照已检查。审查发现的信号删除/顺序、窄幅文字与动画、真实鼠标移入箭头及穿过外框离开问题均有回归验证。
- 本地照片位于忽略目录；未提交。Windows/MSVC、Qt 6.8 和全仓发布门禁未在本次环境验证。
