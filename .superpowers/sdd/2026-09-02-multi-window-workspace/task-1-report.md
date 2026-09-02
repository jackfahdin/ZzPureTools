# 任务 1 报告：稳定页面身份和空组策略

## 变更

- 新增 `ZzWorkspacePageId` UUID 值对象：默认无效、随机创建、规范字符串解析、往返转换、相等比较和 `qHash`。
- 新增 `ZzEmptyGroupPolicy` 枚举，包含 `Keep`、`Remove` 和 `RemoveUnlessLast`。
- 为页面身份声明 Qt 元类型，支持 `QSignalSpy` 捕获。
- 将实现注册到 Foundation CMake，并新增 `ZzWorkspacePageIdTest` 及 CTest 条目。
- 为 Linux 动态库测试运行补充了测试目标的 `LD_LIBRARY_PATH`。

## 测试

### TDD 红灯

命令：

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspacePageIdTest --parallel 2
```

实际结果：失败，CMake 报告 `foundation/src/ZzWorkspacePageId.cpp` 不存在；此时目标实现尚未创建。

### 构建

由于工作树环境变量未提供 preset 中的 GCC 13 路径，使用系统 `/usr/bin/gcc`、`/usr/bin/g++` 和 Qt 6.11.1 完成等价 Debug 配置；并设置 `CMAKE_BUILD_WITH_INSTALL_RPATH=ON` 规避当前 CMake/Ninja RPATH 生成限制。

```text
cmake --build build/linux-gcc-debug --target ZzWorkspacePageIdTest --parallel 2
```

实际结果：成功，目标完成链接，无编译警告。

### 测试

```text
ctest --preset linux-gcc-debug -R '^fluent\\.workspace-page-id$' --output-on-failure
```

实际结果：`1/1` 通过，`100% tests passed, 0 tests failed`。

另外执行 `git diff --check`，无输出且成功。

## 疑虑

- 当前环境没有 preset 所需的 `GCC_13`、`GXX_13` 环境变量，因此无法原样使用其编译器路径；实际构建使用 GCC 15.2.0 和 Qt 6.11.1。
- 配置阶段提示 `WrapVulkanHeaders` 缺失，但该可选依赖不影响本任务目标或测试。
- `fromString` 接受带花括号或不带花括号的 36 字符 UUID，并允许大小写，这是 Qt UUID 的规范表示范围。
