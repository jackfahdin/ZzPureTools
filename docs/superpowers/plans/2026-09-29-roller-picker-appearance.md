# 滚轮选择器外观整理实现计划

**目标：** 按用户认可的方案统一确定、取消图标，改善选中行可读性，保持现有弹层定位与选择事务。

**架构：** 修改库内 `ZzRollerPickerPrivate.cpp` 和 `ZzRoller.cpp`；复用公共字体图标缓存、按钮强调色和主题调色板，不新增公开接口或动画计时器。保持当前 master 和 Debug 示例验收路径。

**技术栈：** Qt 6.8+、C++20；本机 Qt 6.11.1、GCC 15。

- [x] 在 `ZzRollerControlsTest.cpp` 增加选中行渲染对比度、操作图标随调色板变化的回归，先确认旧实现失败。
- [x] 确定使用 `Check`、取消使用 `Xmark`，统一 16px 逻辑尺寸，保留文字及标准按钮角色；确定显式采用公共强调色。图标在调色板、样式或 DPR 变化后刷新，非 Fluent 样式使用同一内嵌字体回退。
- [x] 选中行保留浅强调色底，改用正文前景；悬停行复用选中行圆角尺寸。保留列间细分隔与现有间距，避免影响弹窗定位和容量。
- [x] 重编译 Debug Example，执行本机开发门禁及滚轮定向测试，更新四档 DPR 的滚轮三主题截图并关闭更新模式复验。
- [x] 记录验证范围，以中文提交本轮实现，不自动推送。

验证入口：

```bash
cmake --build build/linux-gcc-debug --target ZzRollerControlsTest ZzPureToolsExample ZzFluentScreenshotTest --parallel 2
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^fluent.roller-controls$'
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 GCC_13=/usr/bin/gcc-15 GXX_13=/usr/bin/g++-15 CMAKE_BUILD_PARALLEL_LEVEL=2 ./scripts/ci/run-local-development-gate.sh --preset linux-gcc-debug --tests '^fluent.roller-controls$'
ctest --test-dir build/linux-gcc-debug --output-on-failure -R '^fluent.screenshot-'
```

截图更新只运行 `ZzFluentScreenshotTest rendersRollerThemes`，分别设置 `QT_SCALE_FACTOR` 与 `--expected-dpr` 为 1、1.25、1.5、2，并指定对应的 `--baseline-subdir dpr-100/125/150/200`。更新时设置 `ZZ_UPDATE_SCREENSHOTS=1`，复验时移除。

## 验证记录

- Linux Qt 6.11.1 / GCC 15.2.0 / Debug：216 项普通测试及滚轮定向测试通过，滚轮测试包含 20 项（含初始化、清理）；Debug Example 已重编译。
- 四档完整 Fluent 截图测试通过，仅更新滚轮场景 12 张基线；最终图标配色修正后再次关闭更新模式复验四档滚轮三主题，全部通过。
- 已目视检查浅色、深色、高对比度截图；文档审计及 `git diff --check` 通过。
- 定向审查发现禁用期间更新调色板会污染 Normal 图标的边界问题。新增实际“禁用→换色→启用”回归并先复现失败，改为显式区分 Normal、Disabled 配色后通过。
- 待验证：Windows/macOS 实机、Qt 6.8、跨屏 DPR 实际交互、物理桌面验收、完整性能门禁、ASan/UBSan 与 clang-tidy。本轮没有增加计时器或动画，对象预算回归通过。
