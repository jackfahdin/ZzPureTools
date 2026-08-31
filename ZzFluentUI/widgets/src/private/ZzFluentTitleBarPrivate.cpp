#include "ZzFluentTitleBarPrivate.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QtCore/QSignalBlocker>
#include <QtGui/QAction>
#include <QtGui/QActionEvent>
#include <QtGui/QActionGroup>
#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QPixmap>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzTitleBarMenuDisplayMode.h>

namespace ZzFluentUI {

namespace {

constexpr int zzTitleBarMargin = 8;
constexpr int zzTitleBarSpacing = 4;
constexpr int zzTitleBarIconExtent = 20;
constexpr int zzTitleBarCommandExtent = 36;
constexpr int zzTitleBarSystemButtonWidth = 46;
constexpr int zzTitleBarCompactMenuWidth = 36;
constexpr int zzTitleBarMinimumTitleWidth = 160;
constexpr int zzTitleBarAdaptiveHysteresis = 24;

/** @brief 标识需要按主题文本色绘制的标题栏图标。 */
enum class ZzTitleBarGlyph
{
    Menu,
    Theme,
    AlwaysOnTop,
    Minimize,
    Maximize,
    Restore,
    Close
};

/** @brief 为当前调色板与 DPR 生成清晰、可访问的标题栏图标。 */
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
    case ZzTitleBarGlyph::Menu:
        for (const qreal y : {4.0, 8.0, 12.0}) {
            painter.drawLine(QPointF(3.0, y), QPointF(13.0, y));
        }
        break;
    case ZzTitleBarGlyph::Theme:
        painter.drawEllipse(QPointF(8.0, 8.0), 4.0, 4.0);
        for (const auto &line : std::array<QLineF, 4>{
                 QLineF(8.0, 1.5, 8.0, 3.0),
                 QLineF(8.0, 13.0, 8.0, 14.5),
                 QLineF(1.5, 8.0, 3.0, 8.0),
                 QLineF(13.0, 8.0, 14.5, 8.0)}) {
            painter.drawLine(line);
        }
        break;
    case ZzTitleBarGlyph::AlwaysOnTop:
        painter.drawLine(QPointF(4.0, 5.0), QPointF(12.0, 5.0));
        painter.drawLine(QPointF(6.0, 5.0), QPointF(6.0, 9.0));
        painter.drawLine(QPointF(10.0, 5.0), QPointF(10.0, 9.0));
        painter.drawLine(QPointF(4.5, 9.0), QPointF(11.5, 9.0));
        painter.drawLine(QPointF(8.0, 9.0), QPointF(8.0, 14.0));
        break;
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

/** @brief 返回当前主题模式的可本地化显示名称。 */
QString zzThemeModeText(ZzThemeMode mode)
{
    switch (mode) {
    case ZzThemeMode::System:
        return ZzFluentTitleBar::tr("跟随系统");
    case ZzThemeMode::Light:
        return ZzFluentTitleBar::tr("浅色");
    case ZzThemeMode::Dark:
        return ZzFluentTitleBar::tr("深色");
    case ZzThemeMode::HighContrast:
        return ZzFluentTitleBar::tr("高对比度");
    }
    return {};
}

} // namespace

ZzFluentTitleBarPrivate::ZzFluentTitleBarPrivate(ZzFluentTitleBar *q)
    : q_ptr(q)
    , iconLabel(new QLabel(q))
    , titleLabel(new QLabel(q))
    , menuBar(new QMenuBar(q))
    , compactMenuButton(new QToolButton(q))
    , compactMenu(new QMenu(compactMenuButton))
    , themeButton(new QToolButton(q))
    , themeMenu(new QMenu(themeButton))
    , themeActionGroup(new QActionGroup(themeMenu))
    , alwaysOnTopButton(new QToolButton(q))
    , minimizeButton(new QToolButton(q))
    , maximizeButton(new QToolButton(q))
    , closeButton(new QToolButton(q))
    , menuDisplayMode(ZzTitleBarMenuDisplayMode::Adaptive)
    , themeMode(ZzThemeMode::System)
{
    Q_ASSERT(q_ptr != nullptr);
    q_ptr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    iconLabel->setObjectName(QStringLiteral("zzTitleBarWindowIcon"));
    iconLabel->setFixedSize(zzTitleBarIconExtent, zzTitleBarIconExtent);
    iconLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setObjectName(QStringLiteral("zzTitleBarTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);

    menuBar->setObjectName(QStringLiteral("zzTitleBarMenuBar"));
    menuBar->setNativeMenuBar(false);
    compactMenuButton->setObjectName(
        QStringLiteral("zzTitleBarCompactMenuButton"));
    compactMenu->setObjectName(QStringLiteral("zzTitleBarCompactMenu"));
    compactMenuButton->setMenu(compactMenu);
    compactMenuButton->setPopupMode(QToolButton::InstantPopup);

    themeButton->setObjectName(QStringLiteral("zzTitleBarThemeButton"));
    themeButton->setCheckable(true);
    themeMenu->setObjectName(QStringLiteral("zzTitleBarThemeMenu"));
    themeButton->setMenu(themeMenu);
    themeButton->setPopupMode(QToolButton::InstantPopup);
    themeActionGroup->setExclusive(true);
    const auto addThemeAction = [this](
                                    const QString &text,
                                    ZzThemeMode mode) -> QAction * {
        auto *action = themeMenu->addAction(text);
        action->setCheckable(true);
        action->setData(static_cast<int>(mode));
        themeActionGroup->addAction(action);
        return action;
    };
    systemThemeAction = addThemeAction(
        ZzFluentTitleBar::tr("跟随系统"), ZzThemeMode::System);
    lightThemeAction = addThemeAction(
        ZzFluentTitleBar::tr("浅色"), ZzThemeMode::Light);
    darkThemeAction = addThemeAction(
        ZzFluentTitleBar::tr("深色"), ZzThemeMode::Dark);

    alwaysOnTopButton->setObjectName(
        QStringLiteral("zzTitleBarAlwaysOnTopButton"));
    alwaysOnTopButton->setCheckable(true);
    minimizeButton->setObjectName(QStringLiteral("zzTitleBarMinimizeButton"));
    maximizeButton->setObjectName(QStringLiteral("zzTitleBarMaximizeButton"));
    closeButton->setObjectName(QStringLiteral("zzTitleBarCloseButton"));

    for (QToolButton *button : {
             compactMenuButton,
             themeButton,
             alwaysOnTopButton,
             minimizeButton,
             maximizeButton,
             closeButton}) {
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
    }

    QObject::connect(
        themeActionGroup,
        &QActionGroup::triggered,
        q_ptr,
        [this](QAction *action) {
            if (action == nullptr) {
                return;
            }
            const auto requested = static_cast<ZzThemeMode>(
                action->data().toInt());
            Q_EMIT q_ptr->themeModeRequested(requested);
            refreshThemeActions();
        });
    QObject::connect(
        themeButton,
        &QToolButton::clicked,
        q_ptr,
        [this] {
            const QSignalBlocker blocker(themeButton);
            themeButton->setChecked(themeMode != ZzThemeMode::System);
        });
    QObject::connect(
        alwaysOnTopButton,
        &QToolButton::clicked,
        q_ptr,
        [this](bool requested) {
            Q_EMIT q_ptr->alwaysOnTopRequested(requested);
            const QSignalBlocker blocker(alwaysOnTopButton);
            alwaysOnTopButton->setChecked(alwaysOnTop);
        });
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
    iconLabel->setAccessibleName(title);
    iconLabel->setPixmap(windowIcon.pixmap(
        QSize(16, 16),
        q_ptr->devicePixelRatioF()));

    const QString menuText = ZzFluentTitleBar::tr("应用菜单");
    const QString themeText = ZzFluentTitleBar::tr("主题：%1").arg(
        zzThemeModeText(themeMode));
    const QString alwaysOnTopText = ZzFluentTitleBar::tr("置顶");
    const QString minimizeText = ZzFluentTitleBar::tr("最小化");
    const QString maximizeText = maximized
        ? ZzFluentTitleBar::tr("还原")
        : ZzFluentTitleBar::tr("最大化");
    const QString closeText = ZzFluentTitleBar::tr("关闭");

    systemThemeAction->setText(ZzFluentTitleBar::tr("跟随系统"));
    lightThemeAction->setText(ZzFluentTitleBar::tr("浅色"));
    darkThemeAction->setText(ZzFluentTitleBar::tr("深色"));

    compactMenuButton->setToolTip(menuText);
    compactMenuButton->setAccessibleName(menuText);
    themeButton->setToolTip(themeText);
    themeButton->setAccessibleName(themeText);
    alwaysOnTopButton->setToolTip(alwaysOnTopText);
    alwaysOnTopButton->setAccessibleName(alwaysOnTopText);
    minimizeButton->setToolTip(minimizeText);
    minimizeButton->setAccessibleName(minimizeText);
    maximizeButton->setToolTip(maximizeText);
    maximizeButton->setAccessibleName(maximizeText);
    closeButton->setToolTip(closeText);
    closeButton->setAccessibleName(closeText);

    compactMenuButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzTitleBarGlyph::Menu));
    themeButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzTitleBarGlyph::Theme));
    alwaysOnTopButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzTitleBarGlyph::AlwaysOnTop));
    minimizeButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzTitleBarGlyph::Minimize));
    maximizeButton->setIcon(zzTitleBarIcon(
        q_ptr,
        maximized
            ? ZzTitleBarGlyph::Restore
            : ZzTitleBarGlyph::Maximize));
    closeButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzTitleBarGlyph::Close));

    minimizeButton->setVisible(systemButtonsVisible);
    maximizeButton->setVisible(systemButtonsVisible);
    closeButton->setVisible(systemButtonsVisible);
    themeButton->setChecked(themeMode != ZzThemeMode::System);
    alwaysOnTopButton->setChecked(alwaysOnTop);
    refreshThemeActions();
    updateLayout();
}

void ZzFluentTitleBarPrivate::refreshTitle()
{
    titleLabel->setAccessibleName(title);
    if (titleLabel->width() <= 0 || title.isEmpty()) {
        titleLabel->clear();
        titleLabel->setVisible(false);
        return;
    }
    // 标题几何已经由安全区计算预留了内边距，省略宽度必须使用同一安全区。
    // 额外扣除像素会让恰好放得下的标题错误地显示省略号。
    const int textWidth = qMax(0, titleLabel->contentsRect().width());
    titleLabel->setText(q_ptr->fontMetrics().elidedText(
        title, Qt::ElideRight, textWidth));
    titleLabel->setVisible(true);
}

void ZzFluentTitleBarPrivate::updateLayout()
{
    const int availableWidth = q_ptr->width();
    const int availableHeight = q_ptr->height();
    const int systemWidth = systemButtonsVisible
        ? (3 * zzTitleBarSystemButtonWidth + 2 * zzTitleBarSpacing)
        : 0;
    const int rightGroupWidth =
        (2 * zzTitleBarCommandExtent)
        + zzTitleBarSpacing
        + (systemWidth > 0 ? zzTitleBarSpacing + systemWidth : 0);
    const int desiredMenuWidth = qMax(1, menuBar->sizeHint().width());
    const int expandedLeftGroupWidth =
        zzTitleBarIconExtent + zzTitleBarSpacing + desiredMenuWidth;
    const int adaptiveThreshold =
        2 * qMax(expandedLeftGroupWidth, rightGroupWidth)
        + zzTitleBarMinimumTitleWidth
        + (2 * zzTitleBarSpacing)
        + (2 * zzTitleBarMargin);
    const int hysteresisHalf = zzTitleBarAdaptiveHysteresis / 2;

    bool expanded = menuDisplayMode == ZzTitleBarMenuDisplayMode::Expanded;
    if (menuDisplayMode == ZzTitleBarMenuDisplayMode::Adaptive) {
        if (adaptiveExpanded) {
            adaptiveExpanded = availableWidth
                >= adaptiveThreshold - hysteresisHalf;
        } else {
            adaptiveExpanded = availableWidth
                >= adaptiveThreshold + hysteresisHalf;
        }
        expanded = adaptiveExpanded;
    }
    menuBar->setVisible(expanded);
    compactMenuButton->setVisible(!expanded);

    int leftCursor = zzTitleBarMargin;
    iconLabel->setGeometry(
        leftCursor,
        (availableHeight - zzTitleBarIconExtent) / 2,
        zzTitleBarIconExtent,
        zzTitleBarIconExtent);
    leftCursor += zzTitleBarIconExtent + zzTitleBarSpacing;
    const int maximumMenuWidth = qMax(
        1,
        availableWidth
            - (2 * zzTitleBarMargin)
            - zzTitleBarIconExtent
            - zzTitleBarSpacing
            - rightGroupWidth
            - zzTitleBarSpacing);
    const int activeMenuWidth = expanded
        ? qMin(desiredMenuWidth, maximumMenuWidth)
        : zzTitleBarCompactMenuWidth;
    QWidget *const activeMenu = expanded
        ? static_cast<QWidget *>(menuBar)
        : static_cast<QWidget *>(compactMenuButton);
    activeMenu->setGeometry(
        leftCursor, 0, activeMenuWidth, availableHeight);
    leftCursor += activeMenuWidth;
    const int leftGroupEnd = leftCursor;

    int rightCursor = availableWidth - zzTitleBarMargin;
    const auto placeRight = [availableHeight, &rightCursor](
                                QWidget *widget,
                                int width) {
        rightCursor -= width;
        widget->setGeometry(rightCursor, 0, width, availableHeight);
        rightCursor -= zzTitleBarSpacing;
    };
    if (systemButtonsVisible) {
        placeRight(closeButton, zzTitleBarSystemButtonWidth);
        placeRight(maximizeButton, zzTitleBarSystemButtonWidth);
        placeRight(minimizeButton, zzTitleBarSystemButtonWidth);
    }
    placeRight(alwaysOnTopButton, zzTitleBarCommandExtent);
    placeRight(themeButton, zzTitleBarCommandExtent);
    const int rightGroupStart = rightCursor + zzTitleBarSpacing;

    const std::array<QWidget *, 8> chromeWidgets{
        iconLabel,
        menuBar,
        compactMenuButton,
        themeButton,
        alwaysOnTopButton,
        minimizeButton,
        maximizeButton,
        closeButton};
    if (q_ptr->layoutDirection() == Qt::RightToLeft) {
        for (QWidget *widget : chromeWidgets) {
            QRect geometry = widget->geometry();
            geometry.moveLeft(availableWidth - geometry.right() - 1);
            widget->setGeometry(geometry);
        }
    }

    int leftSafeBoundary = leftGroupEnd;
    int rightSafeBoundary = rightGroupStart;
    if (q_ptr->layoutDirection() == Qt::RightToLeft) {
        leftSafeBoundary = availableWidth - rightGroupStart;
        rightSafeBoundary = availableWidth - leftGroupEnd;
    }
    const int center = q_ptr->rect().center().x();
    const int safeHalfWidth = qMax(
        0,
        qMin(
            center - leftSafeBoundary - zzTitleBarSpacing,
            rightSafeBoundary - zzTitleBarSpacing - center));
    int titleWidth = qMin(
        q_ptr->fontMetrics().horizontalAdvance(title) + 4,
        (2 * safeHalfWidth) + 1);
    if (titleWidth > 0 && titleWidth % 2 == 0) {
        --titleWidth;
    }
    if (titleWidth <= 0 || title.isEmpty()) {
        titleLabel->setGeometry(0, 0, 0, availableHeight);
    } else {
        titleLabel->setGeometry(
            center - titleWidth / 2,
            0,
            titleWidth,
            availableHeight);
    }
    refreshTitle();
}

void ZzFluentTitleBarPrivate::rebuildCompactMenu()
{
    compactMenu->clear();
    compactMenu->addActions(menuBar->actions());
}

void ZzFluentTitleBarPrivate::handleMenuActionEvent(QActionEvent *event)
{
    if (event == nullptr || event->action() == nullptr) {
        return;
    }
    if (event->type() == QEvent::ActionAdded) {
        // 源列表已包含新动作，投影列表仍保持插入前顺序。
        const QList<QAction *> sourceActions = menuBar->actions();
        const qsizetype sourceIndex = sourceActions.indexOf(event->action());
        const QList<QAction *> projectedActions = compactMenu->actions();
        if (sourceIndex < 0
            || projectedActions.size() + 1 != sourceActions.size()) {
            rebuildCompactMenu();
        } else {
            QAction *const insertionAnchor =
                sourceIndex < projectedActions.size()
                ? projectedActions.at(sourceIndex)
                : nullptr;
            compactMenu->insertAction(insertionAnchor, event->action());
        }
    } else if (event->type() == QEvent::ActionRemoved) {
        compactMenu->removeAction(event->action());
    }
    QMetaObject::invokeMethod(
        q_ptr,
        [this] { updateLayout(); },
        Qt::QueuedConnection);
}

void ZzFluentTitleBarPrivate::refreshThemeActions()
{
    const QSignalBlocker blocker(themeActionGroup);
    themeActionGroup->setExclusive(false);
    systemThemeAction->setChecked(themeMode == ZzThemeMode::System);
    lightThemeAction->setChecked(themeMode == ZzThemeMode::Light);
    darkThemeAction->setChecked(themeMode == ZzThemeMode::Dark);
    themeActionGroup->setExclusive(true);
}

} // namespace ZzFluentUI
