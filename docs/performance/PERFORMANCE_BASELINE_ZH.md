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
| RAM | 32,708,890,624 bytes |
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

本机物理环境没有 immutable container image。为保持可审计身份，`environment.runnerImageDigest` 固定为 `local-release-xvfb.json` 原始字节的 SHA-256：

```bash
sha256sum docs/performance/profiles/local-release-xvfb.json
```

任何档案字段变化都会改变 digest，并强制重新采集六份基线。该 digest 只表示经过版本控制的物理 runner 档案，不表示容器镜像。

## 当前活动基线

本机活动基线于 2026-08-05 建立，六份报告均逐字来自同一次固定环境采集：

| 身份字段 | 固定值 |
|---|---|
| 被测源码 HEAD | `4050bac1561d4f8fe7317aafebd5416f78035a61` |
| runner 档案 SHA-256 | `242e623f21aa12a9c50199595c9427d7ef6754604883838aa894281d42c05fe1` |
| renderer identity | `Mesa llvmpipe LLVM 21.1.8 Mesa 26.0.3 Xvfb 1920x1080x24` |
| reference CTest | 22/22 通过，包含 6 项绝对门禁 |
| Clang ASan/UBSan | `linux-clang-asan-benchmarks` 全量构建通过；ZzLog 四项测试与 100 次窗口生命周期共 5/5 通过 |

| 门禁 | 实测结果 | 要求 | 结论 |
|---|---:|---:|---|
| 启动 `external-total` | P95 20.165138 ms，max 20.532506 ms | P95/max ≤ 300 ms | 通过 |
| 500 控件主题切换 | P95 6.513543 ms | P95 ≤ 50 ms | 通过 |
| Toggle 动画 | P95 16.603062 ms | P95 ≤ 16.7 ms | 通过 |
| 10 万行模型 | P95 1.592757 ms | P95 ≤ 16.7 ms | 通过 |
| 窗口生命周期 | 100 次完成，P95 3.719473 ms | 100 次且诊断计数无残留 | 通过 |
| 空闲 CPU | 0% | 严格 < 0.5% | 通过 |
| 空闲 RSS 增长 | 0% | ≤ 10% | 通过 |

该结果解除 `local-release-xvfb` 档案的性能参考机发布阻断。后续同档案报告只有在完整环境指纹一致时才能与本基线执行相对回归比较；档案变化、环境不匹配或任一报告缺失都必须失败关闭。

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
| 空闲 | 5 秒 | 30 秒单区间 |

| reporter 输出 | 活动基线路径 |
|---|---|
| `build/linux-gcc-reference/reports/benchmark.startup.json` | `docs/performance/reference/linux/startup.json` |
| `build/linux-gcc-reference/reports/benchmark.theme-switch.json` | `docs/performance/reference/linux/theme-switch.json` |
| `build/linux-gcc-reference/reports/benchmark.animation.json` | `docs/performance/reference/linux/animation.json` |
| `build/linux-gcc-reference/reports/benchmark.large-model.json` | `docs/performance/reference/linux/large-model.json` |
| `build/linux-gcc-reference/reports/benchmark.window-lifecycle.json` | `docs/performance/reference/linux/window-lifecycle.json` |
| `build/linux-gcc-reference/reports/benchmark.idle.json` | `docs/performance/reference/linux/idle.json` |

六份 JSON 只能逐字复制 reporter 输出，不得手改数值。复制前必须满足启动 P95/max 不超过 300 ms、主题 P95 不超过 50 ms、动画与大模型 P95 不超过 16.7 ms、空闲 CPU 严格低于 0.5%、RSS 增长不超过 10%，并完成窗口生命周期计数和 ASan/UBSan 门禁。

## 原 CI 参考档案

`ubuntu2204-github-ci` 保留原架构要求：Ubuntu 22.04 兼容构建环境、Qt 6.8+、GCC 13.1+、Release/shared/LTO，以及审核后的 immutable runner image digest。其 CPU、RAM、GPU、显示和精确工具链目前为空，因此状态必须保持 `pending-user-validation`。

用户在 GitHub 平台自行验证或新主机到位后，应执行以下流程：

1. 补全 `ubuntu2204-github-ci.json` 的所有 `null` 字段并提交。
2. 使用该档案的 SHA-256 和真实 GPU/驱动身份运行全部 reference benchmark。
3. 执行全部绝对阈值和 ASan/UBSan，不接受“仅构建通过”。
4. 把报告保存在独立的档案目录并记录 commit，不覆盖本机档案证据。
5. 只有负责人明确切换活动档案后，CI 才能用新档案判定发布。

## 发布边界

本机档案通过性能门禁，只能解除性能参考机这一项阻断。根 `LICENSE` 缺失、第三方溯源或许可证据不完整、Windows/macOS 真机清单未完成等独立条件仍然阻止正式发布，不得因为本机性能达标而绕过。
