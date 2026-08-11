#pragma once

#include <memory>

#include <QtGui/QIcon>
#include <QtWidgets/QLabel>

#include <ZzFluentUI/ZzFluentUIExport.h>
#include <ZzFluentUI/ZzInfoBadgeKind.h>
#include <ZzFluentUI/ZzMessageSeverity.h>

class QEvent;
class QPaintEvent;

namespace ZzFluentUI {

class ZzInfoBadgePrivate;

/** @brief 以圆点、数字或图标展示纯视觉状态，不拥有通知数据。 */
class ZZ_FLUENT_UI_EXPORT ZzInfoBadge final : public QLabel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzInfoBadge)
    Q_PROPERTY(
        ZzInfoBadgeKind kind READ kind WRITE setKind NOTIFY kindChanged)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(
        int maximumValue
        READ maximumValue
        WRITE setMaximumValue
        NOTIFY maximumValueChanged)
    Q_PROPERTY(
        ZzMessageSeverity severity
        READ severity
        WRITE setSeverity
        NOTIFY severityChanged)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)

public:
    /**
     * @brief 创建默认显示信息圆点的徽章。
     * @param parent 可为空的 QObject 所有者。
     */
    explicit ZzInfoBadge(QWidget *parent = nullptr);

    /** @brief 销毁私有主题缓存。 */
    ~ZzInfoBadge() override;

    /** @brief 返回当前展示种类。 */
    [[nodiscard]] ZzInfoBadgeKind kind() const noexcept;

    /** @brief 设置展示种类；重复设置不发信号。 */
    void setKind(ZzInfoBadgeKind kind);

    /** @brief 返回钳制后的非负数值。 */
    [[nodiscard]] int value() const noexcept;

    /** @brief 设置数值，负值按 0 处理。 */
    void setValue(int value);

    /** @brief 返回数字展示上限。 */
    [[nodiscard]] int maximumValue() const noexcept;

    /** @brief 设置至少为 1 的数字展示上限。 */
    void setMaximumValue(int maximumValue);

    /** @brief 返回纯展示严重性。 */
    [[nodiscard]] ZzMessageSeverity severity() const noexcept;

    /** @brief 设置严重性颜色；不改变业务状态。 */
    void setSeverity(ZzMessageSeverity severity);

    /** @brief 返回图标模式使用的隐式共享图标。 */
    [[nodiscard]] QIcon icon() const;

    /** @brief 设置图标模式使用的图标。 */
    void setIcon(QIcon icon);

    /** @brief 返回当前种类和字体对应的稳定建议尺寸。 */
    [[nodiscard]] QSize sizeHint() const override;

    /** @brief 返回不小于命中和可读下限的最小建议尺寸。 */
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /** @brief 展示种类实际变化后发出。 */
    void kindChanged(ZzInfoBadgeKind kind);

    /** @brief 钳制后数值实际变化后发出。 */
    void valueChanged(int value);

    /** @brief 钳制后上限实际变化后发出。 */
    void maximumValueChanged(int maximumValue);

    /** @brief 严重性实际变化后发出。 */
    void severityChanged(ZzMessageSeverity severity);

    /** @brief 图标缓存键实际变化后发出。 */
    void iconChanged(const QIcon &icon);

protected:
    /** @brief 使用主题令牌绘制圆点、胶囊或图标终态。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 在语言、样式和调色板变化时刷新缓存展示。 */
    void changeEvent(QEvent *event) override;

private:
    std::unique_ptr<ZzInfoBadgePrivate> d_ptr;
};

} // namespace ZzFluentUI
