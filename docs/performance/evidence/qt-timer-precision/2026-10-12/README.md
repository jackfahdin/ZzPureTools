# Qt 6.11.1 定时等待精度诊断

本次在 UNIX 事件分发器中验证：`QTimerInfoList::timerWait()` 把剩余等待时间向上
取整到毫秒，是仪表帧间隔超限的一个可复现因素。保留纳秒精度后，两轮仪表 P95
从对照组的 16.723–16.740 ms 降到 16.220–16.238 ms。

这不是正式门禁通过记录。默认 GLib 分发器在消费等待时间时仍向上取整；
候选补丁尚未应用到 SDK，也没有完成 Qt 各平台构建验证。
历史参考、16.7 ms 预算、生产动画和采样逻辑均未修改。

## 对照方法与结果

- Qt 6.11.1、GCC 15.2、Release + LTO；框架及原基准二进制对应
  `d583f1d319c3a5102f53e237e8276493374a3ea2`。
- Ubuntu 26.04.1、kernel 7.0.0-38、Intel i7-14700、Mesa llvmpipe LLVM 21.1.8 /
  Mesa 26.0.8；Xvfb 1920×1080×24，XCB，DPR 1，报告刷新率 60 Hz。
- Xvfb 固定 CPU 8，基准固定 CPU 10，串行运行，采样期间没有构建任务。
- 两组都启用 `QT_NO_GLIB=1`，保留 XCB 共享内存，两组注入同一个诊断库。
  唯一分组开关为 `ZZ_DIAGNOSTIC_PRECISE_WAIT`：未设置时保持原毫秒取整，
  设置时保留剩余纳秒。顺序为取整 / 精确 / 精确 / 取整（ABBA）。
- 每组依次执行原仪表基准和原开关基准，分别采集 1000 和 1200 个帧间隔。
  没有用单帧 Paint 耗时替换 `frame-time`，没有调用正式参考比较器。

| 轮次 | 等待方式 | 仪表 P95（ms） | 开关 P95（ms） |
|---|---|---:|---:|
| 1 | rounded | 16.740077，超限 | 16.604624 |
| 2 | precise | 16.237982 | 16.502516 |
| 3 | precise | 16.220062 | 16.522669 |
| 4 | rounded | 16.722756，超限 | 16.612606 |

表内“超限”仅按 16.7 ms 数值比较。候选组仍存在约 19–20 ms 的最大间隔，
不能声称消除了调度抖动或 X11 同步慢帧。两轮候选结果也不等同于正式三轮验收。

原始报告按 `<轮次>-<模式>-<场景>.json` 保存，相邻 `.run.log` 记录替换函数
实际执行的 `diagnostic timerWait active: rounded/precise` 标记，`.gate.log`
保存原数值校验器输出。报告 schema 没有记录上述注入和事件分发器开关，
必须与本说明及日志一起解读，不能直接提升为正式参考。

## 为什么这次探针有效

此前只替换 `timerWait()` 的尝试未生效：QtCore 内部直接调用没有经过可替换入口。
本目录 [CMakeLists.txt](CMakeLists.txt) 同时编译 SDK 配套源码中的
`qeventdispatcher_unix.cpp`、对应 moc 和 [timer-wait.cpp](timer-wait.cpp)，
让 XCB 插件使用诊断库中的 UNIX 分发器。八次运行均在替换函数入口打印标记，
因此本次对照与[此前未生效的探针](../../frame-timeline/2026-10-12/README.md)有明确区别。
这是限定 SDK 的进程级诊断，依赖私有 ABI，不属于框架实现或公开接入方案。

## 精度契约与候选补丁

[timer-wait-test.cpp](timer-wait-test.cpp) 覆盖空列表、未来截止时间、过期截止时间、
跳过激活中的首个定时器及全部处于激活状态五种情况。实跑结果：

- 原 SDK：三个边界检查通过，两个精确剩余时间检查失败。
- 注入候选实现：五个检查全部通过。
- 独立 CTest 整体返回 8，因为原实现测试刻意保留失败；未使用 `WILL_FAIL`
  反转结果，也未把这个诊断工程加入框架正式测试集。

[Qt 候选补丁](qtbase-6.11.1-timer-wait.patch) 将毫秒取整移到消费端边界：
`timerWait()` 保留精度，WebAssembly 的两个毫秒消费者显式使用 `ceil`，
避免原 `duration_cast` 截断纳秒后提前唤醒。GLib 原本已有 `ceil`，所以此补丁
不能解决默认 GLib 路径的等待精度。UNIX/CoreFoundation 消费者可保留更高精度；
本次只构建并运行 Linux UNIX 诊断，WASM/CoreFoundation 仅做源码审查。
补丁在 Qt 6.11.1 配套源码上通过 `git apply --check`，没有实际修改安装目录。

`timer-wait.cpp` 改编自 Qt 源码，保留 Qt/Intel 版权及 LGPL-3.0-only 标识。
构建引用本机提供的 Qt 分发器源码与私有头；分发派生二进制时须遵守相应 Qt 许可。

## 复现精度检查

以下在仓库根目录执行，路径是本次机器配置，其他机器需替换 SDK、源码和编译器。
使用新临时目录，不修改 SDK：

```bash
probe_source="$PWD/docs/performance/evidence/qt-timer-precision/2026-10-12"
probe_build=$(mktemp -d /tmp/zz-qt-timer-precision-XXXXXXXX)
cmake -S "$probe_source" -B "$probe_build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++-15 \
  -DCMAKE_PREFIX_PATH=/home/zz/Qt/6.11.1/gcc_64 \
  -DZZ_QTBASE_SOURCE_DIR=/home/zz/Qt/6.11.1/Src/qtbase
cmake --build "$probe_build"
env -u LD_PRELOAD -u ZZ_DIAGNOSTIC_PRECISE_WAIT \
  ctest --test-dir "$probe_build" --output-on-failure
git -C /home/zz/Qt/6.11.1/Src/qtbase apply --check \
  "$probe_source/qtbase-6.11.1-timer-wait.patch"
```

上面的 CTest 在原 SDK 下预期返回非零，应逐项检查原实现失败、候选通过；
不能将整个诊断测试集描述为全绿。

## 复现 ABBA 采样

先构建仓库的 `linux-gcc-benchmarks` 原始基准，再完成上面的诊断库构建。
需要 `taskset`、`xvfb-run`、`rg`、`jq` 及可用 CPU 8/10。
为 `ZZ_BENCHMARK_COMMIT` 填写实际基准对应的提交，为 `ZZ_RUNNER_IMAGE_DIGEST`
和 `ZZ_GPU_IDENTITY` 填写实际环境身份，不能为了匹配旧报告伪造值。
这些变量在本次存档中的值见各原始 JSON；本机环境来源见
[环境记录](../../ci-review/2026-10-12/environment.json)。

以下沿用上一节的 `probe_build`，输出到新目录。仅子进程启用诊断配置：

```bash
: "${ZZ_BENCHMARK_COMMIT:?set the actual benchmark commit}"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the actual environment identity}"
: "${ZZ_GPU_IDENTITY:?set the actual GPU identity}"
export ZZ_BENCHMARK_COMMIT ZZ_RUNNER_IMAGE_DIGEST ZZ_GPU_IDENTITY
diagnostic_output=$(mktemp -d /tmp/zz-qt-timer-abba-XXXXXXXX)
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  bash -s -- "$PWD" "$probe_build" "$diagnostic_output" <<'DIAGNOSTIC'
set -euo pipefail
repo=$1
probe_build=$2
output_dir=$3
unset QT_XCB_NO_MITSHM LD_PRELOAD ZZ_DIAGNOSTIC_PRECISE_WAIT
export QT_QPA_PLATFORM=xcb QT_NO_GLIB=1
export ZZ_CMAKE_PRESET=linux-gcc-benchmarks
run=0
for mode in rounded precise precise rounded; do
  run=$((run + 1))
  unset ZZ_DIAGNOSTIC_PRECISE_WAIT
  if [[ $mode == precise ]]; then export ZZ_DIAGNOSTIC_PRECISE_WAIT=1; fi
  for scenario in radial-gauge-animation animation; do
    binary=ZzGaugeAnimationBenchmark
    if [[ $scenario == animation ]]; then binary=ZzAnimationBenchmark; fi
    report="$output_dir/$run-$mode-$scenario.json"
    taskset -c 10 env LD_PRELOAD="$probe_build/libdispatch-probe.so" \
      "$repo/build/linux-gcc-benchmarks/benchmarks/$binary" --report "$report" \
      2>&1 | tee "$report.run.log"
    rg -Fx "diagnostic timerWait active: $mode" "$report.run.log"
    gate_result=0
    cmake "-DZZ_REPORT=$report" "-DZZ_SCENARIO=$scenario" -DZZ_METRIC=frame-time \
      -DZZ_MAX_P95=16.7 -P "$repo/cmake/ZzVerifyPerformanceReport.cmake" \
      > "$report.gate.log" 2>&1 || gate_result=$?
    printf 'run=%s mode=%s scenario=%s gate=%s\n' "$run" "$mode" "$scenario" "$gate_result"
    jq -c '.metrics["frame-time"]' "$report"
  done
done
DIAGNOSTIC
```

对照组数值超限会记录为非零 `gate`，不会中断后续轮次；缺少实际入口标记或
基准执行失败会中止实验。不要通过正式 runner 执行注入实验。

## 正式门禁的防混用修复

`1cf79f1` 给 Linux 门禁、性能复跑、正式参考采集三个入口增加环境检查：
拒绝非空 `QT_NO_GLIB` / `LD_PRELOAD`，拒绝设置了 `QT_XCB_NO_MITSHM`
的环境（空值和 `0` 也会禁用共享内存）。提前返回 64 并提示应取消的变量，
避免未纳入报告指纹的诊断配置被当成正式配置；不打印变量值。
24 个入口组合测试与既有 runner/capture/comparison 契约、脚本契约、架构审计
合计 7 项 CTest 通过，独立审查无阻断项。

最新默认环境的正式数值仍是仪表 16.759650 ms 失败、开关 16.615112 ms 本轮通过，
见[上轮原始证据](../../frame-timeline/2026-10-12/README.md)。本次没有重新运行默认
正式基准。四份参考缺失、历史 Mesa/kernel 环境不匹配仍未解除。
下一步应评估默认 GLib 路径的高精度等待方案或单独审核 UNIX 性能配置，
再决定 Qt 层修复如何落地；不能把本次私有 ABI 注入带入生产。
