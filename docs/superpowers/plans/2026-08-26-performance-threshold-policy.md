# 性能阈值语义化策略实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度；每个任务完成验证后立即创建中文 commit。

**目标：** 把性能相对门禁升级为指标语义驱动的 schema v2，让统计耗时以 P95 阻止可重复回归、max 作为可审计观察证据，同时保持确定性指标、资源指标、绝对预算和环境指纹严格失败关闭。

**架构：** `regression-thresholds.json` 为每个 metric 显式声明 `metricKind` 与 P95/max 策略；通用 CMake 比较器只解释语义合同，不识别业务场景名。噪声分析器按 reporter unit 生成候选分类，Linux 门禁在同一 Xvfb/xcb shell 中保存三轮独立报告并逐轮执行相对和绝对门禁。

**技术栈：** CMake 3.23 script mode、JSON `string(JSON ...)`、Bash、CTest、Qt 6.11.1、GNU 15.2、Xvfb/xcb、Git。

**前置规格：** `docs/superpowers/specs/2026-08-26-performance-threshold-policy-design.md`

**解除阻塞的计划：** `docs/superpowers/plans/2026-08-25-deferred-side-panel-performance.md` 任务 3

**代码基线：** `7d2f176`

---

## 执行约束

- 只在 `.worktrees/command-bar` 的 `feature/command-bar` 分支执行。
- 不读取、不修改、不暂存 `temp_image/`；不 push，不调用 GitHub CLI，不处理远端 CI。
- 不下载 Qt；本机固定使用：

  ```bash
  export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
  export GCC_13=/usr/bin/gcc-15
  export GXX_13=/usr/bin/g++-15
  ```

- 性能发布验收继续使用 `docs/performance/profiles/local-release-xvfb.json`、
  `linux-gcc-benchmarks`、Release/shared/LTO、Xvfb/xcb 和既有 CPU affinity。
- 严格执行 TDD：每个任务先运行能证明合同缺失的失败测试，再写最小实现并重新运行。
- 每个逻辑任务单独提交；标题使用中文简述，正文使用中文详细说明修改和验证边界。
- 12 份 `docs/performance/reference/linux/<scenario>.json` 是只读历史 reporter 输出；禁止
  修改其 metric、commit、环境、样本或格式。
- 不修改 benchmark 采样代码、预热次数、正式样本数和绝对性能预算。
- Windows MSVC、Windows MinGW 和 macOS 只做 CMake/JSON/脚本静态合同检查，不宣称
  真机性能通过。

## 文件结构与职责

- 修改 `cmake/ZzComparePerformanceReport.cmake`：解析 schema v2、校验指标语义、计算
  相对变化并输出 `PASS/OBSERVE/FAIL/INVALID`。
- 修改 `benchmarks/testdata/performance-thresholds-valid.json`：提供最小 schema v2 测试
  策略。
- 修改 `benchmarks/CMakeLists.txt`：保持最小相对比较正反例与新语义一致。
- 修改 `tests/Platform/ZzPerformanceThresholdContract.cmake`：覆盖 schema、分类、模式、
  输出、正式阈值完整性和噪声候选合同。
- 修改 `scripts/ci/ZzAnalyzePerformanceNoise.cmake`：按 unit 推导 `metricKind`，生成符合
  schema v2 的候选策略。
- 修改 `docs/performance/reference/linux/regression-thresholds.json`：迁移所有正式指标，
  不触碰相邻 12 份 reporter JSON。
- 创建 `scripts/ci/run-linux-performance-gates.sh`：在固定显示环境中运行、保存并比较三轮
  benchmark。
- 修改 `scripts/ci/run-linux-gates.sh`：把单轮性能执行替换为三轮 helper，非 benchmark
  测试仍执行一次。
- 修改 `tests/Platform/ZzGateScriptContract.cmake`：静态验证三轮 helper 和主门禁接线。
- 修改 `docs/performance/PERFORMANCE_BASELINE_ZH.md`：记录语义策略、迁移表和最终三轮
  结果。
- 修改 `.superpowers/sdd/2026-08-25-deferred-side-panel-performance/progress.md`：只有最终
  验证和审查通过后，才把原 Task 3 从 `BLOCKED` 更新为完成。

## 任务 1：让比较器执行 schema v2 语义合同

**交付物：** 比较器拒绝无效分类；统计耗时 P95 回归会失败、单独 max 尖峰会观察通过；
确定性和资源指标仍严格 gate；每个字段及场景都有稳定状态输出。

**文件：**

- 修改：`cmake/ZzComparePerformanceReport.cmake`
- 修改：`benchmarks/testdata/performance-thresholds-valid.json`
- 修改：`benchmarks/CMakeLists.txt`
- 测试：`tests/Platform/ZzPerformanceThresholdContract.cmake`

- [ ] **步骤 1：把生成式合同改成 schema v2 失败测试。** 在
  `ZzPerformanceThresholdContract.cmake` 中用 `string(JSON ...)` 从
  `performance-valid.json` 生成三个 current 报告：`p95=111,max=100`、
  `p95=100,max=111` 和未回归报告。生成以下策略，不复制业务场景名：

  ```cmake
  set(duration_thresholds
      [=[{"schemaVersion":2,"scenarios":{"contract":{"metrics":{"latency":{"metricKind":"statistical-duration","p95":{"mode":"gate","percent":10},"max":{"mode":"observe","percent":10}}}}}}]=])
  set(deterministic_thresholds
      [=[{"schemaVersion":2,"scenarios":{"contract":{"metrics":{"latency":{"metricKind":"deterministic","p95":{"mode":"gate","percent":10},"max":{"mode":"gate","percent":10}}}}}}]=])
  set(resource_thresholds
      [=[{"schemaVersion":2,"scenarios":{"contract":{"metrics":{"latency":{"metricKind":"sampled-resource","p95":{"mode":"gate","percent":10},"max":{"mode":"gate","percent":10}}}}}}]=])
  ```

  分别用 `execute_process()` 调用比较器并合并 `OUTPUT_VARIABLE`、`ERROR_VARIABLE`，断言：

  - 未回归 duration 返回 0，日志包含 `PASS contract/latency.p95`、
    `PASS contract/latency.max` 和 `PASS contract`；
  - 只有 max=111 的 duration 返回 0，日志包含
    `OBSERVE contract/latency.max` 和最终 `OBSERVE contract`；
  - p95=111 的 duration 返回非零，日志包含 `FAIL contract/latency.p95`；
  - deterministic/resource 的 max=111 返回非零并包含 `FAIL`。

- [ ] **步骤 2：增加无效策略矩阵。** 从合法 duration JSON 分别构造以下策略，每个都
  必须返回非零。schema version 错误的日志包含 `INVALID thresholds`；其余策略错误包含
  `INVALID contract/latency`：

  ```text
  schemaVersion = 1
  metricKind 缺失
  metricKind = unknown
  statistical-duration.max.mode = gate
  statistical-duration.p95.mode = gate, percent = 11
  deterministic.max.mode = observe
  sampled-resource.p95.mode = observe
  deterministic.max.mode = gate, percent = 21
  ```

  同时保留并收紧环境不匹配测试，要求非零日志包含
  `INVALID contract environment;gpu`，而不是只检查退出码。

- [ ] **步骤 3：运行失败测试。** 配置已存在时直接运行；若测试尚未注册，先配置一次：

  ```bash
  cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  ctest --preset linux-gcc-debug \
    -R '^platform\.performance-threshold-contract$' --output-on-failure
  ```

  预期：FAIL。旧比较器要求 threshold schema 1，且不会识别 `metricKind`、逐字段状态或
  duration max observe 不变量。

- [ ] **步骤 4：实现带前缀的无效输入门禁。** 在比较器增加专用 helper，并让所有
  JSON/schema/fingerprint/preset/metric/unit/policy 验证统一通过它失败：

  ```cmake
  function(zz_invalid context detail)
      message(FATAL_ERROR "INVALID ${context}: ${detail}")
  endfunction()
  ```

  Reporter baseline/current 继续要求 schema 1；threshold 单独要求 schema 2。比较前校验
  baseline 与 current 的 metric 名集合完全相等；threshold 对当前场景必须恰好覆盖全部
  reporter metric，缺项时失败关闭。baseline 为零时相对变化显示为 `0%` 或
  `new-nonzero`，不得除零。

- [ ] **步骤 5：实现语义校验。** 对每个 metric 读取一次 `metricKind`，只接受三个固定
  值。用一个 helper 校验字段配置：

  ```cmake
  function(zz_validate_policy scenario metric kind field mode percent)
      # statistical-duration: p95 gate<=10 或 observe<=100；max 只能 observe<=100
      # deterministic/sampled-resource: p95/max 都只能 gate<=20
  endfunction()
  ```

  `percent` 必须是 0 至 100 的 JSON integer；不得从 metric 名推断 kind。P95 observe 是
  经人工审核的配置例外，因此 duration P95 允许 `observe`，但 gate 时上限固定为 10%。

- [ ] **步骤 6：把比较改为收集结果后汇总。** 将当前首次回归即 `FATAL_ERROR` 的
  `zz_assert_regression()` 改为返回字段状态。每个字段始终输出：

  ```text
  PASS contract/latency.p95 baseline=100 current=100 change=0% mode=gate band=10%
  OBSERVE contract/latency.max baseline=100 current=111 change=11% mode=observe band=10%
  ```

  循环累计 `has_gate_failure` 和 `has_observation`。全部字段处理后：gate 失败用
  `message(FATAL_ERROR "FAIL ${current_scenario}: one or more gated fields regressed")`；
  只有 observe 越带用
  `message(STATUS "OBSERVE <scenario>")`；否则输出 `PASS <scenario>`。允许上限继续用
  millionths 和有溢出保护的整数算法，不引入浮点比较。

- [ ] **步骤 7：迁移最小 benchmark fixture。** 把
  `performance-thresholds-valid.json` 改为 schema 2、`metricKind` 为
  `statistical-duration`、P95 gate 10%、max observe 10%。
  `benchmark.relative-comparison-rejects-regression` 仍由 p95=111 失败；valid 和环境不匹配
  语义不变。在 `benchmarks/CMakeLists.txt` 为这三项补充准确 label/预期，不依赖 max
  制造失败。

- [ ] **步骤 8：运行任务 1 验证。**

  ```bash
  ctest --preset linux-gcc-debug \
    -R '^(platform\.performance-threshold-contract|benchmark\.relative-comparison-(valid|rejects-regression|rejects-environment-mismatch))$' \
    --output-on-failure
  ```

  预期：4/4 通过；合同日志证明四种状态和三种指标分类均按预期判定。

- [ ] **步骤 9：提交任务 1。**

  ```bash
  git add \
    cmake/ZzComparePerformanceReport.cmake \
    benchmarks/testdata/performance-thresholds-valid.json \
    benchmarks/CMakeLists.txt \
    tests/Platform/ZzPerformanceThresholdContract.cmake
  git diff --cached --check
  git commit -m "性能：实现语义化相对门禁" \
    -m "升级比较器的阈值合同，按统计耗时、确定性结构和采样资源校验 P95 与 max 策略。" \
    -m "增加 PASS、OBSERVE、FAIL、INVALID 输出以及 schema、模式、门限和环境负向测试。"
  ```

## 任务 2：迁移正式阈值并校正噪声候选

**交付物：** 12 个正式场景的全部指标都具有可审计 `metricKind`；统计耗时 max 统一
observe；噪声分析器生成的候选策略满足同一 schema 和语义规则。

**文件：**

- 修改：`docs/performance/reference/linux/regression-thresholds.json`
- 修改：`scripts/ci/ZzAnalyzePerformanceNoise.cmake`
- 修改：`tests/Platform/ZzPerformanceThresholdContract.cmake`
- 修改：`docs/performance/PERFORMANCE_BASELINE_ZH.md`

- [ ] **步骤 1：先写正式阈值完整性失败测试。** 在
  `ZzPerformanceThresholdContract.cmake` 中枚举
  `docs/performance/reference/linux/*.json`，排除 `regression-thresholds.json`，并验证：

  - 场景集合与 threshold `scenarios` 集合完全相同；
  - 每个 reporter 的 metric 集合与对应策略集合完全相同；
  - `ms`、`us` 必须分类为 `statistical-duration`；
  - `count`、`ratio` 必须分类为 `deterministic`；
  - `bytes`、`percent` 必须分类为 `sampled-resource`；
  - duration max 全部 observe；deterministic/resource 的 P95/max 全部 gate；
  - duration P95 gate 不超过 10%，其他 gate 不超过 20%。

  读取 JSON 时按 MEMBER 枚举，不用正则解析结构化数据。

- [ ] **步骤 2：先写噪声分析器 schema v2 失败测试。** 复用现有三轮 contract fixture，
  把预期从旧 `schemaVersion=1,p95 gate 20` 改为：

  ```text
  schemaVersion = 2
  latency.metricKind = statistical-duration
  latency.p95 = observe 20%, noisePercent 20
  latency.max = observe 20%, noisePercent 20
  ```

  再生成 `count`、`bytes` 两个指标并验证分别得到 `deterministic` 和
  `sampled-resource`。让 count 三轮波动达到 21%，断言分析器返回非零并说明该语义指标
  超过可接受 20% 稳定带，而不是自动降为 observe。

- [ ] **步骤 3：运行失败测试。**

  ```bash
  ctest --preset linux-gcc-debug \
    -R '^platform\.performance-threshold-contract$' --output-on-failure
  ```

  预期：FAIL。正式阈值仍是 schema 1 且缺少 kind；旧分析器仍生成 schema 1，并仅按噪声
  大小选择 mode。

- [ ] **步骤 4：实现 unit 到 kind 的唯一映射。** 在
  `ZzAnalyzePerformanceNoise.cmake` 增加：

  ```cmake
  function(zz_noise_metric_kind output unit)
      if(unit STREQUAL "ms" OR unit STREQUAL "us")
          set(kind statistical-duration)
      elseif(unit STREQUAL "count" OR unit STREQUAL "ratio")
          set(kind deterministic)
      elseif(unit STREQUAL "bytes" OR unit STREQUAL "percent")
          set(kind sampled-resource)
      else()
          message(FATAL_ERROR "Unsupported performance metric unit: ${unit}")
      endif()
      set(${output} "${kind}" PARENT_SCOPE)
  endfunction()
  ```

  候选根对象改为 schema 2，每个 metric 写入 `metricKind`。统计耗时 P95 噪声不超过
  10% 时建议 gate 10%，超过 10% 时建议 observe 实测向上取整值；统计耗时 max 始终
  observe。确定性和资源字段不超过 10% 时 gate 10%，10% 至 20% 时 gate 实测值，超过
  20% 时分析失败并要求修复 benchmark 或环境。`noisePercent` 继续只存在于候选输出，
  正式阈值不添加该字段。

- [ ] **步骤 5：按真实 unit 迁移全部正式指标。** 把 threshold schema 改为 2，并使用
  以下完整分类：

  ```text
  statistical-duration:
    animation/frame-time
    example-large-model/frame-time
    example-navigation/latency
    example-startup/external-total, first-paint
    example-theme-switch/latency
    large-model/frame-time
    navigation-pane/frame-time, mapping-time, render-time, reset-time, state-update-time
    startup/external-total, first-paint, modules-started, page-created, qt-created
    theme-switch/latency
    window-lifecycle/lifecycle-time

  deterministic:
    example-large-model/multi-data-calls, requested-rows, viewport-paints
    large-model/multi-data-calls, requested-rows, viewport-paints
    navigation-pane/descendants, paint-complexity-ratio,
                    projections-per-pane, views-per-pane

  sampled-resource:
    example-idle/*
    idle/*
    window-lifecycle/rss-bytes
  ```

  所有 duration max 改为 observe。保留已有证据带：
  `example-large-model/frame-time.max=25`、
  `navigation-pane/mapping-time.max=85`、
  `navigation-pane/reset-time.max=26`、
  `theme-switch/latency.p95=66,max=100`；
  `animation/frame-time.max=21` 使用本轮 20.76% 参考复采证据。其余新迁移的 duration max
  使用 observe 10%。所有确定性和资源指标继续 gate 10%。

- [ ] **步骤 6：更新性能策略说明。** 在
  `PERFORMANCE_BASELINE_ZH.md` 的“跨轮噪声与相对回归门限”中：

  - 把“每字段仅按噪声选 mode”改成先按 kind 约束、再由噪声决定 band；
  - 记录 schema v2 三类 kind 及 unit 映射；
  - 记录 duration max 只观察、P95 与绝对预算仍失败关闭；
  - 增加 2026-08-26 animation/example-startup 三轮证据及选择 21%/10% observe band 的理由；
  - 明确 12 份历史 reporter JSON 没有修改。

- [ ] **步骤 7：运行 JSON、合同和十二场景自比较验证。**

  ```bash
  jq empty docs/performance/reference/linux/regression-thresholds.json
  ctest --preset linux-gcc-debug \
    -R '^(platform\.performance-threshold-contract|benchmark\.relative-comparison-(valid|rejects-regression|rejects-environment-mismatch))$' \
    --output-on-failure

  for report in docs/performance/reference/linux/*.json; do
    scenario=$(basename "$report" .json)
    [[ "$scenario" == regression-thresholds ]] && continue
    cmake \
      -DZZ_BASELINE="$report" \
      -DZZ_CURRENT="$report" \
      -DZZ_THRESHOLDS=docs/performance/reference/linux/regression-thresholds.json \
      -P cmake/ZzComparePerformanceReport.cmake
  done
  ```

  预期：JSON 合法、4 项 CTest 通过、12 个历史报告自比较全部输出 `PASS`；正式 threshold
  完整覆盖所有场景和 metric。

- [ ] **步骤 8：确认历史 reporter 未改变并提交任务 2。**

  ```bash
  test -z "$(git diff --name-only -- 'docs/performance/reference/linux/*.json' \
    ':!docs/performance/reference/linux/regression-thresholds.json')"
  git add \
    docs/performance/reference/linux/regression-thresholds.json \
    scripts/ci/ZzAnalyzePerformanceNoise.cmake \
    tests/Platform/ZzPerformanceThresholdContract.cmake \
    docs/performance/PERFORMANCE_BASELINE_ZH.md
  git diff --cached --check
  git commit -m "性能：迁移指标语义阈值" \
    -m "为十二个正式场景补齐统计耗时、确定性结构和采样资源分类，并将耗时 max 统一调整为观察证据。" \
    -m "同步噪声候选生成规则、完整性合同和基线文档，保持历史 reporter 数据及绝对预算不变。"
  ```

## 任务 3：建立三轮 Linux 发布性能编排

**交付物：** 统一 Linux 门禁只运行一次非 benchmark 测试，并在同一固定显示环境中运行
三轮 benchmark；每轮报告、命令和相对判定互不覆盖，任一强制门禁失败立即阻断。

**文件：**

- 创建：`scripts/ci/run-linux-performance-gates.sh`
- 修改：`scripts/ci/run-linux-gates.sh`
- 测试：`tests/Platform/ZzGateScriptContract.cmake`

- [ ] **步骤 1：先扩展脚本静态合同。** 在 `ZzGateScriptContract.cmake` 增加以下必须
  token，并断言 helper 是存在、非空的普通文件：

  ```text
  scripts/ci/run-linux-gates.sh|run-linux-performance-gates.sh
  scripts/ci/run-linux-gates.sh|-LE benchmark
  scripts/ci/run-linux-performance-gates.sh|for round in 1 2 3
  scripts/ci/run-linux-performance-gates.sh|-L benchmark
  scripts/ci/run-linux-performance-gates.sh|release-rounds/round-${round}
  scripts/ci/run-linux-performance-gates.sh|ZzComparePerformanceReport.cmake
  scripts/ci/run-linux-performance-gates.sh|regression-thresholds.json
  scripts/ci/run-linux-performance-gates.sh|commands.log
  ```

  合同还要拒绝主脚本中遗留的旧单轮 `performance_scenarios=(` 块，防止新旧门禁同时运行。

- [ ] **步骤 2：运行失败测试。**

  ```bash
  ctest --preset linux-gcc-debug \
    -R '^platform\.gate-script-contract$' --output-on-failure
  ```

  预期：FAIL，明确指出 `scripts/ci/run-linux-performance-gates.sh` 尚不存在。

- [ ] **步骤 3：创建三轮 helper。** 新脚本固定从自身位置解析 source root，要求当前
  shell 已有可用 `DISPLAY`、`QT_QPA_PLATFORM=xcb` 和 benchmark 所需环境变量。定义 12
  个正式场景，输出根固定为：

  ```bash
  build/linux-gcc-benchmarks/release-rounds/round-1
  build/linux-gcc-benchmarks/release-rounds/round-2
  build/linux-gcc-benchmarks/release-rounds/round-3
  ```

  每轮执行以下原子流程：

  ```bash
  taskset -c 10 ctest --preset linux-gcc-benchmarks \
    -L benchmark --output-on-failure -j1

  for scenario in "${performance_scenarios[@]}"; do
    current="build/linux-gcc-benchmarks/reports/benchmark.${scenario}.json"
    test -f "$current"
    cmake -E copy "$current" \
      "${round_dir}/benchmark.${scenario}.json"
    cmake \
      -DZZ_BASELINE="docs/performance/reference/linux/${scenario}.json" \
      -DZZ_CURRENT="${round_dir}/benchmark.${scenario}.json" \
      -DZZ_THRESHOLDS="docs/performance/reference/linux/regression-thresholds.json" \
      -P cmake/ZzComparePerformanceReport.cmake
  done
  ```

  每轮开始前删除当前 12 份 reporter 输出，避免 benchmark 未产出时误用上一轮文件；只
  允许删除解析后位于 `build/linux-gcc-benchmarks/reports/` 下的明确文件。用
  `commands.log` 记录 round、commit、CTest 命令、每个比较命令和报告绝对路径。
  任一 CTest、缺失报告、相对 gate 或 absolute reference gate 失败时由 `set -euo pipefail`
  立即停止。最后只在三轮都完成时输出 `PASS three-round Linux performance gates`。

- [ ] **步骤 4：接入统一 Linux 门禁。** 在 `run-linux-gates.sh` 完成 benchmark configure
  和 build 后，把原单轮：

  ```bash
  taskset -c 10 ctest --preset linux-gcc-benchmarks --output-on-failure -j1
  # performance_scenarios + compare loop
  ```

  替换为：

  ```bash
  taskset -c 10 ctest --preset linux-gcc-benchmarks \
    -LE benchmark --output-on-failure -j1
  scripts/ci/run-linux-performance-gates.sh
  ```

  Xvfb 的启动、健康检查、trap 和后续 ASan benchmark 保持原顺序；helper 不自行启动第二
  个显示服务器。

- [ ] **步骤 5：验证 shell 语法和静态接线。**

  ```bash
  bash -n scripts/ci/run-linux-performance-gates.sh
  bash -n scripts/ci/run-linux-gates.sh
  ctest --preset linux-gcc-debug \
    -R '^platform\.gate-script-contract$' --output-on-failure
  ```

  预期：两个脚本语法检查通过，脚本合同 1/1 通过；主脚本不再包含旧单轮场景循环。

- [ ] **步骤 6：提交任务 3。**

  ```bash
  git add \
    scripts/ci/run-linux-performance-gates.sh \
    scripts/ci/run-linux-gates.sh \
    tests/Platform/ZzGateScriptContract.cmake
  git diff --cached --check
  git commit -m "质量：增加三轮性能发布门禁" \
    -m "在同一 Xvfb/xcb 环境连续运行三轮 benchmark，逐轮保存报告、命令和相对比较结果。" \
    -m "让非 benchmark 测试只运行一次，并保持绝对预算、环境指纹及任一轮强制失败的阻断语义。"
  ```

## 任务 4：完成本机三轮验收并解除原计划阻塞

**交付物：** 最终 HEAD 的定向合同、十二场景自比较和真实三轮 Linux 性能门禁都有新鲜
通过证据；文档记录 observe 告警与强制门禁结果，原延迟侧面板 Task 3 可以进入最终审查。

**文件：**

- 修改：`docs/performance/PERFORMANCE_BASELINE_ZH.md`
- 修改：`.superpowers/sdd/2026-08-25-deferred-side-panel-performance/progress.md`
- 不提交：`build/linux-gcc-benchmarks/release-rounds/**`

- [ ] **步骤 1：运行定向合同和十二场景自比较。**

  ```bash
  ctest --preset linux-gcc-debug \
    -R '^(platform\.(performance-threshold-contract|gate-script-contract)|benchmark\.relative-comparison-(valid|rejects-regression|rejects-environment-mismatch))$' \
    --output-on-failure

  for report in docs/performance/reference/linux/*.json; do
    scenario=$(basename "$report" .json)
    [[ "$scenario" == regression-thresholds ]] && continue
    cmake -DZZ_BASELINE="$report" -DZZ_CURRENT="$report" \
      -DZZ_THRESHOLDS=docs/performance/reference/linux/regression-thresholds.json \
      -P cmake/ZzComparePerformanceReport.cmake
  done
  ```

  预期：5 项 CTest 和 12 个自比较全部通过；自比较没有 `OBSERVE` 越带。

- [ ] **步骤 2：准备固定本机环境。** 校验 profile digest、runner digest、GPU identity、
  CPU 8/10 affinity 和 Xvfb；使用与 `run-linux-gates.sh` 完全相同的启动参数。不得下载或
  切换 Qt。配置并构建一次：

  ```bash
  export QT_ROOT=/home/zz/Qt/6.11.1/gcc_64
  export GCC_13=/usr/bin/gcc-15
  export GXX_13=/usr/bin/g++-15
  export ZZ_BENCHMARK_COMMIT=$(git rev-parse --verify HEAD)
  cmake --preset linux-gcc-benchmarks -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  cmake --build --preset linux-gcc-benchmarks --parallel 2
  ```

  `ZZ_RUNNER_IMAGE_DIGEST` 和 `ZZ_GPU_IDENTITY` 必须来自当前已审定 profile/renderer，不能
  写 `unknown` 或临时占位值。

- [ ] **步骤 3：执行真实三轮 helper。** 在固定 Xvfb/xcb shell 中运行：

  ```bash
  scripts/ci/run-linux-performance-gates.sh 2>&1 | \
    tee build/linux-gcc-benchmarks/performance-gates-final.log
  ```

  预期：三轮 benchmark、absolute reference gates 和 36 次相对场景比较全部成功。允许
  statistical-duration max 输出 `OBSERVE`；不允许 P95 gate、确定性、资源、绝对预算或
  环境指纹失败。若出现后者，保留报告继续定位，不修改阈值绕过。

- [ ] **步骤 4：审计三轮产物。** 确认每轮恰好包含 12 份 reporter JSON，三个报告集合
  相同，所有环境指纹相同，commit 等于最终待验收 HEAD；确认 `commands.log` 和
  `build/linux-gcc-benchmarks/performance-gates-final.log` 非空。使用结构化 JSON 读取，
  不用 grep 解析 metric：

  ```bash
  for round in 1 2 3; do
    test "$(find "build/linux-gcc-benchmarks/release-rounds/round-${round}" \
      -maxdepth 1 -name 'benchmark.*.json' -type f | wc -l)" -eq 12
    jq -e '.scenario and .environment and .build.commit and .metrics' \
      build/linux-gcc-benchmarks/release-rounds/round-${round}/benchmark.*.json \
      >/dev/null
  done
  ```

- [ ] **步骤 5：执行整实现审查。** 使用 `superpowers:requesting-code-review` 审查
  `7d2f176..HEAD`，重点核对：schema/状态语义是否和规格一致、比较器整数运算是否会
  溢出、正式指标是否完整分类、三轮脚本是否可能复用旧报告、绝对 gate 是否仍在每轮
  执行。Critical 或 Important finding 必须先修复、运行对应回归并单独提交；最多执行五轮
  “审查 -> 修复 -> 复验”，未清零时保持原 Task 3 阻塞。

- [ ] **步骤 6：更新最终证据说明。** 在 `PERFORMANCE_BASELINE_ZH.md` 追加最终 HEAD、
  三轮报告目录、environment fingerprint、animation 与 example-startup 的三轮 P95/max、
  observe 告警、全部强制门禁和绝对预算结果。明确 build 目录原始报告不进入 Git，历史
  reference reporter 仍未修改。

  在 SDD ledger 中追加：新策略计划 commit 范围、定向合同结果、三轮结果和 Task 3 的
  审查状态。只有实现审查无 Critical/Important 且全部要求满足时写 `Task 3: complete`；
  否则记录具体 open finding，不能仅因 max 改为 observe 就宣称完成。

- [ ] **步骤 7：运行最终差异与文档验证。**

  ```bash
  test -z "$(git diff --name-only -- 'docs/performance/reference/linux/*.json' \
    ':!docs/performance/reference/linux/regression-thresholds.json')"
  git diff --check
  git status --short
  ```

  预期：只出现本步骤两份文档修改；没有历史 reporter、`temp_image/` 或 build evidence 被
  暂存。

- [ ] **步骤 8：提交任务 4。**

  ```bash
  git add \
    docs/performance/PERFORMANCE_BASELINE_ZH.md \
    .superpowers/sdd/2026-08-25-deferred-side-panel-performance/progress.md
  git diff --cached --check
  git commit -m "质量：完成性能阈值发布验收" \
    -m "记录最终 HEAD 的三轮 Linux 性能报告、观察告警、强制相对门禁和绝对预算结果。" \
    -m "在不修改历史 reporter 的前提下解除延迟侧面板 Task 3 阻塞，并保留跨平台真机待验证边界。"
  ```

## 最终完成标准

1. Threshold schema version 2 为正式 12 个场景的每个 metric 声明合法 `metricKind`、P95
   和 max 策略，新增或缺失指标失败关闭。
2. 统计耗时 P95 gate 超过 10% 会阻断；统计耗时 max 越过记录带只输出 OBSERVE，不会
   单独阻断。
3. 确定性和采样资源指标的 P95/max 均严格 gate，任一超限都会阻断。
4. 绝对 reference gates、环境指纹和历史 reporter JSON 没有被 observe 策略弱化或修改。
5. 噪声分析器按 unit 生成 schema v2 候选；不稳定的确定性/资源指标要求修复，不自动
   降级为 observe。
6. Linux 发布性能验收固定三轮、每轮独立保存 12 份报告和命令；任一 P95、确定性、资源、
   绝对或 INVALID 失败都会阻断。
7. 最终 HEAD 的定向合同、十二场景自比较和三轮真实性能验收有新鲜通过证据。
8. 原延迟侧面板计划 Task 3 经最终实现审查后才从 BLOCKED 改为 complete；Windows
   MSVC、Windows MinGW 和 macOS 继续准确标记真机待验证。

## 计划自检

- 规格第 1、2 节的根因与方案边界由任务 2 的正式迁移和文档证据覆盖。
- 规格第 3 节的 schema v2、三类 kind 和策略不变量由任务 1、任务 2 覆盖。
- 规格第 4 节的绝对/相对边界由任务 1 比较器、既有 reference gate 和任务 4 真正运行
  共同覆盖，没有复制绝对阈值。
- 规格第 5 节的四种状态、逐字段输出和失败前缀由任务 1 正反例覆盖。
- 规格第 6 节的三轮目录、逐轮失败和禁止多数表决由任务 3、任务 4 覆盖。
- 规格第 7、8 节的迁移边界与十二项验收合同全部映射到任务 1 至 4。
- 类型名统一为 `metricKind`、`statistical-duration`、`deterministic`、
  `sampled-resource`；threshold 使用 schema 2，reporter 继续使用 schema 1。
- 所有实现步骤都给出明确输入、输出和失败行为；每个任务都包含红测、实现、绿测和中文
  提交步骤。
