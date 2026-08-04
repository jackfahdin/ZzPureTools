#include "ZzFluentTitleBarPrivate.h"

#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentTitleBar.h>

namespace ZzFluentUI {

namespace {

/** @brief 标识需要按主题文本色绘制的标题栏系统图标。 */
enum class ZzTitleBarGlyph
{
    Minimize,
    Maximize,
    Restore,
    Close
};

/** @brief 为当前调色板与 DPR 生成清晰、可访问的系统按钮图标。 */
QIcon zzTitleBarIcon(const QWidget *widget, ZzTitleBarGlyph glyph)
{
    constexpr int logicalExtent = 16;
    const qreal dpr = qMax(qreal(1.0), widget->devicePixelRatioF());
    QPixmap pixmap(
        qMax(1, qRound(logicalExtent * dpr)),
        qMax(1, qRound(logicalExtent * dpr)));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(
        widget->palette().color(QPalette::ButtonText),
        1.4,
        Qt::SolidLine,
        Qt::SquareCap,
        Qt::MiterJoin));
    switch (glyph) {
    case ZzTitleBarGlyph::Minimize:
        painter.drawLine(QPointF(3.0, 11.5), QPointF(13.0, 11.5));
        break;
    case ZzTitleBarGlyph::Maximize:
        painter.drawRect(QRectF(3.5, 3.5, 9.0, 9.0));
        break;
    case ZzTitleBarGlyph::Restore:
        painter.drawRect(QRectF(3.5, 5.5, 7.0, 7.0));
        painter.drawLine(QPointF(5.5, 3.5), QPointF(12.5, 3.5));
        painter.drawLine(QPointF(12.5, 3.5), QPointF(12.5, 10.5));
        painter.drawLine(QPointF(10.5, 5.5), QPointF(10.5, 3.5));
        break;
    case ZzTitleBarGlyph::Close:
        painter.drawLine(QPointF(4.0, 4.0), QPointF(12.0, 12.0));
        painter.drawLine(QPointF(12.0, 4.0), QPointF(4.0, 12.0));
        break;
    }
    painter.end();
    return QIcon(pixmap);
}

} // namespace

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

    minimizeButton->setIcon(zzTitleBarIcon(
        minimizeButton,
        ZzTitleBarGlyph::Minimize));
    maximizeButton->setIcon(zzTitleBarIcon(
        maximizeButton,
        maximized
            ? ZzTitleBarGlyph::Restore
            : ZzTitleBarGlyph::Maximize));
    closeButton->setIcon(zzTitleBarIcon(
        closeButton,
        ZzTitleBarGlyph::Close));
    minimizeButton->setVisible(systemButtonsVisible);
    maximizeButton->setVisible(systemButtonsVisible);
    closeButton->setVisible(systemButtonsVisible);
}

} // namespace ZzFluentUI
