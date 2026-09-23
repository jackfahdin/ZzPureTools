# Linux ASan/UBSan 链接修复报告

日期：2026-09-23

## 修复

仅修改 `ZzFluentUI/tests/CMakeLists.txt`，在以下两个一方测试目标的
`zz_enable_project_warnings(...)` 后补充 `zz_enable_sanitizers(...)`：

- `ZzWorkspaceTransferRegistryPrivateTest`
- `ZzWorkspaceTransferRegistryLifetimeTest`

未修改测试源、生产代码或 sanitizer helper。

## 红灯证据

修复前的诊断报告已稳定复现：两个目标最终链接命令缺少
`-fsanitize=address,undefined`，链接 `ZzFluentUI`/`ZzLog` 时报告
`__asan_*` 与 `__ubsan_*` 未解析符号。生成图审计确认 92 个可执行链接段中
恰好只有这两个一方目标缺少 sanitizer 链接选项。

## 绿灯证据

- 配置：`cmake --preset linux-clang-asan -DZZ_BUILD_EXAMPLES=ON` 在本机因
  preset 环境变量未设置而无法定位编译器；使用 `/usr/bin/clang++-20` 对应
  环境变量，并追加本机 Ninja RPATH 兼容开关
  `-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON` 后配置和生成成功。
- 定向构建：两个目标均成功链接；verbose 链接命令和
  `build/linux-clang-asan/build.ninja` 的 `LINK_FLAGS` 均包含
  `-fno-omit-frame-pointer -fsanitize=address,undefined`。
- 定向测试：
  `ctest --test-dir build/linux-clang-asan --output-on-failure -R
  'fluent.workspace-transfer-registry-(private|lifetime)'`，2/2 通过。
- 全量构建：`cmake --build --preset linux-clang-asan --parallel 2` 完成；
  最终复跑输出 `ninja: no work to do.`，没有发现下一批 sanitizer 链接遗漏。
- 格式检查：`git diff --check` 通过。

## 自审

变更严格限于两个相邻第一方测试目标各一行 sanitizer 配置，目标名与
`add_executable` 完全对应；未引入传递式库选项、测试行为或无关格式变化。

## 提交

实现提交号：`b2dbc0c`（构建：补齐工作区传输测试的 sanitizer 链接）。

