# 任务 5 审查：多窗口配置契约

## 规格结论

- 通过：四个公共 header 完整提供简报规定的三个枚举、三个配置值类型、观察句柄和两个工厂别名；枚举项、字段顺序、字段类型和默认值均与简报一致。`QSize` 与 `QRect` 保持 Qt 默认的空值哨兵，没有提前改为其他尺寸或几何默认值；非负尺寸、min/max 关系及对象归属/线程校验仍留给后续工厂注册和 `applyConfiguration` 实现。
- 通过：[ZzWorkspaceWindowConfiguration.h](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowConfiguration.h:3) 显式引入值类型和 `<optional>`；[ZzWorkspaceWindowCreateOptions.h](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowCreateOptions.h:3) 显式引入 `QPointer`；[ZzWorkspaceWindowFactory.h](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowFactory.h:3) 显式引入 `<functional>`、`<memory>` 和 `QStringView`。`ZzApplicationWindow`、`ZzWorkspaceShell`、`QWidget` 均以前向声明使用。
- 通过：[ZzWorkspaceWindowHandle.h](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/ZzPureTools/widgets/include/ZzPureTools/ZzWorkspaceWindowHandle.h:12) 只保存两个非拥有 `QPointer`，`isValid()` 只检查两个观察对象仍存活，不产生所有权或需要完整类型的访问。测试以真实 `ZzPureApplication`/`ZzApplicationWindow`/`ZzWorkspaceShell` 构造有效句柄，并验证 Shell 和窗口销毁后的自动失效，没有伪造动态类型或不安全转换。
- 通过：所有简报要求的枚举和结构体值类型均有 `Q_DECLARE_METATYPE`；`ZzWorkspacePageResolver` 精确为 `QStringView -> ZzResult<std::unique_ptr<QWidget>>`，窗口工厂签名也一致。
- 通过：新增公共 API 都有简体中文 Doxygen；命名空间为非链式写法；新头未包含或暴露 `ZzFluentUI`、`ZzWindowKit`。既有链接断言维持 `Zz::AppCore`/`Zz::FluentFoundation`/`Qt6::Widgets` 为 PUBLIC 和 `Zz::WindowKit`/`Zz::FluentUI` 为 PRIVATE。
- 通过：未修改 CMake 的结论有依据。[ZzLibraryTarget.cmake](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/cmake/ZzLibraryTarget.cmake:64) 用 `CONFIGURE_DEPENDS` 递归收集公共头并用于安装/导出，故四个新增 `.h` 自动覆盖。

## Critical 发现

无。

## Important 发现

无。

## Minor 发现

无。

## 测试卫生与范围外观察

- [ZzWorkspacePublicApiTest.cpp](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/ZzPureTools/tests/ZzWorkspacePublicApiTest.cpp:90) 覆盖默认值、Patch 字段、枚举、复制移动、元类型、两个工厂别名和真实 `QPointer` 生命周期，断言的是调用方可观察的公共契约，而非比较源文本或变更探测。
- 运行了唯一的聚焦检查：以报告所列 `LD_LIBRARY_PATH` 执行 `ctest --preset linux-gcc-debug -R '^puretools\\.workspace-public-api$' --output-on-failure`，结果为 1/1 通过（0.03 秒）。未运行完整测试集。
- 报告关于裸 `ctest` 缺少构建树运行时库路径的说明，与 [ZzPureTools/tests/CMakeLists.txt](/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsFrame/.worktrees/multi-window-workspace/ZzPureTools/tests/CMakeLists.txt:79) 只设置 `QT_QPA_PLATFORM=offscreen` 的现状一致；该问题早于本任务且不在本次允许修改范围内。
- 本次只进行 Linux 聚焦运行验证；Windows MSVC/MinGW 和 macOS 未运行，新增代码仅使用 Qt 6.8+ 的跨平台值类型、`QPointer` 和 C++20 标准库，可作静态兼容判断。

## 结论

通过。任务 5 的 `6ae675a` 相对 `c4b18c7` 符合任务简报和既有链接边界；无需修复。
