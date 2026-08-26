# Linux 性能参考档案与基线

## 档案选择

项目同时记录两个 Linux 性能参考档案，但任何一次报告只能属于其中一个档案，禁止跨档案比较。

| 档案 | 状态 | 用途 |
|---|---|---|
| `local-release-xvfb` | 当前活动，基线已建立 | 本机性能参考与本地发布性能门禁 |
| `ubuntu2204-github-ci` | 待用户在 GitHub 或新主机验证 | 原规划的 Ubuntu 22.04 兼容 CI 参考档案 |

负责人于 2026-08-05 确认：现阶段只有当前主机，因此选用 `local-release-xvfb` 作为发布参考档案。以后上传 GitHub 或购置新主机时，先完成 `ubuntu2204-github-ci` 的独立验证，再决定是否切换活动档案。切换不得删除本机档案及其 Git 历史，也不得把两个档案的 JSON 混为同一基线。

Linux runner 同样遵循该选择：`scripts/ci/run-linux-gates.sh` 在当前主机直接执行 GCC shared/static/LTO 发布组合，并使用本机活动性能基线完成相对门禁。只有显式提供合法的 `ZZ_UBUNTU2204_BUILD_IMAGE` 时，才追加执行原 Ubuntu 22.04 兼容发布脚本；未提供镜像不阻止当前参考机发布，但原档案必须继续保持 `pending-user-validation`。

结构化档案位于：

- `docs/performance/profiles/local-release-xvfb.json`
- `docs/performance/profiles/ubuntu2204-github-ci.json`

## 当前本机参考环境

seat0 KDE Wayland 会话使用 Intel UHD 770 硬件合成，但主机当前没有连接物理显示器，Qt 只能看到 0×0 output，无法完成窗口 exposed 验收。因此自动性能门禁固定使用同机专用 Xvfb，不使用 SSH 转发 display，也不把 Intel GPU 冒充为 benchmark renderer。

| 字段 | 固定值 |
|---|---|
| CPU | Intel Core i7-14700，1 socket，20 core，28 logical CPU |
| RAM 指纹 | 32,682,016,768 bytes；`MemTotal` 按 64 MiB 向下归一化 |
| 主机 GPU | Intel UHD Graphics 770，i915，当前不参与 Xvfb 绘制 |
| benchmark renderer | Mesa llvmpipe，LLVM 21.1.8，Mesa 26.0.3 |
| 显示 | Xvfb 21.1.22，xcb，1920×1080×24，60 Hz，DPR 1.0 |
| OS | Ubuntu 26.04 LTS，kernel 7.0.0-28-generic |
| Qt | 6.11.1 |
| 编译器 | GCC 15.2.0 |
| libstdc++ | `libstdc++.so.6.0.35` |
| CMake / Ninja | 4.3.3 / 1.12.1 |
| CMake preset | `linux-gcc-reference` |
| Xvfb CPU 亲和性 | 逻辑 CPU 8，P-core 4 |
| benchmark CPU 亲和性 | 逻辑 CPU 10，P-core 5 |
| 测试并行度 | 1 |

Linux `/proc/meminfo` 的 `MemTotal` 会随重启时内核和固件保留内存发生少量变化，
不能作为逐字节稳定的硬件身份。Reporter 因此先执行
`floor(MemTotal / 64 MiB) * 64 MiB`，再写入 `environment.memoryBytes`；活动档案同时
记录 `memoryFingerprintBucketBytes=67108864`。本次原始值 32,709,496,832 bytes
归一为 32,682,016,768 bytes，历史原始值 32,708,886,528 bytes 也落入同一容量桶。
比较器没有放宽：归一后的 `memoryBytes` 仍要求完全相等，跨一个或更多 64 MiB 桶的
runner 必须失败关闭。

本机物理环境没有 immutable container image。为保持可审计身份，`environment.runnerImageDigest` 固定为 `local-release-xvfb.json` 原始字节的 SHA-256：

```bash
sha256sum docs/performance/profiles/local-release-xvfb.json
```

任何档案字段变化都会改变 digest，并强制重新采集十二份基线。该 digest 只表示经过版本控制的物理 runner 档案，不表示容器镜像。

## 当前活动基线

本机活动基线指标于 2026-08-10 建立，十二份报告来自同一次固定环境采集。2026-08-25
只把十二份报告的 `memoryBytes` 迁移到 64 MiB 归一值，并同步活动档案摘要；
`build.commit` 和全部 metric 保持原值，不把新采样静默提升为基线。原采集同时生成
七个组件场景与五个完整 `ZzPureToolsExample` 场景：

| 身份字段 | 固定值 |
|---|---|
| 被测源码 HEAD | `a990498ddfb8ab8770dc2ee4f6b6a2c2281321c4` |
| runner 档案 SHA-256 | `0a6cc154c5e565cdac99c7a83b82cc7625fab23cb6af2c4701119221bd947295` |
| renderer identity | `Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.3 Xvfb 1920x1080x24` |
| reference CTest | 37/37 通过，包含 12 个报告生产者与 15 项绝对门禁 |
| Clang ASan/UBSan | `linux-clang-asan-benchmarks` 构建通过；窗口生命周期与导航面板场景 2/2 通过，保持 LeakSanitizer 开启 |

| 门禁 | 实测结果 | 要求 | 结论 |
|---|---:|---:|---|
| 启动 `external-total` | P95 19.987948 ms，max 20.027428 ms | P95/max ≤ 300 ms | 通过 |
| 500 控件主题切换 | P95 6.730136 ms | P95 ≤ 50 ms | 通过 |
| Toggle 动画 | P95 16.604626 ms | P95 ≤ 16.7 ms | 通过 |
| 10 万行模型 | P95 1.529310 ms | P95 ≤ 16.7 ms | 通过 |
| 窗口生命周期 | 100 次完成，P95 3.886237 ms | 100 次且诊断计数无残留 | 通过 |
| 40 个导航面板整帧 | P95 8.159529 ms | P95 ≤ 12 ms | 通过 |
| 导航绘制复杂度 | 0.9663735099 倍 | 严格 ≤ 1.5 倍 | 通过 |
| 10 万行导航 reset | P95 17.077180 ms | P95 ≤ 80 ms | 通过 |
| 空闲 CPU | 0% | 严格 < 0.5% | 通过 |
| 空闲 RSS 增长 | 0% | ≤ 10% | 通过 |
| 综合示例启动 `external-total` | P95 72.994552 ms，max 73.026990 ms；首次绘制 P95 64.331592 ms | P95/max ≤ 300 ms | 通过 |
| 综合示例页面切换 | P50 10.137042 ms，P95 11.352345 ms，max 18.545556 ms | P95 ≤ 50 ms | 通过 |
| 综合示例主题切换 | P50 3.814043 ms，P95 9.305927 ms，max 9.393607 ms | P95 ≤ 50 ms | 通过 |
| 综合示例 10 万行模型 | P95 0.468593 ms，22 次 `multiData`、11 个请求行、2 次 viewport paint/帧 | P95 ≤ 16.7 ms | 通过 |
| 综合示例空闲 CPU | 0.033327% | 严格 < 0.5% | 通过 |
| 综合示例空闲 RSS | 64,344,064 增至 64,475,136 bytes，增长 0.203705% | ≤ 10% | 通过 |

该历史结果建立 `local-release-xvfb` 活动基线。提交
`35362b90715c2c1a76ffe9c2adfcd39606f203cb` 的 2026-08-25 候选运行通过 benchmark
标签 41/41 与全部绝对门禁，但相对比较首次运行有 7/12 场景失败；静置后三轮复测仍
稳定复现综合示例启动和 RSS 回归，因此候选没有解除当前发布阻断，也没有覆盖本表
metric。后续同档案报告只有在完整环境指纹一致时才能与本基线执行相对回归比较；
档案变化、环境不匹配或任一报告缺失都必须失败关闭。

## 测量与文件映射

所有报告必须来自干净工作树的同一 HEAD，使用相同档案 digest 和 renderer identity：

```bash
export ZZ_BENCHMARK_COMMIT="$(git rev-parse --verify HEAD)"
export ZZ_RUNNER_IMAGE_DIGEST="sha256:$(sha256sum \
  docs/performance/profiles/local-release-xvfb.json | awk '{print $1}')"
export ZZ_GPU_IDENTITY="Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.3 Xvfb 1920x1080x24"
export GCC_13=/usr/bin/gcc-15
export GXX_13=/usr/bin/g++-15
export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64

cmake --preset linux-gcc-reference \
  -DXKB_INCLUDE_DIR="$PWD/build/dependencies/xkbcommon/root/usr/include" \
  -DXKB_LIBRARY=/usr/lib/x86_64-linux-gnu/libxkbcommon.so.0
cmake --build --preset linux-gcc-reference
taskset -c 8 xvfb-run -a \
  -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 ctest --preset linux-gcc-reference \
    -L benchmark --output-on-failure -j1
```

CPU 亲和性是本机参考档案的一部分：Xvfb 固定到逻辑 CPU 8，被测进程固定到另一物理 P-core 的逻辑 CPU 10。这样避免显示服务与 GUI 进程互相抢占同一物理核心，也避免调度迁移把数十微秒噪声计入 16.7 ms 动画 P95。不得在缺少 `taskset`、CPU 编号变化或亲和性设置失败时降级运行；必须更新档案、提交并重新采集。

固定采样条件如下：

| 场景 | 预热 | 正式样本 |
|---|---:|---:|
| 启动 | 5 个子进程 | 30 个子进程 |
| 主题切换 | 10 轮 | 100 轮 |
| 动画 | 10 次 toggle | 100 次 toggle 的全部相邻 Paint 间隔 |
| 10 万行模型 | 10 帧 | 100 帧 |
| 窗口生命周期 | 0 | 100 个窗口 |
| 导航面板 | 10 帧 | 120 帧、1000 次映射激活、20 次 reset |
| 空闲 | 5 秒 | 30 秒单区间 |
| 综合示例启动 | 5 个子进程 | 30 个子进程；父测量器连续拉起真实示例 |
| 综合示例页面切换 | 10 轮 | 100 轮真实路由切换与绘制完成 |
| 综合示例主题切换 | 10 轮 | 100 轮真实主题切换与绘制完成 |
| 综合示例 10 万行模型 | 10 帧 | 100 帧真实滚动与 viewport 绘制 |
| 综合示例空闲 | 5 秒 | 30 秒单区间 |

| reporter 输出 | 活动基线路径 |
|---|---|
| `build/linux-gcc-reference/reports/benchmark.startup.json` | `docs/performance/reference/linux/startup.json` |
| `build/linux-gcc-reference/reports/benchmark.theme-switch.json` | `docs/performance/reference/linux/theme-switch.json` |
| `build/linux-gcc-reference/reports/benchmark.animation.json` | `docs/performance/reference/linux/animation.json` |
| `build/linux-gcc-reference/reports/benchmark.large-model.json` | `docs/performance/reference/linux/large-model.json` |
| `build/linux-gcc-reference/reports/benchmark.window-lifecycle.json` | `docs/performance/reference/linux/window-lifecycle.json` |
| `build/linux-gcc-reference/reports/benchmark.navigation-pane.json` | `docs/performance/reference/linux/navigation-pane.json` |
| `build/linux-gcc-reference/reports/benchmark.idle.json` | `docs/performance/reference/linux/idle.json` |
| `build/linux-gcc-reference/reports/benchmark.example-startup.json` | `docs/performance/reference/linux/example-startup.json` |
| `build/linux-gcc-reference/reports/benchmark.example-navigation.json` | `docs/performance/reference/linux/example-navigation.json` |
| `build/linux-gcc-reference/reports/benchmark.example-theme-switch.json` | `docs/performance/reference/linux/example-theme-switch.json` |
| `build/linux-gcc-reference/reports/benchmark.example-large-model.json` | `docs/performance/reference/linux/example-large-model.json` |
| `build/linux-gcc-reference/reports/benchmark.example-idle.json` | `docs/performance/reference/linux/example-idle.json` |

十二份 JSON 只能逐字复制 reporter 输出，不得手改数值。复制前必须满足组件与综合示例启动 P95/max 不超过 300 ms，组件与综合示例主题切换 P95 不超过 50 ms，综合示例页面切换 P95 不超过 50 ms，动画、组件与综合示例大模型 P95 不超过 16.7 ms，导航整帧 P95 不超过 12 ms，绘制复杂度不超过 1.5 倍，导航 reset P95 不超过 80 ms，两项空闲 CPU 均严格低于 0.5%、RSS 增长均不超过 10%，并完成窗口生命周期计数和对应 ASan/UBSan 门禁。

## 跨轮噪声与相对回归门限

绝对门限判断产品是否满足性能预算；相对门限判断同一参考档案下的新提交是否发生退化。相对门限位于 `docs/performance/reference/linux/regression-thresholds.json`，必须对每个场景、指标以及 `p95`、`max` 显式声明策略，新增 reporter 指标但未增加策略时失败关闭。

候选门限必须来自至少三轮完整 benchmark。每轮报告复制到互不覆盖的目录后执行：

```bash
cmake \
  '-DZZ_RUN_DIRECTORIES=build/noise/round-1;build/noise/round-2;build/noise/round-3' \
  -DZZ_OUTPUT_JSON=build/noise/candidate.json \
  -DZZ_OUTPUT_MARKDOWN=build/noise/candidate.md \
  -P scripts/ci/ZzAnalyzePerformanceNoise.cmake
```

工具对每个字段计算 `((max-min)/min)*100` 的向上取整结果。`schemaVersion: 2`
先按 `metricKind` 约束策略，再用噪声选择门限带：`ms`、`us` 是
`statistical-duration`，`count`、`ratio` 是 `deterministic`，`bytes`、`percent`
是 `sampled-resource`。统计耗时 P95 在噪声不超过 10% 时使用 10% gate，超过 10%
时记录实测向上取整值为 observe；max 始终 observe。确定性和采样资源字段在不超过
10% 时使用 10% gate，10% 至 20% 使用实测 gate，超过 20% 则候选生成失败，必须修复
benchmark 或运行环境，不能自动降为 observe。实测噪声超过 100% 时，候选 `percent`
封顶为 100，而 `noisePercent` 保留真实向上取整值。只有 `gate` 模式的相对 P95
会阻断比较；所有绝对预算始终失败关闭。
候选文件不得直接覆盖正式策略，必须结合原始三轮报告人工审核。

2026-08-11 在源码 `9b79f65cd107fdd25d99cbfb9e7528c69ea74c29` 上完成三轮校准，每轮 23/23 benchmark 与契约测试通过，单轮约 113 秒。原始报告保留在本机 `build/noise/round-{1,2,3}`，不进入版本库。审核结果如下：

| 场景/指标 | P95 噪声 | max 噪声 | 正式策略 |
|---|---:|---:|---|
| `theme-switch/latency` | 66% | 123% | P95 observe 66%，max observe 100% |
| `navigation-pane/mapping-time` | 8% | 85% | P95 gate 10%，max observe 85% |
| `navigation-pane/reset-time` | 1% | 26% | P95 gate 10%，max observe 26% |
| `example-large-model/frame-time` | 2% | 25% | P95 gate 10%，max observe 25% |
| `animation/frame-time` | 不超过 10% | 20.76% | P95 gate 10%，max observe 21% |
| 其余统计耗时指标 | 0% 至 10% | 0% 至 10% | P95 gate 10%，max observe 10% |
| 确定性与采样资源指标 | 0% 至 10% | 0% 至 10% | P95/max gate 10% |

`theme-switch` 的 P95 也表现出明显跨轮噪声，因此作为 observe 不阻断相对比较；其绝对 P95 50ms 门限继续失败关闭。百分比超过配置允许上限时以 observe 100% 保存，同时在本表保留原始 123% 证据。2026-08-26 的 animation 三轮复采 max 噪声为 20.76%，因此采用向上取整的 21% observe；同日 example-startup 三轮证据没有超过 10%，其 duration max 采用 10% observe。后续只有新的至少三轮校准证据才能修改这些策略，不得凭单轮失败扩大门限。

本次迁移仅修改 `regression-thresholds.json` 的策略元数据；12 份历史 reporter JSON 保持未修改。

## 工作区组件 Observe 记录

2026-08-26 在源码 `3aaae72697d8e1676ff4b91eb2ff4890d4bd5bff` 完成
`benchmark.workspace-components` 最终三轮观测。场景固定创建 32 个可见侧面板、
4/32 个 tab group、三个 Bottom 工具、40 个 CommandBar action 和 20/100000
标记模型；原始 reporter JSON 已入库，分别为：

| 轮次 | 证据文件 | SHA-256 |
|---|---|---|
| 1 | `docs/performance/evidence/workspace-components/2026-08-22/round-1.json` | `df62a0480b16c1aa6e1b978742eb2eb859e00f69ca557d251df7257371cdb87a` |
| 2 | `docs/performance/evidence/workspace-components/2026-08-22/round-2.json` | `65180443edf80fa107325b95af93477643980a57ddb430e9cc111894b0f4712e` |
| 3 | `docs/performance/evidence/workspace-components/2026-08-22/round-3.json` | `14398ef639dba0ec85be1cd5140cdff9c5634bfd9fe83d3794eca29367530369` |

三轮共享 GNU 15.2、Qt 6.11.1、Ubuntu 26.04、Xvfb/`xcb`、DPR 1、
Release/shared/LTO、`linux-gcc-reference`、归一内存指纹 32,682,016,768 bytes、
runner digest
`sha256:0a6cc154c5e565cdac99c7a83b82cc7625fab23cb6af2c4701119221bd947295` 和
`Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.3 Xvfb 1920x1080x24` renderer identity。

每项每轮采集 80 个样本，单位为 ms。下表记录三轮 P50/P95/max 的最小至最大范围：

| 指标 | P50 | P95 | max |
|---|---:|---:|---:|
| `title-menu-switch-time` | 4.771666 - 4.923465 | 6.455298 - 6.642621 | 6.796118 - 6.935034 |
| `activity-activation-time` | 0.046388 - 0.046812 | 0.057445 - 0.059751 | 0.071499 - 0.073380 |
| `explorer-filter-time` | 26.563990 - 27.174630 | 27.105620 - 27.847475 | 27.243064 - 28.864735 |
| `tab-state-time` | 0.002815 - 0.012216 | 0.017427 - 0.028077 | 0.028226 - 0.030489 |
| `command-filter-time` | 3.599609 - 3.682419 | 3.953255 - 4.090190 | 4.192200 - 4.237182 |
| `layout-save-time` | 0.199503 - 0.206380 | 0.218132 - 0.224189 | 0.231050 - 0.240135 |
| `layout-restore-time` | 0.730219 - 0.737227 | 0.759424 - 0.763049 | 0.780227 - 0.813868 |
| `panel-toggle-time` | 1.541653 - 1.549856 | 2.604300 - 2.711097 | 3.007721 - 3.166551 |
| `group-structure-time` | 0.127772 - 0.130598 | 0.147084 - 0.151325 | 0.160553 - 0.186653 |
| `workspace-paint-4-groups-time` | 1.730398 - 1.738980 | 1.772522 - 1.797120 | 1.814271 - 1.834964 |
| `workspace-paint-32-groups-time` | 1.687368 - 1.700983 | 1.733478 - 1.764633 | 1.766063 - 1.813989 |
| `workspace-render-time` | 3.168823 - 3.226492 | 3.642670 - 3.676729 | 3.719376 - 3.913390 |
| `marker-paint-20-time` | 0.027331 - 0.028434 | 0.030040 - 0.032475 | 0.040183 - 0.049027 |
| `marker-paint-100000-time` | 0.016674 - 0.017781 | 0.017320 - 0.019687 | 0.017926 - 0.042728 |

三轮 P95 噪声中，`tab-state-time` 为 61.1121%、`marker-paint-100000-time` 为
13.6663%，均超过 10%；`marker-paint-20-time` 为 8.1059%、`panel-toggle-time` 为
4.1008%、Activity 激活为 4.0143%，其余更低。max 仍有调度尖峰，超过 10% 的仅为
100000 标记 138.3577%、20 标记 22.0093% 和组结构 16.2563%。这些工作区指标继续作为独立 observe 证据，
不据单轮 max 扩大正式相对阈值；进程内仍对结构操作 P95 16.7 ms、render P95
12 ms，以及两组规模 P95 比值 2.0 失败关闭。本次最坏 32/4 组绘制 P95 比值为
0.989，100000/20 标记绘制 P95 比值为 0.563。

三轮结构观测完全一致：QObject 为 1088、结果视图 QWidget 为 5、timer 为 3、
animation 为 3、图标样式缓存为 9296 bytes；Linux RSS 为
190128128 - 190353408 bytes。缓存值包含字体和 SVG ActivityBar 图标，基准在采样前
分别验证两类描述符都会填充缓存。基准同时失败关闭：重复操作的 QObject 增长、结果
列表超过 8 个 QWidget、字体/SVG 缓存路径未生效、失败布局恢复未回滚、全透明绘制、
隐藏页后台唤醒，以及 1000 次显隐/分割/合并/主题切换后的 timer/animation 增长。
2026-08-21 的三份 JSON 仅保留为历史记录，不再作为当前工作区组件验收证据。

## 原 CI 参考档案

`ubuntu2204-github-ci` 保留原架构要求：Ubuntu 22.04 兼容构建环境、Qt 6.8+、GCC 13.1+、Release/shared/LTO，以及审核后的 immutable runner image digest。其 CPU、RAM、GPU、显示和精确工具链目前为空，因此状态必须保持 `pending-user-validation`。

用户在 GitHub 平台自行验证或新主机到位后，应执行以下流程：

1. 补全 `ubuntu2204-github-ci.json` 的所有 `null` 字段并提交。
2. 使用该档案的 SHA-256 和真实 GPU/驱动身份运行全部 reference benchmark。
3. 执行全部绝对阈值和 ASan/UBSan，不接受“仅构建通过”。
4. 把报告保存在独立的档案目录并记录 commit，不覆盖本机档案证据。
5. 只有负责人明确切换活动档案后，CI 才能用新档案判定发布。

## 发布边界

本机档案通过性能门禁，只能解除性能参考机这一项阻断。MIT 根许可证、所有者批准记录和第三方来源审核已经完成，但 Windows/macOS 真机清单与 Linux 物理显示交互证据仍未完成；这些独立条件继续阻止正式发布，不得因为本机性能达标而绕过。
