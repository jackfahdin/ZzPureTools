状态: 未执行
测试日期:
测试人员:
OS/版本:
Qt/工具链:
设备/显示器:
构建产物摘要:
结果:
问题链接:

# Linux 真机验收清单

本清单覆盖 X11 KDE、X11 GNOME、Wayland KDE、Wayland GNOME 和强制 Qt fallback。当前 `local-release-xvfb` 是自动性能参考环境，但主机没有物理显示器，其报告不能代替以下桌面会话交互项。开始前保存 `build/gate-evidence/linux-native.log`，并确保被测 commit、Qt、工具链和产物摘要一致。

## 执行方式

每种真实桌面会话必须从干净工作树和同一 commit 开始。普通四种会话先构建
shared Release：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15

cmake --preset linux-gcc-release -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-release --parallel 2
bash scripts/release/run-linux-desktop-acceptance.sh \
  --session linux-wayland-kde \
  --build-dir build/linux-gcc-release
```

将 `--session` 分别替换为 `linux-x11-kde`、`linux-x11-gnome`、
`linux-wayland-kde` 和 `linux-wayland-gnome`。脚本会校验实际
`XDG_SESSION_TYPE`、`XDG_CURRENT_DESKTOP` 和显示变量，错误会话不能借用名称通过。

Qt fallback 必须使用独立构建目录，不能覆盖正常 native 构建缓存：

```bash
cmake --preset linux-gcc-release \
  -B build/linux-qt-fallback-release \
  -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_WINDOWKIT_FORCE_QT_CONTEXT=ON \
  -DXKB_INCLUDE_DIR="$PWD/build/dependencies/xkbcommon/root/usr/include" \
  -DXKB_LIBRARY=/usr/lib/x86_64-linux-gnu/libxkbcommon.so.0
cmake --build build/linux-qt-fallback-release \
  --target ZzPureToolsExample --parallel 2
bash scripts/release/run-linux-desktop-acceptance.sh \
  --session linux-qt-fallback \
  --build-dir build/linux-qt-fallback-release
```

脚本在 `build/gate-evidence/linux-desktop/` 下创建不可复用的会话目录，记录源码
commit、工作树状态、桌面协议、Qt、编译器、显示器、ELF 依赖和二进制 SHA-256，
同时复核 shared Release 缓存、Qt SDK 路径，并从干净源码增量重建后启动真实
`ZzPureToolsExample`。测试人员完成交互后，从应用的确认关闭路径退出，把截图放入
脚本给出的 `screenshots/`，逐项填写同目录 `RESULT_ZH.md`。脚本只采证，不会自动
把会话判定为通过，也不会修改本清单或平台状态。

| 检查项 | 预期行为 | 实际结果 | 截图/日志路径 | 问题链接 |
|---|---|---|---|---|
| [ ] X11 KDE | 应用启动、退出、标题栏与基础导航正常，记录桌面和窗口管理器版本 |  |  |  |
| [ ] X11 GNOME | 应用启动、退出、标题栏与基础导航正常，记录桌面和窗口管理器版本 |  |  |  |
| [ ] Wayland KDE | 应用使用 Wayland 会话正常运行，拖动、resize 和系统操作符合协议能力 |  |  |  |
| [ ] Wayland GNOME | 应用使用 Wayland 会话正常运行，拖动、resize 和系统操作符合协议能力 |  |  |  |
| [ ] 强制 Qt fallback | 启用 `ZZ_WINDOWKIT_FORCE_QT_CONTEXT` 后基础窗口功能可用且不加载 native 后端 |  |  |  |
| [ ] shared 包 | 安装后的 shared 包可从干净前缀启动，依赖解析无 `not found` |  |  |  |
| [ ] static 包 | static 一方库由外部消费者链接成功，不要求 QWindowKit 包或私有头 |  |  |  |
| [ ] 标题栏拖动 | 可拖动窗口；按钮、输入和交互区域不会误触发拖动 |  |  |  |
| [ ] 四边与四角 resize | 支持的会话中 cursor、双轴方向、最小尺寸和连续 resize 正确 |  |  |  |
| [ ] 双击最大化 | 支持的会话中最大化/还原和窗口几何正确 |  |  |  |
| [ ] best-effort 系统菜单 | 后端支持时菜单位置和命令正确；不支持时返回明确降级结果 |  |  |  |
| [ ] 最小化与恢复 | dock/panel 操作、多次最小化恢复和多窗口状态隔离正确 |  |  |  |
| [ ] 100% 与高 DPI | 字体、图标、标题栏、hit test 和跨显示器 DPR 更新正确 |  |  |  |
| [ ] Light/Dark 主题 | 桌面或应用主题切换后文字、背景、边框和图标同步 |  |  |  |
| [ ] 键盘导航 | Tab、Shift+Tab、Enter、Space、Escape 和方向键符合控件语义 |  |  |  |
| [ ] 可访问性 | Orca 或等价工具可读出名称、角色、状态和焦点顺序 |  |  |  |
| [ ] 三组容差截图 | Light、Dark、HighContrast 在固定 DPR 下通过已审视觉差异 |  |  |  |
| [ ] 参考性能运行 | 在活动档案执行 12 个报告生产者与 15 项绝对门禁，指纹与基线一致 |  |  |  |
| [ ] 反复窗口生命周期 | 连续创建关闭 100 次后无崩溃、泄漏、UAF 或适配层对象残留 |  |  |  |

## 签署规则

五种会话必须分别保存证据，不能用 Xvfb、嵌套 compositor 或另一桌面的一次结果覆盖。无问题时“问题链接”填写“无”；存在问题时填写可访问的问题记录并标明发布影响。只有字段完整、五种会话和所有适用项完成、参考性能结果有效且没有未解决的发布阻断问题，才能把首行状态改为 `真机验收通过`。
