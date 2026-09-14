# 跨平台分层开发门禁与指示条修复实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 为只具备一个原生 Qt 工具链的开发机提供可审计的最小门禁，并修复远程指示条改动中的析构期未定义行为和 Linux 视觉基线欠账。

**架构：** 两个本机入口只编排现有 CMake preset 与 CTest 标签，不复制平台构建参数；普通开发机结果与 CI、Sanitizer、视觉、性能和真机证据保持不同语义。指示条修复先在事件携带的原始 `QObject *` 上确认动态类型，再访问带类型的 `QPointer`，并把私有类析构实现归还同名实现文件。

**技术栈：** Qt 6.8+ Widgets/Test、C++20、CMake 3.23+、CTest、Bash、PowerShell 7、Clang ASan/UBSan、Git。

---

## 文件结构

### 创建

- `scripts/ci/run-local-development-gate.sh`：Linux/macOS 最小门禁入口，负责参数、平台/preset 白名单、普通测试和定向测试。
- `scripts/ci/run-local-development-gate.ps1`：Windows MSVC/MinGW 对等入口。
- `tests/Platform/ZzLocalDevelopmentGateContract.cmake`：跨平台静态契约，并在 UNIX 上执行 Bash 参数负向测试。

### 修改

- `tests/Platform/CMakeLists.txt`：注册 `platform.local-development-gate-contract`。
- `docs/development/BUILDING_ZH.md`：记录三平台最小门禁命令、纯文档模式、输出语义和完整平台 runner 的职责差异。
- `docs/development/CODING_STANDARD_ZH.md`：把本机最小门禁证据和“待验证”状态写入提交规则。
- `ZzFluentUI/tests/ZzSelectionIndicatorTransitionTest.cpp`：增加普通视图与标签栏在样式仍存活时销毁的聚焦回归。
- `ZzFluentUI/widgets/src/private/ZzItemSelectionAnimation.cpp`：先检查原始接收对象的动态类型，再访问视图状态；移除不属于本文件的析构实现。
- `ZzFluentUI/widgets/src/private/ZzTabIndicatorAnimation.cpp`：对标签栏执行相同的析构期防护。
- `ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`：承接 `ZzFluentStylePrivate` 析构实现和完整类型依赖。
- `ZzFluentUI/tests/baselines/linux/dpr-*/*.png`：只更新实际受批准视觉变化影响的 Fluent 图片。
- `ZzPureTools/tests/baselines/linux/dpr-*/*.png`：只更新实际受批准视觉变化影响的 Workspace 图片。
- `examples/ZzPureToolsExample/tests/baselines/linux/dpr-*/*.png`：只更新实际受批准视觉变化影响的 Example 图片。
- `examples/ZzPureToolsExample/tests/baselines/README.md`：更新被替换图片的 SHA-256 和本轮视觉说明。

## 固定行为合同

本机门禁构建模式必须同时具备 `--preset <名称>` 与 `--tests <CTest 正则>`。纯文档模式只能使用 `--docs-only`，与前两个参数任意一个同时出现都失败。普通测试和定向测试都排除以下专项标签：

```text
benchmark|screenshot|install|packaging|release
```

允许的映射固定为：

```text
Linux  -> linux-gcc-debug
Darwin -> macos-clang-release-arm64 | macos-clang-release-x86_64
Windows -> windows-msvc2022-release | windows-mingw-release
```

脚本不得下载依赖、安装 Git hook、修改 Git 配置、暂存文件、提交文件、创建源码树内证据文件，或把零项定向匹配视为成功。

### 任务 1：交付三平台本机最小门禁

**文件：**

- 创建：`scripts/ci/run-local-development-gate.sh`
- 创建：`scripts/ci/run-local-development-gate.ps1`
- 创建：`tests/Platform/ZzLocalDevelopmentGateContract.cmake`
- 修改：`tests/Platform/CMakeLists.txt`
- 修改：`docs/development/BUILDING_ZH.md`
- 修改：`docs/development/CODING_STANDARD_ZH.md`

- [ ] **步骤 1：编写失败的静态与参数契约**

在 `ZzLocalDevelopmentGateContract.cmake` 中读取两个入口，检查允许 preset、强制配置项、排除标签、差异检查、文档审计、CTest 零匹配防护以及禁止副作用；在 UNIX 上实际执行 Bash 负向参数场景：

```cmake
set(unix_presets
    linux-gcc-debug
    macos-clang-release-arm64
    macos-clang-release-x86_64)
set(windows_presets
    windows-msvc2022-release
    windows-mingw-release)
file(READ
    "${ZZ_SOURCE_DIR}/scripts/ci/run-local-development-gate.sh"
    bash_content)
file(READ
    "${ZZ_SOURCE_DIR}/scripts/ci/run-local-development-gate.ps1"
    powershell_content)
foreach(preset IN LISTS unix_presets)
    string(FIND "${bash_content}" "${preset}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Bash 入口缺少 preset：${preset}")
    endif()
endforeach()
foreach(preset IN LISTS windows_presets)
    string(FIND "${powershell_content}" "${preset}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "PowerShell 入口缺少 preset：${preset}")
    endif()
endforeach()

if(UNIX)
    execute_process(
        COMMAND bash
            "${ZZ_SOURCE_DIR}/scripts/ci/run-local-development-gate.sh"
            --preset linux-static-release --tests platform.compile
        RESULT_VARIABLE invalid_preset_result)
    if(invalid_preset_result EQUAL 0)
        message(FATAL_ERROR "非法 preset 被错误接受")
    endif()
endif()
```

在 `tests/Platform/CMakeLists.txt` 注册测试并使用 `platform;contract` 标签。

- [ ] **步骤 2：运行契约并确认 RED**

运行：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzLocalDevelopmentGateContract.cmake
```

预期：FAIL，明确报告两个本机门禁脚本尚不存在。

- [ ] **步骤 3：实现 Bash 入口的最小闭环**

入口采用 `set -euo pipefail`，从脚本位置解析仓库根目录，先验证参数和宿主映射，再执行：

```bash
git -C "$source_dir" diff --check
git -C "$source_dir" diff --cached --check

cmake --preset "$preset" \
  -DZZ_BUILD_TESTS=ON \
  -DZZ_BUILD_EXAMPLES=ON \
  -DZZ_BUILD_BENCHMARKS=OFF \
  -DZZ_WARNINGS_AS_ERRORS=ON
cmake --build --preset "$preset"
ctest --preset "$preset" --output-on-failure \
  -LE 'benchmark|screenshot|install|packaging|release'

inventory="$(ctest --preset "$preset" -N -R "$tests" \
  -LE 'benchmark|screenshot|install|packaging|release')"
if ! grep -Eq 'Total Tests: [1-9][0-9]*$' <<<"$inventory"; then
  echo "定向测试正则没有匹配普通测试：$tests" >&2
  exit 1
fi
ctest --preset "$preset" --output-on-failure -R "$tests" \
  -LE 'benchmark|screenshot|install|packaging|release'
```

纯文档分支只在两类 diff 检查后执行：

```bash
cmake "-DZZ_SOURCE_DIR=$source_dir" \
  -P "$source_dir/tests/Architecture/ZzDocumentationAudit.cmake"
```

构建完成后从 `build/<preset>/CMakeCache.txt` 读取编译器路径，并输出宿主、架构、CMake、Qt SDK 路径、编译器、preset、定向正则、排除标签、通过项和仍待 CI/参考机验证的项目。

- [ ] **步骤 4：实现 PowerShell 入口的对等闭环**

使用 `[CmdletBinding()]` 和两个互斥参数集表达构建/文档模式，设置 `$ErrorActionPreference = 'Stop'`，每次原生命令后检查 `$LASTEXITCODE`。Windows 只允许两个 preset：

```powershell
param(
    [Parameter(Mandatory, ParameterSetName = 'Build')]
    [ValidateSet('windows-msvc2022-release', 'windows-mingw-release')]
    [string]$Preset,

    [Parameter(Mandatory, ParameterSetName = 'Build')]
    [ValidateNotNullOrEmpty()]
    [string]$Tests,

    [Parameter(Mandatory, ParameterSetName = 'Docs')]
    [switch]$DocsOnly
)

$excludedLabels = 'benchmark|screenshot|install|packaging|release'
cmake --preset $Preset `
    -DZZ_BUILD_TESTS=ON `
    -DZZ_BUILD_EXAMPLES=ON `
    -DZZ_BUILD_BENCHMARKS=OFF `
    -DZZ_WARNINGS_AS_ERRORS=ON
cmake --build --preset $Preset
ctest --preset $Preset --output-on-failure -LE $excludedLabels
```

定向执行前使用 `ctest --preset $Preset -N -R $Tests -LE $excludedLabels`，要求输出中的 `Total Tests` 大于零。输出字段与 Bash 一致，未执行项仍标记“待验证”。

- [ ] **步骤 5：补充构建与提交规范**

`BUILDING_ZH.md` 新增“开发机最小门禁”小节，提供以下可复制命令：

```bash
bash scripts/ci/run-local-development-gate.sh \
  --preset linux-gcc-debug \
  --tests '^fluent\.selection-indicator$'

bash scripts/ci/run-local-development-gate.sh --docs-only
```

```powershell
pwsh -NoProfile -File scripts/ci/run-local-development-gate.ps1 `
  -Preset windows-msvc2022-release `
  -Tests '^fluent\.selection-indicator$'

pwsh -NoProfile -File scripts/ci/run-local-development-gate.ps1 -DocsOnly
```

同时说明 MinGW 只需替换为 `windows-mingw-release`，macOS 根据机器架构选一个原生 preset。原生平台 runner 章节明确：聚合 runner 仍用于 CI/参考机，本机最小门禁不替代它。

`CODING_STANDARD_ZH.md` 要求生产代码提交正文记录本机环境、最小门禁、定向测试与待验证项，禁止把单平台最小门禁表述为全平台通过。

- [ ] **步骤 6：运行任务 1 验证**

静态与负向契约：

```bash
cmake -DZZ_SOURCE_DIR="$PWD" \
  -P tests/Platform/ZzLocalDevelopmentGateContract.cmake
bash scripts/ci/run-local-development-gate.sh \
  --preset linux-static-release --tests platform.compile
bash scripts/ci/run-local-development-gate.sh \
  --preset linux-gcc-debug
bash scripts/ci/run-local-development-gate.sh \
  --docs-only --tests platform.compile
```

预期：第一条 PASS；后三条均在配置前 FAIL，并分别报告非法 preset、缺少 `--tests`、文档模式参数冲突。

正向门禁：

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
bash scripts/ci/run-local-development-gate.sh \
  --preset linux-gcc-debug \
  --tests '^platform\.(local-development-gate-contract|compile)$'
```

预期：完整构建成功；普通矩阵和两项定向测试通过；输出清楚列出 Linux 本机通过项和 Windows、macOS、Sanitizer、视觉、性能、真机待验证项。

文档模式：

```bash
bash scripts/ci/run-local-development-gate.sh --docs-only
git diff --check
git diff --cached --check
```

预期：PASS，无源码树新增证据文件。

- [ ] **步骤 7：提交任务 1**

```bash
git add \
  scripts/ci/run-local-development-gate.sh \
  scripts/ci/run-local-development-gate.ps1 \
  tests/Platform/ZzLocalDevelopmentGateContract.cmake \
  tests/Platform/CMakeLists.txt \
  docs/development/BUILDING_ZH.md \
  docs/development/CODING_STANDARD_ZH.md
git commit \
  -m "构建：增加跨平台本机最小门禁" \
  -m "新增 Bash 与 PowerShell 统一入口，只接受一个原生 shared preset，并强制完整构建、普通测试和至少一项定向测试。" \
  -m "补充参数与副作用契约、纯文档模式及分层证据说明；本机验证 Linux GCC，Windows 与 macOS 入口保留为静态验证和待平台验证。"
```

### 任务 2：修复指示条析构生命周期与代码归属

**文件：**

- 修改：`ZzFluentUI/tests/ZzSelectionIndicatorTransitionTest.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzItemSelectionAnimation.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzTabIndicatorAnimation.cpp`
- 修改：`ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp`

- [ ] **步骤 1：增加能够触发析构期事件的聚焦回归**

在测试类中增加两个槽。测试必须先通过绘制建立对应动画控制器，再在 `ZzFluentStyle` 仍存活时销毁控件：

```cpp
void itemViewCanBeDestroyedWhileStyleRemainsAlive()
{
    ZzFluentUI::ZzThemeController controller;
    ZzFluentUI::ZzFluentStyle style(&controller);
    QStandardItemModel model(2, 1);
    auto view = std::make_unique<QListView>();
    view->setStyle(&style);
    view->setModel(&model);
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view.get()));
    (void)view->viewport()->grab();
    QCOMPARE(style.findChildren<QVariantAnimation *>().size(), 1);
    view.reset();
    QCOMPARE(style.findChildren<QVariantAnimation *>().size(), 0);
}

void tabBarCanBeDestroyedWhileStyleRemainsAlive()
{
    ZzFluentUI::ZzThemeController controller;
    ZzFluentUI::ZzFluentStyle style(&controller);
    auto bar = std::make_unique<QTabBar>();
    bar->setStyle(&style);
    bar->addTab(QStringLiteral("First"));
    bar->addTab(QStringLiteral("Second"));
    bar->show();
    QVERIFY(QTest::qWaitForWindowExposed(bar.get()));
    (void)bar->grab();
    QCOMPARE(style.findChildren<QVariantAnimation *>().size(), 1);
    bar.reset();
    QCOMPARE(style.findChildren<QVariantAnimation *>().size(), 0);
}
```

- [ ] **步骤 2：在 Sanitizer 下确认 RED**

```bash
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
export CLANG_17=/usr/bin/clang-20
export CLANGXX_17=/usr/bin/clang++-20
export GCC_13_TOOLCHAIN_ROOT=/usr
cmake --preset linux-clang-asan
cmake --build --preset linux-clang-asan \
  --target ZzSelectionIndicatorTransitionTest
ctest --preset linux-clang-asan --output-on-failure \
  -R '^fluent\.selection-indicator$'
```

预期：测试退出非零，ASan/UBSan 指向 `ZzItemSelectionAnimation::eventFilter()` 或 `ZzTabIndicatorAnimation::eventFilter()` 在 QWidget 析构阶段访问已经退化的派生对象。若单次只命中一个路径，使用 QtTest 的函数名参数分别运行两个槽并保存两次非零结果。

- [ ] **步骤 3：实现事件过滤器的最小生命周期修复**

普通视图路径必须先使用原始事件接收者确认类型，再比较带类型的观察指针：

```cpp
bool ZzItemSelectionAnimation::eventFilter(QObject *watched, QEvent *event)
{
    auto *view = qobject_cast<QAbstractItemView *>(watched);
    if (view == nullptr || view != view_.data()) {
        return QObject::eventFilter(watched, event);
    }
    if (event->type() == QEvent::Hide
        || event->type() == QEvent::EnabledChange
        || event->type() == QEvent::StyleChange
        || event->type() == QEvent::LayoutDirectionChange) {
        settle();
    }
    return QObject::eventFilter(watched, event);
}
```

标签路径使用相同顺序：

```cpp
auto *bar = qobject_cast<QTabBar *>(watched);
if (bar == nullptr || bar != bar_.data()) {
    return QObject::eventFilter(watched, event);
}
```

只有确认 `bar` 仍是有效 `QTabBar` 后才允许调用 `settle()`。把当前两行英文析构说明替换成简体中文 Doxygen/行注释，不改变隐藏、缩放、布局方向、主题或 reduced motion 的正常路径。

- [ ] **步骤 4：把私有析构实现移回同名文件**

从 `ZzItemSelectionAnimation.cpp` 删除：

```cpp
ZzFluentStylePrivate::~ZzFluentStylePrivate()
{
    qDeleteAll(itemAnimations);
    qDeleteAll(tabAnimations);
}
```

在 `ZzFluentStylePrivate.cpp` 同一命名空间内定义它，并显式包含 `ZzItemSelectionAnimation.h` 与 `ZzTabIndicatorAnimation.h`，保证 `qDeleteAll()` 看到两个完整类型。`ZzItemSelectionAnimation.cpp` 不再为了析构另一个类而包含标签动画头。

- [ ] **步骤 5：运行 Sanitizer 与 GCC 回归确认 GREEN**

```bash
cmake --build --preset linux-clang-asan \
  --target ZzSelectionIndicatorTransitionTest
ctest --preset linux-clang-asan --output-on-failure \
  -R '^fluent\.selection-indicator$'

export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
cmake --preset linux-gcc-debug \
  -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug --output-on-failure \
  -R '^(fluent\.selection-indicator|fluent\.style|platform\.compile|architecture\.)'
```

预期：两组测试均 PASS，Sanitizer 无非法向下转换、释放后使用或泄漏诊断；正常动画终态断言保持不变。

- [ ] **步骤 6：提交任务 2**

```bash
git add \
  ZzFluentUI/tests/ZzSelectionIndicatorTransitionTest.cpp \
  ZzFluentUI/widgets/src/private/ZzItemSelectionAnimation.cpp \
  ZzFluentUI/widgets/src/private/ZzTabIndicatorAnimation.cpp \
  ZzFluentUI/widgets/src/private/ZzFluentStylePrivate.cpp
git commit \
  -m "修复：加固选择指示条析构生命周期" \
  -m "事件过滤器先检查原始 QObject 的当前动态类型，再访问 QPointer 保存的视图或标签栏，避免 QWidget 析构阶段继续调用已失效的派生接口。" \
  -m "新增普通视图和标签栏销毁回归，把 ZzFluentStylePrivate 析构归还同名实现文件，并以 Linux Clang ASan/UBSan 与 GCC 定向矩阵验证。"
```

### 任务 3：审查并更新 Linux 视觉基线

**文件：**

- 修改：`ZzFluentUI/tests/baselines/linux/dpr-*/*.png`
- 修改：`ZzPureTools/tests/baselines/linux/dpr-*/*.png`
- 修改：`examples/ZzPureToolsExample/tests/baselines/linux/dpr-*/*.png`
- 修改：`examples/ZzPureToolsExample/tests/baselines/README.md`

- [ ] **步骤 1：关闭更新模式生成失败报告**

```bash
unset ZZ_UPDATE_SCREENSHOTS
unset ZZ_UPDATE_EXAMPLE_SCREENSHOTS
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^(fluent\.screenshot|puretools\.workspace-screenshot|example\.puretools-screenshot)-(100|125|150|200)$'
```

预期：受 `e11e964` 指示条和标签布局变化影响的场景 FAIL，并在各自构建报告目录生成 actual/diff；不得在这一步改写基线。

- [ ] **步骤 2：逐图人工检查差异边界**

逐一查看 actual、baseline 和 diff，确认：

- 选择指示条不与标签或内容文字重叠，无双指示条、残影或错误侧边。
- List、Tree、Table、Tab、Activity Bar 和 Workspace 的选中状态一致。
- 标签文字没有裁切，窄宽度下省略行为稳定。
- Light、Dark、HighContrast 都保留可辨识的 hover、pressed、focus 和 selected 状态。
- LTR/RTL 的指示条方向正确，DPR 1.0、1.25、1.5、2.0 没有一像素漂移或边框断裂。
- Example 的标题栏、Activity Bar、左右侧面板、中心标签与设置窗口没有重叠或错位。

任一差异不满足这些条件时，返回任务 2 修复生产代码；不得用覆盖图片隐藏缺陷。

- [ ] **步骤 3：仅在参考机更新批准的图片**

```bash
ZZ_UPDATE_SCREENSHOTS=1 \
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^(fluent\.screenshot|puretools\.workspace-screenshot)-(100|125|150|200)$'

ZZ_UPDATE_EXAMPLE_SCREENSHOTS=1 \
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^example\.puretools-screenshot-(100|125|150|200)$'
```

检查 `git diff --name-only`，只允许出现三个 Linux 基线目录内的 PNG 与 Example 基线说明；删除或忽略构建报告，不把 actual/diff 加入 Git。

- [ ] **步骤 4：更新 Example 基线摘要**

对实际变化的 Example PNG 运行：

```bash
sha256sum examples/ZzPureToolsExample/tests/baselines/linux/dpr-*/*.png
```

把 `examples/ZzPureToolsExample/tests/baselines/README.md` 表格中的摘要替换为当前值，并把本轮说明限定为指示条与标签文字变化，不声称未检查的平台视觉通过。

- [ ] **步骤 5：关闭更新模式确认 GREEN**

```bash
unset ZZ_UPDATE_SCREENSHOTS
unset ZZ_UPDATE_EXAMPLE_SCREENSHOTS
ctest --preset linux-gcc-debug --parallel 4 --output-on-failure \
  -R '^(fluent\.screenshot|puretools\.workspace-screenshot|example\.puretools-screenshot)-(100|125|150|200)$'
git diff --check
git diff --cached --check
```

预期：12 个截图测试入口全部 PASS；再次人工查看 100% 的三主题、RTL 场景和 200% Light；差异检查 PASS。

- [ ] **步骤 6：运行最终分层验收**

```bash
bash scripts/ci/run-local-development-gate.sh \
  --preset linux-gcc-debug \
  --tests '^(fluent\.selection-indicator|platform\.local-development-gate-contract)$'

ctest --preset linux-clang-asan --output-on-failure \
  -R '^fluent\.selection-indicator$'

ctest --preset linux-gcc-debug --output-on-failure \
  -R '^(install\.consumer|platform\.package-relocation|architecture\.)'
```

预期：Linux 最小门禁、Sanitizer 聚焦回归、安装消费、包重定位、公开头和架构边界通过。Windows、macOS 和三平台真机交互仍明确记录为待验证。

- [ ] **步骤 7：提交任务 3**

```bash
git add \
  ZzFluentUI/tests/baselines/linux \
  ZzPureTools/tests/baselines/linux \
  examples/ZzPureToolsExample/tests/baselines/linux \
  examples/ZzPureToolsExample/tests/baselines/README.md
git commit \
  -m "测试：同步选择指示条Linux视觉基线" \
  -m "在登记的 Linux Qt 6.11.1 参考环境审查并更新 Fluent、Workspace 与 Example 四档 DPR 基线，只接纳已批准的指示条和标签文字视觉变化。" \
  -m "关闭更新模式重跑十二个截图入口，并人工检查三主题、RTL、常用 DPR、Activity Bar 与工作区布局；Windows 和 macOS 视觉仍待对应平台验证。"
```

## 最终状态要求

- [ ] `git status --short` 只显示任务开始前已有的 `logcat.log` 与 `multi-window-requirements.md` 两个未跟踪文件。
- [ ] 三个实现提交各自只包含计划声明的文件，提交标题为中文简述，正文为中文验证与边界说明。
- [ ] 不自动推送；只有用户明确要求时才执行 `git push`。
- [ ] 交付说明分别列出 Linux 已执行结果，以及 Windows、macOS、性能和真机仍待执行的结果，禁止用“全部通过”概括分层证据。
