#pragma once

#include <memory>

#include <QtWidgets/QDockWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzIconDescriptor.h>

namespace ZzFluentUI {

class ZzDockPanelPrivate;

/**
 * @brief 使用 Fluent 标题栏视觉并保留 Qt 原生停靠协议的面板。
 *
 * 内容所有权遵循 QDockWidget；takeContentWidget() 会解除父对象并归还内容。
 * 浮动窗口完全由 Qt 管理，不创建额外的窗口适配对象。
 */
class ZZ_FLUENT_UI_EXPORT ZzDockPanel final : public QDockWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzDockPanel)

public:
    /**
     * @brief 创建带固定 Fluent 标题栏的空停靠面板。
     * @param title 面板标题，同时同步给原生 toggleViewAction。
     * @param parent 可为空的 QMainWindow 所有者。
     */
    explicit ZzDockPanel(
        const QString &title = {},
        QWidget *parent = nullptr);

    /** @brief 销毁标题栏以及仍由面板拥有的内容。 */
    ~ZzDockPanel() override;

    /**
     * @brief 更新标题栏图标描述。
     * @param descriptor 使用 Fluent 图标缓存渲染的值描述。
     */
    void setIconDescriptor(const ZzIconDescriptor &descriptor);

    /**
     * @brief 解除当前内容的父对象并归还所有权。
     * @return 原内容指针；没有内容时返回 nullptr。
     */
    [[nodiscard]] QWidget *takeContentWidget();

private:
    std::unique_ptr<ZzDockPanelPrivate> d_ptr;
};

} // namespace ZzFluentUI
