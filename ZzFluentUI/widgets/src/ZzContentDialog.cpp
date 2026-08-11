#include <ZzFluentUI/ZzContentDialog.h>

#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QShowEvent>

#include <ZzFluentUI/ZzFluentPainter.h>
#include <ZzFluentUI/ZzPushButton.h>

#include "private/ZzContentDialogPrivate.h"

namespace ZzFluentUI {

ZzContentDialog::ZzContentDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
    , d_ptr(std::make_unique<ZzContentDialogPrivate>(this))
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizeGripEnabled(false);
    d_ptr->refreshPresentation();
}

ZzContentDialog::~ZzContentDialog() = default;

QString ZzContentDialog::title() const
{
    return d_ptr->title;
}

void ZzContentDialog::setTitle(QString title)
{
    if (d_ptr->title == title) {
        return;
    }
    d_ptr->title = std::move(title);
    d_ptr->refreshPresentation();
    Q_EMIT titleChanged(d_ptr->title);
}

QString ZzContentDialog::text() const
{
    return d_ptr->text;
}

void ZzContentDialog::setText(QString text)
{
    if (d_ptr->text == text) {
        return;
    }
    d_ptr->text = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT textChanged(d_ptr->text);
}

QWidget *ZzContentDialog::contentWidget() const noexcept
{
    return d_ptr->contentWidget.data();
}

void ZzContentDialog::setContentWidget(QWidget *widget)
{
    d_ptr->setContentWidget(widget);
}

QWidget *ZzContentDialog::takeContentWidget()
{
    return d_ptr->takeContentWidget();
}

QString ZzContentDialog::primaryButtonText() const
{
    return d_ptr->primaryButtonText;
}

void ZzContentDialog::setPrimaryButtonText(QString text)
{
    if (d_ptr->primaryButtonText == text) {
        return;
    }
    d_ptr->primaryButtonTextCustomized = true;
    d_ptr->primaryButtonText = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT primaryButtonTextChanged(d_ptr->primaryButtonText);
}

bool ZzContentDialog::isPrimaryButtonVisible() const noexcept
{
    return d_ptr->primaryButtonVisible;
}

void ZzContentDialog::setPrimaryButtonVisible(bool visible)
{
    if (d_ptr->primaryButtonVisible == visible) {
        return;
    }
    d_ptr->primaryButtonVisible = visible;
    d_ptr->refreshPresentation();
    Q_EMIT primaryButtonVisibleChanged(visible);
}

bool ZzContentDialog::isPrimaryButtonEnabled() const noexcept
{
    return d_ptr->primaryButtonEnabled;
}

void ZzContentDialog::setPrimaryButtonEnabled(bool enabled)
{
    if (d_ptr->primaryButtonEnabled == enabled) {
        return;
    }
    d_ptr->primaryButtonEnabled = enabled;
    d_ptr->refreshPresentation();
    Q_EMIT primaryButtonEnabledChanged(enabled);
}

QString ZzContentDialog::secondaryButtonText() const
{
    return d_ptr->secondaryButtonText;
}

void ZzContentDialog::setSecondaryButtonText(QString text)
{
    if (d_ptr->secondaryButtonText == text) {
        return;
    }
    d_ptr->secondaryButtonTextCustomized = true;
    d_ptr->secondaryButtonText = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT secondaryButtonTextChanged(d_ptr->secondaryButtonText);
}

bool ZzContentDialog::isSecondaryButtonVisible() const noexcept
{
    return d_ptr->secondaryButtonVisible;
}

void ZzContentDialog::setSecondaryButtonVisible(bool visible)
{
    if (d_ptr->secondaryButtonVisible == visible) {
        return;
    }
    d_ptr->secondaryButtonVisible = visible;
    d_ptr->refreshPresentation();
    Q_EMIT secondaryButtonVisibleChanged(visible);
}

bool ZzContentDialog::isSecondaryButtonEnabled() const noexcept
{
    return d_ptr->secondaryButtonEnabled;
}

void ZzContentDialog::setSecondaryButtonEnabled(bool enabled)
{
    if (d_ptr->secondaryButtonEnabled == enabled) {
        return;
    }
    d_ptr->secondaryButtonEnabled = enabled;
    d_ptr->refreshPresentation();
    Q_EMIT secondaryButtonEnabledChanged(enabled);
}

QString ZzContentDialog::closeButtonText() const
{
    return d_ptr->closeButtonText;
}

void ZzContentDialog::setCloseButtonText(QString text)
{
    if (d_ptr->closeButtonText == text) {
        return;
    }
    d_ptr->closeButtonTextCustomized = true;
    d_ptr->closeButtonText = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT closeButtonTextChanged(d_ptr->closeButtonText);
}

bool ZzContentDialog::isCloseButtonVisible() const noexcept
{
    return d_ptr->closeButtonVisible;
}

void ZzContentDialog::setCloseButtonVisible(bool visible)
{
    if (d_ptr->closeButtonVisible == visible) {
        return;
    }
    d_ptr->closeButtonVisible = visible;
    d_ptr->refreshPresentation();
    Q_EMIT closeButtonVisibleChanged(visible);
}

bool ZzContentDialog::isCloseButtonEnabled() const noexcept
{
    return d_ptr->closeButtonEnabled;
}

void ZzContentDialog::setCloseButtonEnabled(bool enabled)
{
    if (d_ptr->closeButtonEnabled == enabled) {
        return;
    }
    d_ptr->closeButtonEnabled = enabled;
    d_ptr->refreshPresentation();
    Q_EMIT closeButtonEnabledChanged(enabled);
}

ZzContentDialogButton ZzContentDialog::defaultButton() const noexcept
{
    return d_ptr->defaultButton;
}

void ZzContentDialog::setDefaultButton(ZzContentDialogButton button)
{
    if (d_ptr->defaultButton == button) {
        return;
    }
    d_ptr->defaultButton = button;
    d_ptr->refreshPresentation();
    Q_EMIT defaultButtonChanged(button);
}

ZzContentDialogResult ZzContentDialog::dialogResult() const noexcept
{
    return d_ptr->dialogResult;
}

void ZzContentDialog::done(int resultCode)
{
    const QPointer<QWidget> previousFocus = d_ptr->previousFocus;
    ZzContentDialogResult result = ZzContentDialogResult::None;
    if (resultCode == static_cast<int>(ZzContentDialogResult::Primary)) {
        result = ZzContentDialogResult::Primary;
    } else if (resultCode
               == static_cast<int>(ZzContentDialogResult::Secondary)) {
        result = ZzContentDialogResult::Secondary;
    } else if (resultCode == QDialog::Rejected) {
        result = ZzContentDialogResult::Close;
    }
    d_ptr->setDialogResult(result);
    QDialog::done(resultCode);
    if (previousFocus != nullptr
        && previousFocus->isVisible()
        && previousFocus->isEnabled()) {
        QWidget *focusWindow = previousFocus->window();
        if (focusWindow != nullptr) {
            focusWindow->activateWindow();
        }
        previousFocus->setFocus(Qt::OtherFocusReason);
    }
}

void ZzContentDialog::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    ZzFluentPainter::drawPopupSurface(
        &painter,
        QRectF(rect()),
        *d_ptr->theme.snapshot());
}

void ZzContentDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event == nullptr) {
        return;
    }
    if (event->type() == QEvent::LanguageChange) {
        d_ptr->refreshDefaultButtonTexts(true);
        d_ptr->refreshPresentation();
        return;
    }
    if (event->type() == QEvent::StyleChange
        || event->type() == QEvent::PaletteChange) {
        d_ptr->refreshTheme();
        return;
    }
    if (event->type() == QEvent::FontChange
        || event->type() == QEvent::DevicePixelRatioChange) {
        d_ptr->refreshPresentation();
    }
}

void ZzContentDialog::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr
        && (event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter)) {
        if (!event->isAutoRepeat()) {
            d_ptr->triggerDefaultButton();
        }
        event->accept();
        return;
    }
    if (event != nullptr && event->key() == Qt::Key_Escape) {
        if (!event->isAutoRepeat()) {
            reject();
        }
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void ZzContentDialog::showEvent(QShowEvent *event)
{
    d_ptr->beginPresentation();
    QDialog::showEvent(event);
    if (auto *button = d_ptr->activeDefaultButton()) {
        button->setFocus(Qt::TabFocusReason);
    }
}

void ZzContentDialog::hideEvent(QHideEvent *event)
{
    QDialog::hideEvent(event);
    d_ptr->endPresentation();
}

} // namespace ZzFluentUI
