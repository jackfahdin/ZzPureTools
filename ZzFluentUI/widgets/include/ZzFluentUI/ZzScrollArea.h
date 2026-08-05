#pragma once

#include <QtWidgets/QScrollArea>

#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzScrollBar;

/**
 * @brief 直接安装 Fluent 滚动条并保留 QScrollArea 内容语义。
 *
 * 本类不改变 widgetResizable、滚动条 policy、输入手势或 viewport，
 * 调用方可以继续使用全部 QScrollArea 公共接口。
 */
class ZZ_FLUENT_UI_EXPORT ZzScrollArea final : public QScrollArea
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzScrollArea)

public:
    /**
     * @brief 创建无边框、使用两条 ZzScrollBar 的滚动区域。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzScrollArea(QWidget *parent = nullptr);

    /** @brief 使用 Qt 父子所有权销毁 viewport、内容和滚动条。 */
    ~ZzScrollArea() override;

    /**
     * @brief 返回当前水平 Fluent 滚动条。
     * @return 当前为 ZzScrollBar 时返回非拥有指针，被替换后返回空。
     */
    [[nodiscard]] ZzScrollBar *fluentHorizontalScrollBar() const noexcept;

    /**
     * @brief 返回当前垂直 Fluent 滚动条。
     * @return 当前为 ZzScrollBar 时返回非拥有指针，被替换后返回空。
     */
    [[nodiscard]] ZzScrollBar *fluentVerticalScrollBar() const noexcept;
};

} // namespace ZzFluentUI
