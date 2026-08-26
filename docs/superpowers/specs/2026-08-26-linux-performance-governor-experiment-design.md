# Linux 启动性能 Governor 可逆实验设计

## 背景与目标

性能阈值计划 Task 4 在固定 `local-release-xvfb` 指纹下通过了 42 项 benchmark、
绝对预算和合同测试，但 `startup/external-total.p95` 与
`startup/first-paint.p95` 随机越过 10% 相对门限。当前构建和昨夜保留的旧构建都能
复现高尾；增加 probe 预热或丢弃完整启动轮次仍不能稳定消除，因此不能把失败简单归因
于业务代码，也不能扩大阈值规避。

本实验只验证 CPU 8、10 的 `intel_pstate` governor 是否是启动 P95 高尾的重要来源。
实验必须在一个可恢复事务内临时把 governor 从当前值改为 `performance`，完成对照后
恢复全部原值。实验不修改业务代码、benchmark 采样代码、预热次数、正式样本数、相对
阈值、历史 reporter 或活动性能 profile。

## 方案选择

采用仅修改两个固定 CPU governor 的最小实验，不采用以下方案：

- 不把统计耗时改成多数表决或中位数门禁，因为这会改变已经批准的失败关闭语义。
- 不修改 `zzWarmupIterations` 或样本数，因为已有实验表明额外预热不能稳定解决问题，
  且原性能阈值计划明确禁止这类变化。
- 不设置 `isolcpus`、`nohz_full`、IRQ affinity、固定频率或关闭 SMT/Turbo；这些操作
  需要重启或扩大主机影响面，不符合本次最小、可逆实验边界。

## 组件与职责

### Root 事务包装器

新增 `scripts/ci/run-linux-startup-governor-experiment.sh`，只能通过 `sudo` 由 root
执行，并要求 `SUDO_USER`、`SUDO_UID`、`SUDO_GID` 都存在。包装器承担以下职责：

1. 从 `docs/performance/profiles/local-release-xvfb.json` 结构化读取 Xvfb CPU 和
   benchmark CPU，当前固定为 8、10，不在脚本中复制魔法数字。
2. 验证两个 CPU 在线、使用 `intel_pstate`，并记录各自的 governor、EPP、最小频率、
   最大频率以及当前 `powerprofilesctl` 档位。
3. 在原配置下以发起 sudo 的普通用户运行对照阶段。
4. 只把两个 CPU 的 governor 写为 `performance`，读取回验后以同一普通用户运行处理
   阶段。
5. 通过统一的 EXIT trap 恢复每个 CPU 原 governor，并验证 governor、EPP、最小频率、
   最大频率和系统电源档位与快照完全一致。
6. 把事务日志复制为普通用户拥有的 build evidence；任何应用、测试、恢复或验真失败
   都返回非零。

包装器不得以 root 身份运行 Qt、Xvfb 或 benchmark，不修改整个系统的 power profile，
也不修改 CPU 8/10 之外的 sysfs 项。若恢复失败，必须输出每个 CPU 的原值和明确恢复
命令，同时保持非零退出；不得打印成功状态。

### 普通用户稳定性探针

新增 `scripts/ci/run-linux-startup-stability-probe.sh`，由 root 包装器降权到原用户后
调用。探针承担以下职责：

1. 验证当前 HEAD、Qt 6.11.1、GCC 15、benchmark cache 的
   `ZZ_PERFORMANCE_REFERENCE:BOOL=ON`、现有 `ZzStartupBenchmark` 和活动 profile。
2. 为每个阶段执行 10 个独立会话。每个会话启动一个新 Xvfb，固定在 profile 的
   Xvfb CPU；启动 benchmark 固定在 profile 的 benchmark CPU。
3. 每个会话只生成一份 startup reporter JSON，并用现有
   `ZzComparePerformanceReport.cmake` 与只读 `startup.json`、
   `regression-thresholds.json` 比较。
4. 不在首个比较失败时停止，而是保存 10 份报告、逐轮比较日志和汇总 JSON，使随机
   高尾仍可审计。Xvfb 启动失败、报告缺失、INVALID、环境不匹配或绝对执行错误仍立即
   失败。
5. 每个 Xvfb 都由局部 trap 清理；不得结束或修改其他 display 的进程。

对照与处理阶段分别写入：

```text
build/linux-gcc-benchmarks/governor-experiment/control/
build/linux-gcc-benchmarks/governor-experiment/performance/
```

这些原始证据不进入 Git。

## 数据与判定

每个阶段的汇总至少记录：HEAD、profile digest、两个 CPU 的 governor/EPP/min/max、
10 个报告路径、每轮 `external-total` 和 `first-paint` 的 P50/P95/max、相对比较退出码、
通过轮数和失败轮数。

实验按以下规则裁定，不自动修改正式策略：

- 对照组少于 10/10、处理组为 10/10：governor 是可信的稳定化候选。恢复主机后停止，
  后续另写 profile/基线迁移规格；本实验不能直接解除 Task 4 阻塞。
- 处理组少于 10/10：否定“仅修改 governor 足以稳定”的假设，恢复主机后保留 BLOCKED，
  下一步调查 CPU 隔离或启动基准统计合同。
- 两组都为 10/10：实验没有复现原问题，结论为不确定；不得据此宣称门禁完成。
- 任一阶段出现环境指纹、绝对执行或恢复失败：实验失败，且恢复验证优先于所有性能
  结论。

## 安全与恢复合同

Root 包装器进入实验前必须把原始状态写入 root 专用临时目录。应用任何配置前注册
EXIT trap；SIGINT、SIGTERM、子脚本失败和正常退出都走同一个恢复函数。恢复函数必须
幂等，先恢复 governor，再验证所有快照字段。完成恢复前不得移动最终日志、打印实验
结论或退出成功。

脚本不得使用宽泛递归删除、不得操作 `temp_image/`、不得下载 Qt、不得调用 GitHub
CLI、不得 push。所有删除仅限本次创建的明确临时状态文件；build evidence 保留供审计。

## 测试与交付边界

实现采用 TDD：先扩展 `tests/Platform/ZzGateScriptContract.cmake`，让合同因缺少两个脚本、
root/降权边界、快照、trap、恢复验真、10 轮和 evidence 路径而失败；再实现最小脚本使
合同通过。两份脚本还必须通过 `bash -n`；本机存在 `shellcheck` 时必须通过其检查，
不存在时明确记录未运行且不得为本实验下载工具。

实现提交与真实 sudo 实验分开：脚本和合同先用中文提交；用户执行一次 root 事务后，
控制者读取实验输出和恢复证据，但不把 build evidence 提交。只有恢复状态与实验前快照
完全一致，才可以报告主机已恢复。
