# GLib 默认事件循环的 Qt 定时精度修复验证

保留 GLib 后端、XCB 共享内存和原基准后，候选 Qt 修复把仪表 P95 从原实现的
16.753–16.764 ms 降到 16.226–16.229 ms。四轮 ABBA 对照中，候选两轮低于
原 16.7 ms 门限；进程 CPU 占用与对照组同为约 1%。这是独立候选库的验证，
没有替换安装的 Qt SDK，也没有把诊断结果提升为正式参考。

## 修复位置与行为

Qt 6.11.1 的 `timerWait()` 先将等待向上取整到毫秒，GLib 分发器又通过
`GSource.prepare` 的整数毫秒参数交给 GLib。本机 GLib 2.88 的默认等待实现
已经可以用 `ppoll` 消费微秒截止时间，Qt 的传递方式损失了这部分精度。

本候选由两份补丁组成，顺序应用到配套 Qt 源码副本：

1. [保留 timerWait 纳秒精度](../../qt-timer-precision/2026-10-12/qtbase-6.11.1-timer-wait.patch)。
2. [GLib ready_time 补丁](qtbase-6.11.1-glib-ready-time.patch)：用微秒截止时间替代
   毫秒 prepare 参数；普通和空闲定时源分别管理自身截止时间。禁用、无待激活
   定时器、优先级让出和 dispatch 时清除；同一 Qt 绝对目标只映射一次，防止自唤醒。

不假定 C++ 与 GLib 两种单调时钟具有相同起点。实际到期激活仍由原 Qt 定时器列表
处理，CoarseTimer/VeryCoarseTimer 的取整与合并规则不变。
补丁使用的 ready_time API 要求 GLib ≥ 2.36；仅在 2.88 上验证性能，旧版 GLib
或自定义 poll 函数仍可能按毫秒等待。未验证其他平台，也未构建完整修改版 Qt SDK。

## ABBA 数据与 CPU 成本

| 轮次 | GLib 实现 | 仪表 P95（ms） | 开关 P95（ms） |
|---|---|---:|---:|
| 1 | original | 16.753429，超限 | 16.613331 |
| 2 | candidate | 16.229435 | 16.410104 |
| 3 | candidate | 16.225988 | 16.459490 |
| 4 | original | 16.764049，超限 | 16.631064 |

所有原始报告、入口标记、数值校验和 CPU 时间均位于 `abba/`：每个 JSON 附有
`.run.log`、`.gate.log`、`.time.log`。仪表每轮 1000 个样本，开关每轮 1200 个样本。
GNU time 记录的进程 CPU 占用均为 1%（整数显示，包含基准预热/采样/收尾，
不包含 Xvfb），不能据此声称 CPU 成本逐周期完全相同。
候选仍有 18.654 ms 的最大仪表间隔，未消除所有抖动，也没有完成正式三轮门禁。

同一台 i7-14700 / Ubuntu 26.04.1 / kernel 7.0.0-38 / Mesa 26.0.8，
Qt 6.11.1 / GCC 15.2 Release + LTO；原基准二进制对应
`d583f1d319c3a5102f53e237e8276493374a3ea2`。Xvfb 固定 CPU 8，基准固定 CPU 10，
分辨率 1920×1080×24，DPR 1，报告刷新率 60 Hz；采样时没有并行构建。

两组均未设置 `QT_NO_GLIB` / `QT_XCB_NO_MITSHM`，使用相同 SDK 的 XCB 插件。
通过 `LD_PRELOAD` 分别加载独立编译的原版/候选 GLib 分发器与 Qt 定时器列表。
两组都有相同的入口标记以及线程数据访问适配：SDK 隐藏了
`QThreadData::current()` 的内部辅助符号，诊断工程改用
`QThreadData::get2(QThread::currentThread())` 取得同一数据；该适配不属于 Qt 补丁。
八份运行日志均确认真正进入了相应分发器。原报告 schema 未编码注入配置，
须结合本说明和 [manifest.json](manifest.json) 解读，禁止当作正式参考比较。

## 忙等回归与事件循环检查

首次候选在 prepare 中反复清除、重设截止时间，GLib 每次变化都会唤醒 context。
新增底层 poll 计数在 30 ms 内实测 27850 次，原实现只有 2 次，证实了忙等。
这批早期性能数据已弃用，不纳入表格。修正后缓存不变的绝对目标，同一检查为
3 次 poll；[失败记录](self-wakeup-red.log) 保留了问题复现。

检查实际执行 Qt 源码中的 prepare/check/dispatch，覆盖微秒精度、目标稳定性、
更早定时器插入与删除、重新启用、优先级切换、禁用、过期、激活中、空列表及清理。
事件循环测试验证重复与嵌套定时器、跨线程投递、Socket 通知、停止/删除、
不提前触发，以及底层 poll 不自旋。poll 计数阶段使用转发到 `g_poll` 的自定义回调，
只验证忙等；[独立系统调用追踪](runtime-waits.log) 另行确认默认路径调用
非整毫秒的 `ppoll`，例如 9.978 ms。性能基准没有启用追踪或自定义 poll。

源码原版测试预期因未提供微秒截止时间而失败；手工设置旧 ready_time 后的清理检查
是候选防回归要求，不能把原版各项失败解释为多个既有生产缺陷。测试没有反转
原版退出码，也没有注册到框架正式测试集。

最终 [CTest 记录](contract-tests.log)：候选 15 项定时源检查通过，原版和候选各
9 项事件循环检查通过；原版定时源测试按预期失败，因此独立 CTest 整体返回 8。
两份补丁在配套 Qt 6.11.1 源码副本上实际应用并构建，原源码上的
`git apply --check` 通过。独立审查发现的忙等问题已修正并复核。
候选补丁与诊断工程提交为 `23c0abf`。

## 恢复原 SDK 的正式复跑

移除注入、未设置事件循环/共享内存开关，重新生成 startup、animation 和
radial-gauge-animation 报告，再执行原绝对门限与参考完整性契约：6 项测试
4 项通过、2 项失败，详见 [formal-default](formal-default/)。

- 仪表 P95 **16.759788 ms**，仍超过 16.7 ms。
- 开关 P95 **16.622811 ms**，本轮通过。
- 仍缺 `backdrop`、`fluent-standard-surfaces`、`radial-gauge-animation`、
  `workspace-components` 四份历史参考。

这是未修改 SDK 的最新正式数值，不能用上面的候选结果覆盖。历史 Mesa 26.0.3
参考与当前 Mesa/kernel 环境仍不一致，相对门禁未执行。框架的 7 项相关门禁契约
及架构审计通过，但不等于性能门禁全绿。

正式复跑命令（身份变量使用上面同一台机器的真实值）：

```bash
env -u LD_PRELOAD -u QT_NO_GLIB -u QT_XCB_NO_MITSHM -u ZZ_DIAGNOSTIC_PRECISE_WAIT \
  QT_QPA_PLATFORM=xcb ZZ_CMAKE_PRESET=linux-gcc-benchmarks \
  taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 ctest --preset linux-gcc-benchmarks \
  -R '^(benchmark\.(startup|animation|radial-gauge-animation|reference-gate\.(animation|radial-gauge-animation))|platform.performance-threshold-contract)$' \
  --output-on-failure -j1
```

落地到发布环境还需要使用两份补丁构建独立 Qt SDK，完成完整兼容性验证并登记
新 SDK 的环境身份；随后重新执行正式门禁。不能通过生产 `LD_PRELOAD`、禁用 GLib
或修改报告指纹来代替这一步。本次交付的是经过本机验证的 Qt 候选修复及证据。

## 复现

从仓库根目录执行。临时构建目录由 CMake 复制需要的 Qt 源码并应用补丁，不修改 SDK。
需要 Qt 6.11.1 配套源码/私有头、GLib 开发包、Ninja、Git 和 pkg-config；路径按机器调整。

```bash
probe_source="$PWD/docs/performance/evidence/qt-glib-precision/2026-10-12"
probe_root=$(mktemp -d /tmp/zz-qt-glib-XXXXXXXX)
cmake -S "$probe_source" -B "$probe_root/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++-15 \
  -DCMAKE_PREFIX_PATH=/home/zz/Qt/6.11.1/gcc_64 \
  -DZZ_QTBASE_SOURCE_DIR=/home/zz/Qt/6.11.1/Src/qtbase
cmake --build "$probe_root/build" -j4
env -u LD_PRELOAD -u QT_NO_GLIB \
  ctest --test-dir "$probe_root/build" -R 'runtime|source-candidate' -V
# 单独检查原版；预期返回非零，不要当作整个测试集全绿。
env -u LD_PRELOAD -u QT_NO_GLIB \
  ctest --test-dir "$probe_root/build" -R '^qt.glib-source-original$' -V
```

ABBA 使用原 `linux-gcc-benchmarks` 构建产物，不要在采样期间编译。需要 CPU 8/10、
Xvfb、GNU time、rg 和 jq。设置与实际基准提交/机器对应的三个身份变量；本次机器身份
沿用[环境记录](../../ci-review/2026-10-12/environment.json)，GLib 和诊断库信息见 manifest。

```bash
: "${ZZ_BENCHMARK_COMMIT:?set the actual benchmark commit}"
: "${ZZ_RUNNER_IMAGE_DIGEST:?set the actual environment identity}"
: "${ZZ_GPU_IDENTITY:?set the actual GPU identity}"
export ZZ_BENCHMARK_COMMIT ZZ_RUNNER_IMAGE_DIGEST ZZ_GPU_IDENTITY
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  bash "$probe_source/run-abba.sh" "$PWD" "$probe_root/build" "$probe_root/abba"
```

脚本要求新输出目录。基准失败或缺少分发器入口标记会中止；数值门限失败写入日志，
保留后续对照轮次。正式脚本会拒绝 `LD_PRELOAD`，请勿为运行本实验移除这项保护。

Qt 派生源码只在临时构建目录生成，保留上游版权及许可头；分发候选二进制需遵守
相应 Qt 许可。此目录不提供生产私有 ABI 注入方案。
