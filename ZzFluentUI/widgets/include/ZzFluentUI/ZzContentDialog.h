#pragma once

#include <memory>

#include <QtCore/QString>
#include <QtWidgets/QDialog>

#include <ZzFluentUI/ZzContentDialogButton.h>
#include <ZzFluentUI/ZzContentDialogResult.h>
#include <ZzFluentUI/ZzFluentUIExport.h>

class QEvent;
class QHideEvent;
class QKeyEvent;
class QPaintEvent;
class QShowEvent;

namespace ZzFluentUI {

class ZzContentDialogPrivate;

/**
 * @brief 提供具有主题遮罩、可替换内容和三个标准操作的 Fluent 对话框。
 *
 * 对话框只承载展示内容并返回用户操作，不读取业务模型。自定义内容默认由
 * 对话框接管；需要保留旧内容时，调用者必须先调用 takeContentWidget()。
 */
class ZZ_FLUENT_UI_EXPORT ZzContentDialog final : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ZzContentDialog)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        QWidget *contentWidget
        READ contentWidget
        WRITE setContentWidget
        NOTIFY contentWidgetChanged)
    Q_PROPERTY(
        QString primaryButtonText
        READ primaryButtonText
        WRITE setPrimaryButtonText
        NOTIFY primaryButtonTextChanged)
    Q_PROPERTY(
        bool primaryButtonVisible
        READ isPrimaryButtonVisible
        WRITE setPrimaryButtonVisible
        NOTIFY primaryButtonVisibleChanged)
    Q_PROPERTY(
        bool primaryButtonEnabled
        READ isPrimaryButtonEnabled
        WRITE setPrimaryButtonEnabled
        NOTIFY primaryButtonEnabledChanged)
    Q_PROPERTY(
        QString secondaryButtonText
        READ secondaryButtonText
        WRITE setSecondaryButtonText
        NOTIFY secondaryButtonTextChanged)
    Q_PROPERTY(
        bool secondaryButtonVisible
        READ isSecondaryButtonVisible
        WRITE setSecondaryButtonVisible
        NOTIFY secondaryButtonVisibleChanged)
    Q_PROPERTY(
        bool secondaryButtonEnabled
        READ isSecondaryButtonEnabled
        WRITE setSecondaryButtonEnabled
        NOTIFY secondaryButtonEnabledChanged)
    Q_PROPERTY(
        QString closeButtonText
        READ closeButtonText
        WRITE setCloseButtonText
        NOTIFY closeButtonTextChanged)
    Q_PROPERTY(
        bool closeButtonVisible
        READ isCloseButtonVisible
        WRITE setCloseButtonVisible
        NOTIFY closeButtonVisibleChanged)
    Q_PROPERTY(
        bool closeButtonEnabled
        READ isCloseButtonEnabled
        WRITE setCloseButtonEnabled
        NOTIFY closeButtonEnabledChanged)
    Q_PROPERTY(
        ZzContentDialogButton defaultButton
        READ defaultButton
        WRITE setDefaultButton
        NOTIFY defaultButtonChanged)
    Q_PROPERTY(
        ZzContentDialogResult dialogResult
        READ dialogResult
        NOTIFY dialogResultChanged)

public:
    /**
     * @brief 创建默认仅显示关闭按钮的无边框内容对话框。
     * @param parent 可为空的所属窗口；模态遮罩只覆盖该窗口。
     */
    explicit ZzContentDialog(QWidget *parent = nullptr);

    /** @brief 清理实例私有遮罩并销毁所拥有的内容。 */
    ~ZzContentDialog() override;

    /** @brief 返回标题文本。 */
    [[nodiscard]] QString title() const;

    /** @brief 设置标题文本；重复设置不发信号。 */
    void setTitle(QString title);

    /** @brief 返回正文文本。 */
    [[nodiscard]] QString text() const;

    /** @brief 设置可换行正文；重复设置不发信号。 */
    void setText(QString text);

    /** @brief 返回当前由对话框拥有的自定义内容。 */
    [[nodiscard]] QWidget *contentWidget() const noexcept;

    /**
     * @brief 接管并展示自定义内容，替换时删除旧内容。
     * @param widget 可为空；非空对象会重挂到内部内容容器。
     */
    void setContentWidget(QWidget *widget);

    /**
     * @brief 取回自定义内容并解除 parent。
     * @return 原内容；为空表示没有内容，非空时所有权交给调用者。
     */
    [[nodiscard]] QWidget *takeContentWidget();

    /** @brief 返回主按钮文本。 */
    [[nodiscard]] QString primaryButtonText() const;

    /** @brief 设置主按钮文本。 */
    void setPrimaryButtonText(QString text);

    /** @brief 返回主按钮是否可见。 */
    [[nodiscard]] bool isPrimaryButtonVisible() const noexcept;

    /** @brief 设置主按钮是否可见。 */
    void setPrimaryButtonVisible(bool visible);

    /** @brief 返回主按钮是否启用。 */
    [[nodiscard]] bool isPrimaryButtonEnabled() const noexcept;

    /** @brief 设置主按钮是否启用。 */
    void setPrimaryButtonEnabled(bool enabled);

    /** @brief 返回次按钮文本。 */
    [[nodiscard]] QString secondaryButtonText() const;

    /** @brief 设置次按钮文本。 */
    void setSecondaryButtonText(QString text);

    /** @brief 返回次按钮是否可见。 */
    [[nodiscard]] bool isSecondaryButtonVisible() const noexcept;

    /** @brief 设置次按钮是否可见。 */
    void setSecondaryButtonVisible(bool visible);

    /** @brief 返回次按钮是否启用。 */
    [[nodiscard]] bool isSecondaryButtonEnabled() const noexcept;

    /** @brief 设置次按钮是否启用。 */
    void setSecondaryButtonEnabled(bool enabled);

    /** @brief 返回关闭按钮文本。 */
    [[nodiscard]] QString closeButtonText() const;

    /** @brief 设置关闭按钮文本。 */
    void setCloseButtonText(QString text);

    /** @brief 返回关闭按钮是否可见。 */
    [[nodiscard]] bool isCloseButtonVisible() const noexcept;

    /** @brief 设置关闭按钮是否可见。 */
    void setCloseButtonVisible(bool visible);

    /** @brief 返回关闭按钮是否启用。 */
    [[nodiscard]] bool isCloseButtonEnabled() const noexcept;

    /** @brief 设置关闭按钮是否启用。 */
    void setCloseButtonEnabled(bool enabled);

    /** @brief 返回当前 Enter 默认操作。 */
    [[nodiscard]] ZzContentDialogButton defaultButton() const noexcept;

    /** @brief 设置 Enter 默认操作；None 禁止隐式选择。 */
    void setDefaultButton(ZzContentDialogButton button);

    /** @brief 返回本次显示周期内最近一次完成结果。 */
    [[nodiscard]] ZzContentDialogResult dialogResult() const noexcept;

    /** @brief 完成对话框并把 Qt 结果映射为稳定的公开结果。 */
    void done(int resultCode) override;

Q_SIGNALS:
    /** @brief 标题实际变化后发出。 */
    void titleChanged(const QString &title);

    /** @brief 正文实际变化后发出。 */
    void textChanged(const QString &text);

    /** @brief 内容所有权实际变化后发出。 */
    void contentWidgetChanged(QWidget *widget);

    /** @brief 主按钮文本实际变化后发出。 */
    void primaryButtonTextChanged(const QString &text);

    /** @brief 主按钮可见性实际变化后发出。 */
    void primaryButtonVisibleChanged(bool visible);

    /** @brief 主按钮启用状态实际变化后发出。 */
    void primaryButtonEnabledChanged(bool enabled);

    /** @brief 次按钮文本实际变化后发出。 */
    void secondaryButtonTextChanged(const QString &text);

    /** @brief 次按钮可见性实际变化后发出。 */
    void secondaryButtonVisibleChanged(bool visible);

    /** @brief 次按钮启用状态实际变化后发出。 */
    void secondaryButtonEnabledChanged(bool enabled);

    /** @brief 关闭按钮文本实际变化后发出。 */
    void closeButtonTextChanged(const QString &text);

    /** @brief 关闭按钮可见性实际变化后发出。 */
    void closeButtonVisibleChanged(bool visible);

    /** @brief 关闭按钮启用状态实际变化后发出。 */
    void closeButtonEnabledChanged(bool enabled);

    /** @brief 默认按钮实际变化后发出。 */
    void defaultButtonChanged(ZzContentDialogButton button);

    /** @brief 当前显示周期结果实际变化后发出。 */
    void dialogResultChanged(ZzContentDialogResult result);

protected:
    /** @brief 使用主题浮层原语绘制无边框对话框表面。 */
    void paintEvent(QPaintEvent *event) override;

    /** @brief 更新语言、主题、字体和遮罩快照。 */
    void changeEvent(QEvent *event) override;

    /** @brief 将 Enter 和 Escape 映射为显式默认操作或关闭。 */
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief 保存焦点、重置结果并为模态显示创建实例遮罩。 */
    void showEvent(QShowEvent *event) override;

    /** @brief 清理遮罩并恢复显示前焦点。 */
    void hideEvent(QHideEvent *event) override;

private:
    std::unique_ptr<ZzContentDialogPrivate> d_ptr;
};

} // namespace ZzFluentUI
