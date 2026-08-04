#include "ZzMessageBarPrivate.h"

#include <algorithm>
#include <utility>

#include <QtCore/QTimer>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzMessageBar.h>

namespace ZzFluentUI {

ZzMessageBarPrivate::ZzMessageBarPrivate(ZzMessageBar *q)
    : q_ptr(q)
    , iconLabel(new QLabel(q))
    , textLabel(new QLabel(q))
    , closeButton(new QToolButton(q))
    , timer(new QTimer(q))
{
    Q_ASSERT(q_ptr != nullptr);
    auto *layout = new QHBoxLayout(q_ptr);
    layout->setContentsMargins(12, 8, 8, 8);
    layout->setSpacing(8);
    iconLabel->setFixedSize(20, 20);
    iconLabel->setAlignment(Qt::AlignCenter);
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    closeButton->setObjectName(
        QStringLiteral("zzMessageBarCloseButton"));
    closeButton->setAutoRaise(true);
    closeButton->setFocusPolicy(Qt::StrongFocus);
    timer->setSingleShot(true);
    layout->addWidget(iconLabel);
    layout->addWidget(textLabel, 1);
    layout->addWidget(closeButton);

    QObject::connect(
        closeButton,
        &QToolButton::clicked,
        q_ptr,
        [this] {
            requestClose();
        });
    QObject::connect(
        timer,
        &QTimer::timeout,
        q_ptr,
        [this] {
            requestClose();
        });
    refreshPresentation();
}

void ZzMessageBarPrivate::refreshPresentation()
{
    textLabel->setText(text);
    q_ptr->setAccessibleName(text);
    closeButton->setVisible(closable);
    const QString closeText = ZzMessageBar::tr("关闭");
    closeButton->setToolTip(closeText);
    closeButton->setAccessibleName(closeText);
    closeButton->setIcon(q_ptr->style()->standardIcon(
        QStyle::SP_TitleBarCloseButton,
        nullptr,
        q_ptr));

    QStyle::StandardPixmap standardPixmap = QStyle::SP_MessageBoxInformation;
    switch (severity) {
    case ZzMessageSeverity::Information:
    case ZzMessageSeverity::Success:
        standardPixmap = QStyle::SP_MessageBoxInformation;
        break;
    case ZzMessageSeverity::Warning:
        standardPixmap = QStyle::SP_MessageBoxWarning;
        break;
    case ZzMessageSeverity::Error:
        standardPixmap = QStyle::SP_MessageBoxCritical;
        break;
    }
    QPixmap pixmap = q_ptr->style()
        ->standardIcon(standardPixmap, nullptr, q_ptr)
        .pixmap(QSize(20, 20), q_ptr->devicePixelRatioF());
    if (severity == ZzMessageSeverity::Success && !pixmap.isNull()) {
        QImage image = pixmap.toImage();
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(
            image.rect(),
            q_ptr->palette().color(QPalette::Highlight));
        painter.end();
        pixmap = QPixmap::fromImage(std::move(image));
        pixmap.setDevicePixelRatio(q_ptr->devicePixelRatioF());
    }
    iconLabel->setPixmap(pixmap);
}

void ZzMessageBarPrivate::restartTimer()
{
    timer->stop();
    remainingMilliseconds = timeoutMilliseconds;
    if (timeoutMilliseconds > 0
        && q_ptr->isVisible()
        && !hovered
        && !closePending) {
        timer->start(remainingMilliseconds);
    }
}

void ZzMessageBarPrivate::pauseTimer() noexcept
{
    if (timer->isActive()) {
        remainingMilliseconds = std::max(1, timer->remainingTime());
        timer->stop();
    }
}

void ZzMessageBarPrivate::resumeTimer()
{
    if (timeoutMilliseconds <= 0
        || !q_ptr->isVisible()
        || hovered
        || closePending) {
        return;
    }
    if (remainingMilliseconds <= 0) {
        remainingMilliseconds = timeoutMilliseconds;
    }
    timer->start(remainingMilliseconds);
}

void ZzMessageBarPrivate::requestClose()
{
    if (closePending) {
        return;
    }
    closePending = true;
    timer->stop();
    Q_EMIT q_ptr->closeRequested();
}

} // namespace ZzFluentUI
