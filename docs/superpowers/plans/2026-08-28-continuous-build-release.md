# ZzPureToolsExample 持续构建与跨平台发布实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 将 `ZzPureToolsExample` 以一个 Linux AppImage、两个 Windows ZIP 和两个 macOS DMG 自动发布到固定的 `continuous-build` GitHub Pre-release，同时把远端 Linux CI 收敛为唯一 Release/shared/LTO 配置。

**架构：** 一个 workflow 并行执行五条原生平台构建通道，每条通道从同一 Qt SDK 构建、测试、部署、冒烟和生成结构化产物；唯一 publish job 在校验五组 artifact 属于同一提交后更新固定 Release。平台打包逻辑放在受测脚本中，YAML 只负责编排和最小权限授权。

**技术栈：** CMake 3.23+、CMake Presets、Qt 6.8.3、C++20、GCC 13、MSVC 2022、Qt MinGW、Apple Clang、linuxdeploy/AppImage、windeployqt、macdeployqt、GitHub Actions、GitHub CLI、Bash、PowerShell、CTest。

---

## 文件结构

### 应用身份与安装

- 修改：`examples/ZzPureToolsExample/CMakeLists.txt`，声明 GUI/bundle 属性、版本、安装目标和平台图标。
- 修改：`examples/ZzPureToolsExample/main.cpp`，从构建版本定义设置应用版本和统一窗口图标。
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.png`，由旧项目中 Jackfahdin 拥有的 `APPICON.png` 导入的新项目受控主图标。
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.ico`，Windows 多尺寸图标。
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.icns`，macOS 应用图标。
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.rc.in`，Windows PE 图标资源模板。
- 创建：`packaging/linux/io.github.jackfahdin.ZzPureToolsExample.desktop.in`，Linux desktop entry。
- 创建：`packaging/linux/io.github.jackfahdin.ZzPureToolsExample.appdata.xml.in`，Linux AppStream 元数据。
- 创建：`packaging/assets/README.md`，记录图标所有权、导入来源和可替换边界。

### 公共打包基础设施

- 创建：`scripts/package/PrepareReleaseEvidence.cmake`，下载并校验 manifest 指定的公开外部证据。
- 创建：`scripts/package/StageRuntimeLicenses.cmake`，把项目、第三方、Qt 和 GNU runtime 许可证放入部署根。
- 创建：`scripts/package/WriteBuildInfo.cmake`，生成规范 `build-info.json` 与包 SHA-256。
- 创建：`scripts/package/VerifyArtifactSet.cmake`，验证五个平台 artifact 的文件数、命名、摘要和 commit 一致性。
- 创建：`tests/Platform/ZzReleasePackagingSupportContract.cmake`，对上述公共脚本做确定性正反例测试。

### 平台打包

- 创建：`scripts/package/package-linux-appimage.sh`，组装、部署、审计并生成 AppImage。
- 创建：`scripts/package/package-windows.ps1`，以 MSVC/MinGW 参数生成部署 ZIP。
- 创建：`scripts/package/package-macos.sh`，以目标架构参数生成部署 DMG。
- 创建：`tests/Platform/ZzPlatformPackagingScriptContract.cmake`，静态检查三个脚本的输入边界、部署工具、smoke 和审计调用。

### Continuous Release

- 创建：`scripts/release/publish-continuous-build.sh`，校验 artifact 并事务式更新固定 Release。
- 创建：`tests/Platform/ZzContinuousPublishTest.sh`，使用 fake `gh` 验证首次创建、覆盖更新和失败保留旧资产。
- 创建：`tests/Platform/ZzContinuousReleaseContract.cmake`，验证发布脚本、workflow 权限、依赖和产物集合。

### CI、Preset 与文档

- 修改：`CMakePresets.json`，增加/收敛 continuous Release/shared/LTO preset；保留本地专项 preset。
- 修改：`.github/workflows/ci.yml`，把远端矩阵改为五条发布配置和一个集中 publish job。
- 修改：`tests/Platform/CMakeLists.txt`，注册新增合同与发布脚本测试。
- 修改：`tests/Platform/PresetMatrixContract.cmake`，区分“本地保留 preset”和“远端唯一发布 preset”。
- 修改：`tests/Platform/ZzGitHubActionsContract.cmake`，从旧矩阵合同改为 continuous build 合同。
- 修改：`README.md`，增加 continuous build 下载入口与未签名提示。
- 修改：`docs/development/BUILDING_ZH.md`，增加五个平台本地复现命令。
- 修改：`docs/development/GITHUB_ACTIONS_ZH.md`，记录权限、artifact 和 Release 更新事务。
- 修改：`docs/development/PLATFORM_SUPPORT_ZH.md`，区分预发布 smoke 与真机验收。

## 任务 1：建立 Example 的可部署应用身份

**文件：**
- 创建：`tests/Platform/ZzExampleApplicationMetadataContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`
- 修改：`examples/ZzPureToolsExample/CMakeLists.txt`
- 修改：`examples/ZzPureToolsExample/main.cpp`
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.png`
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.ico`
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.icns`
- 创建：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.rc.in`
- 创建：`packaging/linux/io.github.jackfahdin.ZzPureToolsExample.desktop.in`
- 创建：`packaging/linux/io.github.jackfahdin.ZzPureToolsExample.appdata.xml.in`
- 创建：`packaging/assets/README.md`

- [ ] **步骤 1：编写失败的应用元数据合同**

合同读取 Example CMake、`main.cpp`、desktop/appdata 模板和资源文件，精确要求：

```cmake
set(required_cmake_tokens
    "MACOSX_BUNDLE TRUE"
    "WIN32_EXECUTABLE TRUE"
    "MACOSX_BUNDLE_GUI_IDENTIFIER io.github.jackfahdin.ZzPureToolsExample"
    "ZZ_EXAMPLE_VERSION=\"${PROJECT_VERSION}\""
    "install(TARGETS ZzPureToolsExample")
set(required_identity_tokens
    "Jackfahdin"
    "ZzPureToolsExample"
    "io.github.jackfahdin.ZzPureToolsExample")
```

同时用 `file(SIZE)` 要求 PNG/ICO/ICNS 非空，并拒绝模板中的绝对主机路径和旧产品标识。把测试注册为 `platform.example-application-metadata-contract`。

- [ ] **步骤 2：运行合同并确认红灯**

运行：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzExampleApplicationMetadataContract.cmake
```

预期：FAIL，首先报告 desktop 模板或平台图标缺失。

- [ ] **步骤 3：导入并记录项目图标**

只读取旧项目的：

```text
/home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsExample/Resource/Image/APPICON.png
```

将其复制到新项目 application 资源目录，生成包含 16/32/48/64/128/256
尺寸的 ICO 和标准 iconset 生成的 ICNS。`packaging/assets/README.md` 明确记录资源所有者
为 Jackfahdin、原始文件 SHA-256、导入日期和以后替换三个平台变体的命令。不得修改
旧项目源文件。

- [ ] **步骤 4：实现平台元数据与安装目标**

在 Example CMake 中使用以下固定语义：

```cmake
set_target_properties(ZzPureToolsExample PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER
        io.github.jackfahdin.ZzPureToolsExample
    MACOSX_BUNDLE_BUNDLE_NAME ZzPureToolsExample
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
target_compile_definitions(ZzPureToolsExample PRIVATE
    ZZ_EXAMPLE_VERSION="${PROJECT_VERSION}")
install(TARGETS ZzPureToolsExample
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT ExampleRuntime
    BUNDLE DESTINATION . COMPONENT ExampleRuntime)
```

Windows 配置生成 `.rc` 后加入 target sources；macOS 给 ICNS 设置
`MACOSX_PACKAGE_LOCATION Resources`；Linux 配置并安装 desktop/appdata/PNG。把 PNG
加入现有 Qt resource，`main.cpp` 使用 `ZZ_EXAMPLE_VERSION` 和资源 `QIcon`，不再写死
`1.0.0`。

- [ ] **步骤 5：验证应用元数据和 Linux 构建**

运行：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzExampleApplicationMetadataContract.cmake
cmake --preset linux-gcc-release-lto -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-release-lto --parallel 2 \
  --target ZzPureToolsExample
ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1000 \
QT_QPA_PLATFORM=offscreen \
  build/linux-gcc-release-lto/examples/ZzPureToolsExample/ZzPureToolsExample \
  --smoke-test
```

预期：合同 PASS、构建成功、smoke 返回 0。

- [ ] **步骤 6：提交应用身份**

```bash
git add examples/ZzPureToolsExample packaging/assets packaging/linux \
  tests/Platform/CMakeLists.txt \
  tests/Platform/ZzExampleApplicationMetadataContract.cmake
git commit -m "feat(示例): 建立跨平台部署应用身份" \
  -m "增加 Example 安装规则、应用版本、Linux desktop 元数据和三平台图标。" \
  -m "记录 Jackfahdin 图标来源，不修改或提交外层旧项目内容。" \
  -m "验证：应用元数据合同、Linux LTO 构建与 offscreen smoke。"
```

## 任务 2：实现发布证据、许可证和构建身份公共脚本

**文件：**
- 创建：`scripts/package/PrepareReleaseEvidence.cmake`
- 创建：`scripts/package/StageRuntimeLicenses.cmake`
- 创建：`scripts/package/WriteBuildInfo.cmake`
- 创建：`scripts/package/VerifyArtifactSet.cmake`
- 创建：`tests/Platform/ZzReleasePackagingSupportContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`

- [ ] **步骤 1：编写公共脚本正反例合同**

合同在 `${ZZ_TEST_ROOT}` 下创建五个小型假包和 build-info fixture，覆盖：

```cmake
# 正例：五个平台、同一 40 位 commit、摘要匹配。
execute_process(COMMAND "${CMAKE_COMMAND}"
    -DZZ_ARTIFACT_ROOT=${valid_root}
    -DZZ_EXPECTED_COMMIT=0123456789abcdef0123456789abcdef01234567
    -P ${ZZ_SOURCE_DIR}/scripts/package/VerifyArtifactSet.cmake
    RESULT_VARIABLE valid_result)
if(NOT valid_result EQUAL 0)
    message(FATAL_ERROR "Valid continuous artifact set was rejected")
endif()

# 反例分别修改 commit、删掉 macOS x86_64 包、篡改一个包字节；三次都必须失败。
```

另用临时 Qt `LICENSES` fixture 验证 `StageRuntimeLicenses.cmake`：完整输入产生
`licenses/Qt`，缺少 Qt LGPL 正文时失败。`WriteBuildInfo.cmake` 输出必须能被 CMake
`string(JSON)` 读取且包摘要与 `file(SHA256)` 一致。

- [ ] **步骤 2：运行合同并确认红灯**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -DZZ_TEST_ROOT="$PWD/build/continuous-support-contract" \
  -P tests/Platform/ZzReleasePackagingSupportContract.cmake
```

预期：FAIL，报告 `scripts/package/VerifyArtifactSet.cmake` 不存在。

- [ ] **步骤 3：实现公开证据准备脚本**

`PrepareReleaseEvidence.cmake` 接收唯一输出目录，使用 `file(DOWNLOAD ...
EXPECTED_HASH SHA256=...)` 创建 manifest 已声明的三个 external 文件：

```text
qwindowkit/qwindowkit-2813c1f810cb3fb1999a14ad524124562081f2c2.tar.gz
qt-5.15.2/qttools-src-shared-winutils-utils.cpp
qt-5.15.2/LICENSE.GPL3-EXCEPT
```

URL 固定到 QWindowKit commit 和 Qt 5.15.2 tag，摘要直接读取
`docs/third-party/release-evidence.json`，不在脚本中维护第二套 hash。输出目录必须为空或
由调用方显式创建；拒绝仓库根和源码目录。

- [ ] **步骤 4：实现许可证暂存脚本**

`StageRuntimeLicenses.cmake` 接收 `ZZ_STAGE_ROOT`、`ZZ_QT_ROOT`、可选
`ZZ_GNU_RUNTIME_LICENSE_DIR`，创建：

```text
licenses/ZzPureToolsFrame/
licenses/Qt/
licenses/GNU-runtime/              # 仅 Linux
THIRD_PARTY_NOTICES.md
```

要求项目 MIT、现有第三方通知、Qt SDK 中 LGPL/GPL 正文和实际部署模块信息非空；Linux
额外要求 `COPYING3`、`COPYING.RUNTIME`。脚本拒绝符号链接越过输入根目录。

- [ ] **步骤 5：实现 build info 与 artifact 集合验证**

`WriteBuildInfo.cmake` 接收包路径和下列非空字段：

```text
commit, builtAtUtc, runnerOs, architecture, qtVersion,
compilerId, compilerVersion, preset, linkage, lto
```

输出 JSON 和相邻 `<package>.sha256`；`dirty` 在 CI 固定为 false，本机调用必须显式
传入。`VerifyArtifactSet.cmake` 精确接受五种平台 id，要求每种一个 package、一个
checksum、一个 build-info，校验文件名 short SHA 与完整 commit 对应。

- [ ] **步骤 6：运行公共脚本合同**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -DZZ_TEST_ROOT="$PWD/build/continuous-support-contract" \
  -P tests/Platform/ZzReleasePackagingSupportContract.cmake
git diff --check
```

预期：正例 PASS，三类篡改反例由合同确认失败，整体命令返回 0。

- [ ] **步骤 7：提交公共打包基础设施**

```bash
git add scripts/package tests/Platform
git commit -m "feat(发布): 增加产物身份与许可证基础设施" \
  -m "实现公开证据下载、运行时许可证暂存、build-info 生成和五平台 artifact 一致性校验。" \
  -m "通过本地 fixture 覆盖缺包、跨提交和摘要篡改失败路径。"
```

## 任务 3：实现 Ubuntu 22.04 AppImage 打包

**文件：**
- 创建：`scripts/package/package-linux-appimage.sh`
- 创建：`tests/Platform/ZzLinuxAppImagePackagingContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`
- 修改：`CMakePresets.json`

- [ ] **步骤 1：编写失败的 Linux 打包合同**

合同要求脚本包含并按顺序执行：输入目录 realpath 校验、`cmake --install`、
linuxdeploy Qt 插件、GNU runtime 复制、AppImage 提取、
`check-ubuntu2204-runtime.sh`、Xvfb smoke、许可证暂存和 build-info。禁止 `rm -rf`
作用于参数原值、禁止读取 `temp_image`、禁止下载浮动 `continuous` 部署工具。

Preset 合同要求 `linux-continuous-release`：

```text
CMAKE_BUILD_TYPE=Release
BUILD_SHARED_LIBS=true
ZZ_ENABLE_LTO=true
ZZ_BUILD_TESTS=true
ZZ_BUILD_EXAMPLES=true
ZZ_RELEASE_BUILD=true
ZZ_BUNDLE_GNU_RUNTIME=true
```

- [ ] **步骤 2：运行合同并确认红灯**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzLinuxAppImagePackagingContract.cmake
```

预期：FAIL，报告 Linux package 脚本或 preset 缺失。

- [ ] **步骤 3：实现 Linux continuous preset**

新增 configure/build/test 三类同名 preset。它继承 GCC base，但只用于发布应用；本地
Debug/static/Clang/benchmark preset 原样保留，不再由 GitHub 日常 workflow 调用。

- [ ] **步骤 4：实现 AppImage 脚本**

脚本参数固定为：

```text
--build-dir --qt-root --evidence-root --gnu-license-dir
--linuxdeploy --qt-plugin --appimagetool --output-dir
--commit --built-at-utc
```

部署工具路径必须是存在的可执行文件；CI 在下载步骤使用固定 release URL 和硬编码
SHA-256 验证后传入，脚本自身不访问网络。脚本组装 AppDir，安装 Runtime 与
ExampleRuntime，传入 desktop/PNG，明确 `QMAKE`，把项目和 GNU runtime 设置为
`$ORIGIN` 可解析布局，不复制 glibc。

生成 AppImage 后使用 `--appimage-extract` 得到新目录，运行：

```bash
scripts/ci/check-ubuntu2204-runtime.sh squashfs-root/usr \
  "$qt_root"
xvfb-run -a env APPIMAGE_EXTRACT_AND_RUN=1 \
  ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1500 \
  "$package" --smoke-test
```

许可证必须在生成最终 AppImage 前进入 AppDir。AppImage 生成后再调用 build-info
脚本，在包外生成相邻的 SHA-256 与 `build-info.json`，避免包摘要递归引用自身。

- [ ] **步骤 5：本机合同与可用工具条件验证**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzLinuxAppImagePackagingContract.cmake
cmake --preset linux-continuous-release \
  -DZZ_RELEASE_EVIDENCE_ROOT="$ZZ_RELEASE_EVIDENCE_ROOT" \
  -DZZ_GNU_RUNTIME_LICENSE_DIR="$ZZ_GNU_RUNTIME_LICENSE_DIR"
cmake --build --preset linux-continuous-release --parallel 2
ctest --preset linux-continuous-release --output-on-failure
```

若本机没有固定 AppImage 工具，只完成合同、构建和 CTest；真实 AppImage 必须在任务 7
的 Ubuntu 22.04 runner 生成并通过最终入口 smoke 后才能声明通过。

- [ ] **步骤 6：提交 Linux AppImage 支持**

```bash
git add CMakePresets.json scripts/package/package-linux-appimage.sh \
  tests/Platform
git commit -m "feat(发布): 增加 Ubuntu 22.04 AppImage 打包" \
  -m "新增唯一 Linux continuous Release/shared/LTO preset 和失败关闭的 AppImage 部署审计。" \
  -m "保留本地专项 preset，不再把它们定义为远端发布矩阵。"
```

## 任务 4：实现 Windows MSVC 与 MinGW 部署 ZIP

**文件：**
- 创建：`scripts/package/package-windows.ps1`
- 创建：`tests/Platform/ZzWindowsPackagingContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`
- 修改：`CMakePresets.json`

- [ ] **步骤 1：编写失败的 Windows 脚本合同**

合同要求 PowerShell 使用 `[ValidateSet('msvc','mingw')]`，精确验证 Qt prefix、目标
EXE、`windeployqt.exe`、`dumpbin`/`objdump`；必须从部署目录运行 smoke，再生成 ZIP。
MSVC 模式拒绝 `libgcc_s`/`libstdc++`，MinGW 模式拒绝 `vcruntime`/`msvcp` 混入。

同时要求 `windows-msvc2022-continuous` 和 `windows-mingw-continuous` 都是
Release/shared/LTO、启用 tests/examples/release，且不继承 static 配置。

- [ ] **步骤 2：运行合同并确认红灯**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzWindowsPackagingContract.cmake
```

预期：FAIL，报告 Windows package 脚本缺失。

- [ ] **步骤 3：实现两个 Windows continuous preset**

MSVC preset 保留 Visual Studio 2022 x64；MinGW preset 保留 Qt 官方 Ninja kit。两者
设置 shared/LTO/tests/examples/release。MSVC 日常发布 preset 不启用 `/analyze`；
`/analyze` 保留在本地质量 preset，避免生成用户产物时重复做昂贵静态分析。

- [ ] **步骤 4：实现参数化 PowerShell 打包**

脚本输入：

```powershell
param(
  [ValidateSet('msvc','mingw')][string]$Mode,
  [string]$BuildDir, [string]$QtRoot, [string]$EvidenceRoot,
  [string]$OutputDir, [string]$Commit, [string]$BuiltAtUtc,
  [string]$DumpBin, [string]$ObjDump
)
```

脚本安装 ExampleRuntime/Runtime 到新 staging，调用同一 Qt 的
`windeployqt --release`；MSVC 添加 compiler runtime，MinGW 验证 GCC runtime。
运行 `ZzPureToolsExample.exe --smoke-test` 时设置 auto-close 和 `QT_QPA_PLATFORM=offscreen`。
随后检查依赖并在 staging 内暂存许可证，再用 `Compress-Archive` 生成唯一 ZIP；最后在
ZIP 外生成相邻的 SHA-256 与 `build-info.json`。

- [ ] **步骤 5：运行静态合同**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzWindowsPackagingContract.cmake
git diff --check
```

预期：PASS。真实 ZIP 和 PE/ABI 验证留给任务 7 两个 Windows runner。

- [ ] **步骤 6：提交 Windows 部署支持**

```bash
git add CMakePresets.json scripts/package/package-windows.ps1 \
  tests/Platform
git commit -m "feat(发布): 增加 Windows 双工具链部署包" \
  -m "为 MSVC 2022 与 Qt MinGW 提供 Release/shared/LTO ZIP、部署后 smoke 和 ABI 隔离检查。"
```

## 任务 5：实现 macOS 双架构 DMG

**文件：**
- 创建：`scripts/package/package-macos.sh`
- 创建：`tests/Platform/ZzMacosPackagingContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`
- 修改：`CMakePresets.json`

- [ ] **步骤 1：编写失败的 macOS 脚本合同**

合同要求架构只接受 `arm64|x86_64`，使用 `macdeployqt`、`lipo`、`otool`、`hdiutil`，
从部署 `.app` 和挂载 DMG 各执行一次 smoke。要求所有一方 Mach-O 精确为一个目标
架构，拒绝绝对 build RPATH。

- [ ] **步骤 2：运行合同并确认红灯**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzMacosPackagingContract.cmake
```

预期：FAIL，报告 macOS package 脚本缺失。

- [ ] **步骤 3：收敛两个 macOS continuous preset**

新增 `macos-continuous-arm64` 与 `macos-continuous-x86_64`，继承各自 Qt prefix 和架构，
使用 Release/shared/LTO/tests/examples/release，但关闭日常发布的 clang-tidy；现有 tidy
能力继续由本地显式 target 使用。

- [ ] **步骤 4：实现 DMG 打包脚本**

脚本参数固定为 build/Qt/evidence/output/commit/time/architecture。先安装 `.app`，调用：

```bash
"$qt_root/bin/macdeployqt" "$app" -always-overwrite
```

再检查主程序、一方 dylib 和 Qt framework 的架构与 `@rpath`/`@executable_path`。从
`.app/Contents/MacOS/ZzPureToolsExample` 执行 offscreen smoke，把许可证暂存到
`Contents/Resources`，最后用 `hdiutil` 生成 DMG、只读挂载并从挂载点再次 smoke。
DMG 通过后在包外生成相邻的 SHA-256 与 `build-info.json`。所有退出路径都卸载自己
创建的挂载点。

- [ ] **步骤 5：运行静态合同**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzMacosPackagingContract.cmake
git diff --check
```

预期：PASS。真实 DMG、lipo 和挂载 smoke 留给任务 7 原生 runners。

- [ ] **步骤 6：提交 macOS 部署支持**

```bash
git add CMakePresets.json scripts/package/package-macos.sh \
  tests/Platform
git commit -m "feat(发布): 增加 macOS 双架构 DMG" \
  -m "为 arm64 与 x86_64 提供未签名 continuous DMG、架构审计和挂载后 smoke。"
```

## 任务 6：实现 continuous-build Release 事务

**文件：**
- 创建：`scripts/release/publish-continuous-build.sh`
- 创建：`tests/Platform/ZzContinuousPublishTest.sh`
- 创建：`tests/Platform/ZzContinuousReleaseContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`

- [ ] **步骤 1：编写 fake-gh 失败/恢复测试**

测试在临时 `PATH` 前放置 fake `gh`，记录每个 `release view/create/upload/edit/delete-asset`
调用。覆盖三个场景：

1. 没有旧 Release：上传五组资产后创建/更新 Pre-release。
2. 存在旧 Release：全部新资产上传并验证后才删除旧资产。
3. 缺少一个 DMG 或 fake upload 返回非零：不得移动 tag、编辑正文或删除旧资产。

关键断言：

```bash
first_delete=$(grep -n 'delete-asset' "$log" | head -1 | cut -d: -f1)
fifth_upload=$(grep -n 'release upload' "$log" | tail -1 | cut -d: -f1)
[[ -z "$first_delete" || "$first_delete" -gt "$fifth_upload" ]]
```

- [ ] **步骤 2：运行测试并确认红灯**

```bash
bash tests/Platform/ZzContinuousPublishTest.sh \
  "$PWD" "$PWD/build/continuous-publish-test"
```

预期：FAIL，报告 publish 脚本不存在。

- [ ] **步骤 3：实现发布脚本**

脚本输入 artifact root、repository、完整 commit、run URL。先调用
`VerifyArtifactSet.cmake`，生成包含五个平台、SHA-256、Qt/编译器和未签名提示的中文
正文。仅通过 `gh` 操作固定 `continuous-build`：上传本轮唯一文件名、API 验证后移动
tag/更新 Pre-release，最后删除非本轮资产。不得删除 Release 本身。

首次发布时先创建指向 commit 的空 Pre-release，再上传；若上传失败则删除这次首次创建
且没有完整资产的 Release。已存在 Release 的失败路径始终保留旧完整资产。

- [ ] **步骤 4：运行发布事务测试和静态合同**

```bash
bash tests/Platform/ZzContinuousPublishTest.sh \
  "$PWD" "$PWD/build/continuous-publish-test"
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzContinuousReleaseContract.cmake
```

预期：三个 fake-gh 场景 PASS；缺包失败发生在任何远端写操作之前。

- [ ] **步骤 5：提交 Release 事务**

```bash
git add scripts/release tests/Platform
git commit -m "feat(发布): 实现 continuous-build 更新事务" \
  -m "在五平台同提交和摘要校验后更新固定 Pre-release，并保证失败时保留上一轮完整资产。"
```

## 任务 7：重写 GitHub Actions 为发布即验证的精简矩阵

**文件：**
- 修改：`.github/workflows/ci.yml`
- 修改：`tests/Platform/ZzGitHubActionsContract.cmake`
- 修改：`tests/Platform/PresetMatrixContract.cmake`
- 修改：`tests/Platform/ZzGateScriptContract.cmake`
- 修改：`tests/Platform/ZzContinuousReleaseContract.cmake`

- [ ] **步骤 1：先更新 workflow 合同并确认旧 YAML 失败**

新合同精确要求：

```text
ubuntu-22.04
linux-continuous-release（只出现于一个 Linux configure 路径）
windows-msvc2022-continuous
windows-mingw-continuous
macos-continuous-arm64
macos-continuous-x86_64
publish-continuous-build
contents: write（只在 publish job）
continuous-build
actions/download-artifact 固定 commit
```

并禁止旧 workflow 中 Debug/static/clang-tidy/ASan preset、`continue-on-error`、浮动
Action tag、`*-latest` runner 和 PR publish 条件。

运行：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzGitHubActionsContract.cmake
```

预期：FAIL，报告仍使用 `ubuntu-24.04` 或旧 Linux matrix。

- [ ] **步骤 2：实现 contracts 与 Linux job**

contracts 运行 preset/gate/workflow/package 合同。Linux job 安装 Ubuntu 22.04 GCC 13、
Xvfb 与 ELF 依赖，安装 Qt 6.8.3，下载固定版本 linuxdeploy、Qt plugin、appimagetool 并
核对硬编码 SHA-256。准备公开 release evidence 和 GNU runtime 许可证，配置/构建/
CTest 后调用 AppImage 脚本。PR 验证包但不上传 Release artifact；master/手动运行使用
固定 `actions/upload-artifact` 上传三文件组。

- [ ] **步骤 3：实现两个 Windows jobs**

MSVC 和 MinGW 各只运行一个 continuous preset、完整 CTest 和参数化 package 脚本。
延续 Qt kit 精确身份检查。上传 artifact 前检查 ZIP/checksum/build-info 恰好各一个。

- [ ] **步骤 4：实现 macOS 双架构 job**

使用 matrix 的两个固定 runner，每个安装匹配架构 Qt SDK，运行一个 continuous preset
和 package 脚本。不得使用另一架构 SDK 或把 universal Qt 结果描述为目标单架构。

- [ ] **步骤 5：实现集中 publish job**

publish job：

```yaml
if: >-
  github.event_name != 'pull_request' &&
  github.ref == 'refs/heads/master'
permissions:
  contents: write
needs: [linux, windows-msvc, windows-mingw, macos]
```

下载五组 artifact 到独立子目录，调用发布脚本。workflow 顶层仍为
`permissions: contents: read`。publish concurrency 不取消已进入写入阶段的上一轮事务。

- [ ] **步骤 6：运行所有本地静态合同**

```bash
cmake -DZZ_PRESETS_FILE="$PWD/CMakePresets.json" \
  -P tests/Platform/PresetMatrixContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzGateScriptContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzGitHubActionsContract.cmake
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzContinuousReleaseContract.cmake
git diff --check
```

预期：全部 PASS，workflow 中 Linux 只有一个编译 preset。

- [ ] **步骤 7：提交 CI/CD workflow**

```bash
git add .github/workflows/ci.yml CMakePresets.json tests/Platform
git commit -m "ci(发布): 收敛矩阵并接入持续预发布" \
  -m "每个平台只构建实际发布的 Release/shared/LTO 产物，Linux 固定 Ubuntu 22.04。" \
  -m "五平台同提交通过后由最小写权限 job 更新 continuous-build。"
```

## 任务 8：更新用户文档并完成本机与远端验收

**文件：**
- 修改：`README.md`
- 修改：`docs/development/BUILDING_ZH.md`
- 修改：`docs/development/GITHUB_ACTIONS_ZH.md`
- 修改：`docs/development/PLATFORM_SUPPORT_ZH.md`
- 修改：`tests/Architecture/ZzDocumentationAudit.cmake`

- [ ] **步骤 1：先更新文档审计合同**

要求 README 和三份开发文档包含：固定 Release URL、五个产物、Ubuntu 22.04、
Pre-release、未签名、SHA-256、Qt 集中升级和“CI smoke 不等于真机验收”。禁止继续
宣称远端 Linux 构建 Debug/static/Clang/ASan 全矩阵。

- [ ] **步骤 2：运行文档审计并确认红灯**

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Architecture/ZzDocumentationAudit.cmake
```

预期：FAIL，报告 README 缺少 continuous build 下载入口。

- [ ] **步骤 3：更新文档**

README 使用稳定链接：

```text
https://github.com/jackfahdin/ZzPureTools/releases/tag/continuous-build
```

构建手册给出五个平台 package 脚本的完整参数；Actions 文档解释最小权限、同提交
artifact 和失败保留旧 Release；平台状态明确 unsigned continuous build 只能证明原生
runner 构建与部署 smoke，不能提升真机验收状态。

- [ ] **步骤 4：执行完整本机验证**

```bash
cmake --preset linux-gcc-debug -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug --parallel 2
ctest --preset linux-gcc-debug --output-on-failure
cmake --preset linux-continuous-release \
  -DZZ_RELEASE_EVIDENCE_ROOT="$ZZ_RELEASE_EVIDENCE_ROOT" \
  -DZZ_GNU_RUNTIME_LICENSE_DIR="$ZZ_GNU_RUNTIME_LICENSE_DIR"
cmake --build --preset linux-continuous-release --parallel 2
ctest --preset linux-continuous-release --output-on-failure
git diff --check
```

预期：所有本机测试通过；`temp_image/` 保持未跟踪且未被任何脚本读取。

- [ ] **步骤 5：提交文档**

```bash
git add README.md docs/development tests/Architecture/ZzDocumentationAudit.cmake
git commit -m "docs(发布): 补充持续构建下载与维护说明" \
  -m "记录五平台产物、Ubuntu 22.04 基线、未签名边界、Qt 升级方法和本地复现命令。"
```

- [ ] **步骤 6：推送并验证 GitHub 原生 runners**

推送当前 `master` 后检查同一 workflow run：

```bash
git push origin master
```

必须取得以下实际结果：

```text
contracts: PASS
linux-x86_64: PASS + AppImage smoke
windows-msvc-x86_64: PASS + ZIP smoke
windows-mingw-x86_64: PASS + ZIP smoke
macos-arm64: PASS + DMG mount smoke
macos-x86_64: PASS + DMG mount smoke
publish-continuous-build: PASS
```

不得用 Linux 静态合同替代 Windows/macOS 结果。失败时下载对应日志，按平台修复并单独
中文提交，再重新运行完整 workflow。

- [ ] **步骤 7：核对 GitHub Release 最终状态**

Release 必须满足：

```text
tag: continuous-build
prerelease: true
latest: false
target commit: 当前 master HEAD
packages: 5
每包 checksum/build-info: 完整
旧提交 package assets: 0
```

下载五个包重新计算 SHA-256，与 Release 资产一致。确认失败的旧 workflow 不曾删除
上一轮完整资产。

- [ ] **步骤 8：提交远端验证记录**

在 `docs/development/GITHUB_ACTIONS_ZH.md` 记录 workflow URL、commit、五平台 job
结论和 Release URL后提交：

```bash
git add docs/development/GITHUB_ACTIONS_ZH.md
git commit -m "docs(CI): 记录持续发布首次跨平台验证" \
  -m "登记同一提交五平台部署 smoke、产物摘要和 continuous-build Release 结果。"
git push origin master
```

第二次 push 还必须证明 continuous Release 可以从上一提交安全更新到新提交，且删除
旧资产后保留精确五个平台的新资产集合。
