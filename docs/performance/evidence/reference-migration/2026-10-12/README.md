# 活动性能基线迁移 · 2026-10-12

用户确认“归档旧基线，验证并启用新完整基线”后，保留历史文件原字节，
用同一环境、同一提交的十六份 reporter 输出建立新活动基线。
JSON 的统计值未经手工修改；归档不是跨环境性能比较。

**最终验收通过：** 锁定策略后的独立 `acceptance/` 运行 55/55 CTest、
26 项绝对门禁、16/16 场景相对比较全部通过。源码提交为
`7fffaa1030abed28f1b7b8e5feac6728548a250d`，16 份报告均为该身份。
下面同时保留校准失败、无效手动运行及其修复过程。

## 身份与原始记录

| 项目 | 记录 |
|---|---|
| 旧基线 | [`archive/2026-10-12-mesa-26.0.3`](../../../archive/2026-10-12-mesa-26.0.3/)；12 份报告、原门限及 profile |
| 原文件来源 | `35ea0718045dfa6b0d570027f92aa76da998bccb`，逐文件 `cmp` 验证相同 |
| 旧 profile SHA-256 | `18b3c573eb8ae733ed4f1992b3d0dc2d2c6e265cf975fe1975a7103701a8ec0b` |
| 新 profile SHA-256 | `d9c4d03b3c3ef2caad176ed0381eb947574b01cadfef08e86df0be9af9ffcedf` |
| 采集源码 | `1548d4b5ce0b5e498d46654d2f9461b27c057a3a` |
| 采集构建 | `linux-gcc-reference`，Release/shared/LTO，无 sanitizer |
| 正式复测构建 | `linux-gcc-benchmarks`，其他构建与环境字段一致 |
| SDK | [正常链接的 Qt 精度修复 SDK](../../qt-precision-sdk/2026-10-12/README.md)，外置安装，Qt 库摘要已核对 |
| 执行环境 | i7-14700 / Ubuntu 26.04.1 / kernel 7.0.0-38 / Mesa 26.0.8 / GLib 2.88 |
| 显示与亲和性 | Xvfb CPU 8，基准 CPU 10，1920×1080×24，xcb，DPR 1，串行无并发构建 |

`capture/` 是完整采集输出，`capture-validation.log` 是实际捕获校验器逐文件发布记录。
先在 staging 目录验证完整性，再逐字复制到活动目录；活动参考完整性契约已通过。
`framework-ldd.log` 记录最终外置 SDK 链接路径，`framework-regression.log`
记录 263/263 非性能 CTest（文档审计单独在文档完成后执行）。

| 轮次 | CTest | 原相对策略 | 审查后策略复验 |
|---|---|---|---|
| capture | 51/51；24 项绝对门禁，bootstrap 仅排除缺报告的完整性契约 | 基线采集，不计独立比较 | 25 项绝对门禁通过；自比较仅校验格式 |
| round-1 | 52/52；24 项绝对门禁 | 15/16，Tab 状态 P95 失败 | 25 项绝对门禁、16/16 相对通过 |
| round-2 | 52/52；24 项绝对门禁 | 15/16，Tab 状态 P95 失败 | 25 项绝对门禁、16/16 相对通过 |
| round-3 | 52/52；24 项绝对门禁 | 16/16 | 25 项绝对门禁、16/16 相对通过 |
| holdout-invalid-startup | 52/53；启动探针失败 | **整轮无效**，混入上轮启动文件 | 不作通过证据，见该目录 `INVALID.md` |
| holdout-final | 54/54；25 项绝对门禁 | 12/16，材质层/大模型/导航/工作区相对超限 | 失败保留，继续定向排查 |
| validation-1 | 54/54；25 项绝对门禁 | 15/16，仅活动激活 P95 +14% | 26 项绝对门禁、16/16 相对通过 |
| validation-2 | 54/54；25 项绝对门禁 | 15/16，仅活动激活 P95 +16% | 26 项绝对门禁、16/16 相对通过 |
| validation-3 | 54/54；25 项绝对门禁 | 15/16，仅活动激活 P95 +10.8% | 26 项绝对门禁、16/16 相对通过 |
| acceptance | 55/55；26 项绝对门禁 | 锁定最终策略后独立采样 | 16/16 相对通过，作为最终验收 |

原始 `compare.*.log` 没有覆盖。`revalidated/` 使用表中对应阶段的门禁逐个重放
保存的报告：capture/round 为 25 项、validation 为 26 项；它不等价于新采样。
最终独立采样见下节。holdout-final 的四项失败没有被策略校准改写为通过。

`relative-diagnostic-1` 至 `relative-diagnostic-3` 对上述四个超限场景重复测量。
原超限指标三轮均恢复到原相对门限内；活动切换在前两轮分别出现 16% / 15% 超限，
第三轮通过。它们是定向诊断，不冒充完整门禁；后续用固定三轮完整验证加
holdout-final 校准活动激活，其他超限指标的相对策略保持不变。

## 新活动参考的主要指标

这是 `capture/` 原始报告的 P95 摘要，完整指标、max、对象数与环境字段以 JSON 为准。
窗口生命周期没有单独的毫秒绝对门禁，仍检查 100 次循环、对象残留及相对耗时。

| 场景 | 指标 | 基线 P95 |
|---|---|---:|
| 组件启动 | external-total | 21.528300 ms |
| 主题切换 | latency | 7.015411 ms |
| 开关动画 | frame-time | 16.438308 ms |
| 10 万行模型 | frame-time | 1.549220 ms |
| 窗口生命周期 | lifecycle-time | 4.772092 ms |
| 导航面板 | frame-time | 8.563779 ms |
| 工作区 | workspace-render-time | 1.929566 ms |
| 标准表面 | render-time | 1.499273 ms |
| 材质层 | frame-time | 0.139343 ms |
| 径向仪表动画 | frame-time | 16.306775 ms |
| 组件空闲 | average-cpu-percent | 0% |
| Example 启动 | external-total | 127.129707 ms |
| Example 导航 | latency | 10.884383 ms |
| Example 主题切换 | latency | 9.074499 ms |
| Example 大模型 | frame-time | 0.625320 ms |
| Example 空闲 | average-cpu-percent | 0% |

## 噪声与门禁取舍

`noise-candidate.json` / `.md` 是 capture 加三轮报告的校准输出，**不是活动门限**。
工具建议的生命周期、两组绘制等降级未采用，避免相同比例退化只靠规模比值仍能通过。

第一项调整为 `workspace-components/tab-state-time.p95`：

| capture | round-1 | round-2 | round-3 | 跨轮噪声 |
|---:|---:|---:|---:|---:|
| 0.026524 ms | 0.031662 ms | 0.033249 ms | 0.023097 ms | 向上取整 44% |

该操作为“200 个标签中单个标签的四项状态更新”，采用 P95 ≤ 1 ms 绝对预算，
约占 16.7 ms 帧预算的 6%；相对 P95 改为 observe 44%。这意味着预算内的
百分比退化会记录但不阻断，不能声称仍保留 10% 灵敏度。
`tab-budget-boundary.log` 证明 1 ms 通过、1.000001 ms 拒绝。
第二项调整为 `workspace-components/activity-activation-time.p95`。
`validation-noise-candidate.json` / `.md` 使用同一源码 `63c7b37` 的四轮完整报告
（holdout-final、validation-1/2/3），其 P95 依次为
0.153810 / 0.186194 / 0.190087 / 0.180969 ms，噪声向上取整 24%。
同期代码没有更改活动激活生产实现；这证明运行值存在波动，但没有定位到具体 OS
调度来源，不能简单断言所有差异都是已查明的系统噪声。

该项为 32 个侧面板中一次同步激活与选中状态检查，不包含绘制或完整用户感知延迟。
新增独立 P95 ≤ 1 ms 预算，相对 P95 改 observe 24%；相比当前值仍有约 5–6 倍余量，
预算内的真实退化不会阻断。`activity-budget-boundary.log` 验证 1 ms 通过、
1.000001 ms 拒绝。最终仅改变这两项 P95 策略，其他相对策略保持原样，
16.7 ms 动画预算与 20 倍布局增长限制未变。

Example 空闲 CPU 有一轮从 0 量化为 0.03333163%。噪声分析器现与比较器一致：
仅 sampled-resource 且显式证明本轮绝对门禁通过时，允许零到非零量化噪声，
仍维持 gate/10%，不转 observe。无证明、确定性对象增长及非零资源 >20% 噪声仍失败。
对应契约已做先失败后通过的验证，提交 `0bde759`。

## 启动探针竞态

首次 holdout 的真实失败为 visible=1、enabled=1、exposed=1、focus=0，见
`startup-diagnostic-failure.log`：首 Paint 后的下一次事件轮次并不保证收到 XCB FocusIn。
手动 holdout 又遗漏清理旧报告，启动失败后遗留上轮 JSON；整轮因此标记无效。
正式 `run-linux-performance-gates.sh` 已有清理流程，不受该手动操作遗漏影响。

提交 `63c7b37` 保留首 Paint 时间，只在异步就绪后输出一次 JSON；等待期间每 1 ms
让出事件循环，原 5 秒 watchdog 和 external-total 300 ms 门禁保持不变。
新增测试覆盖延后焦点、真实超时、宿主销毁、观察器销毁和重复启动，
先验证原一次检查失败，再验证修复；见 `startup-readiness-*.log`。
XCB 启动基准及就绪测试各连续 30 轮通过；每轮启动 5 次预热加 30 次测量。

## 独立最终验证与复现

最终运行从空的 `build/linux-gcc-qt-precision/reports` 开始，旧报告保留到
`reports-before-acceptance`。必须同时确认 CTest 返回 0、16 份 JSON 的 commit
均为本轮 HEAD、环境字段一致，然后才能声明绝对门禁证明并运行相对比较。

在活动档案指定主机与 SDK 上、完成构建后执行：

```bash
export QT_QPA_PLATFORM=xcb
export ZZ_CMAKE_PRESET=linux-gcc-benchmarks
export ZZ_BENCHMARK_COMMIT=$(git rev-parse HEAD)
export ZZ_RUNNER_IMAGE_DIGEST="sha256:$(sha256sum docs/performance/profiles/local-release-xvfb.json | cut -d ' ' -f1)"
export ZZ_GPU_IDENTITY='Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.8 Xvfb 1920x1080x24'
env -u LD_PRELOAD -u QT_NO_GLIB -u QT_XCB_NO_MITSHM \
  taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 ctest --test-dir build/linux-gcc-qt-precision \
  -L benchmark --output-on-failure -j1
```

以上是本次隔离目录的单轮命令，运行前须归档并清空 reports，不能复用上轮 JSON。
常规三轮发布验证应将相同 SDK 配置到 `build/linux-gcc-benchmarks` 后，
在同样的 Xvfb/环境下运行 `scripts/ci/run-linux-performance-gates.sh`，
由正式脚本处理清理、归档和比较。不得在其他机器伪填此 profile digest。

本轮未重新运行远端 CI，也未验证 macOS 的原生窗口销毁；QtMultimedia 缺席，
真实音频源测试不在此次 SDK 的覆盖范围。真实桌面验收仍按平台清单进行。
