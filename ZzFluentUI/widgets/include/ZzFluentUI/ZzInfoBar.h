#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QPaintEvent;
class QPushButton;

namespace ZzFluentUI {

class ZzInfoBarPrivate;
class ZzInfoBarHost;

/** @brief 页面内可展开的非阻塞状态通知，也可交给 ZzInfoBarHost 浮动展示。 */
class ZZ_FLUENT_UI_EXPORT ZzInfoBar final : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzInfoBar)

public:
    /** @brief 状态通知级别。 */
    enum Severity { Informational, Success, Warning, Error };
    Q_ENUM(Severity)

    Q_PROPERTY(Severity severity READ severity WRITE setSeverity NOTIFY severityChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(QString actionButtonText READ actionButtonText WRITE setActionButtonText NOTIFY
            actionButtonTextChanged)
    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY openChanged)
    Q_PROPERTY(bool closable READ isClosable WRITE setClosable NOTIFY closableChanged)
    Q_PROPERTY(bool iconVisible READ isIconVisible WRITE setIconVisible NOTIFY iconVisibleChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY
            animationEnabledChanged)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration NOTIFY
            animationDurationChanged)

    /** @brief 创建初始关闭且隐藏的信息栏。 */
    explicit ZzInfoBar(QWidget* parent = nullptr);
    ~ZzInfoBar() override;

    [[nodiscard]] Severity severity() const;
    void setSeverity(Severity severity);
    [[nodiscard]] QString title() const;
    void setTitle(const QString& title);
    [[nodiscard]] QString message() const;
    void setMessage(const QString& message);
    [[nodiscard]] QString actionButtonText() const;
    void setActionButtonText(const QString& text);
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isClosable() const;
    void setClosable(bool closable);
    [[nodiscard]] bool isIconVisible() const;
    void setIconVisible(bool visible);
    [[nodiscard]] bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enabled);
    [[nodiscard]] int animationDuration() const;
    void setAnimationDuration(int duration);

    /** @brief 返回内建操作按钮；无文字时按钮隐藏。 */
    [[nodiscard]] QPushButton* actionButton() const;
    /** @brief 返回当前由信息栏拥有的自定义操作控件。 */
    [[nodiscard]] QWidget* actionWidget() const;
    /** @brief 接管控件，替换时销毁旧控件；拒绝会形成所有权环的控件。 */
    void setActionWidget(QWidget* widget);
    /** @brief 解除控件所有权并返回给调用方。 */
    [[nodiscard]] QWidget* takeActionWidget();

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;

public Q_SLOTS:
    /** @brief 展开或收起；重复设置无效果。 */
    void setOpen(bool open);
    /** @brief 收起信息栏。 */
    void dismiss();

Q_SIGNALS:
    void severityChanged(ZzInfoBar::Severity severity);
    void titleChanged(const QString& title);
    void messageChanged(const QString& message);
    void actionButtonTextChanged(const QString& text);
    void openChanged(bool open);
    void closableChanged(bool closable);
    void iconVisibleChanged(bool visible);
    void animationEnabledChanged(bool enabled);
    void animationDurationChanged(int duration);
    void actionTriggered();
    void closeButtonClicked();
    /** @brief 展开实际完成后发出。 */
    void opened();
    /** @brief 收起实际完成后发出。 */
    void closed();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class ZzInfoBarHost;
    void finishTransition();
    void finishPopupClose();
    void refreshContent();
    std::unique_ptr<ZzInfoBarPrivate> d_ptr;
};

} // namespace ZzFluentUI
