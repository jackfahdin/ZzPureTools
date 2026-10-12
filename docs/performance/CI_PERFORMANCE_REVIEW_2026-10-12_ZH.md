# CI 与性能门禁复核 · 2026-10-12

## 结论与范围

CI 兼容性修复和“正式报告 + 绝对预算 + 同环境相对比较”的门禁方向可行，
但当前状态不能标记为性能门禁全绿。本次修复了采集启动循环、导航树隐藏投影开销、
滚轮弹出尺寸重复计算和截图版本混用；保留历史性能档案、参考 JSON 与门限不变。

审查范围为 `6a49d3a..0c455de` 的 27 个提交，以及本轮修复；重点检查
CI / Preset / CTest 接线、编译兼容性、报告完整性、环境身份、运行时行为与截图。
公开类型命名调整属于此前有意进行的 API 清理，旧调用方仍需同步名称。

## 已修复

| 问题 | 修复与验证 |
|---|---|
| 参考采集脚本不可直接执行，内联 CMake 语法错误，且先运行要求新基线存在的契约 | 修复执行权限；提取 CMake 校验；采集阶段只排除完整性契约，仍运行所有生产者和绝对门限 |
| 采集可能混入旧报告或不同环境，批次中途失败会留下不完整结果 | 先清理选定场景的生成报告；校验场景、提交、preset、schema、指标及批次/保留基线的环境指纹，全部验证后才复制 |
| 平面导航创建隐藏树模型，超过对象形状预算，并展开不需要的十万行投影 | 首次进入树模式才创建树模型；离开时解除源绑定；覆盖切换、重新进入、源变更和选择恢复 |
| 滚轮弹窗反复扫描大量文本计算宽度，1000 次事务超过 75 ms | 复用滚轮已有的尺寸缓存，保留字体和条目变化时的失效规则；原预算复验通过 |
| Qt 6.8 CI 截图覆盖共用参考图，导致 Qt 6.11 本机误报 | 共用图保留；增加按 Qt minor 和 DPR 优先选择的覆盖目录，更新时只写当前版本；未改变比较容差 |

采集契约包含成功复制、拒绝覆盖、强制覆盖、错误参数、陈旧报告、错误提交、
批次缺报告、混合环境和 CTest 失败。测试替换昂贵的构建命令，但调用真实的
报告校验/发布代码；实际性能测试另行执行，不能用契约模拟替代。

## 本机验证

本次使用 GCC 15.2 / Qt 6.11.1 / Release + LTO，
`linux-gcc-benchmarks` 配置启用 `ZZ_PERFORMANCE_REFERENCE=ON`。
完整构建通过。非性能首轮 258 项中 246 项通过，12 项截图失败；
完成版本隔离后，12 项截图在关闭更新模式下全部通过。
导航树单元测试 15/15、滚轮选择器单元测试 20/20 通过。
本轮没有重新执行 Windows / macOS 编译、Clang 静态分析或 sanitizer 矩阵。

性能运行使用真实当前环境，不复用旧档案摘要：

| 字段 | 本次诊断环境 | 保留的历史参考 |
|---|---|---|
| CPU | Intel Core i7-14700 | 同型号 |
| kernel | 7.0.0-38-generic | 7.0.0-28-generic |
| Mesa renderer | llvmpipe LLVM 21.1.8 / Mesa 26.0.8 | llvmpipe LLVM 21.1.8 / Mesa 26.0.3 |
| 显示 | Xvfb 1920×1080×24，xcb，DPR 1 | 同几何与平台 |
| 亲和性 | Xvfb CPU 8，基准 CPU 10，串行执行 | 同亲和性 |

CPU governor 保持本机 `powersave`，未修改主机电源策略。
每轮都执行完整 `-L benchmark`，没有排除失败场景或提高阈值。
初始运行 44/51 通过，暴露滚轮、导航投影、仪表动画与缺基线问题。

修复后保留三轮独立执行的原始
[报告和日志](evidence/ci-review/2026-10-12/)，编号沿用本机运行顺序 round-2 / 3 / 4。
报告中的生产代码提交为 `78fe10c`；之后的 `967125b` 只修改截图测试、辅助代码和图片。
每轮均为 49/51 通过，失败项均为仪表动画绝对预算和参考报告完整性契约。

| P95 指标（ms） | round-2 | round-3 | round-4 | 原预算 |
|---|---:|---:|---:|---:|
| 径向仪表动画帧间隔 | 16.714150 | 16.720621 | 16.708231 | ≤ 16.7 |
| 导航面板整帧 | 8.709638 | 8.858177 | 8.652520 | ≤ 12 |
| 导航十万行 reset | 17.303023 | 17.616026 | 17.392340 | ≤ 80 |

三轮中其余 23 项绝对门限均通过；这不是完整三轮发布门禁通过。
环境摘要由 [environment.json](evidence/ci-review/2026-10-12/environment.json)
原始字节生成，原始报告未转换、平均或改写数值。

### 尚未通过的性能条件

1. **径向仪表动画帧间隔。** 当前环境下 P95 仍略超 16.7 ms。
   该指标记录相邻 Paint 事件开始时间，包含 Qt 动画调度、事件循环和绘制间隔，
   不是单次 `paintEvent()` 的耗时。尚不能把轻微超限归因于某一段绘制代码，
   也不能因接近门限就算通过。后续应在受控参考环境定位调度与绘制开销。
2. **四份正式参考报告缺失。** `workspace-components`、`fluent-standard-surfaces`、
   `backdrop`、`radial-gauge-animation` 已声明门限但尚未建立对应参考报告，
   `platform.performance-threshold-contract` 正确失败关闭。
3. **旧、新环境不能做相对比较。** 用本轮导航报告执行真实比较器，得到
   `INVALID navigation-pane environment;gpu`，明确拒绝 Mesa 26.0.3 与 26.0.8 混比。
   按负责人选择保留历史基线，相对门禁标记为待复核。

要恢复正式门禁，需要回到匹配的原参考环境补采缺失报告并执行三轮比较，
或另行决定完整迁移参考环境、审核全部新报告。不能只补四个新环境报告到旧的十二份中。

## 托管 CI 证据

- [a9396b1 的完整成功矩阵](https://github.com/jackfahdin/ZzPureTools/actions/runs/38049701780)
  已发布 `Continuous Build a9396b1edd3d`，五个平台下载资产实际存在。
- [0c455de 的新增门禁运行](https://github.com/jackfahdin/ZzPureTools/actions/runs/38198884242)
  中，已检查的 Linux 与 macOS arm64 都完成编译，但分别在 257 / 234 项 CTest 中
  仅失败 `platform.performance-threshold-contract`，错误为
  `Formal threshold scenarios differ from reporters`。
- 因此“之前的编译矩阵全绿”和“新增性能集成尚未就绪”可以同时成立。
  本轮本地提交尚未推送，不宣称这些提交已经过远端矩阵验证。

## 截图来源与审查边界

三套截图采用 `linux/qt-<major>.<minor>/dpr-<scale>/` 覆盖，逐文件回退到
`linux/dpr-<scale>/`；Qt 6.8 总览原有的专用目录继续生效。
普通比较不会写参考图；控件/工作区更新使用 `ZZ_UPDATE_SCREENSHOTS`，
Example 使用独立的 `ZZ_UPDATE_EXAMPLE_SCREENSHOTS`。

- Fluent 的 35 张 Qt 6.11 覆盖图取自 `bc762e6^` 历史原始 PNG，未重新绘制；
  其中总览与本机实际输出逐像素相同。
- Workspace 144 张、Example 12 张 Qt 6.11 图由当前实现重新采集。
  旧图中树/表格行高与首页 29 页信息已经过时：`ca67d53` 统一了 32 px 数据视图行高，
  当前首页按路由注册表显示 41 页。此次同步同时解决版本隔离与已有视觉变更漏更新，
  不能笼统解释成 Qt 渲染差异。
- 检查了四档 DPR 的所有场景接触表，抽查窄窗、RTL、高对比与 Example 原图，
  未发现新增内容缺失或布局错位。Qt 6.8/common 图片未改动。
- Example 快捷卡片尾箭头在深色/高对比下对比度偏低，是 `324826f` 起就存在的
  非阻断问题；此次未扩大到主题图标修改。仍需后续改善，不能将截图通过理解为
  所有视觉或无障碍问题均已解决。

## 可复现命令

在已经配置并构建的本机诊断目录中，使用真实环境身份运行：

```bash
export QT_QPA_PLATFORM=xcb
export ZZ_BENCHMARK_COMMIT=$(git rev-parse HEAD)
export ZZ_RUNNER_IMAGE_DIGEST="sha256:$(sha256sum docs/performance/evidence/ci-review/2026-10-12/environment.json | awk '{print $1}')"
export ZZ_GPU_IDENTITY='Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.8 Xvfb 1920x1080x24'
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 ctest --preset linux-gcc-benchmarks \
  -L benchmark --output-on-failure -j1
```

此命令只适用于表中这台诊断环境，不是跨机器通用的身份配置。
正式参考采集应在匹配环境中使用
`scripts/ci/capture-linux-performance-reference.sh`；脚本仍会拒绝绝对门限失败和环境混用。
