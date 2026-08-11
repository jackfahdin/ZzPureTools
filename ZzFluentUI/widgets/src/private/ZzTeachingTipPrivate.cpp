#include "ZzTeachingTipPrivate.h"

#include <algorithm>
#include <array>
#include <limits>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>
#include <QtGui/QMouseEvent>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSizePolicy>
#include <QtWidgets/QVBoxLayout>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzButtonAppearance.h>
#include <ZzFluentUI/ZzIconDescriptor.h>
#include <ZzFluentUI/ZzIconButton.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzPushButton.h>
#include <ZzFluentUI/ZzTeachingTip.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include <ZzFluentUI/ZzTypographyToken.h>

namespace ZzFluentUI {

namespace {

/** @brief 返回显式方向对应的反向候选。 */
ZzTeachingTipPlacement zzOppositePlacement(
    ZzTeachingTipPlacement placement) noexcept
{
    switch (placement) {
    case ZzTeachingTipPlacement::Top:
        return ZzTeachingTipPlacement::Bottom;
    case ZzTeachingTipPlacement::Bottom:
        return ZzTeachingTipPlacement::Top;
    case ZzTeachingTipPlacement::Left:
        return ZzTeachingTipPlacement::Right;
    case ZzTeachingTipPlacement::Right:
        return ZzTeachingTipPlacement::Left;
    case ZzTeachingTipPlacement::Auto:
        return ZzTeachingTipPlacement::Bottom;
    }
    return ZzTeachingTipPlacement::Bottom;
}

/** @brief 返回首选、反向和另两向组成的确定性候选顺序。 */
std::array<ZzTeachingTipPlacement, 4> zzPlacementOrder(
    ZzTeachingTipPlacement preferred) noexcept
{
    if (preferred == ZzTeachingTipPlacement::Auto) {
        return {
            ZzTeachingTipPlacement::Bottom,
            ZzTeachingTipPlacement::Top,
            ZzTeachingTipPlacement::Right,
            ZzTeachingTipPlacement::Left};
    }
    const ZzTeachingTipPlacement opposite = zzOppositePlacement(preferred);
    if (preferred == ZzTeachingTipPlacement::Top
        || preferred == ZzTeachingTipPlacement::Bottom) {
        return {
            preferred,
            opposite,
            ZzTeachingTipPlacement::Right,
            ZzTeachingTipPlacement::Left};
    }
    return {
        preferred,
        opposite,
        ZzTeachingTipPlacement::Bottom,
        ZzTeachingTipPlacement::Top};
}

/** @brief 构造指定方向未经边界钳制的候选矩形。 */
QRect zzPlacementRect(
    ZzTeachingTipPlacement placement,
    const QRect &target,
    const QSize &tipSize,
    int gap) noexcept
{
    QPoint topLeft;
    switch (placement) {
    case ZzTeachingTipPlacement::Top:
        topLeft = QPoint(
            target.center().x() - tipSize.width() / 2,
            target.top() - gap - tipSize.height());
        break;
    case ZzTeachingTipPlacement::Bottom:
    case ZzTeachingTipPlacement::Auto:
        topLeft = QPoint(
            target.center().x() - tipSize.width() / 2,
            target.bottom() + 1 + gap);
        break;
    case ZzTeachingTipPlacement::Left:
        topLeft = QPoint(
            target.left() - gap - tipSize.width(),
            target.center().y() - tipSize.height() / 2);
        break;
    case ZzTeachingTipPlacement::Right:
        topLeft = QPoint(
            target.right() + 1 + gap,
            target.center().y() - tipSize.height() / 2);
        break;
    }
    return QRect(topLeft, tipSize);
}

/** @brief 计算候选矩形落在可用区域外的像素面积。 */
qint64 zzOverflowArea(const QRect &candidate, const QRect &available) noexcept
{
    const QRect intersection = candidate.intersected(available);
    const qint64 candidateArea = static_cast<qint64>(candidate.width())
        * static_cast<qint64>(candidate.height());
    const qint64 intersectionArea = static_cast<qint64>(intersection.width())
        * static_cast<qint64>(intersection.height());
    return candidateArea - intersectionArea;
}

/** @brief 将不大于可用区域的矩形钳制到区域内部。 */
QRect zzClampToAvailable(QRect candidate, const QRect &available) noexcept
{
    const int maximumX = available.right() - candidate.width() + 1;
    const int maximumY = available.bottom() - candidate.height() + 1;
    candidate.moveLeft(std::clamp(
        candidate.left(), available.left(), maximumX));
    candidate.moveTop(std::clamp(
        candidate.top(), available.top(), maximumY));
    return candidate;
}

} // namespace

ZzTeachingTipPrivate::ZzTeachingTipPrivate(ZzTeachingTip *q)
    : q_ptr(q)
    , theme(q)
    , headerHost(new QWidget(q))
    , headerLayout(new QHBoxLayout(headerHost))
    , titleLabel(new QLabel(headerHost))
    , closeButton(new ZzIconButton(headerHost))
    , textLabel(new QLabel(q))
    , contentHost(new QWidget(q))
    , contentLayout(new QVBoxLayout(contentHost))
    , actionButton(new ZzPushButton(q))
    , animationGroup(new QParallelAnimationGroup(q))
    , opacityAnimation(
          new QPropertyAnimation(q, QByteArrayLiteral("windowOpacity")))
    , positionAnimation(new QPropertyAnimation(q, QByteArrayLiteral("pos")))
{
    Q_ASSERT(q_ptr != nullptr);
    opacityAnimation->setParent(animationGroup);
    positionAnimation->setParent(animationGroup);
    animationGroup->addAnimation(opacityAnimation);
    animationGroup->addAnimation(positionAnimation);

    auto *rootLayout = new QVBoxLayout(q_ptr);
    rootLayout->setObjectName(QStringLiteral("zzTeachingTipRootLayout"));
    headerHost->setObjectName(QStringLiteral("zzTeachingTipHeader"));
    headerLayout->setContentsMargins(0, 0, 0, 0);
    titleLabel->setObjectName(QStringLiteral("zzTeachingTipTitle"));
    titleLabel->setWordWrap(true);
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    closeButton->setObjectName(QStringLiteral("zzTeachingTipCloseButton"));
    closeButton->setAutoRaise(true);
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addWidget(closeButton, 0, Qt::AlignTop);
    textLabel->setObjectName(QStringLiteral("zzTeachingTipText"));
    textLabel->setWordWrap(true);
    textLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentHost->setObjectName(QStringLiteral("zzTeachingTipContentHost"));
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    actionButton->setObjectName(QStringLiteral("zzTeachingTipActionButton"));
    actionButton->setAppearance(ZzButtonAppearance::Accent);
    rootLayout->addWidget(headerHost);
    rootLayout->addWidget(textLabel);
    rootLayout->addWidget(contentHost);
    rootLayout->addWidget(actionButton, 0, Qt::AlignTrailing);

    QObject::connect(
        actionButton,
        &QPushButton::clicked,
        q_ptr,
        &ZzTeachingTip::actionTriggered);
    QObject::connect(
        closeButton,
        &QToolButton::clicked,
        q_ptr,
        [this] {
            dismiss();
        });
    animationFinishedConnection = QObject::connect(
        animationGroup,
        &QParallelAnimationGroup::finished,
        q_ptr,
        [this] {
            finishAnimation();
        });
}

ZzTeachingTipPrivate::~ZzTeachingTipPrivate()
{
    if (animationGroup != nullptr) {
        animationGroup->stop();
    }
    if (animationFinishedConnection) {
        QObject::disconnect(animationFinishedConnection);
    }
    if (contentDestroyedConnection) {
        QObject::disconnect(contentDestroyedConnection);
    }
    if (targetDestroyedConnection) {
        QObject::disconnect(targetDestroyedConnection);
    }
    if (targetWidget != nullptr) {
        targetWidget->removeEventFilter(this);
    }
    if (targetWindow != nullptr && targetWindow != targetWidget) {
        targetWindow->removeEventFilter(this);
    }
    if (applicationFilterInstalled
        && QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeEventFilter(this);
    }
}

void ZzTeachingTipPrivate::refreshPresentation()
{
    const auto snapshot = theme.snapshot();
    titleLabel->setText(title);
    titleLabel->setVisible(!title.isEmpty());
    titleLabel->setFont(snapshot->font(ZzTypographyToken::Subtitle));
    textLabel->setText(text);
    textLabel->setVisible(!text.isEmpty());
    textLabel->setFont(snapshot->font(ZzTypographyToken::Body));
    actionButton->setText(actionText);
    actionButton->setEnabled(actionEnabled);
    actionButton->setVisible(actionVisible);
    actionButton->setFont(snapshot->font(ZzTypographyToken::Body));
    closeButton->setVisible(closeButtonVisible);
    headerHost->setVisible(!title.isEmpty() || closeButtonVisible);

    const QString closeText = ZzTeachingTip::tr("关闭");
    closeButton->setAccessibleName(closeText);
    closeButton->setToolTip(closeText);
    const int closeExtent = qCeil(
        snapshot->metric(ZzMetricToken::IconMedium));
    closeButton->setFixedSize(closeExtent, closeExtent);
    closeButton->setIconDescriptor(ZzIconDescriptor::fromBundledSvg(
        ZzBundledSvgIcon::Close));

    const int arrowInset = qCeil(
        snapshot->metric(ZzMetricToken::TeachingTipTargetGap));
    const int contentPadding = qCeil(
        snapshot->metric(ZzMetricToken::OverlayPadding));
    const int totalInset = arrowInset + contentPadding;
    const int sectionSpacing = qCeil(
        snapshot->metric(ZzMetricToken::HorizontalPadding));
    auto *rootLayout = qobject_cast<QVBoxLayout *>(q_ptr->layout());
    Q_ASSERT(rootLayout != nullptr);
    rootLayout->setContentsMargins(
        totalInset, totalInset, totalInset, totalInset);
    rootLayout->setSpacing(sectionSpacing);
    headerLayout->setSpacing(sectionSpacing);
    q_ptr->setMaximumWidth(qCeil(
        snapshot->metric(ZzMetricToken::TeachingTipMaxWidth)));
    q_ptr->setAccessibleDescription(text);
    if (q_ptr->accessibleName().isEmpty()
        || q_ptr->accessibleName() == generatedAccessibleName) {
        generatedAccessibleName = title;
        q_ptr->setAccessibleName(generatedAccessibleName);
    }
    q_ptr->updateGeometry();
    q_ptr->update();
    if (q_ptr->isVisible() && !dismissing) {
        static_cast<void>(reposition());
    }
}

void ZzTeachingTipPrivate::refreshTheme()
{
    theme.refreshFallback();
    refreshPresentation();
}

void ZzTeachingTipPrivate::setContentWidget(QWidget *widget)
{
    if (contentWidget == widget) {
        return;
    }
    if (widget == q_ptr || widget == contentHost
        || (widget != nullptr && widget->isAncestorOf(q_ptr))) {
        qWarning("ZzTeachingTip rejected a cyclic content widget");
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
    refreshPresentation();
    Q_EMIT q_ptr->contentWidgetChanged(widget);
}

QWidget *ZzTeachingTipPrivate::takeContentWidget()
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
    refreshPresentation();
    Q_EMIT q_ptr->contentWidgetChanged(nullptr);
    return widget;
}

void ZzTeachingTipPrivate::setTargetWidget(QWidget *target)
{
    if (targetWidget == target) {
        return;
    }
    if (target == q_ptr || (target != nullptr && q_ptr->isAncestorOf(target))) {
        qWarning("ZzTeachingTip rejected an owned target widget");
        return;
    }
    if (targetDestroyedConnection) {
        QObject::disconnect(targetDestroyedConnection);
        targetDestroyedConnection = {};
    }
    if (targetWidget != nullptr) {
        targetWidget->removeEventFilter(this);
    }
    if (targetWindow != nullptr && targetWindow != targetWidget) {
        targetWindow->removeEventFilter(this);
    }
    targetWindow = nullptr;
    targetWidget = target;
    if (target != nullptr) {
        target->installEventFilter(this);
        targetDestroyedConnection = QObject::connect(
            target,
            &QObject::destroyed,
            q_ptr,
            [this] {
                if (targetWindow != nullptr) {
                    targetWindow->removeEventFilter(this);
                }
                targetWidget = nullptr;
                targetWindow = nullptr;
                targetDestroyedConnection = {};
                Q_EMIT q_ptr->targetWidgetChanged(nullptr);
                dismiss();
            });
        refreshTargetWindowFilter();
    }
    Q_EMIT q_ptr->targetWidgetChanged(target);
    if (target == nullptr) {
        dismiss();
    } else if (q_ptr->isVisible() && !dismissing) {
        static_cast<void>(reposition());
    }
}

void ZzTeachingTipPrivate::showForTarget()
{
    QWidget *target = targetWidget.data();
    if (target == nullptr || !target->isVisible() || target->screen() == nullptr) {
        dismiss();
        return;
    }
    const bool wasVisible = q_ptr->isVisible();
    const QPoint previousPosition = q_ptr->pos();
    const qreal previousOpacity = q_ptr->windowOpacity();
    animationGroup->stop();
    dismissing = false;
    dismissSignalPending = false;
    refreshPresentation();
    q_ptr->adjustSize();
    if (!reposition()) {
        return;
    }
    const int duration = animationDuration();
    if (duration <= 50) {
        q_ptr->move(finalPosition);
        q_ptr->setWindowOpacity(1.0);
        q_ptr->show();
        syncApplicationEventFilter();
        return;
    }
    const QPoint startPosition = wasVisible
        ? previousPosition
        : finalPosition + targetDisplacement();
    const qreal startOpacity = wasVisible ? previousOpacity : 0.0;
    q_ptr->move(startPosition);
    q_ptr->setWindowOpacity(startOpacity);
    q_ptr->show();
    syncApplicationEventFilter();
    startAnimation(
        startOpacity, 1.0, startPosition, finalPosition, duration);
}

void ZzTeachingTipPrivate::dismiss()
{
    if ((!q_ptr->isVisible()
         && animationGroup->state() == QAbstractAnimation::Stopped)
        || dismissing) {
        return;
    }
    animationGroup->stop();
    dismissing = true;
    dismissSignalPending = true;
    const int duration = animationDuration();
    const QPoint endPosition = finalPosition + targetDisplacement();
    if (duration <= 50 || !q_ptr->isVisible()) {
        q_ptr->move(finalPosition);
        q_ptr->setWindowOpacity(1.0);
        q_ptr->hide();
        return;
    }
    startAnimation(
        q_ptr->windowOpacity(),
        0.0,
        q_ptr->pos(),
        endPosition,
        duration);
}

bool ZzTeachingTipPrivate::reposition()
{
    QWidget *target = targetWidget.data();
    QScreen *screen = target != nullptr ? target->screen() : nullptr;
    if (target == nullptr || !target->isVisible() || screen == nullptr) {
        dismiss();
        return false;
    }
    refreshTargetWindowFilter();
    q_ptr->adjustSize();
    const QRect available = screen->availableGeometry();
    QSize tipSize = q_ptr->size().boundedTo(available.size());
    tipSize.setWidth(std::max(1, tipSize.width()));
    tipSize.setHeight(std::max(1, tipSize.height()));
    q_ptr->resize(tipSize);
    const QRect targetRect(
        target->mapToGlobal(QPoint()), target->size());
    const int gap = qCeil(
        theme.snapshot()->metric(ZzMetricToken::TeachingTipTargetGap));
    const auto order = zzPlacementOrder(preferredPlacement);

    QRect selected;
    ZzTeachingTipPlacement selectedPlacement = order.front();
    qint64 selectedOverflow = std::numeric_limits<qint64>::max();
    for (const ZzTeachingTipPlacement placement : order) {
        const QRect candidate = zzPlacementRect(
            placement, targetRect, tipSize, gap);
        if (available.contains(candidate)) {
            selected = candidate;
            selectedPlacement = placement;
            selectedOverflow = 0;
            break;
        }
        const qint64 overflow = zzOverflowArea(candidate, available);
        if (overflow < selectedOverflow) {
            selected = candidate;
            selectedPlacement = placement;
            selectedOverflow = overflow;
        }
    }
    if (selectedOverflow > 0) {
        selected = zzClampToAvailable(selected, available);
    }
    setEffectivePlacement(selectedPlacement);
    finalPosition = selected.topLeft();
    q_ptr->move(finalPosition);

    const qreal arrowInset = theme.snapshot()->metric(
        ZzMetricToken::TeachingTipTargetGap);
    const qreal arrowHalf = std::max<qreal>(2.0, arrowInset / 2.0);
    const qreal radius = theme.snapshot()->metric(
        ZzMetricToken::CornerRadiusMedium);
    if (selectedPlacement == ZzTeachingTipPlacement::Top
        || selectedPlacement == ZzTeachingTipPlacement::Bottom) {
        const qreal desired = static_cast<qreal>(
            targetRect.center().x() - selected.left());
        arrowCenter = std::clamp(
            desired,
            arrowInset + radius + arrowHalf,
            static_cast<qreal>(tipSize.width())
                - arrowInset - radius - arrowHalf);
    } else {
        const qreal desired = static_cast<qreal>(
            targetRect.center().y() - selected.top());
        arrowCenter = std::clamp(
            desired,
            arrowInset + radius + arrowHalf,
            static_cast<qreal>(tipSize.height())
                - arrowInset - radius - arrowHalf);
    }
    q_ptr->update();
    return true;
}

void ZzTeachingTipPrivate::syncApplicationEventFilter()
{
    QCoreApplication *application = QCoreApplication::instance();
    const bool shouldInstall = q_ptr->isVisible() && lightDismissEnabled
        && application != nullptr;
    if (shouldInstall == applicationFilterInstalled) {
        return;
    }
    if (shouldInstall) {
        application->installEventFilter(this);
    } else if (application != nullptr) {
        application->removeEventFilter(this);
    }
    applicationFilterInstalled = shouldInstall;
}

void ZzTeachingTipPrivate::handleHidden()
{
    if (animationGroup->state() != QAbstractAnimation::Stopped) {
        animationGroup->stop();
    }
    syncApplicationEventFilter();
    q_ptr->setWindowOpacity(1.0);
    q_ptr->move(finalPosition);
    dismissing = false;
    if (dismissSignalPending) {
        dismissSignalPending = false;
        Q_EMIT q_ptr->dismissed();
    }
}

bool ZzTeachingTipPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (event == nullptr) {
        return QObject::eventFilter(watched, event);
    }
    if (applicationFilterInstalled
        && event->type() == QEvent::MouseButtonPress
        && lightDismissEnabled && q_ptr->isVisible()) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint globalPosition = mouseEvent->globalPosition().toPoint();
        const QRect targetRect = targetWidget != nullptr
            ? QRect(
                  targetWidget->mapToGlobal(QPoint()), targetWidget->size())
            : QRect();
        if (!q_ptr->frameGeometry().contains(globalPosition)
            && !targetRect.contains(globalPosition)) {
            dismiss();
        }
        return false;
    }

    if (watched == targetWidget.data() || watched == targetWindow.data()) {
        if (event->type() == QEvent::Hide) {
            dismiss();
        } else if (event->type() == QEvent::ParentChange) {
            refreshTargetWindowFilter();
            if (q_ptr->isVisible() && !dismissing) {
                static_cast<void>(reposition());
            }
        } else if (event->type() == QEvent::Move
                   || event->type() == QEvent::Resize
                   || event->type() == QEvent::Show
                   || event->type() == QEvent::ScreenChangeInternal) {
            if (q_ptr->isVisible() && !dismissing) {
                static_cast<void>(reposition());
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void ZzTeachingTipPrivate::refreshTargetWindowFilter()
{
    QWidget *nextWindow = targetWidget != nullptr
        ? targetWidget->window()
        : nullptr;
    if (targetWindow == nextWindow) {
        return;
    }
    if (targetWindow != nullptr && targetWindow != targetWidget) {
        targetWindow->removeEventFilter(this);
    }
    targetWindow = nextWindow;
    if (targetWindow != nullptr && targetWindow != targetWidget) {
        targetWindow->installEventFilter(this);
    }
}

int ZzTeachingTipPrivate::animationDuration() const noexcept
{
    const auto snapshot = theme.snapshot();
    return ZzAnimationPolicy::adjustedDuration(
        snapshot->duration(ZzMotionToken::Fast),
        snapshot->reducedMotion(),
        false);
}

QPoint ZzTeachingTipPrivate::targetDisplacement() const noexcept
{
    constexpr int displacement = 4;
    switch (effectivePlacement) {
    case ZzTeachingTipPlacement::Top:
        return QPoint(0, displacement);
    case ZzTeachingTipPlacement::Bottom:
    case ZzTeachingTipPlacement::Auto:
        return QPoint(0, -displacement);
    case ZzTeachingTipPlacement::Left:
        return QPoint(displacement, 0);
    case ZzTeachingTipPlacement::Right:
        return QPoint(-displacement, 0);
    }
    return {};
}

void ZzTeachingTipPrivate::startAnimation(
    qreal startOpacity,
    qreal endOpacity,
    const QPoint &startPosition,
    const QPoint &endPosition,
    int duration)
{
    opacityAnimation->setDuration(duration);
    opacityAnimation->setStartValue(startOpacity);
    opacityAnimation->setEndValue(endOpacity);
    positionAnimation->setDuration(duration);
    positionAnimation->setStartValue(startPosition);
    positionAnimation->setEndValue(endPosition);
    animationGroup->start();
}

void ZzTeachingTipPrivate::finishAnimation()
{
    if (dismissing) {
        q_ptr->hide();
        return;
    }
    q_ptr->move(finalPosition);
    q_ptr->setWindowOpacity(1.0);
}

void ZzTeachingTipPrivate::setEffectivePlacement(
    ZzTeachingTipPlacement placement)
{
    if (effectivePlacement == placement) {
        return;
    }
    effectivePlacement = placement;
    Q_EMIT q_ptr->effectivePlacementChanged(placement);
}

} // namespace ZzFluentUI
