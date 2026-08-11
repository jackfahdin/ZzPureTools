#include "ZzContentDialogPrivate.h"

#include <algorithm>

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzContentDialog.h>
#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

namespace ZzFluentUI {

ZzContentDialogPrivate::ZzContentDialogPrivate(ZzContentDialog *q)
    : q_ptr(q)
    , theme(q)
    , titleLabel(new QLabel(q))
    , textLabel(new QLabel(q))
    , contentHost(new QWidget(q))
    , contentLayout(new QVBoxLayout(contentHost))
    , buttonHost(new QWidget(q))
    , buttonLayout(new QHBoxLayout(buttonHost))
    , primaryButton(new ZzPushButton(buttonHost))
    , secondaryButton(new ZzPushButton(buttonHost))
    , closeButton(new ZzPushButton(buttonHost))
{
    Q_ASSERT(q_ptr != nullptr);
    auto *rootLayout = new QVBoxLayout(q_ptr);
    rootLayout->setObjectName(QStringLiteral("zzContentDialogRootLayout"));
    titleLabel->setObjectName(QStringLiteral("zzContentDialogTitle"));
    titleLabel->setWordWrap(true);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLabel->setObjectName(QStringLiteral("zzContentDialogText"));
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    contentHost->setObjectName(QStringLiteral("zzContentDialogContentHost"));
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    buttonHost->setObjectName(QStringLiteral("zzContentDialogButtonHost"));
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    primaryButton->setObjectName(
        QStringLiteral("zzContentDialogPrimaryButton"));
    secondaryButton->setObjectName(
        QStringLiteral("zzContentDialogSecondaryButton"));
    closeButton->setObjectName(QStringLiteral("zzContentDialogCloseButton"));
    primaryButton->setAppearance(ZzButtonAppearance::Accent);
    for (ZzPushButton *button : {
             primaryButton, secondaryButton, closeButton}) {
        button->setAutoDefault(false);
        button->setDefault(false);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        buttonLayout->addWidget(button, 1);
    }
    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(textLabel);
    rootLayout->addWidget(contentHost);
    rootLayout->addWidget(buttonHost);

    QObject::connect(
        primaryButton,
        &QPushButton::clicked,
        q_ptr,
        [this] {
            q_ptr->done(static_cast<int>(ZzContentDialogResult::Primary));
        });
    QObject::connect(
        secondaryButton,
        &QPushButton::clicked,
        q_ptr,
        [this] {
            q_ptr->done(static_cast<int>(ZzContentDialogResult::Secondary));
        });
    QObject::connect(
        closeButton,
        &QPushButton::clicked,
        q_ptr,
        &QDialog::reject);
    refreshDefaultButtonTexts(false);
}

ZzContentDialogPrivate::~ZzContentDialogPrivate()
{
    if (contentDestroyedConnection) {
        QObject::disconnect(contentDestroyedConnection);
        contentDestroyedConnection = {};
    }
    removeOverlay();
}

void ZzContentDialogPrivate::refreshPresentation()
{
    const auto snapshot = theme.snapshot();
    titleLabel->setText(title);
    titleLabel->setVisible(!title.isEmpty());
    titleLabel->setFont(snapshot->font(ZzTypographyToken::Title));
    textLabel->setText(text);
    textLabel->setVisible(!text.isEmpty());
    textLabel->setFont(snapshot->font(ZzTypographyToken::Body));
    q_ptr->setWindowTitle(title);
    q_ptr->setAccessibleDescription(text);
    if (q_ptr->accessibleName().isEmpty()
        || q_ptr->accessibleName() == generatedAccessibleName) {
        generatedAccessibleName = title;
        q_ptr->setAccessibleName(generatedAccessibleName);
    }

    const int outerPadding = qCeil(
        snapshot->metric(ZzMetricToken::OverlayPadding));
    const int sectionSpacing = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    const int buttonSpacing = qCeil(
        snapshot->metric(ZzMetricToken::VerticalPadding));
    auto *rootLayout = qobject_cast<QVBoxLayout *>(q_ptr->layout());
    Q_ASSERT(rootLayout != nullptr);
    rootLayout->setContentsMargins(
        outerPadding,
        outerPadding,
        outerPadding,
        outerPadding);
    rootLayout->setSpacing(sectionSpacing);
    buttonLayout->setSpacing(buttonSpacing);
    q_ptr->setMinimumWidth(qCeil(
        snapshot->metric(ZzMetricToken::DialogMinWidth)));
    q_ptr->setMaximumWidth(qCeil(
        snapshot->metric(ZzMetricToken::DialogMaxWidth)));
    for (ZzPushButton *button : {
             primaryButton, secondaryButton, closeButton}) {
        button->setFont(snapshot->font(ZzTypographyToken::Body));
    }
    refreshButtons();
    q_ptr->updateGeometry();
    q_ptr->update();
    if (overlay != nullptr) {
        overlay->update();
    }
}

void ZzContentDialogPrivate::refreshTheme()
{
    theme.refreshFallback();
    refreshPresentation();
}

void ZzContentDialogPrivate::setContentWidget(QWidget *widget)
{
    if (contentWidget == widget) {
        return;
    }
    if (widget == q_ptr || widget == contentHost
        || (widget != nullptr && widget->isAncestorOf(q_ptr))) {
        qWarning("ZzContentDialog rejected a cyclic content widget");
        return;
    }

    if (contentDestroyedConnection) {
        QObject::disconnect(contentDestroyedConnection);
        contentDestroyedConnection = {};
    }
    QWidget *oldWidget = contentWidget.data();
    contentWidget = nullptr;
    if (oldWidget != nullptr) {
        contentLayout->removeWidget(oldWidget);
        delete oldWidget;
    }

    contentWidget = widget;
    if (widget != nullptr) {
        widget->setParent(contentHost);
        contentLayout->addWidget(widget);
        contentDestroyedConnection = QObject::connect(
            widget,
            &QObject::destroyed,
            q_ptr,
            [this] {
                contentWidget = nullptr;
                contentDestroyedConnection = {};
                contentHost->hide();
                q_ptr->updateGeometry();
                Q_EMIT q_ptr->contentWidgetChanged(nullptr);
            });
    }
    contentHost->setVisible(widget != nullptr);
    q_ptr->updateGeometry();
    Q_EMIT q_ptr->contentWidgetChanged(widget);
}

QWidget *ZzContentDialogPrivate::takeContentWidget()
{
    QWidget *widget = contentWidget.data();
    if (widget == nullptr) {
        return nullptr;
    }
    if (contentDestroyedConnection) {
        QObject::disconnect(contentDestroyedConnection);
        contentDestroyedConnection = {};
    }
    contentWidget = nullptr;
    contentLayout->removeWidget(widget);
    widget->setParent(nullptr);
    contentHost->hide();
    q_ptr->updateGeometry();
    Q_EMIT q_ptr->contentWidgetChanged(nullptr);
    return widget;
}

ZzPushButton *ZzContentDialogPrivate::activeDefaultButton() const noexcept
{
    ZzPushButton *button = nullptr;
    switch (defaultButton) {
    case ZzContentDialogButton::None:
        break;
    case ZzContentDialogButton::Primary:
        button = primaryButton;
        break;
    case ZzContentDialogButton::Secondary:
        button = secondaryButton;
        break;
    case ZzContentDialogButton::Close:
        button = closeButton;
        break;
    }
    return button != nullptr && button->isVisible() && button->isEnabled()
        ? button
        : nullptr;
}

void ZzContentDialogPrivate::triggerDefaultButton()
{
    if (ZzPushButton *button = activeDefaultButton()) {
        button->click();
    }
}

void ZzContentDialogPrivate::beginPresentation()
{
    QWidget *focus = QApplication::focusWidget();
    previousFocus = focus != nullptr && !q_ptr->isAncestorOf(focus)
        ? focus
        : nullptr;
    setDialogResult(ZzContentDialogResult::None);
    ensureOverlay();
}

void ZzContentDialogPrivate::endPresentation()
{
    removeOverlay();
    QWidget *focus = previousFocus.data();
    previousFocus = nullptr;
    if (focus != nullptr && focus->isVisible() && focus->isEnabled()) {
        focus->setFocus(Qt::OtherFocusReason);
    }
}

void ZzContentDialogPrivate::setDialogResult(ZzContentDialogResult result)
{
    if (dialogResult == result) {
        return;
    }
    dialogResult = result;
    Q_EMIT q_ptr->dialogResultChanged(result);
}

bool ZzContentDialogPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (event == nullptr) {
        return QObject::eventFilter(watched, event);
    }
    if (watched == overlay.data() && event->type() == QEvent::Paint) {
        QPainter painter(overlay.data());
        ZzFluentPainter::drawOverlayScrim(
            &painter,
            QRectF(overlay->rect()),
            *theme.snapshot());
        return true;
    }
    if (watched == overlayHost.data()
        && (event->type() == QEvent::Resize
            || event->type() == QEvent::Show)) {
        if (overlay != nullptr && overlayHost != nullptr) {
            overlay->setGeometry(overlayHost->rect());
            overlay->raise();
        }
    }
    return QObject::eventFilter(watched, event);
}

void ZzContentDialogPrivate::refreshDefaultButtonTexts(bool notify)
{
    const auto updateText = [this, notify](
                                bool customized,
                                QString *stored,
                                const QString &translated,
                                auto signal) {
        if (customized || *stored == translated) {
            return;
        }
        *stored = translated;
        if (notify) {
            Q_EMIT (q_ptr->*signal)(*stored);
        }
    };
    updateText(
        primaryButtonTextCustomized,
        &primaryButtonText,
        ZzContentDialog::tr("确定"),
        &ZzContentDialog::primaryButtonTextChanged);
    updateText(
        secondaryButtonTextCustomized,
        &secondaryButtonText,
        ZzContentDialog::tr("取消"),
        &ZzContentDialog::secondaryButtonTextChanged);
    updateText(
        closeButtonTextCustomized,
        &closeButtonText,
        ZzContentDialog::tr("关闭"),
        &ZzContentDialog::closeButtonTextChanged);
}

void ZzContentDialogPrivate::refreshButtons()
{
    primaryButton->setText(primaryButtonText);
    primaryButton->setVisible(primaryButtonVisible);
    primaryButton->setEnabled(primaryButtonEnabled);
    secondaryButton->setText(secondaryButtonText);
    secondaryButton->setVisible(secondaryButtonVisible);
    secondaryButton->setEnabled(secondaryButtonEnabled);
    closeButton->setText(closeButtonText);
    closeButton->setVisible(closeButtonVisible);
    closeButton->setEnabled(closeButtonEnabled);
    buttonHost->setVisible(
        primaryButtonVisible || secondaryButtonVisible || closeButtonVisible);

    ZzPushButton *active = activeDefaultButton();
    for (ZzPushButton *button : {
             primaryButton, secondaryButton, closeButton}) {
        button->setDefault(button == active);
    }
}

void ZzContentDialogPrivate::ensureOverlay()
{
    if (!q_ptr->isModal() || overlay != nullptr) {
        return;
    }
    QWidget *host = modalHost();
    if (host == nullptr) {
        return;
    }
    overlayHost = host;
    overlay = new QWidget(host);
    overlay->setObjectName(QStringLiteral("zzContentDialogOverlay"));
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlay->setAttribute(Qt::WA_NoSystemBackground, true);
    overlay->setFocusPolicy(Qt::NoFocus);
    overlay->setGeometry(host->rect());
    overlay->installEventFilter(this);
    host->installEventFilter(this);
    overlay->show();
    overlay->raise();
}

void ZzContentDialogPrivate::removeOverlay()
{
    QWidget *host = overlayHost.data();
    QWidget *widget = overlay.data();
    overlay = nullptr;
    overlayHost = nullptr;
    if (host != nullptr) {
        host->removeEventFilter(this);
    }
    if (widget != nullptr) {
        widget->removeEventFilter(this);
        delete widget;
    }
}

QWidget *ZzContentDialogPrivate::modalHost() const noexcept
{
    QWidget *parent = q_ptr->parentWidget();
    return parent != nullptr ? parent->window() : nullptr;
}

} // namespace ZzFluentUI
