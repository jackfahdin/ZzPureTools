# ZzPureToolsFrame 标识迁移与文档整理设计

## 目标

将项目产品标识一次性迁移为 `ZzPureToolsFrame`，并完善中文 README 与跨平台构建手册。迁移完成后，受版本控制的源码、CMake 配置、安装包元数据、测试消费者、脚本、CI 和文档中不得再出现旧产品标识的大小写变体，也不提供旧包名兼容入口。

## 范围与边界

### 纳入迁移

- 顶层 CMake 项目名、选项描述和错误消息。
- CMake package/export/config/version 文件名、变量名、安装目录和 `find_package` 调用。
- 安装许可证、第三方通知、发布证据和检查脚本中的产品路径。
- 测试/示例中的安装消费者、包重定位检查和路径断言。
- GitHub Actions、CI 辅助脚本、性能 profile/evidence 中的项目标识。
- README、开发/发布文档、历史 plans/specs 中的产品名称和路径文本。
- 受版本控制的文件名：配置模板、导出文件及同类低写文件名统一改为
  `ZzPureToolsFrame` 前缀。

### 明确保留

以下名称不是产品标识，不做无关改动：

- 模块目录和目标：`ZzPureTools`、`Zz::PureTools`、`Zz::AppCore`。
- 公共命名空间：`ZzPureTools`。
- 公共 include 前缀：`ZzPureTools/...`。
- 已发布组件类名和第三方库名（如 `ZzLog`、`QWindowKit`）。

### 物理 checkout 路径

当前本机 checkout 路径包含旧产品标识，属于工作区元数据，不参与 Git 提交内容。为避免中途破坏已有 worktree 和构建缓存，实施阶段先完成仓库内容清零；所有提交完成并验证后，再由维护者选择是否把外层目录迁移为 `ZzPureToolsFrame`。迁移路径时应重新打开终端并删除/重建构建目录，不能把旧绝对路径写回仓库。

## 命名映射

| 旧标识 | 新标识 | 使用位置 |
|---|---|---|
| 旧产品标识 | `ZzPureToolsFrame` | 产品名、CMake package 名、安装数据目录、文档标题 |
| 旧产品标识低写变体 | `zzpuretoolsframe` | 低写文件名、路径片段和脚本匹配 |
| 旧产品配置模板文件 | `ZzPureToolsFrameConfig.cmake.in` | CMake 配置模板 |
| 旧产品导出文件 | `ZzPureToolsFrameTargets.cmake` | 安装导出文件 |
| 旧产品包目录变量 | `ZzPureToolsFrame_DIR` | 包消费者和重定位检查 |
| 旧产品 `find_package` 调用 | `find_package(ZzPureToolsFrame ...)` | 安装消费测试、文档示例 |

不得建立旧名称的 alias、转发配置、兼容变量或兼容安装目录。配置文件中的 `*_FOUND`、`*_NOT_FOUND_MESSAGE` 和 `check_required_components()` 必须使用新包名。

## README 整理

README 保持“先能运行、再查细节”的结构：

1. 项目定位、Qt/C++ 标准和平台状态。
2. 核心模块表格：`ZzCore`、`ZzWindowKit`、`ZzFluentUI`、`ZzPureTools`，列出职责和 CMake 目标。
3. `ZzFluentUI` 公开组件表格，按基础布局、输入选择、导航内容、反馈表面分类；每行包含组件、用途和相关示例。
4. 标准 Qt Widgets 覆盖范围与模型/无障碍语义说明。
5. Linux 快速开始和示例入口。
6. 指向详细构建手册、平台支持状态、人工验收、编码规范和第三方通知的链接。
7. MIT 许可证和版权所有者 Jackfahdin。

README 只描述当前可验证的能力，不承诺未在同一提交通过的托管 CI 矩阵；平台差异和发布门禁放入构建手册。

## 跨平台构建手册

在 `docs/development/BUILDING_ZH.md` 中统一说明：

- 通用前置条件：CMake、Ninja、Qt 6.8+ 模块、C++20 编译器和环境变量约定。
- Linux GCC：Debug/Release/shared/static/LTO，示例、测试、安装消费和常见 Qt 私有头问题。
- Linux Clang：Clang 17+ 配置、clang-tidy、ASan/UBSan 和与 GCC 的差异。
- Windows MSVC 2022：开发者命令行、Qt 路径、shared/static 预设、跳过测试方式和 UTF-8 编译要求。
- Windows Qt MinGW：Qt 官方 MinGW 工具链检查、生成器、运行时 DLL 部署和静态检查边界。
- macOS Apple Clang：arm64/x86_64 预设、部署目标、shared/static 构建和架构检查。
- 发布验证：`cmake --install`、安装消费者、包重定位、许可证和二进制依赖检查。
- 故障排查：Qt 版本不匹配、`GuiPrivate`/平台依赖缺失、Windows 编码、macOS 标准库能力和缓存污染。

每个平台都给出可复制的命令块，并明确命令是在仓库根目录执行；文档中的包名统一为 `ZzPureToolsFrame`。

## 验证策略

### 静态标识审计

```bash
git grep -n -i -F '旧产品标识'
```

命令必须没有输出；对文件名使用 `git ls-files | rg -i '旧产品标识的实际字面量'` 检查。

### Linux 功能验证

在清洁构建目录中执行：

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug --output-on-failure
```

再执行安装和消费者检查，确认生成的目录为 `lib/cmake/ZzPureToolsFrame`，消费者只能通过 `find_package(ZzPureToolsFrame CONFIG REQUIRED)` 找到包。

### 静态平台审查

Windows MSVC、Windows Qt MinGW 和 macOS 的 CMake Preset、脚本和文档使用 `git grep`、CMake 语法检查和路径审计进行静态确认；未在 Linux 主机上伪造这些平台的编译结果。

### 文档链接与格式

- README 中的相对链接全部指向现存文件。
- `git diff --check` 无空白错误。
- 组件表格行数与当前公开组件清单一致。

## 提交拆分

按独立可审查交付物提交，每次提交使用中文标题并在正文描述影响范围和验证命令：

1. `重构：将项目标识迁移为 ZzPureToolsFrame`：代码、CMake、脚本、测试、文件名和历史资料中的标识迁移。
2. `文档：完善 ZzPureToolsFrame 跨平台构建手册`：更新/补充 `BUILDING_ZH.md`。
3. `文档：优化 README 组件目录与项目入口`：README 结构、组件表格和链接导航。

每次提交只暂存本任务文件，不包含 `build/`、临时报告和顶层 `temp_image/`。
