#include "ZzFluentTitleBarPrivate.h"

#include <algorithm>
#include <array>
#include <utility>

#include <QtCore/QSignalBlocker>
#include <QtGui/QAction>
#include <QtGui/QActionEvent>
#include <QtGui/QActionGroup>
#include <QtGui/QFontMetrics>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QFrame>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QToolButton>

#include <ZzFluentUI/ZzColorToken.h>
#include <ZzFluentUI/ZzFluentTitleBar.h>
#include <ZzFluentUI/ZzFluentStyle.h>
#include <ZzFluentUI/ZzIconAssets.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzBundledSvgIcon.h>
#include <ZzFluentUI/ZzThemeMode.h>
#include <ZzFluentUI/ZzTitleBarThemeInteractionMode.h>
#include <ZzFluentUI/ZzTitleBarMenuDisplayMode.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

namespace {

constexpr int zzTitleBarMargin = 8;
constexpr int zzTitleBarSpacing = 4;
constexpr int zzTitleBarIconExtent = 20;
constexpr int zzTitleBarCommandExtent = 36;
constexpr int zzTitleBarSystemButtonWidth = 46;
constexpr int zzTitleBarCompactMenuWidth = 36;
constexpr int zzTitleBarAdaptiveTitleWidthCap = 160;
constexpr int zzTitleBarAdaptiveHysteresis = 24;
/** @brief 保留状态语义但隐藏标题栏工具按钮的 checked 面板。 */
constexpr char zzSuppressCheckedSurfaceProperty[] =
    "zzFluentSuppressCheckedSurface";

/** @brief 将标题栏分隔线混合到表面色，避免与主体形成生硬高对比。 */
QColor zzSubtleSeparatorColor(const ZzFluentStyle *style)
{
    if (style == nullptr || style->themeSnapshot() == nullptr) {
        return {};
    }
    const auto snapshot = style->themeSnapshot();
    const QColor surface = snapshot->color(ZzColorToken::Surface);
    const QColor stroke = snapshot->color(ZzColorToken::ControlStroke);
    constexpr qreal strokeWeight = 0.35;
    QColor result;
    result.setRgbF(
        static_cast<float>(surface.redF() * (1.0 - strokeWeight)
            + stroke.redF() * strokeWeight),
        static_cast<float>(surface.greenF() * (1.0 - strokeWeight)
            + stroke.greenF() * strokeWeight),
        static_cast<float>(surface.blueF() * (1.0 - strokeWeight)
            + stroke.blueF() * strokeWeight),
        1.0);
    return result;
}

/** @brief 使用样式缓存渲染标题栏内嵌 SVG，必要时执行轻量回退着色。 */
QIcon zzTitleBarIcon(const QWidget *widget, ZzBundledSvgIcon icon)
{
    if (widget == nullptr) {
        return {};
    }
    const auto descriptor = ZzIconDescriptor::fromBundledSvg(icon);
    const QSize logicalSize(16, 16);
    const qreal devicePixelRatio = qMax(
        qreal(1.0), widget->devicePixelRatioF());
    const QColor color = widget->palette().color(QPalette::ButtonText);
    if (auto *style = qobject_cast<ZzFluentStyle *>(widget->style());
        style != nullptr) {
        const QPixmap pixmap = style->iconPixmap(
            descriptor,
            logicalSize,
            devicePixelRatio,
            color,
            widget->layoutDirection());
        if (!pixmap.isNull()) {
            return QIcon(pixmap);
        }
    }

    if (!ZzIconAssets::ensureInitialized()) {
        return {};
    }
    QPixmap pixmap = QIcon(descriptor.resourceId).pixmap(
        logicalSize, devicePixelRatio);
    if (pixmap.isNull()) {
        return {};
    }
    QImage image = pixmap.toImage().convertToFormat(
        QImage::Format_ARGB32_Premultiplied);
    QPainter tintPainter(&image);
    tintPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tintPainter.fillRect(image.rect(), color);
    tintPainter.end();
    pixmap = QPixmap::fromImage(std::move(image));
    pixmap.setDevicePixelRatio(devicePixelRatio);
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
    , separator(new QFrame(q))
    , menuDisplayMode(ZzTitleBarMenuDisplayMode::Adaptive)
    , themeMode(ZzThemeMode::System)
    , themeInteractionMode(ZzTitleBarThemeInteractionMode::Menu)
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
    themeButton->setProperty(zzSuppressCheckedSurfaceProperty, true);
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
    highContrastThemeAction = addThemeAction(
        ZzFluentTitleBar::tr("高对比度"), ZzThemeMode::HighContrast);

    alwaysOnTopButton->setObjectName(
        QStringLiteral("zzTitleBarAlwaysOnTopButton"));
    alwaysOnTopButton->setCheckable(true);
    alwaysOnTopButton->setProperty(zzSuppressCheckedSurfaceProperty, true);
    minimizeButton->setObjectName(QStringLiteral("zzTitleBarMinimizeButton"));
    maximizeButton->setObjectName(QStringLiteral("zzTitleBarMaximizeButton"));
    closeButton->setObjectName(QStringLiteral("zzTitleBarCloseButton"));
    separator->setObjectName(QStringLiteral("zzTitleBarSeparator"));
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Plain);
    separator->setLineWidth(1);
    separator->setMidLineWidth(0);

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
            if (themeInteractionMode
                == ZzTitleBarThemeInteractionMode::Toggle) {
                Q_EMIT q_ptr->themeToggleRequested();
            }
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

ZzFluentTitleBarPrivate::~ZzFluentTitleBarPrivate()
{
    restoreWindowMinimumWidth();
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
    highContrastThemeAction->setText(ZzFluentTitleBar::tr("高对比度"));

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

    themeButton->setPopupMode(
        themeInteractionMode == ZzTitleBarThemeInteractionMode::Menu
            ? QToolButton::InstantPopup
            : QToolButton::DelayedPopup);

    compactMenuButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::MoreLine));
    const ZzBundledSvgIcon themeIcon = themeMode == ZzThemeMode::Light
        ? ZzBundledSvgIcon::Moon
        : themeMode == ZzThemeMode::Dark
        ? ZzBundledSvgIcon::Sun
        : ZzBundledSvgIcon::ComputerSystem;
    themeButton->setIcon(zzTitleBarIcon(q_ptr, themeIcon));
    alwaysOnTopButton->setIcon(zzTitleBarIcon(
        q_ptr,
        alwaysOnTop
            ? ZzBundledSvgIcon::PinFill
            : ZzBundledSvgIcon::Pin));
    minimizeButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::Minimize));
    maximizeButton->setIcon(zzTitleBarIcon(
        q_ptr,
        maximized
            ? ZzBundledSvgIcon::Restore
            : ZzBundledSvgIcon::Maximize));
    closeButton->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::Close));
    systemThemeAction->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::ComputerSystem));
    lightThemeAction->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::Moon));
    darkThemeAction->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::Sun));
    highContrastThemeAction->setIcon(zzTitleBarIcon(
        q_ptr, ZzBundledSvgIcon::ComputerSystem));

    if (auto *const fluentStyle = qobject_cast<ZzFluentStyle *>(
            q_ptr->style()); fluentStyle != nullptr) {
        const QColor separatorColor = zzSubtleSeparatorColor(fluentStyle);
        if (separatorColor.isValid()) {
            QPalette separatorPalette = separator->palette();
            for (const QPalette::ColorRole role : {
                     QPalette::WindowText,
                     QPalette::Text,
                     QPalette::ButtonText,
                     QPalette::Light,
                     QPalette::Midlight,
                     QPalette::Mid,
                     QPalette::Dark,
                     QPalette::Shadow}) {
                separatorPalette.setColor(role, separatorColor);
            }
            separator->setPalette(separatorPalette);
        }
    }

    minimizeButton->setVisible(minimizeButtonVisible);
    maximizeButton->setVisible(maximizeButtonVisible);
    closeButton->setVisible(closeButtonVisible);
    themeButton->setVisible(commandButtonsVisible);
    alwaysOnTopButton->setVisible(commandButtonsVisible);
    separator->setVisible(true);
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

int ZzFluentTitleBarPrivate::minimumExpandedWidth() const noexcept
{
    const int visibleSystemButtons = static_cast<int>(minimizeButtonVisible)
        + static_cast<int>(maximizeButtonVisible)
        + static_cast<int>(closeButtonVisible);
    const int systemWidth = visibleSystemButtons > 0
        ? visibleSystemButtons * zzTitleBarSystemButtonWidth
            + (visibleSystemButtons - 1) * zzTitleBarSpacing
        : 0;
    const int commandWidth = commandButtonsVisible
        ? (2 * zzTitleBarCommandExtent + zzTitleBarSpacing)
        : 0;
    const int rightGroupWidth = commandWidth
        + ((commandWidth > 0 && systemWidth > 0) ? zzTitleBarSpacing : 0)
        + systemWidth;
    const int desiredMenuWidth = qMax(1, menuBar->sizeHint().width());
    const int expandedLeftGroupWidth =
        zzTitleBarIconExtent + zzTitleBarSpacing + desiredMenuWidth;
    const int requestedTitleWidth =
        q_ptr->fontMetrics().horizontalAdvance(title) + 4;
    const int adaptiveTitleWidth = qMin(
        zzTitleBarAdaptiveTitleWidthCap,
        qMax(0, requestedTitleWidth));
    return 2 * qMax(expandedLeftGroupWidth, rightGroupWidth)
        + adaptiveTitleWidth
        + (2 * zzTitleBarSpacing)
        + zzTitleBarMargin;
}

void ZzFluentTitleBarPrivate::restoreWindowMinimumWidth() noexcept
{
    QWidget *const host = minimumWidthHost.data();
    if (host != nullptr
        && host->minimumWidth() == enforcedHostMinimumWidth) {
        host->setMinimumWidth(originalHostMinimumWidth);
    }
    minimumWidthHost.clear();
    originalHostMinimumWidth = 0;
    enforcedHostMinimumWidth = 0;
}

void ZzFluentTitleBarPrivate::syncWindowMinimumWidth(int requiredWidth)
{
    if (menuCollapseEnabled) {
        restoreWindowMinimumWidth();
        return;
    }

    QWidget *const host = q_ptr->window();
    if (host == nullptr) {
        return;
    }
    if (minimumWidthHost != host) {
        restoreWindowMinimumWidth();
        minimumWidthHost = host;
        originalHostMinimumWidth = host->minimumWidth();
        // 初次绑定时把宿主现值标记为已观察值，避免被误认为外部变更。
        enforcedHostMinimumWidth = originalHostMinimumWidth;
    } else if (host->minimumWidth() != enforcedHostMinimumWidth) {
        // 组件上次同步后宿主值发生变化，视为外部约束并在恢复时保留。
        originalHostMinimumWidth = host->minimumWidth();
    }

    const int targetWidth = qMax(originalHostMinimumWidth, requiredWidth);
    // 先记录组件将要写入的值，QWidget 可能同步触发下一次布局回调。
    enforcedHostMinimumWidth = targetWidth;
    if (host->minimumWidth() < targetWidth) {
        host->setMinimumWidth(targetWidth);
    }
}

void ZzFluentTitleBarPrivate::updateLayout()
{
    const int availableWidth = q_ptr->width();
    const int availableHeight = q_ptr->height();
    const int visibleSystemButtons = static_cast<int>(minimizeButtonVisible)
        + static_cast<int>(maximizeButtonVisible)
        + static_cast<int>(closeButtonVisible);
    const int systemWidth = visibleSystemButtons > 0
        ? visibleSystemButtons * zzTitleBarSystemButtonWidth
            + (visibleSystemButtons - 1) * zzTitleBarSpacing
        : 0;
    const int commandWidth = commandButtonsVisible
        ? (2 * zzTitleBarCommandExtent + zzTitleBarSpacing)
        : 0;
    const int rightGroupWidth = commandWidth
        + ((commandWidth > 0 && systemWidth > 0) ? zzTitleBarSpacing : 0)
        + systemWidth;
    const QSize menuSizeHint = menuBar->sizeHint();
    const int desiredMenuWidth = qMax(1, menuSizeHint.width());
    // 标题以窗口中心为锚点；短标题只按实际宽度预留，避免菜单过早折叠。
    const int adaptiveThreshold = minimumExpandedWidth();
    const int hysteresisHalf = zzTitleBarAdaptiveHysteresis / 2;

    bool expanded = !menuCollapseEnabled
        || menuDisplayMode == ZzTitleBarMenuDisplayMode::Expanded;
    if (menuCollapseEnabled
        && menuDisplayMode == ZzTitleBarMenuDisplayMode::Adaptive) {
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
            - zzTitleBarMargin
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
    // 菜单 action 的完整高度可能超过标题栏固定高度，保留 sizeHint 高度
    // 让 Qt 基于完整菜单区域进行溢出判定，外围像素由标题栏自然裁剪。
    const int activeMenuHeight = expanded
        ? qMax(availableHeight, menuSizeHint.height())
        : availableHeight;
    const int activeMenuTop = expanded
        ? (availableHeight - activeMenuHeight) / 2
        : 0;
    activeMenu->setGeometry(
        leftCursor,
        activeMenuTop,
        activeMenuWidth,
        activeMenuHeight);
    leftCursor += activeMenuWidth;
    const int leftGroupEnd = leftCursor;

    int rightCursor = availableWidth;
    const auto placeRight = [availableHeight, &rightCursor](
                                QWidget *widget,
                                int width) {
        rightCursor -= width;
        widget->setGeometry(rightCursor, 0, width, availableHeight);
        rightCursor -= zzTitleBarSpacing;
    };
    if (closeButtonVisible) {
        placeRight(closeButton, zzTitleBarSystemButtonWidth);
    }
    if (maximizeButtonVisible) {
        placeRight(maximizeButton, zzTitleBarSystemButtonWidth);
    }
    if (minimizeButtonVisible) {
        placeRight(minimizeButton, zzTitleBarSystemButtonWidth);
    }
    if (commandButtonsVisible) {
        placeRight(alwaysOnTopButton, zzTitleBarCommandExtent);
        placeRight(themeButton, zzTitleBarCommandExtent);
    }
    const bool hasRightGroup = commandButtonsVisible || visibleSystemButtons > 0;
    const int rightGroupStart = hasRightGroup
        ? rightCursor + zzTitleBarSpacing
        : availableWidth;

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
    syncWindowMinimumWidth(adaptiveThreshold);
    separator->setGeometry(0, qMax(0, availableHeight - 1), availableWidth, 1);
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
    highContrastThemeAction->setChecked(
        themeMode == ZzThemeMode::HighContrast);
    themeActionGroup->setExclusive(true);
}

} // namespace ZzFluentUI
