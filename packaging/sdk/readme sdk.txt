ZzPureTools SDK 使用说明
=====================

一、本包内容
------------
  include/ZzPureTools/          公开头文件（对外 API）
  lib/ZzPureTools.lib           导入库（链接时使用）
  lib/cmake/ZzPureTools/        CMake 配置文件（find_package 用）
  bin/ZzPureTools.dll           动态库（运行时需要，静态库构建时无此文件）
  examples/minimal/          最小集成示例
  LICENSE                    许可证

二、使用者环境要求（需自行安装，SDK 不包含 Qt）
------------------------------------------------
  - CMake 3.21+、C++20
  - Qt 6.8.0+（Widgets 模块），版本需与 BUILD_INFO.txt 中记录一致
  - 工具链需与 SDK 构建时一致（见 BUILD_INFO.txt）：
      Windows x64  : MSVC 2022 或 MinGW-w64 13.1
      Windows arm64: MSVC 2022
      Linux x64    : GCC x86_64
      Linux arm64  : GCC aarch64
      macOS        : Clang x86_64 或 arm64
  - 本项目仅支持 Qt 6.8.0 及以上版本

三、CMake 集成（推荐）
----------------------
  假设 SDK 解压到 D:\sdk\ZzPureTools，Qt 安装在 D:\Qt\6.8.0\msvc2022_64：

  cmake -B build -S your_project ^
    -DCMAKE_PREFIX_PATH="D:/Qt/6.8.0/msvc2022_64;D:/sdk/ZzPureTools" ^
    -DCMAKE_BUILD_TYPE=Release

  在 CMakeLists.txt 中：

    find_package(Qt6 6.8.0 REQUIRED COMPONENTS Widgets)
    find_package(ZzPureTools REQUIRED)
    target_link_libraries(YourApp PRIVATE Zz::PureTools)

  代码中：

    #include <ZzPureTools/ZzPureTools.hpp>

    QApplication app(argc, argv);
    ZzApplication::instance().initialize(app);

四、运行你自己编译的程序
------------------------
  动态库模式下，需确保 ZzPureTools.dll 在 exe 同目录，或在 PATH 中。
  你的程序仍依赖 Qt，发布时对自己的 exe 使用 windeployqt。

五、试用包内最小示例
--------------------
  cd examples/minimal
  cmake -B build -S . ^
    -DZZ_PURE_TOOLS_SDK_DIR="D:/sdk/ZzPureTools" ^
    -DCMAKE_PREFIX_PATH="D:/Qt/6.8.0/msvc2022_64" ^
    -DCMAKE_BUILD_TYPE=Release
  cmake --build build

  将 bin\ZzPureTools.dll 复制到 build\ZzPureToolsMinimalConsumer.exe 同目录后运行。
