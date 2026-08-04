#include "ZzFluentTitleBarPrivate.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentTitleBar.h>

namespace ZzFluentUI {

ZzFluentTitleBarPrivate::ZzFluentTitleBarPrivate(ZzFluentTitleBar *q)
    : q_ptr(q)
    , iconLabel(new QLabel(q))
    , titleLabel(new QLabel(q))
    , minimizeButton(new QToolButton(q))
    , maximizeButton(new QToolButton(q))
    , closeButton(new QToolButton(q))
{
    Q_ASSERT(q_ptr != nullptr);
    auto *layout = new QHBoxLayout(q_ptr);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(8);
    iconLabel->setObjectName(QStringLiteral("zzTitleBarWindowIcon"));
    iconLabel->setFixedSize(20, 20);
    iconLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setObjectName(QStringLiteral("zzTitleBarTitle"));
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    minimizeButton->setObjectName(QStringLiteral("zzTitleBarMinimizeButton"));
    maximizeButton->setObjectName(QStringLiteral("zzTitleBarMaximizeButton"));
    closeButton->setObjectName(QStringLiteral("zzTitleBarCloseButton"));

    for (QToolButton *button : {
             minimizeButton,
             maximizeButton,
             closeButton}) {
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setFixedSize(46, 32);
    }
    layout->addWidget(iconLabel);
    layout->addWidget(titleLabel, 1);
    layout->addWidget(minimizeButton);
    layout->addWidget(maximizeButton);
    layout->addWidget(closeButton);

    QObject::connect(
        minimizeButton,
        &QToolButton::clicked,
        q_ptr,
        &ZzFluentTitleBar::minimizeRequested);
    QObject::connect(
        maximizeButton,
        &QToolButton::clicked,
        q_ptr,
        &ZzFluentTitleBar::maximizeRestoreRequested);
    QObject::connect(
        closeButton,
        &QToolButton::clicked,
        q_ptr,
        &ZzFluentTitleBar::closeRequested);
    refreshPresentation();
}

void ZzFluentTitleBarPrivate::refreshPresentation()
{
    titleLabel->setText(title);
    titleLabel->setAccessibleName(title);
    iconLabel->setAccessibleName(title);
    iconLabel->setPixmap(windowIcon.pixmap(
        QSize(16, 16),
        q_ptr->devicePixelRatioF()));

    const QString minimizeText = ZzFluentTitleBar::tr("最小化");
    const QString maximizeText = maximized
        ? ZzFluentTitleBar::tr("还原")
        : ZzFluentTitleBar::tr("最大化");
    const QString closeText = ZzFluentTitleBar::tr("关闭");
    minimizeButton->setToolTip(minimizeText);
    minimizeButton->setAccessibleName(minimizeText);
    maximizeButton->setToolTip(maximizeText);
    maximizeButton->setAccessibleName(maximizeText);
    closeButton->setToolTip(closeText);
    closeButton->setAccessibleName(closeText);

    minimizeButton->setIcon(q_ptr->style()->standardIcon(
        QStyle::SP_TitleBarMinButton,
        nullptr,
        q_ptr));
    maximizeButton->setIcon(q_ptr->style()->standardIcon(
        maximized
            ? QStyle::SP_TitleBarNormalButton
            : QStyle::SP_TitleBarMaxButton,
        nullptr,
        q_ptr));
    closeButton->setIcon(q_ptr->style()->standardIcon(
        QStyle::SP_TitleBarCloseButton,
        nullptr,
        q_ptr));
    minimizeButton->setVisible(systemButtonsVisible);
    maximizeButton->setVisible(systemButtonsVisible);
    closeButton->setVisible(systemButtonsVisible);
}

} // namespace ZzFluentUI
