# ZzPureToolsFrame 原生平台验收与发布候选计划

**状态：** 阶段 A 已完成，阶段 B 等待物理桌面终端执行。

**目标：** 在不继续扩充组件数量的前提下，把已经完成 Linux 自动质量门禁的
ZzPureToolsFrame 推进到可审计的 Linux 物理桌面验收，并为 Windows MSVC、Windows
Qt SDK MinGW、macOS arm64/x86_64 原生验收建立同一套证据闭环。任何平台问题都先
形成可复现证据，再做最小修复和原平台复验。

**基线提交：** `d13f0e8`（广度扩展路线关闭）。

**技术边界：** Qt 6.8+、C++20、CMakeLists.txt + CMakePresets.json、Qt Test；
Linux 为当前主实现与自动验证平台。Windows 和 macOS 没有原生执行证据时只记录
`未执行`，不得从 Linux 或 CI 结果推导为通过。

---

## 1. 当前已知事实

- ZzFluentUI 公开组件数为 37；标准 Qt Widgets 由应用级 `ZzFluentStyle` 覆盖，
  不再以包装类数量为扩展目标。
- Linux Qt 6.11.1 / GCC 15.2.0 的 Debug、Release、Static 完整 CTest 均为
  131/131；Shared/Static LTO、Clang-Tidy 20、ASan/UBSan 和三轮标准表面性能观测
  已完成。
- `ZzPureToolsExample` 已串联十二路由、标准表面、List/Table/Tree、设置与关于、
  Activity Dock、多窗口、关闭守卫和软件材质。
- 仓库已有三平台人工清单、Linux 桌面采证脚本、三平台聚合 gate 脚本、平台状态
  矩阵和 CMake preset，不重新设计第二套体系。
- 用户目录 `temp_image/` 是本地验收输入，禁止修改、删除或提交；当前 Linux 采证
  脚本把该目录误判为源码脏状态，必须先修正。
- GitHub CI 暂不作为本计划执行前置条件，不调用 GitHub CLI、不 push。

**阶段 A 实施记录（2026-08-13）：** `6463102` 允许采证脚本精确保留顶层
`temp_image/` 本地输入，`579310a` 增加 `loginctl` 本地活动桌面会话绑定。Bash
语法、帮助入口、`platform.linux-desktop-acceptance-contract` 均通过。当前远程
Codex shell 属于 `XDG_SESSION_ID=55`、`Remote=yes` 的 tty；本机可用物理会话是
`session 12 / KDE / Wayland / Remote=no / Active=yes`。脚本从远程 shell 正确以
退出码 65 拒绝，未创建伪造证据目录。

native 和 forced Qt fallback 的 shared Release Example 已在同一候选提交构建完成，
两个 ELF 的 build-tree RUNPATH 完整，`ldd` 未发现 `not found` 依赖；这些是构建和
依赖准备结果，不是人工交互通过结果。

## 2. 状态和证据模型

平台状态继续只使用：

1. `未执行`：没有该环境的原生 configure/build/CTest/二进制证据；
2. `静态验证通过`：原生工具链自动门禁完成，但人工交互没有签署；
3. `真机验收通过`：自动门禁和对应人工清单均完成，且没有发布阻断问题。

每份人工证据必须绑定：40 位源码 commit、工作树中受跟踪文件状态、构建目录、
Qt 根和版本、编译器、链接模式、二进制 SHA-256、OS/桌面/显示协议、显示器与 DPR、
测试人员、日期、截图/日志、问题链接和退出码。`temp_image/` 可以作为明确登记的
本地输入存在，但不能被描述为源码的一部分，也不能影响二进制摘要。

问题回流固定为：

```text
原平台失败证据 -> 自动回归测试或合同 -> 最小实现修复 -> Linux 回归
               -> 原平台 shared/static 复验 -> 人工项复验 -> 状态更新
```

禁止通过删除失败项、放宽性能阈值、关闭严格警告、注入 `LD_LIBRARY_PATH`、混用 Qt
ABI 或把不适用项直接写成通过来完成验收。

---

## 3. 阶段 A：加固 Linux 桌面采证入口

### 3.1 工作树判定

修改：

- `scripts/release/run-linux-desktop-acceptance.sh`
- `tests/Platform/ZzLinuxDesktopAcceptanceContract.cmake`
- `docs/release/MANUAL_LINUX_CHECKLIST_ZH.md`

要求：

- tracked、staged、冲突文件和除顶层 `temp_image/` 外的未跟踪文件仍然失败关闭；
- 只允许 `git status --porcelain` 中精确的 `?? temp_image/`，不能按前缀放行
  `temp_image-other/`，也不能放行已跟踪的 `temp_image` 修改；
- `host.log` 将源码状态记录为 `tracked-clean`，并单独记录
  `source.localUntrackedInputs=temp_image/`；没有该目录时记录 `none`；
- 帮助文本和 Linux 清单说明该例外只用于本地截图输入，目录不会被读取、哈希、
  复制或提交；
- 合同测试至少覆盖脚本语法、合法 session、精确白名单令牌、状态日志令牌和帮助
  文本，防止以后把例外扩大成忽略所有未跟踪文件。

验证：

```bash
bash -n scripts/release/run-linux-desktop-acceptance.sh
bash scripts/release/run-linux-desktop-acceptance.sh --help
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzLinuxDesktopAcceptanceContract.cmake
git diff --check
```

提交：`质量：允许验收保留本地截图输入`。

### 3.2 启动前审计

不修改代码，读取并记录：

```bash
printf '%s\n' "$XDG_SESSION_TYPE" "$XDG_CURRENT_DESKTOP"
printf '%s\n' "${DISPLAY:-}" "${WAYLAND_DISPLAY:-}"
cmake --list-presets=all
```

X11 必须由 `xrandr --listmonitors` 证明存在活动输出；Wayland 必须由
`wayland-info` 证明 compositor 发布 `wl_output`。缺少工具时由用户安装对应系统包，
不能用 Xvfb 或伪造环境变量替代。

退出条件：采证脚本合同通过，`temp_image/` 保留且没有进入提交，当前真实 session
和可用显示输出已经识别。**已完成。**

---

## 4. 阶段 B：当前 Linux 物理桌面验收

### 4.1 构建同一提交的 shared Release

沿用本机 Qt，不重新下载 SDK：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15

cmake --fresh --preset linux-gcc-release \
  -DZZ_BUILD_EXAMPLES=ON \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF
cmake --build --preset linux-gcc-release \
  --target ZzPureToolsExample --parallel 2
```

若本机 Qt 的 Linux private GUI 配置需要 xkbcommon 开发头，只通过本机
`XKB_INCLUDE_DIR`/`XKB_LIBRARY` 覆盖解决；本阶段不 vendoring xkbcommon，也不把绝对
路径写入共享 preset。

### 4.2 自动采证并启动

根据真实会话只选择匹配的 session ID：

```bash
bash scripts/release/run-linux-desktop-acceptance.sh \
  --session <检测到的-session-id> \
  --build-dir build/linux-gcc-release
```

脚本负责重建、主机/依赖/摘要采集和启动；用户负责观察真实桌面并在退出前完成：

- 十二路由单击、搜索、返回/前进、Settings/About 选中即时收敛；
- Navigation/List/Table/Tree 指示条、文字、焦点环和内容区域不重叠；
- ToggleSwitch 整个可见表面均可点击，checkable PushButton 语义正确；
- Activity Dock 位于尾部时跟随最新日志，手动上翻后暂停，回到底部后恢复；
- 标题栏拖动、交互区排除、四边四角 resize、最大化/还原、系统菜单或明确降级；
- Light/Dark/HighContrast、键盘焦点、可访问名称、多窗口、三种关闭路径；
- 当前设备能够提供的 DPR 和跨显示器行为。

用户从应用确认关闭后，填写脚本生成的 `RESULT_ZH.md`，截图放在同一证据目录。
脚本退出码 0 只代表应用正常退出，不代表人工项目通过。

### 4.3 强制 Qt fallback

使用独立构建树和同一源码提交：

```bash
cmake --preset linux-gcc-release \
  -B build/linux-qt-fallback-release \
  -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_WINDOWKIT_FORCE_QT_CONTEXT=ON \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF \
  -DXKB_INCLUDE_DIR=<本机开发头目录> \
  -DXKB_LIBRARY=<本机libxkbcommon路径>
cmake --build build/linux-qt-fallback-release \
  --target ZzPureToolsExample --parallel 2
bash scripts/release/run-linux-desktop-acceptance.sh \
  --session linux-qt-fallback \
  --build-dir build/linux-qt-fallback-release
```

fallback 验收必须确认基础移动、resize、导航和关闭可用，能力页不宣称 native 后端
能力。native 与 fallback 不能共用一份人工结论。

### 4.4 结果处理

- 无缺陷：提交签署后的精简结论和 `PLATFORM_SUPPORT_ZH.md` 对应 session 行；构建
  日志、二进制和临时性能报告仍留在忽略目录，不提交。
- 有缺陷：先记录失败证据；每个独立根因一个修复提交，完成 Linux 自动定向回归后
  重跑原 session，不直接编辑矩阵为通过。
- 当前机器没有安装的 GNOME/KDE 或 X11/Wayland 组合保持 `未执行`，不能由当前
  session 代替。它们是兼容矩阵余项，不阻止继续开展其他平台验收，但最终声称该
  session 受支持前必须补证据。

退出条件：当前真实 session 和 forced Qt fallback 各有独立、字段完整的人工结果；
所有发现的问题均有结论；平台矩阵只提升实际执行的行。**待 session 12 的 KDE
Wayland Konsole 执行；当前不能登记人工通过。**

---

## 5. 阶段 C：Windows 原生验收

前置：Windows 10/11 x64、Visual Studio 2022 19.38+、MSVC Qt 6.8+、同一 Qt SDK
中的 MinGW kit/工具链/Ninja。MSVC 与 MinGW 使用完全独立目录和 Qt ABI。

自动门禁：

```powershell
pwsh -NoProfile -File scripts/ci/run-windows-gates.ps1
```

最低结果：MSVC shared/static 与 MinGW shared/static 均完成 configure、全部目标严格
警告构建、CTest、安装消费、包重定位和 PE/依赖扫描；日志保存到
`build/gate-evidence/windows-native.log`。用户已经明确暂缓 Windows CI，因此只在
原生 Windows 主机执行，不调用远端接口。

人工验收使用 `docs/release/MANUAL_WINDOWS_CHECKLIST_ZH.md`，重点检查 100/150/200%
DPI、混合 DPI 多显示器、八方向 resize、Snap Layout、系统菜单、Narrator、主题、
四种 ABI/链接产物和 100 次窗口生命周期。Windows 10 与 Windows 11 不互相替代。

退出条件：完成的 ABI 行先提升到 `静态验证通过`；具备完整人工证据的 OS/ABI 行再
提升到 `真机验收通过`。其余行保持 `未执行`。

---

## 6. 阶段 D：macOS 原生验收

前置：macOS 13.3+、Apple Clang 15+、架构匹配的 Qt arm64 和 Qt x86_64 SDK。
Rosetta 只能补充 x86_64 运行，不能把 arm64 Qt SDK 当作 x86_64 构建证据。

自动门禁：

```bash
bash scripts/ci/run-macos-gates.sh
```

四个 preset 分别完成 shared/static、arm64/x86_64 构建、CTest、安装消费、包重定位
和 `lipo`/`otool` 检查。人工清单重点覆盖 Retina/混合显示器、traffic lights、原生
全屏、标题栏双击系统偏好、VoiceOver、Blur 降级、Dock 和多窗口。

退出条件：只提升有原生构建日志的架构/链接行；只有同一产物摘要的人工记录才能
提升为 `真机验收通过`。

---

## 7. 阶段 E：发布候选审计

此阶段不开发新功能，只汇总事实：

1. 选择候选 commit，确认工作树中没有项目修改，`temp_image/` 未提交；
2. Linux 自动门禁仍对应候选 commit，必要时只重跑受后续修复影响的完整矩阵；
3. 三平台矩阵、人工清单、第三方审核和许可证记录相互一致；
4. `ZZ_RELEASE_BUILD=ON` 使用仓库外审核证据失败关闭验证，不把证据绝对路径提交；
5. 所有 `observe` 性能项继续标注为观测，不在本阶段临时提升阈值或改成 gate；
6. 输出发布说明，明确已验证平台、未验证平台、已知限制和 QWindowKit/软件材质降级。

发布判定按“已声明支持范围”逐行进行。缺少某一平台证据时，可以形成 Linux-only
候选结论，但不能发布成三平台均已验收。最终是否对外发布由项目所有者 Jackfahdin
签署，本计划不自动打 tag、不 push、不创建 GitHub Release。

---

## 8. 提交边界

预期提交顺序：

```text
文档：规划原生平台验收阶段
质量：允许验收保留本地截图输入
文档：记录Linux当前桌面验收结果
修复：<仅在验收发现真实缺陷时逐项提交>
文档：记录Windows原生验收结果
文档：记录macOS原生验收结果
文档：形成跨平台发布候选结论
```

每个提交标题使用中文简述，正文使用中文说明行为、边界、验证命令、真实执行平台和
未执行项。每笔提交前运行 `git diff --check` 和适用的最小测试；不提交 `build/`、
`install/`、性能报告、临时二进制、未审核图片或 `temp_image/`。

## 9. 停止扩张条件

本计划完成前不启动下一轮 Fluent 控件广度开发。只有以下输入才能新建立项：

- 用户提出明确产品功能；
- 人工验收提供可复现缺陷；
- 旧版能力映射表发现未覆盖且不能由现有 Qt/Zz 组合表达的语义；
- 性能报告证明正式阈值回归；
- 原生平台编译或运行暴露公共 API、ABI 或窗口系统缺口。

“组件数量尚未达到某个整数”不是立项依据。
