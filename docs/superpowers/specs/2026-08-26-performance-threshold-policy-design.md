# 性能阈值语义化策略设计

**状态：** 设计已确认。用于解除
`docs/superpowers/plans/2026-08-25-deferred-side-panel-performance.md` 任务 3 中由单次
调度尖峰造成的阻塞，并形成后续性能场景可复用的统一门禁合同。

**目标：** 发布门禁优先阻止可重复的性能回归；统计耗时中的单次系统调度尖峰必须
保留为可审计证据，但不得单独阻断发布。确定性结构指标、资源指标、绝对性能预算和
环境一致性继续失败关闭。

## 1. 背景与证据

当前相对比较器对每个 reporter 指标的 `p95` 和 `max` 分别执行 `gate` 或 `observe`。
正式策略已经允许经三轮校准后把高噪声字段降为 observe，但配置没有声明指标语义，
仍可能把统计耗时的 max 当成确定性结构合同。

本轮最终候选在固定 Xvfb/xcb 环境中的三轮数据为：

| 场景/指标 | 历史 P95/max | 候选三轮 P95 | 候选三轮 max |
|---|---:|---:|---:|
| `animation/frame-time` | 16.604626 / 16.697546 ms | 16.587064 / 16.560014 / 16.520636 ms | 20.744784 / 19.942989 / 19.893981 ms |
| `example-startup/external-total` | 72.994552 / 73.026990 ms | 78.215545 / 78.042675 / 78.142236 ms | 80.398471 / 82.952843 / 78.497144 ms |

Animation 的 P95 比历史基线快约 0.1% 至 0.5%，只有 max 高约 19% 至 24%。参考提交
在同一环境重新采样时，animation max 的三轮跨度达到 20.76%，而候选与参考提交的
animation 实现没有差异。Example startup P95 稳定回归约 6.9% 至 7.2%，仍小于既有
10% 相对上限；max 是否通过由 30 次独立进程启动中的一个调度长尾决定，三轮只有一轮
通过。

这些证据不支持修改历史 reporter JSON，也不支持提高统一 10% P95 门限；它们支持把
统计耗时的 max 从发布阻断项改成观察项。

## 2. 方案选择

采用“按指标语义分类”的配置驱动策略：

- 统计耗时以 P95 判断可重复回归，max 只记录调度尖峰；
- 确定性结构指标的 P95 和 max 都继续阻断；
- 采样资源指标的 P95 和 max 都继续阻断；
- 绝对性能预算始终阻断，不受相对 max 策略影响。

不采用以下方案：

- 不为 animation 或 example startup 写场景名特例。特例无法覆盖后续新增的启动、绘制、
  路由或布局耗时，并会让比较器逐渐了解业务名称；
- 不采用“三轮两轮通过”的多数表决。该规则可能掩盖只在特定轮次稳定出现的 P95 回归，
  且需要额外定义轮次缺失、无效和部分失败的组合语义；
- 不继续要求所有 max 严格 gate。专用 CPU 隔离参考机可以降低噪声，但当前发布机是
  同时承担桌面工作的物理主机，现有证据证明该要求会持续误报。

## 3. 指标语义合同

`docs/performance/reference/linux/regression-thresholds.json` 升级为 schema version 2。
每个 metric 必须增加 `metricKind`，允许值固定为：

- `statistical-duration`：由多次计时样本组成的耗时分布，例如启动、帧、绘制、路由、
  主题、布局和状态更新时间；
- `deterministic`：由算法或对象模型决定的计数或结构合同，例如对象数、请求行数、
  paint 次数和缓存项；
- `sampled-resource`：操作系统或进程采样得到的 CPU、RSS 等资源指标。

每个 metric 仍必须显式声明 `p95` 和 `max` 的 `{mode, percent}`，不能依赖默认值。示例：

```json
{
  "frame-time": {
    "metricKind": "statistical-duration",
    "p95": { "mode": "gate", "percent": 10 },
    "max": { "mode": "observe", "percent": 21 }
  }
}
```

比较器必须验证以下不变量：

| `metricKind` | P95 | max |
|---|---|---|
| `statistical-duration` | 默认 `gate` 且不超过 10%；只有既有三轮证据证明 P95 本身不稳定时才允许 `observe` | 必须为 `observe` |
| `deterministic` | 必须为 `gate`，相对门限不得超过 20% | 必须为 `gate`，相对门限不得超过 20% |
| `sampled-resource` | 必须为 `gate`，相对门限不得超过 20% | 必须为 `gate`，相对门限不得超过 20% |

既有 `theme-switch/latency.p95` 已有三轮 66% 噪声证据，继续作为有证据的 P95 observe
例外；其绝对 P95 50 ms 预算仍严格阻断。任何新增 P95 observe 必须先把至少三轮原始
报告和校准结论写入 `docs/performance/PERFORMANCE_BASELINE_ZH.md`，再单独修改策略。

Observe 的 `percent` 表示已记录噪声带，不表示允许回归。当前值超过噪声带时输出醒目
告警但返回成功；未超过时也必须输出基线、当前值、变化率和策略，不能静默丢弃 max。

## 4. 绝对预算与相对策略边界

绝对预算继续由现有 `ZzVerifyPerformanceReport.cmake` 和 reference gate 维护，包括：

- 启动 P95/max 不超过 300 ms；
- 主题和页面切换 P95 不超过 50 ms；
- 动画与大模型帧 P95 不超过 16.7 ms；
- 导航整帧 P95 不超过 12 ms；
- idle CPU 严格小于 0.5%，RSS 增长不超过 10%；
- 工作区结构操作 P95、render P95、规模比值、对象和缓存合同。

本轮不把绝对预算复制进 `regression-thresholds.json`。相对比较器回答“相对同环境历史
基线是否退化”，绝对 verifier 回答“产品是否仍满足固定预算”；两者都必须通过，observe
不能覆盖绝对失败。这样避免同一绝对阈值同时存在于 CMake 和 JSON 中并发生漂移。

历史 reference reporter JSON 保持只读。schema version 2 只属于阈值策略文件，不改变
reporter 的 schema version、样本、commit、环境指纹或任何历史 metric。

## 5. 判定流程与输出

单轮比较固定按以下顺序执行：

1. 校验 baseline、current 和 threshold JSON 的 schema、场景、指标、单位及完整字段；
2. 校验 commit 格式、preset 兼容关系和全部环境指纹；
3. 校验每个 metric 的 `metricKind`、P95/max 配置及语义不变量；
4. 对每个字段计算允许上限和相对变化，执行 gate 或 observe；
5. 输出每个字段的基线、当前值、变化率、策略、噪声带和判定；
6. 输出整场景最终状态。

输出状态固定为：

- `PASS`：所有强制相对门禁通过，可以继续检查绝对预算；
- `OBSERVE`：强制门禁通过，但至少一个观察字段越过记录噪声带；
- `FAIL`：数据有效，但至少一个强制相对或绝对门禁失败；
- `INVALID`：schema、策略、样本身份或环境指纹无效，禁止把它解释为性能回归。

CMake 进程对 `FAIL` 和 `INVALID` 返回非零；`PASS` 和 `OBSERVE` 返回零。错误信息必须
带场景、metric 和 field，不能只输出通用失败。现阶段无需新增持久化汇总格式，终端日志
是规范输出；原始 reporter JSON 继续作为数值证据。

## 6. 三轮发布验收

策略比较器保持一次消费一对 baseline/current 报告。发布性能验收在同一个固定 Xvfb/xcb
shell 中连续执行三轮，每轮写入独立目录，禁止覆盖前一轮。每轮都必须先通过环境指纹和
报告结构校验，再独立执行相对比较与绝对预算。

三轮汇总规则为：

- 任一轮 `INVALID`，整体验收无效；
- 任一轮统计耗时 P95 gate、确定性指标、资源指标或绝对预算失败，整体验收失败；
- 只有统计耗时 max 越过 observe 噪声带时，整体验收通过并保留三轮告警；
- 不使用平均值、多数表决或删除离群样本挽救失败结果。

门禁脚本必须保留每轮完整命令和报告路径，使人工能够追溯是哪一个样本分布造成告警。
普通开发者的单轮定向测试仍可快速运行，但不得以单轮结果替代最终发布验收。

## 7. 迁移范围

本轮修改只包含：

- 升级阈值策略 schema，并为现有全部 metric 声明 `metricKind`；
- 把所有统计耗时 max 统一迁移为 observe，保留有三轮证据的既有噪声带；
- 修改比较器的 schema/语义校验、逐字段输出和最终状态；
- 扩展阈值合同测试、比较器正反例和门禁脚本合同；
- 更新性能基线文档，解释新策略和本轮迁移依据；
- 在固定 Xvfb/xcb 环境重新运行三轮最终验证。

本轮不修改 benchmark 采样代码、预热次数、正式样本数、历史 reference JSON、绝对预算、
产品实现、Qt/C++ 版本或平台矩阵。Windows MSVC、Windows MinGW 和 macOS 只进行
CMake/JSON/公共脚本静态合同检查；真实性能结论仍只来自已审定 Linux 发布机。

## 8. 测试与验收合同

实现必须先增加失败测试，再修改策略或比较器。至少覆盖：

1. schema version 1 阈值文件被明确拒绝；
2. 缺失或未知 `metricKind` 返回 `INVALID`；
3. 统计耗时 P95 超过 10% 返回 `FAIL`；
4. 统计耗时只有 max 超过噪声带时返回 `OBSERVE` 且进程成功；
5. 统计耗时 max 配置成 gate 被拒绝；
6. 确定性或资源指标 max 超限返回 `FAIL`；
7. 确定性或资源指标配置成 observe 被拒绝；
8. 绝对预算超限仍由 reference gate 返回失败；
9. 环境指纹、场景、单位或 metric 集合不一致返回 `INVALID`；
10. 每个字段都输出基线、当前值、变化率和策略；
11. threshold 必须完整覆盖 reporter metric，reporter 新增指标但策略未增加时失败关闭；
12. 三轮汇总中 P95/确定性/资源/绝对预算任一轮失败都会阻断，只有统计耗时 max 告警
    时三轮通过。

最终验收使用当前已固定的 Qt 6.11.1、GNU 15.2、Release/shared/LTO、Xvfb/xcb、相同
CPU affinity、runner digest、GPU identity、DPR 和刷新率。预期 animation 三轮 P95 继续
通过、max 作为 observe 保存；example startup 三轮 P95 继续在 10% 内、max 不再单独
阻断。任何实现前无法预知的新 P95 或绝对预算失败必须继续定位，不能借本策略放行。
