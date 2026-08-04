#pragma once

#include <memory>

#include <QtWidgets/QPushButton>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzFluentUIExport.h>

namespace ZzFluentUI {

class ZzPushButtonPrivate;

/**
 * @brief 提供 Fluent 外观级别并保留 QPushButton 原生交互语义。
 *
 * 控件必须在 GUI 线程创建和使用，由 QObject parent 所有；控件不持有业务对象。
 */
class ZZ_FLUENT_UI_EXPORT ZzPushButton final : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(
        ZzFluentUI::ZzButtonAppearance appearance
        READ appearance
        WRITE setAppearance)
    Q_DISABLE_COPY_MOVE(ZzPushButton)

public:
    /**
     * @brief 创建无文本按钮。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzPushButton(QWidget *parent = nullptr);

    /**
     * @brief 创建显示指定文本的按钮。
     * @param text 可本地化的按钮文本。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzPushButton(
        const QString &text,
        QWidget *parent = nullptr);

    /** @brief 销毁私有状态，不改变 QObject parent 所有权。 */
    ~ZzPushButton() override;

    /**
     * @brief 返回当前视觉强调级别。
     * @return 当前按钮外观。
     */
    [[nodiscard]] ZzButtonAppearance appearance() const noexcept;

    /**
     * @brief 更新视觉强调级别而不改变按钮业务状态。
     * @param appearance 新外观。
     */
    void setAppearance(ZzButtonAppearance appearance);

protected:
    /** @brief 使用当前样式绘制外观 option，并保留按钮原生状态。 */
    void paintEvent(QPaintEvent *event) override;

private:
    friend class ZzPushButtonPrivate;
    std::unique_ptr<ZzPushButtonPrivate> d_ptr;
};

} // namespace ZzFluentUI
