# Linux 性能基线状态

## 当前结论

正式 Linux 参考基线尚未建立，状态为阻断。

`docs/performance/reference/linux/` 下的六份 JSON 在参考机完成审核前必须保持缺失。不得把 `build/` 中的本机报告复制到该目录，也不得手工修改 reporter 输出。本状态不会影响普通开发机验证采样器和比较器，但所有依赖正式参考指纹的绝对门禁与相对回归门禁都必须失败。

阻断原因如下：

- 尚未指定并审核 Linux 参考机及 immutable runner image digest。
- 当前会话是 Ubuntu 26.04 LTS 上的 X11 转发，GPU 为未加速的 Mesa llvmpipe，不符合 Ubuntu 22.04 发布构建与稳定硬件渲染环境要求。
- 当前六个候选报告不是在同一 commit、同一 `linux-gcc-reference` preset 下生成，不能组成可比较基线。

## 本机候选证据

以下信息只证明采样代码已运行，不是性能承诺，也不得用作 CI 基线：

| 字段 | 本机观测值 |
|---|---|
| CPU | Intel Core i7-14700，1 socket，20 core，28 logical CPU |
| RAM | 32,708,890,624 bytes |
| GPU/驱动 | Mesa llvmpipe，LLVM 21.1.8，Mesa 26.0.3，未加速 |
| 显示 | xcb，60 Hz，DPR 1.0，X11 转发 display |
| OS | Ubuntu 26.04 LTS |
| Qt | 6.11.1 |
| 编译器 | GCC 15.2.0 |
| libstdc++ | `libstdc++.so.6.0.35` |
| 构建 preset | `linux-gcc-benchmarks` |
| 空闲预热/测量 | 5 秒 / 30 秒 |
| 空闲 CPU | 0%，单样本 |
| RSS 起始/结束 | 42,450,944 / 42,450,944 bytes |
| RSS 增长 | 0%，单样本 |
| 原始候选报告 | `build/linux-gcc-benchmarks/reports/benchmark.*.json`，构建树文件，不纳入 Git |

空闲 probe 使用 `Qt::PreciseTimer` 完整等待测量区间。CPU 百分比由进程 `utime + stime`、`CLK_TCK` 和实际墙钟计算，不除以逻辑 CPU 数；RSS 同时保留起止值，只有增长百分比按契约截断到非负数。

## 正式基线建立条件

负责人必须先确认以下参考机身份，实施者才能生成正式文件：

- CPU 型号、物理核和逻辑核数量。
- RAM 总字节数。
- GPU 型号及精确驱动版本，禁止 `unknown` 和软件渲染器。
- 显示刷新率、DPR、Linux 桌面和窗口协议。
- Linux 镜像 immutable digest，格式为 `sha256:` 加 64 位小写十六进制。
- Qt、GCC 和 libstdc++ 精确版本。

在参考机上从干净工作树执行：

```bash
export ZZ_BENCHMARK_COMMIT="$(git rev-parse --verify HEAD)"
export ZZ_RUNNER_IMAGE_DIGEST="sha256:<审核后的镜像摘要>"
export ZZ_GPU_IDENTITY="<GPU 型号和驱动版本>"

cmake --preset linux-gcc-reference
cmake --build --preset linux-gcc-reference
ctest --preset linux-gcc-reference -L benchmark --output-on-failure
```

只有全部绝对门禁通过后，才允许逐字复制下列 reporter 输出：

| reporter 输出 | Git 基线路径 |
|---|---|
| `build/linux-gcc-reference/reports/benchmark.startup.json` | `docs/performance/reference/linux/startup.json` |
| `build/linux-gcc-reference/reports/benchmark.theme-switch.json` | `docs/performance/reference/linux/theme-switch.json` |
| `build/linux-gcc-reference/reports/benchmark.animation.json` | `docs/performance/reference/linux/animation.json` |
| `build/linux-gcc-reference/reports/benchmark.large-model.json` | `docs/performance/reference/linux/large-model.json` |
| `build/linux-gcc-reference/reports/benchmark.window-lifecycle.json` | `docs/performance/reference/linux/window-lifecycle.json` |
| `build/linux-gcc-reference/reports/benchmark.idle.json` | `docs/performance/reference/linux/idle.json` |

六份文件必须具有相同的 `build.commit`、`environment.runnerImageDigest` 和完整环境指纹。复制后使用 `cmake/ZzVerifyPerformanceReport.cmake` 执行启动 300 ms、主题 50 ms、动画/大模型 16.7 ms、空闲 CPU 严格低于 0.5% 和 RSS 增长不超过 10% 的绝对门禁，再使用 `cmake/ZzComparePerformanceReport.cmake` 验证同环境报告的 10% 相对回归限制。
