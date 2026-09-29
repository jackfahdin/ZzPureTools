# 滚动条两端箭头实现计划

**目标：** 按已确认方案，在横纵滚动条两端增加随悬停淡入的小三角，支持点击与长按步进。

**架构：** `ZzFluentStylePrivate` 统一提供 AddLine/SubLine 的绘制、几何和命中。
范围、步进和长按复用 QScrollBar；透明度复用 ZzScrollBar 已有动画，不新增 QObject。
常规尺寸两端各预留 12px，滑块最短 24px；短尺寸先缩减箭头空间，长度不超过 24px 时省略箭头。
横向 RTL 与 invertedAppearance 合并决定数值方向，几何不受悬停影响。

**技术栈：** Qt 6.8+ 公共 Widgets 接口、C++20。

## 实施与验证

- [x] 在 `ZzFluentUI/tests/ZzScrollControlsTest.cpp` 更新几何契约并补充点击、长按、边界、RTL、反向和短尺寸测试，先运行确认失败。
- [x] 修改 `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`：统一固定箭头槽和中央 groove；绘制内接小三角；边界箭头透明度降低；加入 AddLine/SubLine 命中。
- [x] 运行滚动控件与注释滚动条测试，确认分页、拖动与共享动画行为。
- [x] 更新 `docs/development/SCROLL_CONTROLS_ZH.md`、Example 提示及英文翻译；更新受影响的四档滚动条截图。
- [x] 完整构建常用 Debug Example，执行本机开发门禁、四档完整截图及独立审查，纳入同批中文提交。

关键验证命令：

```bash
cmake --build build/linux-gcc-debug --target ZzScrollControlsTest --parallel 2
QT_QPA_PLATFORM=offscreen ./build/linux-gcc-debug/ZzFluentUI/tests/ZzScrollControlsTest
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 GCC_13=/usr/bin/gcc-15 GXX_13=/usr/bin/g++-15 \
  CMAKE_BUILD_PARALLEL_LEVEL=2 ./scripts/ci/run-local-development-gate.sh \
  --preset linux-gcc-debug --tests '^fluent.scroll-controls$|^fluent.annotated-scroll-bar$'
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^fluent.screenshot-'
```

本轮只在当前主仓库实施，不操作无关 `docs/research/`，不推送远程。

结果：217 项常规测试、两组定向测试、滚动控件 27 项及四档完整截图通过。
独立审查未发现阻断项。Windows/macOS、Qt 6.8、性能门禁与物理桌面仍未在本轮验证。
