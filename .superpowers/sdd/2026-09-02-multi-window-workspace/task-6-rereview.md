# 任务 6 第 1/5 轮定向复审

## 待核实发现

**ADDRESSED。** `ZzPureTools/tests/CMakeLists.txt:192-207` 新增统一运行时 helper，
并分别在 Builder（第 236 行）、Coordinator（第 271 行）和 MultiWindow（第 327
行）的测试注册循环内调用。三族分别覆盖 17、8、11 个场景，共 36 个计划原命令
匹配的 CTest 条目，没有漏项。

helper 使用 `ENVIRONMENT_MODIFICATION` 与 `path_list_prepend`，依次预置测试所需
ZzCore、FluentFoundation、FluentUI、PureTools、WindowKit、ZzLog 和 Qt Core
目标目录。Linux/其他 Unix 默认使用 `LD_LIBRARY_PATH`，Apple 使用
`DYLD_LIBRARY_PATH`，Windows（含 MSVC/MinGW）使用 `PATH`，选择正确。

`$<TARGET_FILE_DIR:...>` 对单配置和多配置生成器均能解析到对应目标产物目录；
当前 Linux 生成的 `CTestTestfile.cmake` 已确认 36 条均展开为实际绝对目录，没有
残留生成器表达式。`ENVIRONMENT_MODIFICATION` 自 CMake 3.22 提供，而项目最低
CMake 为 3.23，兼容性合理。该写法也与仓库已有截图测试运行时配置模式一致。

主控已在不注入外部 `LD_LIBRARY_PATH` 的条件下新鲜执行构建与计划原始 CTest
命令，结果 36/36 通过。因此上轮 Important 的加载阶段失败已被修复。

## 新破坏

未发现新的 Critical 或 Important。修复 diff 只改测试注册环境与报告，不改变
产品代码、测试选择、GUI 平台设置或测试断言。

## 范围外观察

- 上轮 stale pointer 身份测试仍存在，但生产查询未命中前不解引用；按本轮要求不
  作为修复未完成项。
- 上轮 Window/Shell affinity 分支缺少直接测试仍存在；按本轮要求不作为修复未
  完成项。
- Windows 与 macOS 的变量和生成器用法静态核验通过，但本轮证据仍只有 Linux
  原生执行。

## 结论

**通过。** 上轮 Important 已完整解决，修复未引入新的 Critical/Important。
