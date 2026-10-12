# 仪表动画分层诊断

这些文件是诊断证据，不是发布参考。没有修改 `reference/linux/` 或 16.7 ms 预算。

- 生产代码：`773a669ef3b61b2a326cda3505eece1ed83c487a`；仪表基准源文件未修改。
- 环境沿用 [environment.json](../../ci-review/2026-10-12/environment.json)，本次再次核对 kernel、Mesa 与 Xvfb 包版本。
- Qt 6.11.1 / GCC 15.2 Release，Xvfb 1920×1080×24、60 Hz、DPR 1；Xvfb 固定 CPU 8，采样程序固定 CPU 10。
- `gauge-timing-probe.patch` 是临时基准副本的完整插桩差异，不应用到生产代码。
- `gauge-instrumented.json` 是插桩副本的正式格式输出；`gauge-timings.json` 保存全部附加采样。
- `gauge-uninstrumented.json` 与 `toggle-uninstrumented.json` 来自随后无并行编译的原始 CTest 基准，门限结论见上级复核文档。

插桩复用原始基准的 10 次预热、100 次实测、200 ms 动画和最小 Paint 数量检查。
`QApplication::notify` 在实际仪表 Paint 分发前后计时（包含过滤器及事件处理，
不含合成器呈现）；另记录生产 `QVariantAnimation::valueChanged` 相邻时刻。
每轮重新清空前一 tick，避免把两轮之间的停顿计入间隔；统计前关闭预热采样。
额外测量会引入开销，不能把插桩结果作为未插桩报告的替代。

附加数组按排序后的第 `ceil(N * q)` 项（从 1 开始）取分位点，与 Reporter 的 nearest-rank 定义一致。
Paint 分发 P95 = 0.212209 ms（1100 次），动画更新间隔 P95 = 16.715581 ms（1200 次）。
生产指标包含帧间等待；这些结果不支持“单帧绘制用了 16.7 ms”的解释。

## 重现插桩

先构建 `linux-gcc-benchmarks` 和安装包。在临时目录复制基准源，再应用此补丁：

```bash
probe_dir=$(mktemp -d)
cp benchmarks/ZzGaugeAnimationBenchmark.cpp "$probe_dir/main.cpp"
patch "$probe_dir/main.cpp" < docs/performance/evidence/window-hardening/2026-10-12/gauge-timing-probe.patch
```

临时工程的 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.23)
project(GaugeDiagnostic LANGUAGES CXX)
find_package(ZzPureToolsFrame CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test)
add_executable(gauge-diagnostic main.cpp)
target_compile_features(gauge-diagnostic PRIVATE cxx_std_20)
target_include_directories(gauge-diagnostic PRIVATE "${ZZ_SOURCE}/benchmarks/common")
target_link_libraries(gauge-diagnostic PRIVATE
    "${ZZ_BUILD}/benchmarks/libZzBenchmarkSupport.a"
    Zz::FluentUI Zz::Core Qt6::Test)
```

配置时传入框架源码绝对路径 `ZZ_SOURCE`、对应基准构建目录 `ZZ_BUILD`，
以及包含已安装框架和同版本 Qt 的 `CMAKE_PREFIX_PATH`。此临时工程针对本次 Linux/GCC 环境。
运行时设置 `LD_LIBRARY_PATH` 包含已安装框架与 Qt 的 `lib`，并设置
`QT_QPA_PLATFORM=xcb`、`ZZ_CMAKE_PRESET=linux-gcc-benchmarks`、完整 40 位
`ZZ_BENCHMARK_COMMIT`、实际 `ZZ_GPU_IDENTITY` 和环境文件摘要 `ZZ_RUNNER_IMAGE_DIGEST`。
本次摘要为 `sha256:00f36e63d6f253dbb5856af53b3a71a59eba4aa8a7c45ef1645c924692a9b989`。

```bash
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 "$probe_dir/build/gauge-diagnostic" --report "$probe_dir/gauge.json"
```

结果为 `gauge.json` 和 `gauge.json.diagnostic.json`。未插桩复验使用相同环境身份：

```bash
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 ctest --preset linux-gcc-benchmarks -j1 --output-on-failure \
  -R '^benchmark.(animation|radial-gauge-animation|reference-gate.(animation|radial-gauge-animation))$'
```
