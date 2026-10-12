# 动画调度与绘制对照 · 2026-10-12

本目录记录诊断实验，不能作为正式参考基线，也不能用来替换原 `frame-time` 门限。
控件与基准源代码起点为 `96f5e248a6acc283b8b689757e3b9d99f9786b6e`。
除明确标注的试改外，使用该版本的原始动画与采样逻辑。

## 环境与方法

沿用 [环境记录](../../ci-review/2026-10-12/environment.json)：Qt 6.11.1、GCC 15.2、
Release + LTO、kernel 7.0.0-38、Mesa 26.0.8-1ubuntu0.3、Xvfb 21.1.22。
环境摘要为 `sha256:00f36e63d6f253dbb5856af53b3a71a59eba4aa8a7c45ef1645c924692a9b989`。
Xvfb 固定 CPU 8、程序固定 CPU 10，1920×1080×24，所有性能程序串行运行，期间没有并行构建。
未修改 CPU governor、历史参考或预算。

P95 使用排序后第 `ceil(N * 0.95)` 项（从 1 开始）。JSON 原始输出逐字保存。
不同控件和采样集合的 P95 不能相减得到某层的精确耗时；对照只用于检验具体假设。

## 1. 切换事件循环后端

保持原基准二进制、控件、显示与亲和性一致，仅切换 `QT_NO_GLIB`。
启动日志确认分别使用 `glib dispatcher` 和 `unix dispatcher`。

| 后端 | 仪表帧间隔 P95（ms） | 开关帧间隔 P95（ms） |
|---|---:|---:|
| 默认 GLib | 16.721026 | 16.591619 |
| UNIX，`QT_NO_GLIB=1` | 16.719596 | 16.594415 |

原始报告为 `gauge-{glib,unix}.json` 和 `toggle-{glib,unix}.json`。
切换后端没有消除仪表超限，因此没有把 `QT_NO_GLIB` 加进正式 runner。
实验后端只在本目录说明中区分，原报告的环境 schema 未扩展；不可据原 JSON 指纹将两组当作同配置参考。

源码交叉核对：Qt 6.11.1 的 `qtbase/src/corelib/animation/qabstractanimation.cpp`
默认动画间隔为 16 ms；`kernel/qtimerinfo_unix.cpp` 的 `timerWait()` 对剩余等待向上取整到毫秒。
GLib 和 UNIX 后端都使用此列表，GLib 的 `timerSourcePrepareHelper()` 还进行毫秒转换。
这说明仅切换后端不能绕开共有的等待取整，但不能据此把全部抖动归因于该实现。

## 2. 不依赖框架的 Qt 对照

`control.cpp` 只链接 Qt Widgets：200 ms 的 QVariantAnimation 驱动简单矩形，10 次预热、
100 次实测；另测 16 ms PreciseTimer，丢弃前 10 次后保存 500 个间隔。
它的绘制内容与整数值仪表不同，不是替代基准。

| 指标 | P95（ms） |
|---|---:|
| 动画更新间隔 | 16.573073 |
| Paint 起始间隔 | 16.611011 |
| 实际绘制耗时 | 0.009601 |
| 16 ms 精确定时器间隔 | 16.315151 |

全部采样见 `control-glib.json`。普通 Qt 事件循环本身存在抖动，但这组对照未超过 16.7 ms。

## 3. 保留仪表动画，只跳过 Paint 处理

在原基准的临时副本应用 `skip-paint.patch`；事件过滤器仍记录 Paint 时刻，
设置 `ZZ_DIAGNOSTIC_SKIP_PAINT=1` 时消费 Paint 事件，不进入控件绘制。
保持生产动画和完整轮数，以“正常、跳过、跳过、正常”的顺序运行：

| 文件 | Paint 行为 | 帧间隔 P95（ms） |
|---|---|---:|
| `paint-1-real.json` | 正常 | 16.988652 |
| `paint-2-skip.json` | 跳过 | 16.672420 |
| `paint-3-skip.json` | 跳过 | 16.591885 |
| `paint-4-real.json` | 正常 | 16.755595 |

跳过绘制后本组帧间隔降低，说明绘制及其对后续事件调度的影响不能排除。
这些被改变行为的报告只作诊断，绝不计入正式门限通过数。

## 4. 刻度批量绘制试验（已撤回）

`rejected-batching.patch` 曾把同类刻度合并为两批 QPainter::drawLines 调用。
仪表行为回归及四档 DPI 截图均通过，但未插桩的 `gauge-batched.json` P95 为
16.737380 ms，原 16.7 ms 校验仍失败。

随后使用[之前的 Paint 分发插桩](../../window-hardening/2026-10-12/gauge-timing-probe.patch)，
保持同一探针二进制，按原库、试改库、试改库、原库的顺序设置运行库目录。
原库取自本轮安装消费的 B 前缀；试改库取自构建目录，仪表源码差异只有上述补丁。

| 文件前缀 | 实现 | Paint 分发 P95（ms） | 动画更新间隔 P95（ms） |
|---|---|---:|---:|
| `batch-1-before` | 原始 | 0.202102 | 16.707991 |
| `batch-2-after` | 批量绘制 | 0.208766 | 16.695487 |
| `batch-3-after` | 批量绘制 | 0.202256 | 16.710748 |
| `batch-4-before` | 原始 | 0.222939 | 16.695873 |

每轮 `.json` 保存原指标，`.json.diagnostic.json` 保存附加原始数组。
收益没有稳定到足以支持保留该优化；代码已撤回，并重新构建原始控件库。

## 5. 最终原始门限复验

| 场景 | P95（ms） | 16.7 ms 门限 |
|---|---:|---|
| 开关，`final-animation.json` | 16.585656 | 本轮通过 |
| 仪表，`final-radial-gauge-animation.json` | 16.725868 | 失败 |

开关本轮通过不覆盖前一轮 17.014635 ms 的失败，仍有运行间波动。
同时执行的新比较器自测通过，正式参考契约仍因缺四份报告失败；6 项定向检查中 4 项通过。
本轮没有得到新的动画生产修复，也未更改采样语义来使数字通过。

## 复现

使用上述实际环境，设置 `QT_QPA_PLATFORM=xcb`、`ZZ_CMAKE_PRESET=linux-gcc-benchmarks`、
完整源码提交 `ZZ_BENCHMARK_COMMIT`、对应环境摘要 `ZZ_RUNNER_IMAGE_DIGEST` 与真实
`ZZ_GPU_IDENTITY`。所有实验程序用下列形式启动，禁止与构建并行：

```bash
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 /absolute/path/to/program --report /absolute/path/to/output.json
```

原程序为 `build/linux-gcc-benchmarks/benchmarks/ZzGaugeAnimationBenchmark` 和
`ZzAnimationBenchmark`。GLib 对照取消 `QT_NO_GLIB`，UNIX 对照设置为 `1`。
只在临时源码副本上应用 `skip-paint.patch` 或 `rejected-batching.patch`。
两个补丁使用无上下文格式，按仓库相对路径准备临时副本后，用 `git apply --unidiff-zero` 应用。
插桩工程按上方链接的重现说明构建，切换库时明确设置 `LD_LIBRARY_PATH`，不要混用库版本。

Qt 对照的最小工程如下；把 `control.cpp` 复制到临时工程，并提供同版本 Qt 的前缀：

```cmake
cmake_minimum_required(VERSION 3.23)
project(AnimationControl LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(qt-animation-control control.cpp)
target_compile_features(qt-animation-control PRIVATE cxx_std_20)
target_link_libraries(qt-animation-control PRIVATE Qt6::Widgets)
```

Qt 对照的输出路径是唯一位置参数，不使用 `--report`。
最终复验使用相同环境设置和亲和性，运行：

```bash
ctest --preset linux-gcc-benchmarks --output-on-failure -j1 \
  -R '^(benchmark.(animation|radial-gauge-animation|reference-gate.(animation|radial-gauge-animation))|platform.performance-(comparison|threshold)-contract)$'
```
