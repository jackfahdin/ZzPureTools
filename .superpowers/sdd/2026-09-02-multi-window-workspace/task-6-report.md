# 任务 6：延迟显示窗口与协调器登记骨架报告

## 目标与范围

本任务为每个 `ZzPureApplication` 建立唯一的工作区窗口协调器，并提供
`Visible`/`Deferred` 两种窗口创建可见性。实现只覆盖 Window/Shell 显式登记、
配置快照与校验、销毁自动注销和关闭阶段的连接解除；未实现任务 7 的工厂、
`createWindow(options)`、撕出或配置应用，也未实现任务 8 的关闭策略事务。

## TDD 记录

### RED

先新增 `ZzWorkspaceWindowCoordinatorTest`、CMake 测试目标及应用的 Deferred
行为测试，然后执行：

```bash
GCC_13=/usr/bin/gcc GXX_13=/usr/bin/g++ \
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 \
cmake --preset linux-gcc-debug -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON && \
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceWindowCoordinatorTest ZzApplicationBuilderTest --parallel 2
```

结果：失败，且失败原因是待实现 API 缺失。

```text
fatal error: ZzPureTools/ZzWorkspaceWindowCoordinator.h: No such file or directory
no matching function for call to
ZzPureApplication::createWindow(ZzApplicationWindowVisibility)
```

### GREEN

实现最小接口和登记记录后，执行：

```bash
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceWindowCoordinatorTest ZzApplicationBuilderTest \
  ZzMultiWindowIsolationTest --parallel 2
```

结果：三个目标均成功链接。

随后使用绝对动态库路径运行回归：

```bash
LD_LIBRARY_PATH=/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzCore:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzFluentUI:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzPureTools:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzWindowKit:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzThirdParty/ZzLog:/home/zz/Qt/6.11.1/gcc_64/lib \
QT_QPA_PLATFORM=offscreen \
ctest --test-dir build/linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|application-builder|multi-window)' \
  --output-on-failure
```

结果：36/36 通过，0 个失败。

## 设计与实现

- `ZzPureApplication::createWindow()` 保持原签名并委托给
  `createWindow(Visible)`；`Deferred` 仍完整执行 WindowKit、导航和 setup
  callback，只在 `adoptWindow()` 跳过 `show()`。非法 visibility 返回
  `InvalidArgument` 且不创建窗口。
- `ZzPureApplicationPrivate` 继续由 `windows` 的 `unique_ptr` 独占所有窗口。
  `coordinator` 声明在 `windows` 之后，因此成员逆序析构时协调器先于窗口。
  `beginShutdown()` 先令协调器进入关闭态、断开工作区观察连接，才清空窗口。
- 协调器构造函数私有，仅由应用私有对象独占创建。登记表保留 Window/Shell 的
  `QPointer`、稳定原始身份、配置值副本、主窗口标志和两端 destroyed 连接。
  `unregisterWindow()` 只移除该记录，不影响 Qt 对象所有权。
- 登记在写入表前校验调用线程和对象亲和性、空 handle、Window/Shell 唯一性、
  Shell 的 `workspaceWidget()` 是否属于指定 Window、唯一 primary，以及配置。
  默认 `QSize()` 与 `QRect()` 被明确视为未覆写哨兵；部分负尺寸、min/max
  反转、负几何尺寸和非法 enum 均返回 `InvalidArgument`。负几何坐标允许。
- 重复 Window/Shell、第二 primary 及跨线程调用返回 `InvalidState`；未登记
  查询/注销返回 `NotFound`。Window 或 Shell 的 destroyed 信号会移除记录。
  窗口销毁测试传入已销毁对象的旧地址仅用于不解引用的身份查表，验证记录已被
  删除；`configuration()` 在未命中前不会访问该对象。

## 测试覆盖

- 无参窗口创建仍显示；Deferred 已由应用接管、完成 WindowKit/导航/setup，
  但保持不可见。
- 协调器指针对同一应用稳定；首次窗口的显式 primary 登记保存独立配置快照。
- 默认尺寸/几何哨兵、负尺寸、min/max、负几何大小、非法标题模式与关闭策略；
  所有失败路径都断言未产生登记。
- 重复 Window、重复 Shell、第二 primary、错误宿主与跨线程调用。
- Shell 销毁、窗口真实 close 后的 queued 应用 erase 自动注销；显式注销不销毁
  Window 或 Shell。
- 现有应用构建与多窗口隔离回归，覆盖原有 close token 和 queued erase 语义。

## 变更文件

- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCoordinator.h`
- `ZzPureTools/widgets/src/ZzWorkspaceWindowCoordinator.cpp`
- `ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.h`
- `ZzPureTools/widgets/src/private/ZzWorkspaceWindowCoordinatorPrivate.cpp`
- `ZzPureTools/widgets/include/ZzPureTools/ZzPureApplication.h`
- `ZzPureTools/widgets/src/ZzPureApplication.cpp`
- `ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.h`
- `ZzPureTools/widgets/src/private/ZzPureApplicationPrivate.cpp`
- `ZzPureTools/CMakeLists.txt`
- `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp`
- `ZzPureTools/tests/ZzApplicationBuilderTest.cpp`
- `ZzPureTools/tests/CMakeLists.txt`

## 自审与平台边界

- 已检查公开签名、错误码、唯一所有权、配置快照、destroyed 自动注销、关闭顺序，
  并执行 `git diff --check`。
- Linux GCC 15 与 Qt 6.11.1 的 offscreen 测试已验证。Windows MSVC/MinGW 与
  macOS 只进行 C++20/Qt 跨平台静态兼容性审查，未在对应原生环境运行。
- 裸 `ctest` 已知会受 RPATH 环境影响；本报告的 ctest 使用了指定的绝对
  `LD_LIBRARY_PATH`，避免将该环境问题误判为功能失败。

## 第 1/5 轮修复：CTest 运行时库路径

### RED

审查发现 Window Builder、Coordinator 和 MultiWindow 三个测试族只注册了
`QT_QPA_PLATFORM`，没有将构建树内共享库路径传递给 CTest 子进程。未设置任何
外部库路径，直接执行计划原命令：

```bash
ctest --preset linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|application-builder|multi-window)' \
  --output-on-failure
```

结果：0/36 通过；每个测试均在断言前因同一动态加载错误退出：

```text
error while loading shared libraries: libZzPureTools.so.0:
cannot open shared object file: No such file or directory
```

### 修复与 GREEN

仅修改 `ZzPureTools/tests/CMakeLists.txt`：提取
`zz_configure_puretools_window_test_runtime()`，为三个既有测试 foreach 注册的每个
CTest 条目追加 `ENVIRONMENT_MODIFICATION`。该 helper 按平台选择
`LD_LIBRARY_PATH`（Linux/其他）、`DYLD_LIBRARY_PATH`（macOS）或 `PATH`
（Windows），并使用 `$<TARGET_FILE_DIR:...>` 预置 ZzCore、FluentFoundation、
FluentUI、PureTools、WindowKit、ZzLog 和 Qt Core 的构建树目录；不依赖调用者
shell 环境，也不硬编码本机路径。

重新配置和验证：

```bash
GCC_13=/usr/bin/gcc GXX_13=/usr/bin/g++ \
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 \
cmake --preset linux-gcc-debug -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-gcc-debug --target \
  ZzWorkspaceWindowCoordinatorTest ZzApplicationBuilderTest \
  ZzMultiWindowIsolationTest --parallel 2
ctest --preset linux-gcc-debug \
  -R '^puretools\.(workspace-window-coordinator|application-builder|multi-window)' \
  --output-on-failure
```

结果：构建成功；未注入外部 `LD_LIBRARY_PATH` 的裸 CTest 36/36 通过，0 个失败。
本轮修复取代上述“裸 ctest 已知受 RPATH 影响”的限制；该限制不再适用于三个
已注册修复路径的测试族。Windows MSVC/MinGW 与 macOS 仍未做原生运行验证。
