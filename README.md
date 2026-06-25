# ZzPureTools

**ZzPureTools** 是一个基于 Qt 6 Widgets 的跨平台 Fluent UI 风格控件库，使用现代 C++20 编写，致力于为 Qt 桌面应用提供一致、流畅、可扩展的现代化用户界面。

---

## ✨ 特性

- **Fluent Design 风格**：圆角、柔和阴影、平滑动画、Light / Dark / System 主题切换。
- **设计令牌驱动**：颜色、尺寸、动画时长集中管理，便于全局换肤与主题扩展。
- **绘制委托架构**：控件负责交互与状态，Delegate 负责绘制，易于扩展和维护。
- **跨平台**：支持 Windows、macOS、Linux（x64 / arm64）。
- **现代 C++20**：积极使用 concept、ranges、std::optional 等特性。
- **Qt 6.8+**：专注最新 Qt 6 LTS 版本，避免 Qt 5 历史包袱。

---

## 📦 已支持控件

> 项目处于早期开发阶段，控件正在持续增加中。

| 控件 | 状态 | 说明 |
|------|------|------|
| `ZzPushButton` | ✅ 已实现 | 支持 Standard / Accent 两种风格 |
| `ZzComboBox` | ✅ 已实现 | 支持下拉列表自定义绘制 |
| `ZzLineEdit` | 🚧 计划中 | 输入框 |
| `ZzCheckBox` | 🚧 计划中 | 复选框 |
| `ZzRadioButton` | 🚧 计划中 | 单选框 |
| `ZzSwitch` | 🚧 计划中 | 开关按钮 |
| `ZzSlider` | 🚧 计划中 | 滑动条 |
| `ZzProgressBar` | 🚧 计划中 | 进度条 |
| `ZzPureTitleBar` | 🚧 计划中 | 自定义标题栏 |
| `ZzPureToolsWindow` | 🚧 计划中 | 无边框 Fluent 窗口 |

---

## 🚀 快速开始

### 环境要求

- CMake 3.21+
- C++20 编译器（MSVC 2022 / GCC 11+ / Clang 14+）
- Qt 6.8.0+（Widgets 模块）

### 构建

```bash
# 1. 克隆仓库
git clone https://github.com/ZzPureTools/ZzPureTools.git
cd ZzPureTools

# 2. 配置（Release，构建示例）
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DZZ_BUILD_EXAMPLE=ON \
    -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.0

# 3. 编译
cmake --build build --parallel

# 4. 运行示例
./build/example/ZzPureToolsExample
```

### 在项目中使用

```cmake
find_package(Qt6 6.8.0 REQUIRED COMPONENTS Widgets)
find_package(ZzPureTools REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Zz::PureTools Qt6::Widgets)
```

```cpp
#include <QApplication>
#include <ZzPureTools/ZzPureTools.hpp>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    ZzApplication::instance().initialize(app);

    ZzPushButton button(QStringLiteral("Hello ZzPureTools"));
    button.setButtonStyle(ZzPushButton::ZzButtonStyle::Accent);
    button.resize(200, 40);
    button.show();

    return app.exec();
}
```

---

## 🏗️ 架构概览

```
ZzPureTools/
├── include/ZzPureTools/      # 公开 API
│   ├── Core/               # 应用、主题、设计令牌、调色板
│   ├── Style/              # 动画、绘制委托、绘制原语
│   └── Widgets/            # 控件
├── src/                    # 实现
│   ├── Core/
│   ├── Style/
│   └── Widgets/
├── example/                # 示例程序
├── packaging/              # CI / SDK 打包脚本
└── cmake/                  # CMake 配置模板
```

---

## 🛣️ 路线图

详见 [docs/ROADMAP.md](docs/ROADMAP.md)。

主要阶段：

1. **基础夯实**：CI 稳定、文档完善、代码规范落地。
2. **核心基础设施**：主题系统增强、动画系统扩展、绘制原语丰富化。
3. **核心控件补齐**：输入、选择、展示、容器类控件。
4. **窗口层集成**：无边框窗口、自定义标题栏、系统按钮。
5. **视觉效果**：Acrylic / Mica、阴影、Reveal 效果。
6. **高级组件与生态**：数据视图、菜单、包管理器分发。

---

## 📖 开发规范

所有贡献者（包括 AI 代理）必须阅读并遵守 [AGENTS.md](AGENTS.md)：

- 类名必须以 `Zz` 为前缀。
- 文件名与主类名完全一致。
- 禁止使用链式命名空间。
- 使用 Doxygen 风格、简体中文注释。
- 积极使用 C++20 现代特性。

---

## 📄 许可证

本项目使用 [MIT](LICENSE) 许可证。

---

## 🙏 致谢

本项目受以下优秀开源项目启发：

- [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools) —— Fluent UI 控件库方向
- [QWindowKit](https://github.com/stdware/qwindowkit) —— 窗口框架定制方向
