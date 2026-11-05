# SDK 分组件安装修复计划

用户已批准按顺序修复安装、验证共享／静态消费、单独提交，再核对上游样式差异。
本轮使用 systematic-debugging、test-driven-development 和独立代码复审；
在本地 master 线性提交，不推送，不读取或修改 docs/research。

## 设计与范围

根因：ZzLog 上游安装规则未分配组件，框架只补了 Runtime 动态库规则。
Development 缺少包配置、公开头和静态库。共享 Development 本来就依赖 Runtime，
不能将缺失运行库与缺失开发文件混为一谈。

选择由框架 CMake 集成层完整管理 ZzLog 安装，关闭嵌入模式的上游安装开关；
复用上游包配置模板和目标 VERSION，不改第三方源码。保持 ZzLog::ZzLog 目标名、
安装目录、许可和文档内容。相比重复补装一套导出规则，这样只有一个规则来源；
相比把所有上游产物塞入 Development，能保留 Runtime／Development 的职责。

- 静态：Development 包含静态归档、头文件、CMake 配置与许可，可独立消费。
- 共享：Runtime 放动态运行库；Development 放导入库／链接名、头文件、CMake 配置与许可。
  两者安装到同一前缀后消费。
- 完整安装继续工作，支持自定义 GNUInstallDirs 和可重定位包。

## 执行步骤

- [x] 扩展 `tests/Platform/fixtures/KissFftSdkInstall/CMakeLists.txt` 与
  `tests/Platform/ZzReleasePackagingSupportContract.cmake`，验证 Development 中
  ZzLog 的公开头、配置、导出、静态库、许可；先运行契约测试确认因缺配置失败。
- [x] 在 `cmake/ZzInstallLogPackage.cmake` 管理 ZzLog 安装，
  `cmake/ZzInstallPackage.cmake` 调用，根 CMake 关闭上游安装规则。
- [x] 扩展 `tests/InstallConsumer/RunInstallConsumer.cmake`：共享安装 Runtime＋Development，
  静态只安装 Development；构建运行消费者和独立 Starter，再验证完整安装消费者。
- [x] 本机共享与静态 Release 构建、契约测试、两种安装消费者通过；核对导出无源码路径泄漏。
- [x] 更新构建／接入文档及上一轮已知限制说明，独立复审后单独提交修复。
- [ ] 对照本机 FluentUIStyle `fb227d1` 的可复用 ExWidgets 和 FluentUI3Style，
  记录已有能力、明确缺口和优先级；不移植播放器、歌词、系统音频捕获。

快速契约命令：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" -DZZ_TEST_ROOT="$PWD/build/sdk-component-contract" \
  -P tests/Platform/ZzReleasePackagingSupportContract.cmake
```

安装验收：对共享和静态测试构建分别执行 `ctest -R '^install.consumer$' --output-on-failure`。
没有改变性能门限；平台结果只按实测报告。

## SDK 验收记录

- 快速契约先因 `ZzLogConfig.cmake` 缺失失败，修复后通过。
- Linux / GCC 15.2 / Qt 6.8.3 / Release：共享 `install.consumer` 215.93 秒、
  静态 216.11 秒，均通过。每次都从全新 producer 开始，检查组件与完整安装消费者、
  组件 SDK 的公开头编译和独立 Starter、导出路径泄漏。
- 验收另发现 Qt 前缀的分号跨函数传递丢失，导致消费者误选系统 Qt 6.11；
  改为 PARSE_ARGV 保留命令参数，并保留参数列表复制时的转义。未放宽 Qt 版本检查。
- 安装规则及参数修复均经独立代码复审；Windows／macOS 只验证脚本契约，未做原生构建。
