# 仪表逐帧追踪 · 2026-10-12

本组追踪确认 X11 绘图缓冲区同步会造成部分慢帧，但关闭共享内存不能消除 P95 超限。
正式门限仍失败，不能把诊断变体当作通过证据。控件实现、历史基线、16.7 ms 预算均未修改。

## 方法与原始数据

沿用[环境记录](../../ci-review/2026-10-12/environment.json)：Qt 6.11.1、GCC 15.2、
Mesa 26.0.8、kernel 7.0.0-38、Xvfb 1920×1080×24；Xvfb 固定 CPU 8，程序固定 CPU 10。
串行运行，测量期间没有并行构建或 CPU 策略修改。
框架实现对应 `32cd6e2459f84fd40c164b67068cc98f905183cd`，生产动画与前一组诊断相同；
探针链接此前安装消费的 B 前缀，其仪表代码与该提交一致。

[probe.cpp](probe.cpp) 使用同一单调时钟，预分配事件数组，运行结束后才输出 CSV。
每次动画 200 ms、0/100 交替，10 次预热、100 次实测。记录 Timer、数值变化、
UpdateRequest、Paint 开始/结束，并转发 `xcb_get_input_focus_reply` 到真实实现，
记录 GUI 线程的 X11 同步等待。它没有跳过绘制或更换动画驱动。

[traces.tar.gz](traces.tar.gz) 包含四份完整 CSV；[summary.json](summary.json) 由
[analyze.py](analyze.py) 从这些 CSV 计算。P95 采用排序后第 `ceil(N × 0.95)` 项，
每轮有 1,000 个 Paint 起始间隔、1,100 次 Paint、1,300 次 Timer。
统计属于插桩诊断；正式未插桩报告另存为 `final-*.json`。

## 共享内存对照

保持默认 GLib 事件循环，按共享内存开、关、关、开的顺序运行。
关闭时仅设置 `QT_XCB_NO_MITSHM=1`，未更改正式 runner。

| 原始文件 | Paint 间隔 P95（ms） | Paint 处理 P95（ms） | X11 同步 P95 / 最大值（ms） | 超 16.7 ms 帧数 |
|---|---:|---:|---:|---:|
| `1-shm.csv` | 16.733676 | 0.206751 | 0.216550 / 2.893232 | 74 |
| `2-no-shm.csv` | 16.728416 | 0.207064 | 未触发该同步调用 | 72 |
| `3-no-shm.csv` | 16.720778 | 0.201037 | 未触发该同步调用 | 66 |
| `4-shm.csv` | 16.767371 | 0.201860 | 0.243182 / 3.404287 | 98 |

Qt 源码路径对应 `qtbase/src/plugins/platforms/xcb/qxcbbackingstore.cpp`：
`QXcbBackingStoreImage::preparePaint()` 在共享图像区域仍可能被服务端读取时调用
`connection()->sync()`；`qxcbconnection.cpp` 的 `sync()` 使用上述 XCB reply 等待。

逐帧对应的例子：`4-shm.csv` 第 56 轮的最慢帧间隔为 **19.737306 ms**。
该帧的 Timer 间隔为 16.569367 ms，Timer 到 Paint 的延迟相对前帧增加了
3.167939 ms，两者相加正好得到该帧间隔；Paint 前的 X11 同步本身耗时
3.404287 ms。这是同一帧的事件对应，不能用两组不相关的 P95 相减替代。

关闭共享内存后，这类同步等待消失，但仍有 Timer 间隔超限，整体 P95 没有过门限。
因此当前证据支持“同步等待是部分慢帧来源”，不支持“禁用共享内存即可修复”或
“全部超限都是控件绘制太慢”。

另尝试在临时进程用动态库替换 Qt `timerWait()` 的毫秒向上取整，探针入口未触发。
反汇编确认本机 Qt 内部直接调用该函数，未经过可替换的动态符号入口。
这次替换实验无效，未纳入统计，也没有修改 Qt 安装或交付私有 API 补丁；
定时器取整的具体影响仍未通过干预实验确认。

## 正式复验与已修复项

恢复默认环境，取消 `QT_NO_GLIB`、`QT_XCB_NO_MITSHM` 和 `LD_PRELOAD` 后，
运行原始两个动画生产器、对应绝对门限及参考完整性契约，共 5 项：3 项通过、2 项失败。

| 原始报告 | P95（ms） | 16.7 ms 门限 |
|---|---:|---|
| [final-animation.json](final-animation.json) | 16.615112 | 本轮通过，不能覆盖此前失败 |
| [final-radial-gauge-animation.json](final-radial-gauge-animation.json) | 16.759650 | 失败 |

另一项失败仍是缺少 `backdrop`、`fluent-standard-surfaces`、
`radial-gauge-animation`、`workspace-components` 四份历史同环境参考。

本轮实际修复为 `a6002e8`：性能 runner 原先在 CTest 失败时提前退出，未归档报告；
现在每次运行使用 `build/linux-gcc-benchmarks/release-rounds/run-XXXXXXXX/round-N/`，
失败也保留所有本轮已生成报告、`ctest.log` 和 `commands.log` 中的退出码。
相对比较日志另存为 `compare.<scenario>.log`。缺报告、比较失败、日志写入失败仍返回非零，
后续轮次不会把失败覆盖成成功。

新增动态契约已证明旧脚本丢失失败报告，新脚本通过失败留档、再次运行不覆盖、
拒绝缺报告、三轮成功、相对比较失败和日志失败场景。6 项相关契约/架构审计通过。
独立审查指出的新测试平台范围问题已修复：该 Linux runner 契约仅在 Linux 注册。

## 复现追踪

在仓库根目录使用本机已安装的框架 B 前缀和相同 Qt；需要 C++20 编译器、XCB 开发头、
Xvfb、Python 3。以下命令只构建独立探针，不更改 SDK 或正式基准：

```bash
timeline_source="$PWD/docs/performance/evidence/frame-timeline/2026-10-12"
framework_prefix="$PWD/build/linux-gcc-benchmarks/tests/InstallConsumer/Release/B"
qt_prefix=/home/zz/Qt/6.11.1/gcc_64
timeline_work=$(mktemp -d /tmp/zz-frame-timeline-XXXXXXXX)
cmake -S "$timeline_source" -B "$timeline_work/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/g++-15 \
  "-DCMAKE_PREFIX_PATH=$framework_prefix;$qt_prefix"
cmake --build "$timeline_work/build"
export LD_LIBRARY_PATH="$framework_prefix/lib:$qt_prefix/lib"
unset QT_NO_GLIB QT_XCB_NO_MITSHM LD_PRELOAD
export QT_QPA_PLATFORM=xcb
taskset -c 8 xvfb-run -a -s '-screen 0 1920x1080x24 -nolisten tcp' \
  taskset -c 10 "$timeline_work/build/gauge-timeline" "$timeline_work/shm.csv"
python3 "$timeline_source/analyze.py" "$timeline_work/shm.csv"
```

对照时在启动程序前设置 `QT_XCB_NO_MITSHM=1`，结束后取消；正式测量不得继承诊断变量。
原始四轮共享同一 Xvfb 进程，顺序为开、关、关、开。解压与重算已有数据可使用：

```bash
tar -xzf "$timeline_source/traces.tar.gz" -C "$timeline_work"
python3 "$timeline_source/analyze.py" "$timeline_work/4-shm.csv"
```
