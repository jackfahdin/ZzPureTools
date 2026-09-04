# Task 11 实现报告：有界多窗口拓扑状态与编解码器

## 红灯证据

在实现 DTO 和 codec 之前，已运行：

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTopologyCodecPrivateTest --parallel 2
```

CMake 重生成失败，报告缺失 `ZzWorkspaceTopologyStatePrivate.cpp` 和
`ZzWorkspaceTopologyCodecPrivate.cpp`，目标无法生成。这是预期的功能缺失红灯，
不是测试拼写或运行时错误。

## 实现

- 新增 `ZzWorkspaceTopologyStatePrivate` 纯 DTO，保存窗口配置、几何、显示状态、
  不透明 `ZZSW` workspaceState、页面归属和来源栈。
- 新增 `ZzWorkspaceTopologyCodecPrivate`，使用 `QDataStream::Qt_6_8`、`ZZWT`
  magic、schema 2、payload 长度和 SHA-256 envelope；编码字段采用显式 UTF-16
  code unit 和固定 16 字节 UUID。
- 加入窗口 32、总页 4096、每窗口组 64、树深 16、字符串 256、单个 `ZZSW`
  1 MiB、payload 16 MiB、来源栈 32 的硬边界及重复 UUID/pageId/layoutKey、负尺寸、
  非法枚举、截断和摘要校验。
- 读取以 `ZZSW` 开头的旧字节时原样保留为默认窗口 workspaceState，不创建
  `QObject`/`QWidget`，窗口配置保持默认值。
- 新增单元测试覆盖双窗口、五组、多页面、来源栈、稳定编码、往返相等、摘要和
  envelope/拓扑篡改拒绝。

## 验证

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTopologyCodecPrivateTest --parallel 2
[5/6] Linking CXX executable ZzPureTools/tests/ZzWorkspaceTopologyCodecPrivateTest

ctest --preset linux-gcc-debug -R '^puretools\\.workspace-topology-codec-private$' --output-on-failure
1/1 Test #81: puretools.workspace-topology-codec-private ... Passed
100% tests passed, 0 tests failed out of 1
```

同时验证了库目标：

```text
cmake --build --preset linux-gcc-debug --target ZzPureTools --parallel 2
[12/13] Creating library symlink ZzPureTools/libZzPureTools.so.0 ZzPureTools/libZzPureTools.so
```

## 自审

- 解码路径只操作 QByteArray、QDataStream 和 DTO，未构造 QObject/QWidget。
- envelope 在分配 payload 前检查总长度和 16 MiB 上限；byte array 和字符串读取均
  先检查长度，截断输入由 stream 状态拒绝。
- 现有 `ZzSplitWorkspace`/`ZZSW` 格式未修改；旧 blob 仅作为原始字节导入。
- 未修改用户已有的 `progress.md`、旧项目、第三方目录或 `temp_image`。
- 图标属于运行时窗口资源，拓扑 blob 不序列化 QIcon；其余窗口配置标量均稳定编码。

## 提交

实现提交：`cc1eeb3238c0f3dddf1cf375f34e6e08eb734792`

## 第 1 轮审查修复

### 红灯回归

先仅更新回归测试并运行：

```text
cmake --build --preset linux-gcc-debug --target ZzWorkspaceTopologyCodecPrivateTest --parallel 2
ctest --preset linux-gcc-debug -R '^puretools\\.workspace-topology-codec-private$' --output-on-failure
```

目标构建成功，但 CTest 按预期失败：

```text
FAIL! : ZzWorkspaceTopologyCodecPrivateTest::rejectsInvalidTopologyBeforeWidgets()
       '!ZzCodec::encode(oversizedTitle)' returned FALSE.
```

该失败证明超长窗口标题 encode 门禁缺失的回归测试有效；同一测试还覆盖
Qt 合法的混合零尺寸、非哨兵负尺寸、非法 closePolicy/titleMode，以及窗口、页和
来源计数篡改的失败 decode。

### 修复内容

- `validSize`/`validRect` 统一为协调器既有语义：仅允许 `QSize()`/`QRect()` 空哨兵
  或 Qt `isValid()`，任一非哨兵负尺寸均拒绝，同时保留合法的混合零尺寸 `QSize`。
- `validate()` 对窗口标题调用 256 UTF-16 code unit `validString()`，保证 encode
  与 decode 对称。
- `validate()` 使用显式 switch 校验 `ZzWindowClosePolicy` 和
  `ZzWorkspaceTitleMode` 枚举范围，非法内存态不会被写出。
- 篡改 decode 用例在所有失败路径后比较 `QApplication::allWidgets()` 数量，确认
  解码过程不创建 QWidget；补充窗口计数、页面计数、来源栈计数和 UUID 篡改。

### 修复验证

```text
cmake --build --preset linux-gcc-debug --target ZzPureTools --parallel 2
[4/5] Creating library symlink ZzPureTools/libZzPureTools.so.0 ZzPureTools/libZzPureTools.so

cmake --build --preset linux-gcc-debug --target ZzWorkspaceTopologyCodecPrivateTest --parallel 2
[3/4] Linking CXX executable ZzPureTools/tests/ZzWorkspaceTopologyCodecPrivateTest

ctest --preset linux-gcc-debug -R '^puretools\\.workspace-topology-codec-private$' --output-on-failure
1/1 Test #81: puretools.workspace-topology-codec-private ... Passed
100% tests passed, 0 tests failed out of 1
```

### 修复自审

- encode/decode 现在共享同一个状态校验入口，几何、字符串和枚举不会出现单向门禁。
- 默认 `QSize()`/`QRect()` 哨兵仍可用于协调器未指定配置；有效的零宽/零高 QSize
  也遵循 Qt `isValid()` 语义，非哨兵负尺寸全部拒绝。
- 所有新增篡改测试均在纯 DTO 解码后检查 QWidget 数量，未引入 QObject 创建。
- 未修改 `progress.md`、旧项目、第三方目录或 `temp_image`。
