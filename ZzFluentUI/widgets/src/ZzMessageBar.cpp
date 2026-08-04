#include <ZzFluentUI/ZzMessageBar.h>

#include <algorithm>
#include <utility>

#include <QtCore/QEvent>
#include <QtGui/QEnterEvent>
#include <QtGui/QHideEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QShowEvent>

#include "private/ZzMessageBarPrivate.h"

namespace ZzFluentUI {

ZzMessageBar::ZzMessageBar(QWidget *parent)
    : QWidget(parent)
    , d_ptr(std::make_unique<ZzMessageBarPrivate>(this))
{
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus);
}

ZzMessageBar::~ZzMessageBar() = default;

QString ZzMessageBar::text() const
{
    return d_ptr->text;
}

void ZzMessageBar::setText(QString text)
{
    if (d_ptr->text == text) {
        return;
    }
    d_ptr->text = std::move(text);
    d_ptr->refreshPresentation();
    Q_EMIT textChanged(d_ptr->text);
}

ZzMessageSeverity ZzMessageBar::severity() const noexcept
{
    return d_ptr->severity;
}

void ZzMessageBar::setSeverity(ZzMessageSeverity severity)
{
    if (d_ptr->severity == severity) {
        return;
    }
    d_ptr->severity = severity;
    d_ptr->refreshPresentation();
    Q_EMIT severityChanged(severity);
}

bool ZzMessageBar::isClosable() const noexcept
{
    return d_ptr->closable;
}

void ZzMessageBar::setClosable(bool closable)
{
    if (d_ptr->closable == closable) {
        return;
    }
    d_ptr->closable = closable;
    d_ptr->refreshPresentation();
    Q_EMIT closableChanged(closable);
}

int ZzMessageBar::timeoutMilliseconds() const noexcept
{
    return d_ptr->timeoutMilliseconds;
}

void ZzMessageBar::setTimeoutMilliseconds(int milliseconds)
{
    const int bounded = std::max(0, milliseconds);
    if (d_ptr->timeoutMilliseconds == bounded) {
        return;
    }
    d_ptr->timeoutMilliseconds = bounded;
    d_ptr->restartTimer();
    Q_EMIT timeoutMillisecondsChanged(bounded);
}

void ZzMessageBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event != nullptr
        && (event->type() == QEvent::LanguageChange
            || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::DevicePixelRatioChange)) {
        d_ptr->refreshPresentation();
    }
}

void ZzMessageBar::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr
        && event->key() == Qt::Key_Escape
        && !event->isAutoRepeat()
        && d_ptr->closable) {
        d_ptr->requestClose();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ZzMessageBar::enterEvent(QEnterEvent *event)
{
    d_ptr->hovered = true;
    d_ptr->pauseTimer();
    QWidget::enterEvent(event);
}

void ZzMessageBar::leaveEvent(QEvent *event)
{
    d_ptr->hovered = false;
    d_ptr->resumeTimer();
    QWidget::leaveEvent(event);
}

void ZzMessageBar::hideEvent(QHideEvent *event)
{
    d_ptr->pauseTimer();
    QWidget::hideEvent(event);
}

void ZzMessageBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    d_ptr->closePending = false;
    d_ptr->hovered = false;
    d_ptr->restartTimer();
}

} // namespace ZzFluentUI
