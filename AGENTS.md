# ZzFluent 项目代理指南

本文件定义 ZzFluent 项目的开发规范、架构约定与协作要求。所有在此项目工作的代码代理（coding agent）必须遵守本文件。

---

## 1. 项目定位

ZzFluent 是一个基于 **Qt 6 Widgets** 的跨平台 Fluent UI 风格控件库，目标是为 Qt 桌面应用提供现代化、一致、可扩展的 UI 组件与视觉体验。

- **技术栈**：C++20、CMake 3.21+、Qt 6.8+
- **核心思想**：设计令牌（Design Tokens）+ 绘制委托（Paint Delegates）+ 动画驱动
- **参考项目**：ElaWidgetTools（控件库方向）、QWindowKit（窗口框架方向）

---

## 2. 代码规范

### 2.1 C++ 标准

- 必须使用 **C++20** 编译。
- 积极使用以下现代特性提升代码质量：
  - `concept`：对模板参数、回调接口进行约束。
  - `std::ranges`：替代传统循环与算法组合。
  - `std::optional` / `std::expected`：表达可选值与错误处理。
  - `std::span`：替代裸指针数组传参。
  - 结构化绑定、`using enum`、指定初始化器等语法糖。
  - 协程（coroutine）：仅用于异步加载、延迟初始化等场景，避免滥用。
- 禁止使用已废弃的 Qt 或 C++ 特性。

### 2.2 命名规范

#### 类名

- 所有自定义类必须以 `Zz` 为前缀。
- 示例：`ZzPushButton`、`ZzPureTitleBar`、`ZzDataModel`、`ZzTheme`。
- 前缀大小写固定为 `Zz`，后续单词首字母大写（PascalCase）。

#### 文件名

- 除 `main.cpp` 外，文件名必须与所包含的主类名**完全一致（含大小写）**。
- 头文件扩展名统一为 `.hpp`，实现文件为 `.cpp`。
- 示例：
  - 类 `ZzPureTitleBar` → `ZzPureTitleBar.hpp` + `ZzPureTitleBar.cpp`
  - 类 `ZzDataModel` → `ZzDataModel.hpp` + `ZzDataModel.cpp`

#### 函数与变量

- 函数名：camelCase，如 `buildStyleContext()`、`controlFill()`。
- 成员变量：以 `m_` 为前缀，如 `m_theme`、`m_hoverProgress`。
- 静态成员：以 `s_` 为前缀。
- 全局常量/函数：使用 `zz` 小写前缀或放在命名空间内，如 `zzLightColorTokens()`。
- 宏：全大写 + 下划线，如 `ZZ_FLUENT_EXPORT`。

### 2.3 命名空间

- **绝对禁止链式命名空间**，如 `namespace ZzAppCore::Network::Http`。
- 使用传统嵌套方式或单一命名空间：

```cpp
namespace ZzAppCore {
namespace Network {

class ZzHttpClient { /* ... */ };

} // namespace Network
} // namespace ZzAppCore
```

- 公开 API 尽量放在顶层 `ZzFluent` 命名空间下，内部实现可放在 `ZzFluent::Private` 等子命名空间。

### 2.4 头文件与导出

- 所有公开头文件位于 `include/ZzFluent/` 下。
- 每个公开类/函数必须正确标记 `ZZ_FLUENT_EXPORT`。
- 私有头文件位于 `src/` 下，不导出。
- 头文件必须包含 `#pragma once` 或标准 include guard。

---

## 3. 注释与文档

### 3.1 强制要求

- 所有公开类、公开方法、复杂逻辑、非平凡算法必须包含标准化注释。
- 注释使用 **Doxygen 风格**。
- 注释内容使用 **简体中文**。

### 3.2 类注释示例

```cpp
/**
 * @brief Fluent 风格按钮控件。
 *
 * 支持标准（Standard）与强调（Accent）两种视觉风格，并内置 hover / press 动画过渡。
 */
class ZZ_FLUENT_EXPORT ZzPushButton : public QPushButton
{
    Q_OBJECT
    // ...
};
```

### 3.3 方法注释示例

```cpp
/**
 * @brief 设置按钮的视觉风格。
 * @param style 按钮风格，可选 Standard 或 Accent。
 *
 * 修改风格后会立即触发重绘，动画状态会被重置。
 */
void setButtonStyle(ZzButtonStyle style);
```

### 3.4 复杂逻辑注释示例

```cpp
/**
 * @brief 根据当前状态混合填充色。
 *
 * 混合顺序为：基础色 → hover 色 → pressed 色。
 * hoverProgress 与 pressProgress 由 ZzAnimator 驱动，范围均为 [0.0, 1.0]。
 */
QColor fill = colors.controlFill;
fill = ZzPaintPrimitives::blend(fill, colors.controlFillHover, context.hoverProgress);
fill = ZzPaintPrimitives::blend(fill, colors.controlFillPressed, context.pressProgress);
```

### 3.5 禁止事项

- 禁止只写无意义的注释，如 `// 设置风格` 对应 `setButtonStyle(style)`。
- 禁止中英文混用导致语义不清。

---

## 4. 架构约定

### 4.1 分层架构

项目必须保持三层结构：

| 层级 | 职责 | 位置 |
|------|------|------|
| **Core** | 应用初始化、主题管理、设计令牌、调色板 | `include/ZzFluent/Core`、`src/Core` |
| **Style** | 动画驱动、绘制委托、底层绘制原语 | `include/ZzFluent/Style`、`src/Style` |
| **Widgets** | 具体控件实现 | `include/ZzFluent/Widgets`、`src/Widgets` |

### 4.2 新增控件流程

新增一个 Fluent UI 控件时，按以下流程：

1. 在 `include/ZzFluent/Widgets/` 创建 `ZzXxx.hpp`。
2. 在 `src/Widgets/` 创建 `ZzXxx.cpp`。
3. 在 `src/Style/` 创建 `ZzXxxDelegate.hpp` / `ZzXxxDelegate.cpp`（若绘制较复杂）。
4. 在 `include/ZzFluent/Style/ZzStyleDelegate.hpp` 的 `ZzStyleContext` 中补充所需状态字段（如有必要）。
5. 在 `include/ZzFluent/ZzFluent.hpp` 中导出该控件头文件。
6. 在 `example/` 的演示界面中添加该控件示例。
7. 在 `README.md` 的组件列表中更新状态。

### 4.3 绘制委托模式

- 控件只负责事件处理与状态管理，绘制逻辑交给 Delegate。
- Delegate 必须继承 `ZzStyleDelegate` 并实现 `paint()` 方法。
- 多个控件可复用同一组 `ZzPaintPrimitives` 原语。

---

## 5. 构建与测试

### 5.1 构建命令

```bash
# 配置（Release）
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DZZ_BUILD_EXAMPLE=ON

# 编译
cmake --build build --parallel

# 安装
cmake --install build --prefix ./install
```

### 5.2 提交前检查

- 运行 `.clang-format` 格式化代码。
- 确保新增文件被加入 `CMakeLists.txt` 的对应列表。
- 确保公开头文件被加入 `include/ZzFluent/ZzFluent.hpp`。
- 确保无编译警告（`-Wall -Wextra` 级别）。

### 5.3 CI

- CI 配置位于 `.github/workflows/`。
- 手动触发工作流：`多平台编译`。
- 推送 `v*` tag 自动触发 Release。

---

## 6. 版本管理

- 单一版本数据源：`packaging/ci/project.json` 中的 `"version"` 字段。
- `CMakeLists.txt` 启动时从该文件读取版本号。
- 修改版本号时只需修改 `packaging/ci/project.json` 一处。

---

## 7. 优先级原则

当任务之间存在冲突时，按以下优先级处理：

1. 用户当前明确指出的需求。
2. 本文件中的规范。
3. 项目现有代码风格。
4. 通用最佳实践。

---

## 8. 沟通语言

- 所有用户-facing 的回复、注释、文档使用 **简体中文**。
- 代码中的标识符、技术术语保持英文。
