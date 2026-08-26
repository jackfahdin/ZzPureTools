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

2026-08-26 的 Task 15 自动门禁运行于远程 TTY 和专用 Xvfb，不满足本清单的
本地会话、活动物理输出和人工签署要求。下面所有交互项继续保持未执行；自动测试、
offscreen 截图和 Xvfb 性能结果只能作为补充证据，不能填写“实际结果”或提升首行状态。

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

工作树检查要求所有受跟踪源码、暂存区和冲突状态干净，并拒绝其他未跟踪文件。唯一
例外是仓库顶层精确命名的 `temp_image/`，它只作为用户保留的本地截图输入登记在
`host.log`；脚本不会读取、复制、哈希或提交该目录。`temp_image-other/`、已跟踪的
`temp_image` 修改或任何其他未跟踪路径都不会被例外覆盖。

仅存在 `DISPLAY` 或 `WAYLAND_DISPLAY` 环境变量不足以证明物理桌面可用。脚本还会
要求 X11 至少存在一个 `xrandr --listmonitors` 活跃输出，或要求 Wayland compositor
实际发布 `wl_output`；同时通过 `XDG_SESSION_ID` 和 `loginctl` 确认当前进程属于本地
活动桌面会话，并核对协议类型与桌面名称。远程 SSH/PTY 即使能访问另一个会话的
display socket，也必须在创建证据目录前失败关闭。

| 检查项 | 预期行为 | 实际结果 | 截图/日志路径 | 问题链接 |
|---|---|---|---|---|
| [ ] X11 KDE | 应用启动、退出、标题栏与基础导航正常，记录桌面和窗口管理器版本 |  |  |  |
| [ ] X11 GNOME | 应用启动、退出、标题栏与基础导航正常，记录桌面和窗口管理器版本 |  |  |  |
| [ ] Wayland KDE | 应用使用 Wayland 会话正常运行，拖动、resize 和系统操作符合协议能力 |  |  |  |
| [ ] Wayland GNOME | 应用使用 Wayland 会话正常运行，拖动、resize 和系统操作符合协议能力 |  |  |  |
| [ ] 强制 Qt fallback | 启用 `ZZ_WINDOWKIT_FORCE_QT_CONTEXT` 后基础窗口功能可用且不加载 native 后端 |  |  |  |
| [ ] shared 包 | 安装后的 shared 包可从干净前缀启动，依赖解析无 `not found` |  |  |  |
| [ ] static 包 | static 一方库由外部消费者链接成功，不要求 QWindowKit 包或私有头 |  |  |  |
| [ ] 工作区标题栏与菜单 | 宽窗口显示横向菜单、窄窗口显示折叠菜单；标题不与内容或系统按钮重叠 |  |  |  |
| [ ] 工作区侧栏与 Explorer | Activity 激活/折叠、Side 宽度和 10 万节点筛选在当前会话可用 |  |  |  |
| [ ] 工作区 Tab、Command 与 Dock | Tab 状态、命令搜索/焦点恢复、Dock 浮动/重新停靠及布局保存恢复正确 |  |  |  |
| [ ] 工作区真实 Tab 拖放 Overlay | 用鼠标从真实 tab 发起拖放，中心、Top、Bottom、Left、Right 五区 overlay 同时可辨且无裁切；保存 overlay 可见态截图 |  |  |  |
| [ ] 工作区拖放提交与取消 | 依次向五区放置并验证结构结果；Escape/移出/取消不改变布局，不遗留 overlay 或输入捕获 |  |  |  |
| [ ] 标题栏拖动 | 可拖动窗口；按钮、输入和交互区域不会误触发拖动 |  |  |  |
| [ ] 四边与四角 resize | 支持的会话中 cursor、双轴方向、最小尺寸和连续 resize 正确 |  |  |  |
| [ ] 双击最大化 | 支持的会话中最大化/还原和窗口几何正确 |  |  |  |
| [ ] best-effort 系统菜单 | 后端支持时菜单位置和命令正确；不支持时返回明确降级结果 |  |  |  |
| [ ] 最小化与恢复 | dock/panel 操作、多次最小化恢复和多窗口状态隔离正确 |  |  |  |
| [ ] 100% 与高 DPI | 字体、图标、标题栏、hit test 和跨显示器 DPR 更新正确 |  |  |  |
| [ ] Light/Dark 主题 | 桌面或应用主题切换后文字、背景、边框和图标同步 |  |  |  |
| [ ] 键盘导航 | Tab、Shift+Tab、Enter、Space、Escape 和方向键符合控件语义 |  |  |  |
| [ ] IME 输入 | 在 Command Palette、Explorer 搜索和可编辑内容中完成中文预编辑、候选确认、取消与焦点切换，无重复提交或丢字 |  |  |  |
| [ ] 工作区重启恢复 | 保存包含双侧面板、Bottom 工具和多组 tab 的布局，正常退出并重启后恢复同一拓扑、尺寸、活动组和页面键 |  |  |  |
| [ ] 静态 Example 启动 | 从静态安装消费产物启动 `ZzPureToolsExample`，工作区标题栏、主题、键盘、IME、拖放与布局恢复行为不依赖项目 shared 库 |  |  |  |
| [ ] 可访问性 | Orca 或等价工具可读出名称、角色、状态和焦点顺序 |  |  |  |
| [ ] 三组容差截图 | Light、Dark、HighContrast 在固定 DPR 下通过已审视觉差异 |  |  |  |
| [ ] 参考性能运行 | 在活动档案执行 12 个报告生产者与 15 项绝对门禁，指纹与基线一致 |  |  |  |
| [ ] 反复窗口生命周期 | 连续创建关闭 100 次后无崩溃、泄漏、UAF 或适配层对象残留 |  |  |  |

## 签署规则

五种会话必须分别保存证据，不能用 Xvfb、嵌套 compositor 或另一桌面的一次结果覆盖。无问题时“问题链接”填写“无”；存在问题时填写可访问的问题记录并标明发布影响。只有字段完整、五种会话和所有适用项完成、参考性能结果有效且没有未解决的发布阻断问题，才能把首行状态改为 `真机验收通过`。
