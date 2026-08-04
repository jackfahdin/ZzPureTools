#pragma once

#include <memory>

#include <QtCore/QtGlobal>
#include <QtWidgets/QApplication>

#include <ZzCore/ZzResult.h>

#include <ZzPureTools/ZzPureToolsExport.h>

namespace ZzFluentUI {
class ZzThemeController;
}

namespace ZzPureTools {

class ZzApplicationBuilderPrivate;
class ZzApplicationWindow;
class ZzPureApplicationPrivate;

/**
 * @brief 拥有应用级 Fluent 样式、主题、模块运行时和全部顶层窗口。
 *
 * 构造后立即安装观察内部主题控制器的 ZzFluentStyle。在本应用存活期间，外部代码
 * 不得再次调用 QApplication::setStyle() 替换该应用级样式。
 */
class ZZ_PURE_TOOLS_EXPORT ZzPureApplication final : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzPureApplication)

public:
    /**
     * @brief 创建唯一 QApplication 并在任何窗口前安装 Fluent 样式。
     * @param argc main() 参数数量引用。
     * @param argv main() 参数数组。
     */
    ZzPureApplication(int &argc, char **argv);

    /** @brief 完成关闭并在主题控制器销毁前替换观察它的应用样式。 */
    ~ZzPureApplication() override;

    /**
     * @brief 使用成功提交的不可变配置创建另一个独立窗口。
     * @return 新窗口非拥有观察指针，或应用状态及窗口装配错误。
     */
    [[nodiscard]] ZzCore::ZzResult<ZzApplicationWindow *> createWindow();

    /** @brief 返回应用当前独占的顶层窗口数量。 */
    [[nodiscard]] qsizetype windowCount() const noexcept;

    /** @brief 返回应用独占主题控制器的非拥有观察指针。 */
    [[nodiscard]] ZzFluentUI::ZzThemeController *themeController()
        const noexcept;

    /** @brief 幂等停止模块、销毁窗口并卸载本次构建的 translators。 */
    void beginShutdown() noexcept;

private:
    friend class ZzApplicationBuilderPrivate;
    std::unique_ptr<ZzPureApplicationPrivate> d_ptr;
};

} // namespace ZzPureTools
