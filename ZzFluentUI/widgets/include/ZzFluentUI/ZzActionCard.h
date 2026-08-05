#pragma once

#include <memory>

#include <QtWidgets/QAbstractButton>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QKeyEvent;
class QPaintEvent;

namespace ZzFluentUI {

class ZzActionCardPrivate;

/**
 * @brief 显示图标、标题、说明和可选尾部指示器的操作卡片。
 *
 * 控件复用 QAbstractButton 的点击、checkable、焦点和无障碍语义。
 * 业务层应连接 clicked 信号执行命令，控件自身不访问业务对象。
 */
class ZZ_FLUENT_UI_EXPORT ZzActionCard final : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(
        QString description
        READ description
        WRITE setDescription
        NOTIFY descriptionChanged)
    Q_PROPERTY(
        bool trailingIndicatorVisible
        READ isTrailingIndicatorVisible
        WRITE setTrailingIndicatorVisible
        NOTIFY trailingIndicatorVisibleChanged)
    Q_DISABLE_COPY_MOVE(ZzActionCard)

public:
    /**
     * @brief 创建无标题的操作卡片。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzActionCard(QWidget *parent = nullptr);

    /**
     * @brief 创建带标题和说明的操作卡片。
     * @param text 可本地化的标题。
     * @param description 可本地化的辅助说明。
     * @param parent 可为空的 QWidget 所有者。
     */
    explicit ZzActionCard(
        const QString &text,
        const QString &description = {},
        QWidget *parent = nullptr);

    /** @brief 销毁私有展示状态。 */
    ~ZzActionCard() override;

    /**
     * @brief 返回辅助说明文字。
     * @return 当前说明文字的隐式共享副本。
     */
    [[nodiscard]] QString description() const;

    /**
     * @brief 更新辅助说明并同步默认可访问描述。
     * @param description 新的可本地化说明。
     */
    void setDescription(QString description);

    /**
     * @brief 返回是否显示随布局方向变化的尾部指示器。
     * @return 显示时返回 true。
     */
    [[nodiscard]] bool isTrailingIndicatorVisible() const noexcept;

    /**
     * @brief 设置是否显示尾部指示器。
     * @param visible 是否显示指示器。
     */
    void setTrailingIndicatorVisible(bool visible);

    /**
     * @brief 返回适合单行标题和说明的建议尺寸。
     * @return 设备无关逻辑像素尺寸。
     */
    [[nodiscard]] QSize sizeHint() const override;

    /**
     * @brief 返回不会让图标和文字互相覆盖的最小尺寸。
     * @return 设备无关逻辑像素尺寸。
     */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /**
     * @brief 说明文字变化后发出。
     * @param description 新说明文字。
     */
    void descriptionChanged(const QString &description);

    /**
     * @brief 尾部指示器可见性变化后发出。
     * @param visible 新可见性。
     */
    void trailingIndicatorVisibleChanged(bool visible);

protected:
    /** @brief 使用当前 style、palette 和按钮状态绘制操作卡片。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 处理 Enter/Return，并保留 QAbstractButton 的 Space 语义。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 在字体、style、palette 或布局方向变化后刷新展示。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzActionCardPrivate> d_ptr;
};

} // namespace ZzFluentUI
