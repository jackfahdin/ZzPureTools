# Linux 启动性能 Governor 可逆实验实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度；每个逻辑任务验证后立即创建中文 commit。

**目标：** 实现并执行一个可审计、必恢复的 CPU governor 对照实验，判断 CPU 8、10 的动态调频是否导致 Linux/Xvfb 启动 P95 随机高尾，同时不修改正式阈值、基线或业务代码。

**架构：** 参数化 shell 状态库负责读取、应用、恢复和验证 governor；普通用户探针在 10 个独立 Xvfb 会话中保存 startup reporter 与相对比较结果；root 包装器只管理 CPU 状态并通过 `runuser` 降权运行探针。包装器在应用配置前注册 EXIT trap，最后用结构化快照证明 governor、EPP、频率上下限和 power profile 已恢复。

**技术栈：** Bash、CMake/CTest、JSON/jq、Linux sysfs、intel_pstate、Xvfb/xcb、Qt 6.11.1、GNU 15.2、Git。

**前置规格：** `docs/superpowers/specs/2026-08-26-linux-performance-governor-experiment-design.md`

**代码基线：** `a51fdbcd8adbf55aa3e912e2275dded869fc84c7`

---

## 执行约束

- 只在 `.worktrees/command-bar` 的 `feature/command-bar` 分支执行。
- 不读取、不修改、不暂存 `temp_image/`；不 push，不调用 GitHub CLI，不处理远端 CI。
- 不下载 Qt 或 shellcheck；Qt 固定为 `/home/zz/Qt/6.11.1/gcc_64`，GCC/G++ 固定为
  `/usr/bin/gcc-15`、`/usr/bin/g++-15`。
- 不修改业务代码、benchmark 采样代码、预热次数、正式样本数、相对阈值、历史 12 份
  reporter JSON 或活动 profile。
- root 包装器只能修改 profile 指定的两个 CPU 的 `scaling_governor`；不得修改 EPP、
  min/max frequency、power profile、SMT、Turbo、IRQ affinity 或内核启动参数。
- Qt、Xvfb、benchmark、CMake 比较器和所有 build evidence 必须由发起 sudo 的普通用户
  运行或拥有，禁止以 root 身份运行 GUI/benchmark。
- 所有主机变更前保存原值；正常退出、失败、SIGINT、SIGTERM、SIGHUP 都必须恢复并
  验真。恢复失败时不得打印成功，不得隐藏手工恢复命令。
- 每个逻辑任务单独提交；标题使用中文简述，正文使用中文详细说明修改和验证边界。
- Windows MSVC、Windows MinGW、macOS 仅通过既有静态脚本合同读取新增文件，不执行
  Linux sysfs 或 Xvfb 行为。

## 文件结构与职责

- 创建 `scripts/ci/ZzLinuxGovernorState.sh`：无顶层副作用的参数化状态函数库。
- 创建 `tests/Platform/ZzGovernorStateTest.sh`：用假 sysfs 验证读取、应用、恢复和验真。
- 修改 `tests/Platform/CMakeLists.txt`：只在 Linux 注册 governor 状态行为测试。
- 创建 `scripts/ci/run-linux-startup-stability-probe.sh`：普通用户十轮 startup 探针。
- 创建 `scripts/ci/run-linux-startup-governor-experiment.sh`：root 可逆事务包装器。
- 修改 `tests/Platform/ZzGateScriptContract.cmake`：静态锁定两脚本的安全边界和接线。
- 修改 `docs/performance/PERFORMANCE_BASELINE_ZH.md`：真实实验后记录临时修改、恢复
  证据、两组结果和对正式发布边界的影响。
- 不提交 `build/linux-gcc-benchmarks/governor-experiment/**` 与本计划 SDD 工作区。

## 任务 1：实现可测试的 governor 状态事务原语

**交付物：** 状态库只对调用者给出的 sysfs 根和 CPU 工作；能输出结构化快照、只修改
governor、恢复原 governor，并拒绝 EPP/min/max 未恢复或 CPU/driver 不合法。

**文件：**

- 创建：`scripts/ci/ZzLinuxGovernorState.sh`
- 创建：`tests/Platform/ZzGovernorStateTest.sh`
- 修改：`tests/Platform/CMakeLists.txt`

- [ ] **步骤 1：先编写假 sysfs 行为测试。** 创建临时根目录并为 CPU 8、10 写入：

  ```text
  online=1
  cpufreq/scaling_driver=intel_pstate
  cpufreq/scaling_governor=powersave
  cpufreq/energy_performance_preference=performance
  cpufreq/scaling_min_freq=800000
  cpufreq/scaling_max_freq=5400000
  ```

  测试 source 状态库后依次断言：

  ```bash
  snapshot=$(zz_governor_snapshot "$fake_root" 8 10)
  jq -e '.cpus["8"].governor == "powersave"' <<<"$snapshot"
  zz_governor_apply "$fake_root" performance 8 10
  [[ $(<"$fake_root/cpu8/cpufreq/scaling_governor") == performance ]]
  zz_governor_restore "$fake_root" "$snapshot" 8 10
  zz_governor_verify "$fake_root" "$snapshot" 8 10
  ```

  负向覆盖 CPU offline、driver 不是 `intel_pstate`、缺字段、EPP 被外部改动以及 governor
  恢复值不一致，均必须非零。测试通过 `mktemp -d` 创建唯一目录，并用绑定到该明确目录
  的 EXIT trap 清理。

- [ ] **步骤 2：注册测试并确认 RED。** 在 Linux 分支注册：

  ```cmake
  add_test(NAME platform.governor-state
      COMMAND bash
          "${CMAKE_CURRENT_SOURCE_DIR}/ZzGovernorStateTest.sh"
          "${PROJECT_SOURCE_DIR}")
  set_tests_properties(platform.governor-state PROPERTIES
      LABELS "platform;contract;linux")
  ```

  运行：

  ```bash
  cmake --preset linux-gcc-debug -DZZ_BUILD_TESTS=ON -DZZ_BUILD_EXAMPLES=ON
  ctest --preset linux-gcc-debug \
    -R '^platform\.governor-state$' --output-on-failure
  ```

  预期：FAIL，原因是 `scripts/ci/ZzLinuxGovernorState.sh` 尚不存在。

- [ ] **步骤 3：实现最小状态库。** 库文件以
  `[[ ${BASH_SOURCE[0]} != "$0" ]]` 拒绝直接执行，并提供四个公开 shell 函数：

  ```bash
  zz_governor_snapshot SYSFS_ROOT CPU...
  zz_governor_apply SYSFS_ROOT GOVERNOR CPU...
  zz_governor_restore SYSFS_ROOT SNAPSHOT_JSON CPU...
  zz_governor_verify SYSFS_ROOT SNAPSHOT_JSON CPU...
  ```

  `snapshot` 用 `jq -n` 生成以下稳定结构，不用字符串拼 JSON：

  ```json
  {
    "cpus": {
      "8": {
        "online": "1",
        "driver": "intel_pstate",
        "governor": "powersave",
        "epp": "performance",
        "minFrequency": "800000",
        "maxFrequency": "5400000"
      }
    }
  }
  ```

  `apply` 先验证全部 CPU，再逐个仅写 `scaling_governor` 并读取回验；`restore` 只把快照中
  的 governor 写回；`verify` 要求快照全部六个字段与当前值逐字相等。所有路径由
  `SYSFS_ROOT/cpu${cpu}` 构造，拒绝空 CPU 列表、非十进制 CPU、非绝对 sysfs 根、符号
  链接字段和换行值。

- [ ] **步骤 4：运行 GREEN 与静态检查。**

  ```bash
  bash -n scripts/ci/ZzLinuxGovernorState.sh
  bash -n tests/Platform/ZzGovernorStateTest.sh
  ctest --preset linux-gcc-debug \
    -R '^platform\.governor-state$' --output-on-failure
  if command -v shellcheck >/dev/null; then
    shellcheck scripts/ci/ZzLinuxGovernorState.sh \
      tests/Platform/ZzGovernorStateTest.sh
  else
    echo 'shellcheck unavailable; not downloaded'
  fi
  ```

  预期：行为测试 1/1 通过；本机当前没有 shellcheck 时只记录未运行。

- [ ] **步骤 5：提交任务 1。**

  ```bash
  git add scripts/ci/ZzLinuxGovernorState.sh \
    tests/Platform/ZzGovernorStateTest.sh tests/Platform/CMakeLists.txt
  git diff --cached --check
  git commit -m "质量：实现可逆 governor 状态事务" \
    -m "增加参数化 Linux CPU 状态快照、应用、恢复和逐字段验真原语。" \
    -m "使用假 sysfs 覆盖正常恢复及 CPU、driver、字段和外部状态污染的失败关闭行为。"
  ```

## 任务 2：实现普通用户十轮启动稳定性探针

**交付物：** 探针在不修改主机配置的情况下运行 10 个独立 Xvfb startup 会话，保存
全部 reporter、比较日志和结构化汇总；相对 FAIL 被计数，INVALID 和执行错误立即失败。

**文件：**

- 创建：`scripts/ci/run-linux-startup-stability-probe.sh`
- 修改：`tests/Platform/ZzGateScriptContract.cmake`

- [ ] **步骤 1：先扩展静态合同。** 要求探针是存在、非空、非目录、非符号链接的普通
  文件，并至少包含以下合同 token：

  ```text
  EUID
  for round in $(seq 1 10)
  -displayfd 3
  taskset -c
  ZzStartupBenchmark
  ZzComparePerformanceReport.cmake
  regression-thresholds.json
  governor-experiment/${phase}
  summary.json
  INVALID
  trap cleanup_xvfb EXIT
  ```

  合同还要拒绝探针调用 `sudo`、`cpupower`、`powerprofilesctl set`，以及写入
  `scaling_governor`。

- [ ] **步骤 2：运行 RED。**

  ```bash
  ctest --preset linux-gcc-debug \
    -R '^platform\.gate-script-contract$' --output-on-failure
  ```

  预期：FAIL，明确指出 `run-linux-startup-stability-probe.sh` 不存在。

- [ ] **步骤 3：实现普通用户探针。** 接口固定为：

  ```bash
  scripts/ci/run-linux-startup-stability-probe.sh \
    --phase control|performance
  ```

  脚本要求 `EUID != 0`、干净 tracked worktree、benchmark cache 精确包含
  `ZZ_PERFORMANCE_REFERENCE:BOOL=ON`，并从 profile 读取 CPU、Xvfb 参数、renderer
  identity 与 Qt/compiler 版本。设置：

  ```bash
  export ZZ_CMAKE_PRESET=linux-gcc-benchmarks
  export ZZ_BENCHMARK_COMMIT=$(git rev-parse --verify HEAD)
  export ZZ_RUNNER_IMAGE_DIGEST="sha256:$(sha256sum "$profile" | awk '{print $1}')"
  export ZZ_GPU_IDENTITY=$(jq -r '.display.rendererIdentity' "$profile")
  export QT_QPA_PLATFORM=xcb
  ```

  `run_round()` 的每次调用都放在独立子 shell 中；每轮用 `Xvfb -displayfd 3` 原子分配
  空闲 display，fd 3 写入该轮 display 文件，并在子 shell 内注册
  `trap cleanup_xvfb EXIT`。该 trap 只结束当前子 shell 记录的 Xvfb PID，不污染后续轮次。
  Xvfb 固定在 profile 的 Xvfb CPU，benchmark 固定在 benchmark CPU；生成报告的命令
  固定为：

  ```bash
  taskset -c "$benchmark_cpu" \
    "$source_dir/build/linux-gcc-benchmarks/benchmarks/ZzStartupBenchmark" \
    --report "$round_report"
  ```

  输出路径固定为：

  ```text
  build/linux-gcc-benchmarks/governor-experiment/${phase}/round-01.json
  build/linux-gcc-benchmarks/governor-experiment/${phase}/round-01.compare.log
  build/linux-gcc-benchmarks/governor-experiment/${phase}/rounds.ndjson
  build/linux-gcc-benchmarks/governor-experiment/${phase}/summary.json
  ```

  每轮比较命令固定为：

  ```bash
  cmake \
    "-DZZ_BASELINE=$source_dir/docs/performance/reference/linux/startup.json" \
    "-DZZ_CURRENT=$round_report" \
    "-DZZ_THRESHOLDS=$source_dir/docs/performance/reference/linux/regression-thresholds.json" \
    -P "$source_dir/cmake/ZzComparePerformanceReport.cmake"
  ```

  比较器非零时先检查日志：含 `INVALID ` 就立即退出；否则作为该轮 gate FAIL 写入 NDJSON
  并继续。用 `jq -e` 校验 reporter 的 scenario、environment、build.commit 和 metrics，
  再用 `jq -n` 为每轮写入以下对象：

  ```json
  {
    "round": 1,
    "report": "absolute path",
    "comparisonExitCode": 1,
    "externalTotal": {"p50": 0, "p95": 0, "max": 0},
    "firstPaint": {"p50": 0, "p95": 0, "max": 0}
  }
  ```

  最后用 `jq -s` 生成 summary，包含 phase、HEAD、profile digest、CPU 当前状态、10 个
  round 对象、`passedRounds`、`failedRounds`。10 轮完整且 summary 结构有效时退出 0，
  即使其中有相对 FAIL；包装器根据 summary 裁定实验。

- [ ] **步骤 4：运行 GREEN 和真实 control smoke。**

  ```bash
  bash -n scripts/ci/run-linux-startup-stability-probe.sh
  ctest --preset linux-gcc-debug \
    -R '^platform\.gate-script-contract$' --output-on-failure
  if command -v shellcheck >/dev/null; then
    shellcheck scripts/ci/run-linux-startup-stability-probe.sh
  else
    echo 'shellcheck unavailable; not downloaded'
  fi
  scripts/ci/run-linux-startup-stability-probe.sh --phase control
  jq -e '
    .phase == "control" and
    (.rounds | length) == 10 and
    (.passedRounds + .failedRounds) == 10
  ' build/linux-gcc-benchmarks/governor-experiment/control/summary.json
  ```

  预期：脚本/合同通过；control 生成 10 份 commit、环境一致的报告。相对失败轮数是实验
  数据，不作为任务 2 代码失败。

- [ ] **步骤 5：提交任务 2。**

  ```bash
  git add scripts/ci/run-linux-startup-stability-probe.sh \
    tests/Platform/ZzGateScriptContract.cmake
  git diff --cached --check
  git commit -m "性能：增加启动稳定性探针" \
    -m "在普通用户下运行十个独立 Xvfb 启动会话，保存 reporter、相对比较日志和结构化汇总。" \
    -m "区分可统计的相对回归与必须立即失败的执行、报告、环境和 INVALID 错误。"
  ```

## 任务 3：实现 root 可逆实验包装器

**交付物：** 用户只需执行一个 sudo 命令；包装器先跑 control，再临时应用 performance，
跑 treatment，最后在所有退出路径恢复并逐字段验证原状态，Qt/Xvfb 始终是普通用户进程。

**文件：**

- 创建：`scripts/ci/run-linux-startup-governor-experiment.sh`
- 修改：`tests/Platform/ZzGateScriptContract.cmake`

- [ ] **步骤 1：先扩展 root 包装器静态合同。** 要求包装器是普通非符号链接文件，并
  包含：

  ```text
  EUID -ne 0
  SUDO_USER
  SUDO_UID
  SUDO_GID
  mktemp -d
  trap finish EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  trap 'exit 129' HUP
  zz_governor_snapshot
  zz_governor_apply
  zz_governor_restore
  zz_governor_verify
  runuser -u
  --phase control
  --phase performance
  host-state-before.json
  host-state-applied.json
  host-state-restored.json
  manual restore
  ```

  合同按文本位置断言 `trap finish EXIT` 早于 `zz_governor_apply`；`runuser` 调用必须早于
  两个 phase 参数且包装器不得直接包含 `ZzStartupBenchmark` 或 `Xvfb` 命令。

- [ ] **步骤 2：运行 RED。**

  ```bash
  ctest --preset linux-gcc-debug \
    -R '^platform\.gate-script-contract$' --output-on-failure
  ```

  预期：FAIL，明确指出 root 包装器不存在。

- [ ] **步骤 3：实现 root 事务。** 包装器固定调用方式：

  ```bash
  sudo scripts/ci/run-linux-startup-governor-experiment.sh
  ```

  启动时验证 `EUID == 0`、sudo 身份字段、源码目录所有者等于 `SUDO_UID`、profile、
  `jq`、`runuser` 和状态库。用 `mktemp -d /tmp/zz-governor-experiment.XXXXXX` 保存 root
  快照和事务日志，并在应用前注册：

  ```bash
  trap finish EXIT
  trap 'exit 130' INT
  trap 'exit 143' TERM
  trap 'exit 129' HUP
  ```

  `finish()` 先保存原退出码并解除 traps，再无条件调用 restore 和 verify。恢复失败把最终
  退出码改为 70，并输出每个 CPU 的：

  ```text
  manual restore: printf '%s\n' 'powersave' > '/sys/devices/system/cpu/cpu8/cpufreq/scaling_governor'
  ```

  上例仅展示日志格式；实现必须通过 `printf '%q'` 把快照中的真实 governor 和经校验的
  绝对字段路径渲染进命令，不能假定原值一定是 `powersave`。

  恢复成功后生成 `host-state-restored.json`，用 `jq -S` 比较 before/restored 的 `.cpus`
  和 `.powerProfile` 完全相等。然后把日志与三份 host-state JSON 安装到
  `build/linux-gcc-benchmarks/governor-experiment/`，owner/group 设回
  `SUDO_UID:SUDO_GID`，最后返回原实验退出码。

  `run_as_invoking_user()` 使用 `runuser -u "$SUDO_USER" -- env`，只传递该用户 HOME、
  `XDG_RUNTIME_DIR=/run/user/$SUDO_UID`、
  `DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$SUDO_UID/bus`，并调用普通用户探针。
  power profile 也通过该函数调用 `powerprofilesctl get`；包装器从不调用 set。

  执行顺序固定为：snapshot -> control -> apply performance -> applied snapshot/verify ->
  performance -> summary verdict -> EXIT restore。判定使用两个 summary：

  ```text
  control < 10/10 且 performance = 10/10 -> GOVERNOR_CANDIDATE
  performance < 10/10                    -> GOVERNOR_INSUFFICIENT
  control = 10/10 且 performance = 10/10 -> INCONCLUSIVE
  ```

  三种性能结论本身都允许事务返回 0；执行、INVALID 或恢复错误返回非零。

- [ ] **步骤 4：运行无 root 的代码验证。**

  ```bash
  bash -n scripts/ci/run-linux-startup-governor-experiment.sh
  ctest --preset linux-gcc-debug \
    -R '^(platform\.governor-state|platform\.gate-script-contract)$' \
    --output-on-failure
  if command -v shellcheck >/dev/null; then
    shellcheck scripts/ci/ZzLinuxGovernorState.sh \
      scripts/ci/run-linux-startup-stability-probe.sh \
      scripts/ci/run-linux-startup-governor-experiment.sh \
      tests/Platform/ZzGovernorStateTest.sh
  else
    echo 'shellcheck unavailable; not downloaded'
  fi
  ```

  预期：bash 语法通过，CTest 2/2 通过。此步骤不调用 sudo、不改变 governor。

- [ ] **步骤 5：提交任务 3。**

  ```bash
  git add scripts/ci/run-linux-startup-governor-experiment.sh \
    tests/Platform/ZzGateScriptContract.cmake
  git diff --cached --check
  git commit -m "质量：封装 governor 可逆实验事务" \
    -m "以 root 包装器管理对照、performance 处理和普通用户探针调用，禁止 root 运行 Qt。" \
    -m "让正常、失败和信号退出统一恢复并逐字段验真 CPU 与电源状态，保留明确手工恢复命令。"
  ```

## 任务 4：执行真实 sudo 实验并记录恢复证据

**交付物：** 对照/处理各 10 轮、实验判定和主机恢复都有新鲜结构化证据；中文文档明确
说明临时修改、恢复值、未触碰范围，以及原性能 Task 4 是否仍保持 BLOCKED。

**文件：**

- 修改：`docs/performance/PERFORMANCE_BASELINE_ZH.md`
- 不提交：`build/linux-gcc-benchmarks/governor-experiment/**`

- [ ] **步骤 1：记录实验前状态并验证工作区。**

  ```bash
  git status --short
  for cpu in 8 10; do
    cat "/sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor"
    cat "/sys/devices/system/cpu/cpu${cpu}/cpufreq/energy_performance_preference"
    cat "/sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_min_freq"
    cat "/sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_max_freq"
  done
  powerprofilesctl get
  ```

  当前预期为 governor `powersave`、EPP `performance`、min `800000`、max `5400000`、
  power profile `performance`。若实际值不同，以脚本真实快照为准，不修改规格期待值来
  隐藏环境变化。

- [ ] **步骤 2：由用户执行唯一 sudo 命令。**

  ```bash
  cd /home/zz/Jackfahdin/github/ZzPureToolsPro/ZzPureToolsPro/.worktrees/command-bar
  sudo scripts/ci/run-linux-startup-governor-experiment.sh
  ```

  控制者不得索取、记录或代填 sudo 密码。用户命令返回后先检查恢复证据，再分析性能。

- [ ] **步骤 3：先验真恢复。**

  ```bash
  evidence=build/linux-gcc-benchmarks/governor-experiment
  jq -S '{cpus,powerProfile}' "$evidence/host-state-before.json" \
    > "$evidence/before-state.normalized.json"
  jq -S '{cpus,powerProfile}' "$evidence/host-state-restored.json" \
    > "$evidence/restored-state.normalized.json"
  cmp "$evidence/before-state.normalized.json" \
    "$evidence/restored-state.normalized.json"
  for cpu in 8 10; do
    test "$(cat "/sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor")" \
      = "$(jq -r ".cpus[\"${cpu}\"].governor" \
          "$evidence/host-state-before.json")"
  done
  ```

  预期：结构化 before/restored 完全相同，实时 governor 也等于原值。若不一致，停止性能
  分析，按 transaction.log 中的 manual restore 命令恢复并再次验真。

- [ ] **步骤 4：审计实验结果。**

  ```bash
  jq -e '(.rounds | length) == 10 and
         (.passedRounds + .failedRounds) == 10' \
    "$evidence/control/summary.json" \
    "$evidence/performance/summary.json"
  find "$evidence/control" -maxdepth 1 -name 'round-*.json' -type f | wc -l
  find "$evidence/performance" -maxdepth 1 -name 'round-*.json' -type f | wc -l
  ```

  两个数量都必须恰好为 10；全部报告 commit 等于实验 HEAD、环境 fingerprint 相同。
  根据设计的三分支规则读取 transaction verdict，不自行扩大阈值或删除失败轮次。

- [ ] **步骤 5：更新中文证据说明。** 在 `PERFORMANCE_BASELINE_ZH.md` 增加
  “Governor 可逆诊断实验”小节，记录：

  - 实验 HEAD、时间、profile digest；
  - CPU 8/10 的 before、临时 performance、restored 六字段；
  - control/performance 各自通过轮数，startup 两指标 P50/P95/max 范围；
  - transaction verdict 和恢复验真结果；
  - 明确只临时修改 governor，未修改 EPP、min/max、power profile、SMT/Turbo、阈值、
    采样代码、历史 reporter 或活动 profile；
  - `build/` 原始 evidence 不进入 Git；本实验不能直接解除原性能 Task 4 阻塞。

- [ ] **步骤 6：运行最终边界验证。**

  ```bash
  test -z "$(git diff --name-only -- \
    'docs/performance/reference/linux/*.json' \
    ':!docs/performance/reference/linux/regression-thresholds.json')"
  git diff --check
  git status --short
  ```

  预期：只出现 `PERFORMANCE_BASELINE_ZH.md` 修改；没有 build evidence、历史 reporter、
  profile、SDD ledger 或 `temp_image/` 被暂存。

- [ ] **步骤 7：提交任务 4。**

  ```bash
  git add docs/performance/PERFORMANCE_BASELINE_ZH.md
  git diff --cached --check
  git commit -m "性能：记录 governor 可逆实验结果" \
    -m "记录对照与 performance 各十轮启动 P95、事务判定和原始证据位置。" \
    -m "逐项说明 CPU 临时修改与恢复验真结果，并保持阈值、基线和原发布阻塞边界不变。"
  ```

## 最终完成标准

1. 状态库对假 sysfs 的 snapshot/apply/restore/verify 有真实行为测试，拒绝无效 CPU、
   driver、字段和外部状态污染。
2. 普通用户探针保存 control/performance 各 10 份 startup reporter、比较日志和结构化
   summary；相对 FAIL 可统计，INVALID/执行错误失败关闭。
3. Root 包装器只修改 profile 指定 CPU 的 governor；Qt、Xvfb 和 benchmark 不以 root
   运行。
4. 正常、失败和信号退出共享恢复路径，before/restored 的 CPU 六字段与 power profile
   完全相等；恢复失败提供明确命令且返回非零。
5. 实验结果按 `GOVERNOR_CANDIDATE`、`GOVERNOR_INSUFFICIENT`、`INCONCLUSIVE` 三分支
   裁定，不自动修改阈值、profile、历史基线或原 Task 4 状态。
6. 中文文档逐项说明临时改动、恢复证据、性能结果和未触碰范围；build 原始 evidence
   不进入 Git。

## 计划自检

- 规格的最小变更边界由任务 1 状态库和任务 3 root 包装器覆盖；没有加入固定频率、
  CPU 隔离、SMT/Turbo 或重启操作。
- 规格的普通用户执行边界由任务 2 探针与任务 3 `runuser` 接线覆盖。
- 规格的十轮、独立 Xvfb、继续收集相对 FAIL、立即拒绝 INVALID 由任务 2 覆盖。
- 规格的 EXIT/信号恢复、幂等 restore、六字段与 power profile 验真由任务 1、3、4 覆盖。
- 规格的三分支判定及“不能直接解除发布阻塞”由任务 3、4 和最终完成标准覆盖。
- 每个新增脚本都有 bash 语法检查；核心 sysfs 行为有真实假文件测试；静态合同只承担
  root/降权/顺序等无法在普通 CTest 中安全执行的接线约束。
- 计划没有修改历史 reporter、阈值、活动 profile、采样代码、warmup 或样本数。
