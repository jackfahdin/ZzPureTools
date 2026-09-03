# 任务 5 报告：多窗口配置契约

## 范围

本任务只新增 header-only 的工作区窗口配置、创建选项、观察句柄和工厂契约，并扩展既有公共 API 测试。没有创建 `.cpp`，没有实现协调器、配置校验或应用配置逻辑。`ZzPureTools/CMakeLists.txt` 经检查无需修改：`zz_configure_library_target()` 已对公共 include 目录使用递归 glob，新增四个头会自动参与构建树、安装和导出；现有 PUBLIC/PRIVATE 链接断言保持 `Zz::WindowKit` 与 `Zz::FluentUI` 为私有依赖。

## RED 证据

先在 `ZzWorkspacePublicApiTest.cpp` 写入配置默认值、字段 Patch、创建选项枚举、观察句柄、复制移动、元类型与工厂签名测试，生产头尚不存在。

命令：

```sh
cmake --build --preset linux-gcc-debug --target ZzWorkspacePublicApiTest --parallel 2
```

结果：exit 1。编译器报错：

```text
fatal error: ZzPureTools/ZzWorkspaceWindowConfiguration.h: No such file or directory
```

这证明测试因新增公共 API 尚未实现而失败。初次尝试使用伪造动态类型来覆盖有效 handle 被复审指出不安全，已在生产代码之前删除；最终测试改用真实 `ZzPureApplication`、`ZzApplicationBuilder` 和 `ZzWorkspaceShell::create()` 路径创建实际窗口/Shell，未使用 `reinterpret_cast` 或仅为测试新增生产 API。

## GREEN 证据

新增四个头后，执行以下新鲜配置和目标构建：

```sh
GCC_13=/usr/bin/gcc GXX_13=/usr/bin/g++ \
QT_ROOT=/home/zz/Qt/6.11.1/gcc_64 \
cmake --preset linux-gcc-debug -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
cmake --build --preset linux-gcc-debug \
  --target ZzWorkspacePublicApiTest --parallel 2
```

结果：配置和构建均为 exit 0。`CMAKE_BUILD_WITH_INSTALL_RPATH=ON` 仅用于恢复本地 fresh build cache：CMake 4.3 对项目的相对安装 RPATH 生成阶段要求此选项；没有修改任何项目 CMake 文件。

测试命令：

```sh
LD_LIBRARY_PATH=/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzCore:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzFluentUI:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzPureTools:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzWindowKit:/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/build/linux-gcc-debug/ZzThirdParty/ZzLog:/home/zz/Qt/6.11.1/gcc_64/lib \
ctest --preset linux-gcc-debug \
  -R '^puretools\.workspace-public-api$' --output-on-failure
```

结果：exit 0，`1/1 Test #78: puretools.workspace-public-api ... Passed`，`100% tests passed, 0 tests failed out of 1`。

裸 ctest 命令会在加载测试二进制前失败，原因是该既有 ctest 条目只设置 `QT_QPA_PLATFORM=offscreen`，没有设置构建树动态库路径，且二进制无 RUNPATH；这不由本任务引入。测试的运行时库路径配置应在 `ZzPureTools/tests/CMakeLists.txt` 处理，但该文件不在本任务允许修改范围内，故未越界修改。

## 文件变更

- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowConfiguration.h`：关闭策略枚举、完整配置和字段级 optional Patch，以及元类型声明。
- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCreateOptions.h`：配置来源、初始可见性、创建选项和元类型声明。
- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowHandle.h`：仅含两个非拥有 `QPointer` 的观察句柄和 `isValid()`。
- `ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowFactory.h`：窗口工厂和页面 resolver 的固定函数签名。
- `ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp`：真实公共 API 契约测试。

## 自审

- 签名、枚举名和值与任务简报一致；不添加超出简报的生产行为。
- 公共头显式包含所需 Qt 值类型、`QPointer`、`QStringView` 及标准库类型；只前向声明应用窗口、Shell 和 `QWidget`。
- `ZzWorkspaceWindowHandle` 不拥有、删除或构造其观察对象；测试验证 Shell 和应用窗口销毁后 `QPointer` 自动清空。
- 新头未包含 `ZzFluentUI` 或 `ZzWindowKit`，既有 CMake 断言继续防止这些私有依赖泄漏到 `Zz::PureTools` 接口。
- `git diff --check` 会在提交前执行；工作树只应包含上述五个生产/测试文件和本报告。

## 疑虑与平台边界

- 未执行 Windows 或 macOS 原生构建/运行；头仅使用 Qt 6.8+ 的跨平台值类型、`QPointer` 和标准 C++20，因此作静态兼容判断，不宣称原生验证。
- 本地 test preset 依赖构建树动态库路径的外部环境，裸 ctest 失败已记录；带显式构建树库路径的目标测试通过。
