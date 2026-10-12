# 独立 Qt 6.11.1 精度修复 SDK

本次完整构建 QtBase、QtSvg、QtTools/Linguist，并正常链接框架及 Example。
默认 GLib、XCB 与 MIT-SHM 均启用；没有通过 `LD_PRELOAD` 注入私有符号。
原安装 `/home/zz/Qt/6.11.1/gcc_64` 保持不变。

## 已验证的行为

| 检查 | 结果 / 原始证据 |
|---|---|
| Qt 原生测试 | 7/7 CTest；498 项检查通过、2 项 Windows 专用跳过；`qt-*-ctest.log` / `qt-*-details.log` |
| Qtimer / QChronoTimer / QEventDispatcher | 默认 GLib 与 no_glib 两种路径；保留上游事件循环测试中的预期 XFAIL |
| QAbstractAnimation | 原生动画测试通过 |
| 动画连续三轮 | 开关 P95 16.541155 / 16.446506 / 16.450113 ms；仪表 16.240016 / 16.263378 / 16.238440 ms；`round-1` 至 `round-3` |
| 首次完整性能测试 | 51/52；24 项绝对预算通过，唯一失败是历史参考缺四份报告；`full-suite/` |
| 外置 SDK 的框架回归 | 263/263 非性能 CTest，含安装消费、公共头、包重定位、二进制依赖；[日志](../../reference-migration/2026-10-12/framework-regression.log) |
| 正式新基线 | 见[完整迁移证据](../../reference-migration/2026-10-12/README.md) |

`environment.json` 和本目录 `framework-ldd.log` 保留首次实验安装在源码 build 内的身份。
该位置被框架的源码路径泄漏检查正确拒绝，随后将同一 SDK 安装到源码树以外：
`/home/zz/.local/share/ZzPureTools/qt-6.11.1-precision-JHwE5eyb`。
迁移前后五个 Qt 库 SHA-256 相同；活动路径、补丁和库摘要见
[`local-release-xvfb.json`](../../../profiles/local-release-xvfb.json)，
最终链接路径见[迁移 ldd 日志](../../reference-migration/2026-10-12/framework-ldd.log)。
构建时提取的依赖包仅用于头文件/链接开发文件，实际 XCB/GLib 运行库取自系统；
包摘要和插件依赖分别见 `dependency-packages.sha256`、`xcb-plugin-ldd.log`。

## 重建 SDK

在独立目录准备 Qt 6.11.1 源码（包含顶层 CMake、qtbase、qtsvg、qttools），
不要覆盖现有安装。按顺序应用两份补丁：

```bash
export ZZ_FRAMEWORK_SOURCE="$PWD"
export ZZ_QT_SOURCE=/path/to/qt-6.11.1-source-copy
export ZZ_QT_BUILD=/path/to/qt-precision-build
export ZZ_QT_INSTALL=/path/outside/framework/qt-6.11.1-precision
git -C "$ZZ_QT_SOURCE/qtbase" apply \
  "$ZZ_FRAMEWORK_SOURCE/docs/performance/evidence/qt-timer-precision/2026-10-12/qtbase-6.11.1-timer-wait.patch"
git -C "$ZZ_QT_SOURCE/qtbase" apply \
  "$ZZ_FRAMEWORK_SOURCE/docs/performance/evidence/qt-glib-precision/2026-10-12/qtbase-6.11.1-glib-ready-time.patch"
cmake -S "$ZZ_QT_SOURCE" -B "$ZZ_QT_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/gcc-15 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-15 -DCMAKE_INSTALL_PREFIX="$ZZ_QT_INSTALL" \
  '-DQT_BUILD_SUBMODULES=qtbase;qtsvg;qttools' \
  -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF \
  -DFEATURE_glib=ON -DFEATURE_xcb=ON -DFEATURE_xkbcommon_x11=ON \
  -DFEATURE_xcb_egl_plugin=ON -DFEATURE_xcb_glx_plugin=OFF \
  -DFEATURE_linguist=ON -DFEATURE_assistant=OFF -DFEATURE_designer=OFF \
  -DFEATURE_qdoc=OFF -DFEATURE_distancefieldgenerator=OFF -DFEATURE_pixeltool=OFF
cmake --build "$ZZ_QT_BUILD" --parallel 8
cmake --install "$ZZ_QT_BUILD"
```

需要匹配系统的 GLib、XCB、xkbcommon-x11、EGL 开发包。当前主机未安装齐开发包，
使用 `build/qt-precision-sdk-JHwE5eyb/dependencies/root/usr` 的本地解包前缀
作为 `CMAKE_PREFIX_PATH`，未替换系统运行库。具体配置输出保存在 `sdk-configure.log`。
不同前缀、依赖或编译选项重建后不得直接沿用活动 profile 摘要，须核对实际环境。

原生测试可分别对 Qt 源码的 `tests/auto/corelib/kernel/{qtimer,qchronotimer,qeventdispatcher}`
和 `tests/auto/corelib/animation/qabstractanimation` 配置独立构建目录，传
`-DCMAKE_PREFIX_PATH="$ZZ_QT_INSTALL"`，然后 build/ctest。本轮测试在首次安装位置执行；
最终外置安装的库摘要相同。

## 框架接入与边界

```bash
export GCC_13=/usr/bin/gcc-15 GXX_13=/usr/bin/g++-15
export QT_ROOT=/home/zz/.local/share/ZzPureTools/qt-6.11.1-precision-JHwE5eyb
cmake --preset linux-gcc-benchmarks -B build/linux-gcc-qt-precision \
  -DZZ_PERFORMANCE_REFERENCE=ON \
  -DXKB_INCLUDE_DIR="$PWD/build/qt-precision-sdk-JHwE5eyb/dependencies/root/usr/include" \
  -DXKB_LIBRARY=/usr/lib/x86_64-linux-gnu/libxkbcommon.so.0
cmake --build build/linux-gcc-qt-precision --parallel 8
```

变量名沿用仓库 preset，实际编译器为 GCC 15.2。性能采样命令见迁移说明；
必须先结束所有构建，不得并行采样。生产框架不依赖新增私有 Qt 接口。

此 SDK 不含 QtMultimedia，真实音频源相关可选测试未覆盖，模拟电平仍可用。
XCB EGL 插件已启用，GLX 插件未构建。补丁只在 Linux / GLib 2.88 实测；
未验证 Windows、macOS、WASM、旧 GLib 或自定义 poll 后端，不能外推为跨平台验收。
