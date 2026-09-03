# 任务 6 独立审查

## 规格结论

**需要修复。** 产品实现未发现 Critical：无参 `createWindow()` 委托
`Visible`，非法 visibility 在对象创建前拒绝，`Deferred` 完成窗口装配、应用接管
及关闭连接但不显示；协调器保持非拥有，应用继续通过 `unique_ptr` 唯一拥有全部
`ZzApplicationWindow`。`coordinator` 声明在 `windows` 之后，且
`beginShutdown()` 在清空窗口前先调用协调器关闭，能够避免窗口 `destroyed`
回调访问已析构协调器。

登记实现覆盖同线程、有效 handle、Window/Shell 唯一性、唯一 primary、Shell
宿主、配置枚举与几何/尺寸约束，并在所有可观察状态写入前完成校验。默认
`QSize()`/`QRect()` 被保留为哨兵，负屏幕坐标可用，min/max 相等边界允许。
`configuration()` 返回值副本；`unregisterWindow()` 只清理协调状态；空透传
`eventFilter()` 没有提前实现任务 8。构造函数保持私有，`Q_OBJECT` 已进入 moc
头列表，公开类使用导出宏，未发现 GCC-only、不完整类型或静态链接问题。

## Critical

无。

## Important

1. `ZzPureTools/tests/CMakeLists.txt:245`：新协调器测试只设置了
   `QT_QPA_PLATFORM`，没有为构建树共享库追加平台对应的搜索路径。按任务简报要求
   直接运行裸 `ctest --preset linux-gcc-debug -R ...` 时，8 个新条目均因找不到
   `libZzPureTools.so.0` 等依赖而失败；实现报告中的 36/36 是通过手工注入完整
   `LD_LIBRARY_PATH` 得到的，不能证明提交中注册的 CTest 可直接运行。应复用项目
   已有的 `ENVIRONMENT_MODIFICATION`/`path_list_prepend` 模式，并按 Linux、macOS、
   Windows 选择 `LD_LIBRARY_PATH`、`DYLD_LIBRARY_PATH`、`PATH`，至少加入测试所需
   项目库和 Qt 目录。

## Minor

1. `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp:333`：测试保存窗口销毁后的
   原始地址，并在第 339 行把悬空指针重新传给公开 API。生产实现会先按身份查表，
   未命中前不会解引用，因此这里没有发现生产 UAF；但 C++ 对对象生命周期结束后
   的无效指针值作此类使用只给出实现定义保证。该测试不宜作为 Windows/macOS
   可移植性的强证明，建议改用仍受支持的可观察状态验证自动注销，或明确把“仅作
   身份令牌、不解引用”的契约编码为独立整数身份。

2. `ZzPureTools/tests/ZzWorkspaceWindowCoordinatorTest.cpp:261`：所谓 foreign-thread
   用例只从 worker 调用协调器，因此在入口线程检查处即返回，没有覆盖“调用发生
   在协调器线程，但 Window/Shell 自身 affinity 不匹配”的分支。实现第 125 行已
   有正确校验，但当前 36 项测试对该规格的证明不完整。空 handle、关闭后拒绝新
   登记以及非法 visibility 确实未运行 setup callback 也缺少直接断言。

## 测试卫生

- 审查包、任务简报和实现报告已完整读取；`2681ab1..688bc83` 的 diff check 无空白
  错误。
- 已有日志显示手工配置动态库路径时相关 36 项可通过；裸 CTest 的 8 个新条目则
  已复现为加载失败，故不接受报告中的 36/36 作为默认测试入口通过证据。
- 本次未重跑测试，避免只读审查写入 CTest 日志；以上加载失败已有新鲜复现，代码
  行为可由实现与现有测试日志回答。

## 范围外观察

- 未发现 factory、tearOff、close policy 或 topology 的提前实现，也未发现
  `findChild` 猜测 Shell。
- `registerWindow()` 不保存应用身份，但 `ZzApplicationWindow` 为 final、构造及
  factory 私有，当前只能经应用私有实现创建，同时 Qt 进程只允许一个
  `QApplication`；因此本任务内没有形成第二所有者或可登记外来 Window 的公开
  路径，不单列缺陷。若以后开放窗口构造/注入，需同步增加显式应用归属校验。
- Windows MSVC/MinGW 与 macOS 未原生运行；静态检查未见平台特有编译阻碍。当前
  实际跨平台阻碍是 Important 所述 CTest 动态库搜索路径。

## 最终判定

**需要修复。** 修复新测试的 CTest 动态库环境后，核心产品实现可通过任务 6 审查；
Minor 测试覆盖问题建议同时补齐。
