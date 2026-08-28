# ZzPureToolsFrame 标识迁移与文档整理实施计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将仓库产品标识统一为 `ZzPureToolsFrame`，取消旧包名兼容，并提供可复制的中文跨平台构建文档和表格化组件目录。

**架构：** 只迁移产品级标识、CMake 包标识和安装路径；保留 `ZzPureTools` 模块、`Zz::` 目标、命名空间和公共 include 前缀。CMake 配置生成、安装消费和重定位测试始终使用同一个新包名。

**技术栈：** CMake 3.23+、CMake Presets、Ninja、Qt 6.8+、C++20、Git、CTest、Markdown。

---

### 任务 1：迁移 CMake 项目与安装包契约

**文件：**
- 修改：`CMakeLists.txt`
- 修改：`cmake/ZzInstallPackage.cmake`
- 修改：`cmake/ZzCompilerCapabilities.cmake`
- 修改：`cmake/ZzVerifyInstalledLicenses.cmake`
- 重命名：当前产品配置模板为 `cmake/ZzPureToolsFrameConfig.cmake.in`
- 修改：`ZzWindowKit/CMakeLists.txt`
- 修改：`tests/InstallConsumer/CMakeLists.txt`
- 修改：`tests/InstallConsumer/RunInstallConsumer.cmake`
- 修改：`tests/PublicHeaderConsumer/CMakeLists.txt`
- 修改：`tests/Platform/ZzPackageRelocationTest.cmake`
- 修改：`tests/Architecture/ZzDocumentationAudit.cmake`

- [ ] **步骤 1：记录迁移前契约并建立失败基线**

```bash
git grep -n -i -E 'ZzPureToolsP[r]o|zzpuretoolspr[o]' -- CMakeLists.txt cmake tests ZzWindowKit
```

预期：命令有输出，证明项目名、配置文件、安装目录、消费者变量和文档审计路径均被覆盖。

- [ ] **步骤 2：重命名 CMake 配置模板**

```bash
legacy_config=$(git ls-files cmake | rg -i 'zzpuretoolspr[o].*Config\\.cmake\\.in$')
test -n "$legacy_config"
git mv "$legacy_config" cmake/ZzPureToolsFrameConfig.cmake.in
```

预期：Git 记录一次重命名，工作树中只保留新配置模板文件名。

- [ ] **步骤 3：更新顶层 CMake 和编译器诊断**

在 `CMakeLists.txt` 中将 `project()` 名称和三个构建选项描述统一为 `ZzPureToolsFrame`；在 `cmake/ZzCompilerCapabilities.cmake` 与 `ZzWindowKit/CMakeLists.txt` 中同步用户可见错误消息。不得修改 `ZzPureTools` target 或命名空间。

- [ ] **步骤 4：更新安装导出和配置生成**

在 `cmake/ZzInstallPackage.cmake` 中使用 `ZzPureToolsFrameTargets`、`${CMAKE_INSTALL_LIBDIR}/cmake/ZzPureToolsFrame`、`ZzPureToolsFrameConfig.cmake` 和 `ZzPureToolsFrameConfigVersion.cmake`，并将许可证、第三方审计和安装文档目录同步改名。不得生成旧目录、旧 alias 或旧变量。

- [ ] **步骤 5：更新配置模板变量**

在 `cmake/ZzPureToolsFrameConfig.cmake.in` 中让版本不匹配分支设置 `ZzPureToolsFrame_FOUND` 与 `ZzPureToolsFrame_NOT_FOUND_MESSAGE`，导入 `ZzPureToolsFrameTargets.cmake`，调用 `check_required_components(ZzPureToolsFrame)`。Qt 版本校验和静态 QWindowKit 私有 target 逻辑保持不变。

- [ ] **步骤 6：更新安装消费者和重定位断言**

在 `tests/InstallConsumer/CMakeLists.txt`、`tests/InstallConsumer/RunInstallConsumer.cmake`、`tests/PublicHeaderConsumer/CMakeLists.txt` 和 `tests/Platform/ZzPackageRelocationTest.cmake` 中，把 `find_package`、`*_DIR`、配置文件 glob、正则断言和错误消息统一到新包名；安装目标仍保持 `Zz::Core`、`Zz::PureTools` 等现有 target。同步 `tests/Architecture/ZzDocumentationAudit.cmake` 的规格文件路径。

- [ ] **步骤 7：运行 CMake 语法和定向构建**

```bash
cmake -S . -B build/name-migration-check -G Ninja \
  -DZZ_BUILD_TESTS=OFF -DZZ_BUILD_EXAMPLES=OFF
cmake --build build/name-migration-check --parallel 2
git diff --check
```

预期：配置、构建和空白检查返回 0；临时构建目录不纳入 Git。

- [ ] **步骤 8：提交 CMake 契约迁移**

```bash
git add CMakeLists.txt cmake ZzWindowKit/CMakeLists.txt tests
git commit -m "refactor(构建): 将包标识迁移为 ZzPureToolsFrame" \
  -m "更新顶层项目名、安装导出、配置模板、许可证路径和安装消费者。" \
  -m "移除旧包名兼容入口，保留 Zz:: 目标与 ZzPureTools 公共 API。" \
  -m "验证：CMake Ninja 配置、增量构建和 git diff --check。"
```

### 任务 2：清理仓库跟踪内容中的历史标识

**文件：**
- 修改：`README.md` 及 `docs/` 下命中的开发、发布、性能、第三方、plans/specs 文件
- 修改：`ZzThirdParty/ZzLog/README.md`
- 修改：`ZzThirdParty/ZzLog/BUILDING-MULTIPLATFORM.md`
- 修改：`scripts/ci/run-linux-gates.sh`
- 修改：`.superpowers/sdd/2026-08-23-workspace-shell-transaction-redesign/progress.md`
- 重命名：架构规格文件为 `docs/superpowers/specs/2026-08-02-zzpuretoolsframe-architecture-design.md`
- 修改：被 Git 跟踪的性能 profile/evidence 日志中的路径文本

- [ ] **步骤 1：生成命中文件清单**

```bash
git grep -Il -i -E 'ZzPureToolsP[r]o|zzpuretoolspr[o]' > /tmp/zzpuretoolsframe-legacy-files.txt
git ls-files | rg -i 'zzpuretoolspr[o]' > /tmp/zzpuretoolsframe-legacy-paths.txt
```

预期：清单覆盖所有跟踪内容；不读取、不修改顶层 `temp_image/`。

- [ ] **步骤 2：执行受控文本迁移**

仅对清单中的文本文件做全量替换：大小写混合产品标识改为 `ZzPureToolsFrame`，全小写路径标识改为 `zzpuretoolsframe`。不得替换 `ZzPureTools` 模块名、命名空间或 include 前缀。

```bash
while IFS= read -r file; do perl -pi -e 's/ZzPureToolsP[r]o/ZzPureToolsFrame/g; s/zzpuretoolspr[o]/zzpuretoolsframe/g' "$file"; done < /tmp/zzpuretoolsframe-legacy-files.txt
```

- [ ] **步骤 3：重命名跟踪路径文件**

对 `/tmp/zzpuretoolsframe-legacy-paths.txt` 中的每个路径执行 `git mv`，将路径片段改为 `zzpuretoolsframe`；架构规格文件使用目标名 `docs/superpowers/specs/2026-08-02-zzpuretoolsframe-architecture-design.md`。大小写文件名无法原地替换时使用临时中间名执行两次 `git mv`。

- [ ] **步骤 4：执行标识清零审计**

```bash
if git grep -n -i -E 'ZzPureToolsP[r]o|zzpuretoolspr[o]'; then exit 1; fi
if git ls-files | rg -i 'zzpuretoolspr[o]'; then exit 1; fi
git diff --check
```

预期：两个检索命令无输出，空白检查返回 0。

- [ ] **步骤 5：提交仓库文本与路径迁移**

```bash
git add -A -- ':!temp_image'
git commit -m "refactor(标识): 清理仓库中的历史产品名称" \
  -m "迁移开发资料、CI 脚本、性能证据、第三方通知和历史规格中的产品路径。" \
  -m "同步重命名受跟踪的配置与规格文件，仓库检索不再命中旧标识。" \
  -m "验证：git grep、git ls-files 路径审计和 git diff --check。"
```

### 任务 3：完善中文跨平台构建手册

**文件：**
- 修改：`docs/development/BUILDING_ZH.md`
- 参考：`CMakePresets.json`
- 参考：`CMakeUserPresets.json.example`
- 参考：`docs/development/PLATFORM_SUPPORT_ZH.md`
- 参考：`docs/development/GITHUB_ACTIONS_ZH.md`

- [ ] **步骤 1：核对 preset 和环境变量**

```bash
cmake --list-presets
rg -n '"name"|QT_ROOT|GCC_13|CLANG_17|QT_MSVC_ROOT|QT_MINGW|QT_MACOS|APPLE_CLANG' CMakePresets.json CMakeUserPresets.json.example
```

预期：手册只引用实际存在的 preset 和变量。

- [ ] **步骤 2：重写构建手册结构**

补齐通用前置条件、Linux GCC shared/static/LTO、Linux Clang/clang-tidy/ASan/UBSan、Windows MSVC 2022、Windows Qt MinGW、macOS Apple Clang arm64/x86_64、安装消费与包重定位、发布验证和常见故障排查。每个平台给出配置、构建、测试、安装命令，并标注只能静态审查的结果。

- [ ] **步骤 3：校验手册**

```bash
rg -n -i -E 'ZzPureToolsP[r]o|zzpuretoolspr[o]' docs/development/BUILDING_ZH.md
git diff --check -- docs/development/BUILDING_ZH.md
```

预期：旧标识无输出，文档链接目标均存在。

- [ ] **步骤 4：提交构建手册**

```bash
git add docs/development/BUILDING_ZH.md
git commit -m "docs(构建): 完善 ZzPureToolsFrame 跨平台手册" \
  -m "补充 Linux、Windows MSVC/MinGW 和 macOS 的 preset、环境变量、测试与安装命令。" \
  -m "区分本机验证、静态平台审查和发布门禁，记录常见 Qt 与工具链故障。"
```

### 任务 4：优化 README 组件目录和入口

**文件：**
- 修改：`README.md`
- 参考：`ZzFluentUI/widgets/include/ZzFluentUI/`
- 参考：`examples/ZzPureToolsExample/main.cpp`
- 参考：`docs/development/BUILDING_ZH.md`

- [ ] **步骤 1：核对公开组件清单**

```bash
find ZzFluentUI/widgets/include/ZzFluentUI -maxdepth 1 -name 'Zz*.h' -printf '%f\n' | sort
```

预期：表格覆盖当前公开组件，不列出 private 头或重复包装类。

- [ ] **步骤 2：重排 README**

将核心模块和 Fluent 组件改为 Markdown 表格，列出职责、CMake 目标或示例入口；保留标准 Qt Widgets 覆盖说明、Linux 快速开始、平台状态、开发资料和 MIT 许可。所有产品名称使用 `ZzPureToolsFrame`，链接使用仓库内相对路径。

- [ ] **步骤 3：执行 README 格式检查**

```bash
rg -n -i -E 'ZzPureToolsP[r]o|zzpuretoolspr[o]' README.md
git diff --check -- README.md
```

预期：旧标识无输出，README 相对链接目标均存在。

- [ ] **步骤 4：提交 README**

```bash
git add README.md
git commit -m "docs(README): 以表格整理组件与构建入口" \
  -m "按职责展示核心模块和 Fluent 公开组件，补充示例入口与平台文档导航。" \
  -m "统一产品标识、中文排版和可验证能力边界。"
```

### 任务 5：全量验证与交付记录

**文件：**
- 验证：`CMakeLists.txt`、`CMakePresets.json`、`tests/`
- 不提交：`build/`、`install/`、`temp_image/`、临时审计清单

- [ ] **步骤 1：清洁 Linux Debug 构建和测试**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug --output-on-failure
```

预期：配置、构建和 CTest 通过；若本机 Qt 私有头或平台依赖缺失，记录实际错误。

- [ ] **步骤 2：验证安装包和消费者**

```bash
cmake --install build/linux-gcc-debug --prefix install/linux-gcc-debug
ctest --test-dir build/linux-gcc-debug -R '^platform\\.package-relocation$|^zz\\.(install-consumer|fluent-install-consumer)\\.run$' --output-on-failure
find install/linux-gcc-debug -path '*cmake/ZzPureToolsFrame*' -print
```

预期：安装目录只包含新包路径；安装消费者通过 `find_package(ZzPureToolsFrame CONFIG REQUIRED)` 构建运行。

- [ ] **步骤 3：运行架构、文档和静态平台检查**

```bash
ctest --preset linux-gcc-debug --output-on-failure -R '^architecture\\.(complete-audit|documentation-audit)$'
git grep -n -i -E 'ZzPureToolsP[r]o|zzpuretoolspr[o]' || true
git ls-files | rg -i 'zzpuretoolspr[o]' || true
git status --short
```

预期：标识检索无输出；工作树只允许用户已有的 `temp_image/` 未跟踪目录；Windows、macOS 仅报告静态审查结果。

- [ ] **步骤 4：提交必要修正**

若验证发现文档链接、安装路径或测试断言遗漏，只修改对应任务文件，使用新的中文 `fix(...)` 提交，并在正文列出失败原因、修正内容和重新运行的命令。不得通过保留旧 alias 绕过失败。

