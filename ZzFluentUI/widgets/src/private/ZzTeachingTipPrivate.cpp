#include "ZzTeachingTipPrivate.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QVariantAnimation>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
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
    , transitionAnimation(new QVariantAnimation)
{
    Q_ASSERT(q_ptr != nullptr);
    opacityAnimation->setParent(animationGroup);
    transitionAnimation->setParent(animationGroup);
    animationGroup->addAnimation(opacityAnimation);
    animationGroup->addAnimation(transitionAnimation);
    opacityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    transitionAnimation->setEasingCurve(QEasingCurve::OutCubic);
    QObject::connect(transitionAnimation, &QVariantAnimation::valueChanged, q_ptr,
        [this](const QVariant &value) {
            const qreal progress = value.toReal();
            openingScale = animationStartScale
                + (animationEndScale - animationStartScale) * progress;
            const QPointF position = QPointF(animationStartPosition)
                + QPointF(animationEndPosition - animationStartPosition) * progress;
            const QPointer<ZzTeachingTip> guard(q_ptr);
            q_ptr->move(position.toPoint());
            if (guard && !openingSnapshot.isNull()) {
                q_ptr->update();
            }
        });

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
    if (applicationFilterInstalled
        && QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeEventFilter(this);
    }
}

void ZzTeachingTipPrivate::refreshPresentation()
{
    const QPointer<ZzTeachingTip> guard(q_ptr);
    settleOpening();
    if (!guard) {
        return;
    }
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
    const QPointer<ZzTeachingTip> guard(q_ptr);
    const QPointer<QWidget> incoming(widget);
    const quint64 revision = ++contentRevision;
    settleOpening();
    if (!guard || contentRevision != revision || (widget && !incoming)) {
        return;
    }
    if (contentDestroyedConnection) {
        QObject::disconnect(contentDestroyedConnection);
        contentDestroyedConnection = {};
    }
    const QPointer<QWidget> oldWidget = contentWidget;
    contentWidget = nullptr;
    if (oldWidget != nullptr) {
        contentLayout->removeWidget(oldWidget);
        // Detach before destruction: a destroyed callback may delete the tip.
        oldWidget->setParent(nullptr);
        delete oldWidget.data();
    }
    if (!guard || contentRevision != revision) {
        return;
    }
    widget = incoming.data();
    contentWidget = widget;
    if (widget != nullptr) {
        widget->setParent(contentHost);
        if (!guard || !incoming || contentRevision != revision) {
            return;
        }
        contentLayout->addWidget(widget);
        contentDestroyedConnection = QObject::connect(
            widget,
            &QObject::destroyed,
            q_ptr,
            [this] {
                contentWidget = nullptr;
                contentDestroyedConnection = {};
                const QPointer<ZzTeachingTip> callbackGuard(q_ptr);
                const quint64 callbackRevision = ++contentRevision;
                settleOpening();
                if (!callbackGuard || callbackRevision != contentRevision) {
                    return;
                }
                contentHost->hide();
                refreshPresentation();
                if (callbackGuard && callbackRevision == contentRevision) {
                    Q_EMIT q_ptr->contentWidgetChanged(nullptr);
                }
            });
    }
    contentHost->setVisible(widget != nullptr);
    refreshPresentation();
    if (guard && contentRevision == revision) {
        Q_EMIT q_ptr->contentWidgetChanged(contentWidget.data());
    }
}

QWidget *ZzTeachingTipPrivate::takeContentWidget()
{
    const QPointer<ZzTeachingTip> guard(q_ptr);
    const quint64 revision = ++contentRevision;
    settleOpening();
    if (!guard || contentRevision != revision) {
        return nullptr;
    }
    const QPointer<QWidget> widget = contentWidget;
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
    if (!guard || contentRevision != revision) {
        return widget.data();
    }
    contentHost->hide();
    refreshPresentation();
    if (guard && contentRevision == revision) {
        Q_EMIT q_ptr->contentWidgetChanged(nullptr);
    }
    return widget.data();
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
    ++visibilityRevision;
    if (targetDestroyedConnection) {
        QObject::disconnect(targetDestroyedConnection);
        targetDestroyedConnection = {};
    }
    targetWindow = nullptr;
    targetWidget = target;
    if (target != nullptr) {
        targetDestroyedConnection = QObject::connect(
            target,
            &QObject::destroyed,
            q_ptr,
            [this] {
                targetWidget = nullptr;
                targetWindow = nullptr;
                targetDestroyedConnection = {};
                const QPointer<ZzTeachingTip> guard(q_ptr);
                Q_EMIT q_ptr->targetWidgetChanged(nullptr);
                if (guard && !targetWidget) {
                    dismiss();
                }
            });
        refreshTargetWindow();
    }
    const QPointer<ZzTeachingTip> guard(q_ptr);
    Q_EMIT q_ptr->targetWidgetChanged(target);
    if (!guard) {
        return;
    }
    if (targetWidget == nullptr) {
        dismiss();
    } else if (q_ptr->isVisible() && !dismissing) {
        static_cast<void>(reposition());
    }
}

void ZzTeachingTipPrivate::showForTarget()
{
    const quint64 revision = ++visibilityRevision;
    QWidget *target = targetWidget.data();
    if (target == nullptr || !target->isVisible() || target->screen() == nullptr) {
        dismiss();
        return;
    }
    const bool wasVisible = q_ptr->isVisible();
    if (!wasVisible) {
        QWidget *focus = QApplication::focusWidget();
        previousFocus = focus && focus->window() == target->window() ? focus : nullptr;
    }
    restoreFocusPending = false;
    const QPoint previousPosition = q_ptr->pos();
    const qreal previousOpacity = q_ptr->windowOpacity();
    const qreal previousScale = openingScale;
    animationGroup->stop();
    dismissing = false;
    dismissSignalPending = false;
    const QPointer<ZzTeachingTip> guard(q_ptr);
    refreshPresentation();
    if (!guard || revision != visibilityRevision) {
        return;
    }
    q_ptr->adjustSize();
    if (!reposition() || !guard || revision != visibilityRevision) {
        return;
    }
    const int duration = animationDuration();
    if (duration <= 50 || restoringSnapshot
        || (wasVisible && previousScale == 1.0 && previousOpacity == 1.0)) {
        q_ptr->move(finalPosition);
        q_ptr->setWindowOpacity(1.0);
        q_ptr->show();
        return;
    }
    const QPoint startPosition = wasVisible
        ? previousPosition
        : finalPosition + targetDisplacement();
    const qreal startOpacity = wasVisible ? previousOpacity : 0.0;
    const bool captured = prepareOpeningSnapshot();
    if (!guard || revision != visibilityRevision) {
        return;
    }
    if (!captured) {
        q_ptr->move(finalPosition);
        q_ptr->setWindowOpacity(1.0);
        q_ptr->show();
        return;
    }
    openingScale = wasVisible ? previousScale : 0.01;
    q_ptr->move(startPosition);
    q_ptr->setWindowOpacity(startOpacity);
    q_ptr->show();
    if (!guard || revision != visibilityRevision || !q_ptr->isVisible()) {
        return;
    }
    startAnimation(
        startOpacity, 1.0, startPosition, finalPosition, duration);
}

void ZzTeachingTipPrivate::dismiss(bool allowFocusRestore)
{
    ++visibilityRevision;
    if (!allowFocusRestore) {
        restoreFocusPending = false;
    }
    if (!q_ptr->isVisible()
        && animationGroup->state() == QAbstractAnimation::Stopped) {
        clearOpeningSnapshot();
        return;
    }
    if (dismissing) {
        return;
    }
    animationGroup->stop();
    QWidget *focus = QApplication::focusWidget();
    restoreFocusPending = allowFocusRestore && focus
        && (focus == q_ptr || q_ptr->isAncestorOf(focus))
        && QApplication::activeWindow() == q_ptr;
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
    const QPointer<ZzTeachingTip> guard(q_ptr);
    settleOpening();
    if (!guard) {
        return false;
    }
    QWidget *target = targetWidget.data();
    QScreen *screen = target != nullptr ? target->screen() : nullptr;
    const QRect targetRect = visibleTargetRect();
    if (targetRect.isEmpty() || screen == nullptr) {
        dismiss();
        return false;
    }
    refreshTargetWindow();
    q_ptr->adjustSize();
    const QRect available = screen->availableGeometry();
    QSize tipSize = q_ptr->size().boundedTo(available.size());
    tipSize.setWidth(std::max(1, tipSize.width()));
    tipSize.setHeight(std::max(1, tipSize.height()));
    q_ptr->resize(tipSize);
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
    setEffectivePlacement(selectedPlacement);
    return !guard.isNull();
}

void ZzTeachingTipPrivate::syncApplicationEventFilter()
{
    QCoreApplication *application = QCoreApplication::instance();
    const bool shouldInstall = q_ptr->isVisible()
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
    ++visibilityRevision;
    const QPointer<ZzTeachingTip> guard(q_ptr);
    if (animationGroup->state() != QAbstractAnimation::Stopped) {
        animationGroup->stop();
    }
    clearOpeningSnapshot();
    if (!guard) {
        return;
    }
    syncApplicationEventFilter();
    q_ptr->setWindowOpacity(1.0);
    q_ptr->move(finalPosition);
    dismissing = false;
    const bool notify = dismissSignalPending;
    dismissSignalPending = false;
    const QPointer<QWidget> focus = previousFocus;
    previousFocus = nullptr;
    const bool restore = restoreFocusPending;
    restoreFocusPending = false;
    QWidget *active = QApplication::activeWindow();
    if (restore && focus && focus->isVisible() && focus->isEnabled()
        && focus->window() == targetWindow
        && (!active || active == q_ptr || active == targetWindow)) {
        focus->window()->activateWindow();
        if (focus) {
            focus->setFocus(Qt::OtherFocusReason);
        }
    }
    if (guard && notify) {
        Q_EMIT q_ptr->dismissed();
    }
}

bool ZzTeachingTipPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (!event || !q_ptr->isVisible()) {
        return false;
    }
    if (applicationFilterInstalled
        && event->type() == QEvent::MouseButtonPress
        && lightDismissEnabled && q_ptr->isVisible()) {
        const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint globalPosition = mouseEvent->globalPosition().toPoint();
        const QRect targetRect = visibleTargetRect();
        if (!q_ptr->frameGeometry().contains(globalPosition)
            && !targetRect.contains(globalPosition)) {
            const QPointer<QObject> receiver(watched);
            // The outside event chooses its own focus recipient.
            dismiss(false);
            return receiver.isNull();
        }
        return false;
    }

    switch (event->type()) {
    case QEvent::Hide:
    case QEvent::Close:
    case QEvent::ParentChange:
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::LayoutRequest:
    case QEvent::Show:
    case QEvent::ScreenChangeInternal:
        break;
    default:
        return false;
    }
    auto *widget = qobject_cast<QWidget *>(watched);
    if (widget && targetWidget && (widget == targetWidget || widget->isAncestorOf(targetWidget))) {
        const QPointer<QObject> receiver(watched);
        if (event->type() == QEvent::Hide || event->type() == QEvent::Close) {
            dismiss();
        } else if (event->type() == QEvent::ParentChange) {
            refreshTargetWindow();
            if (q_ptr->isVisible() && !dismissing) {
                static_cast<void>(reposition());
            }
        } else if (event->type() == QEvent::Move
                   || event->type() == QEvent::Resize
                   || event->type() == QEvent::LayoutRequest
                   || event->type() == QEvent::Show
                   || event->type() == QEvent::ScreenChangeInternal) {
            if (q_ptr->isVisible() && !dismissing) {
                static_cast<void>(reposition());
            }
        }
        return receiver.isNull();
    }
    return false;
}

void ZzTeachingTipPrivate::refreshTargetWindow()
{
    targetWindow = targetWidget != nullptr
        ? targetWidget->window()
        : nullptr;
}

QRect ZzTeachingTipPrivate::visibleTargetRect() const
{
    if (!targetWidget || !targetWidget->isVisible()) {
        return {};
    }
    QRect visible(targetWidget->mapToGlobal(QPoint()), targetWidget->size());
    if (targetWidget->isWindow()) {
        return visible;
    }
    for (QWidget *ancestor = targetWidget->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
        visible = visible.intersected(QRect(ancestor->mapToGlobal(QPoint()), ancestor->size()));
        if (ancestor->isWindow()) {
            break;
        }
    }
    return visible;
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
    animationStartPosition = startPosition;
    animationEndPosition = endPosition;
    animationStartScale = openingScale;
    animationEndScale = dismissing && !openingSnapshot.isNull() ? 0.01 : 1.0;
    transitionAnimation->setDuration(duration);
    transitionAnimation->setStartValue(0.0);
    transitionAnimation->setEndValue(1.0);
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
    clearOpeningSnapshot();
}

bool ZzTeachingTipPrivate::prepareOpeningSnapshot()
{
    const QPointer<ZzTeachingTip> guard(q_ptr);
    const quint64 revision = visibilityRevision;
    q_ptr->layout()->activate();
    headerLayout->activate();
    contentLayout->activate();
    const qreal dpr = q_ptr->devicePixelRatioF();
    QPixmap snapshot(qCeil(q_ptr->width() * dpr), qCeil(q_ptr->height() * dpr));
    snapshot.setDevicePixelRatio(dpr);
    snapshot.fill(Qt::transparent);
    {
        QPainter painter(&snapshot);
        q_ptr->render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    }
    if (!guard || revision != visibilityRevision) {
        return false;
    }
    openingSnapshot = snapshot;
    const std::array<QWidget *, 4> hosts = {headerHost, textLabel, contentHost, actionButton};
    for (QWidget *host : hosts) {
        if (!host->isHidden()) {
            QSizePolicy policy = host->sizePolicy();
            hiddenContent.append({host, policy.retainSizeWhenHidden()});
            policy.setRetainSizeWhenHidden(true);
            host->setSizePolicy(policy);
            host->hide();
            if (!guard || revision != visibilityRevision || openingSnapshot.isNull()) {
                return false;
            }
        }
    }
    return true;
}

void ZzTeachingTipPrivate::clearOpeningSnapshot()
{
    if (openingSnapshot.isNull() && hiddenContent.isEmpty()) {
        return;
    }
    openingSnapshot = QPixmap();
    openingScale = 1.0;
    const auto hosts = std::exchange(hiddenContent, {});
    const QPointer<ZzTeachingTip> guard(q_ptr);
    restoringSnapshot = true;
    for (const auto &entry : hosts) {
        if (entry.widget) {
            QSizePolicy policy = entry.widget->sizePolicy();
            policy.setRetainSizeWhenHidden(entry.retainSize);
            entry.widget->setSizePolicy(policy);
            const bool visible = entry.widget == headerHost ? (!title.isEmpty() || closeButtonVisible)
                : entry.widget == textLabel ? !text.isEmpty()
                : entry.widget == contentHost ? !contentWidget.isNull() : actionVisible;
            entry.widget->setVisible(visible);
            if (!guard) {
                return;
            }
        }
    }
    restoringSnapshot = false;
    q_ptr->update();
}

void ZzTeachingTipPrivate::settleOpening()
{
    if (openingSnapshot.isNull()) {
        return;
    }
    // During closing, release stale pixels without cancelling the close request.
    if (!dismissing) {
        animationGroup->stop();
        q_ptr->move(finalPosition);
        q_ptr->setWindowOpacity(1.0);
    }
    clearOpeningSnapshot();
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
