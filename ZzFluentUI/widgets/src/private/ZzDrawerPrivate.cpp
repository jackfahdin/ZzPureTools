#include "ZzDrawerPrivate.h"

#include <algorithm>
#include <cmath>

#include <QtCore/QAbstractAnimation>
#include <QtCore/QCoreApplication>
#include <QtCore/QEasingCurve>
#include <QtCore/QEvent>
#include <QtCore/QVariant>
#include <QtCore/QVariantAnimation>
#include <QtGui/QKeyEvent>
#include <QtGui/QRegion>
#include <QtWidgets/QApplication>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzDrawer.h>
#include <ZzFluentUI/ZzMetricToken.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>

namespace ZzFluentUI {

ZzDrawerPrivate::ZzDrawerPrivate(ZzDrawer *q)
    : QObject(nullptr)
    , q_ptr(q)
    , theme(q)
    , panelHost(new QWidget(q))
    , contentLayout(new QVBoxLayout(panelHost))
    , progressAnimation(new QVariantAnimation(q))
{
    Q_ASSERT(q_ptr != nullptr);
    panelHost->setObjectName(QStringLiteral("zzDrawerPanelHost"));
    panelHost->setAutoFillBackground(false);
    contentLayout->setSpacing(0);

    progressAnimation->setEasingCurve(QEasingCurve::InOutSine);
    QObject::connect(
        progressAnimation,
        &QVariantAnimation::valueChanged,
        q_ptr,
        [this](const QVariant &value) {
            setProgress(value.toReal());
        });
    QObject::connect(
        progressAnimation,
        &QVariantAnimation::finished,
        q_ptr,
        [this] {
            finishTransition();
        });
    updateHostBinding();
}

ZzDrawerPrivate::~ZzDrawerPrivate()
{
    progressAnimation->stop();
    QObject::disconnect(contentDestroyedConnection);
    if (observedHost != nullptr) {
        observedHost->removeEventFilter(this);
    }
    QCoreApplication *application = QCoreApplication::instance();
    if (applicationFilterInstalled && application != nullptr) {
        application->removeEventFilter(this);
    }
}

void ZzDrawerPrivate::refreshPresentation()
{
    const auto snapshot = theme.snapshot();
    const int padding = qCeil(
        snapshot->metric(ZzMetricToken::OverlayPadding));
    contentLayout->setContentsMargins(padding, padding, padding, padding);

    const QString accessibleName = ZzDrawer::tr("边缘抽屉");
    if (q_ptr->accessibleName().isEmpty()
        || q_ptr->accessibleName() == generatedAccessibleName) {
        generatedAccessibleName = accessibleName;
        q_ptr->setAccessibleName(generatedAccessibleName);
    }
    if (progressAnimation->state() != QAbstractAnimation::Stopped
        && transitionDuration() <= 0) {
        finishTransition();
    } else {
        updateGeometryAndMask();
    }
    syncApplicationEventFilter();
    q_ptr->update();
}

void ZzDrawerPrivate::refreshTheme()
{
    theme.refreshFallback();
    refreshPresentation();
}

void ZzDrawerPrivate::updateHostBinding()
{
    QWidget *const nextHost = q_ptr->parentWidget();
    if (observedHost != nextHost) {
        if (observedHost != nullptr) {
            observedHost->removeEventFilter(this);
        }
        observedHost = nextHost;
        if (observedHost != nullptr) {
            observedHost->installEventFilter(this);
        }
    }
    if (observedHost == nullptr && (open || q_ptr->isVisible())) {
        handleExternalHide();
        q_ptr->hide();
        return;
    }
    updateGeometryAndMask();
    if (q_ptr->isVisible()) {
        q_ptr->raise();
    }
}

void ZzDrawerPrivate::setContentWidget(QWidget *widget)
{
    if (contentWidget == widget) {
        return;
    }
    if (widget == q_ptr || widget == panelHost
        || (widget != nullptr && widget->isAncestorOf(q_ptr))) {
        qWarning("ZzDrawer rejected a cyclic content widget");
        return;
    }

    QObject::disconnect(contentDestroyedConnection);
    contentDestroyedConnection = {};
    QWidget *const oldWidget = contentWidget.data();
    contentWidget = nullptr;
    if (widget != nullptr && oldWidget != nullptr
        && oldWidget->isAncestorOf(widget)) {
        widget->setParent(panelHost);
    }
    if (oldWidget != nullptr) {
        contentLayout->removeWidget(oldWidget);
        delete oldWidget;
    }

    contentWidget = widget;
    if (widget != nullptr) {
        widget->setParent(panelHost);
        contentLayout->addWidget(widget);
        contentDestroyedConnection = QObject::connect(
            widget,
            &QObject::destroyed,
            q_ptr,
            [this] {
                contentWidget = nullptr;
                contentDestroyedConnection = {};
                Q_EMIT q_ptr->contentWidgetChanged(nullptr);
            });
    }
    Q_EMIT q_ptr->contentWidgetChanged(widget);
}

QWidget *ZzDrawerPrivate::takeContentWidget()
{
    QWidget *const widget = contentWidget.data();
    if (widget == nullptr) {
        return nullptr;
    }
    QObject::disconnect(contentDestroyedConnection);
    contentDestroyedConnection = {};
    contentWidget = nullptr;
    contentLayout->removeWidget(widget);
    widget->setParent(nullptr);
    Q_EMIT q_ptr->contentWidgetChanged(nullptr);
    return widget;
}

void ZzDrawerPrivate::openDrawer()
{
    if (observedHost == nullptr) {
        updateHostBinding();
    }
    if (observedHost == nullptr) {
        return;
    }
    if (!open) {
        QWidget *const focus = QApplication::focusWidget();
        if (previousFocus == nullptr && focus != nullptr
            && focus != q_ptr && !q_ptr->isAncestorOf(focus)) {
            previousFocus = focus;
        }
        open = true;
        Q_EMIT q_ptr->openChanged(true);
    }
    q_ptr->show();
    q_ptr->raise();
    updateGeometryAndMask();
    syncApplicationEventFilter();
    focusFirstContent();
    startTransition(1.0);
}

void ZzDrawerPrivate::closeDrawer()
{
    if (!open && progressAnimation->state() == QAbstractAnimation::Stopped
        && progress <= 0.0) {
        return;
    }
    if (open) {
        open = false;
        Q_EMIT q_ptr->openChanged(false);
    }
    syncApplicationEventFilter();
    startTransition(0.0);
}

void ZzDrawerPrivate::handleExternalHide()
{
    if (hidingInternally) {
        return;
    }
    progressAnimation->stop();
    progress = 0.0;
    panelHost->setGeometry(panelRect().toAlignedRect());
    if (open) {
        open = false;
        Q_EMIT q_ptr->openChanged(false);
    }
    syncApplicationEventFilter();
    restorePreviousFocus();
}

QRectF ZzDrawerPrivate::panelRect() const
{
    const int width = panelWidth();
    if (width <= 0 || q_ptr->height() <= 0) {
        return {};
    }
    const qreal logicalWidth = static_cast<qreal>(width);
    const qreal x = edge == ZzDrawerEdge::Left
        ? -logicalWidth + (progress * logicalWidth)
        : static_cast<qreal>(q_ptr->width())
            - (progress * logicalWidth);
    return QRectF(
        x,
        0.0,
        logicalWidth,
        static_cast<qreal>(q_ptr->height()));
}

bool ZzDrawerPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (event == nullptr) {
        return QObject::eventFilter(watched, event);
    }
    if (watched == observedHost.data()) {
        if (event->type() == QEvent::Resize
            || event->type() == QEvent::Move
            || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest) {
            updateGeometryAndMask();
            if (q_ptr->isVisible()) {
                q_ptr->raise();
            }
        } else if (event->type() == QEvent::ChildAdded
                   && q_ptr->isVisible()) {
            q_ptr->raise();
        }
        return QObject::eventFilter(watched, event);
    }

    if (applicationFilterInstalled && event->type() == QEvent::KeyPress) {
        QWidget *const focus = QApplication::focusWidget();
        if (focus != q_ptr
            && (focus == nullptr || !q_ptr->isAncestorOf(focus))) {
            return QObject::eventFilter(watched, event);
        }
        auto *const keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            closeDrawer();
            return true;
        }
        if (modal
            && (keyEvent->key() == Qt::Key_Tab
                || keyEvent->key() == Qt::Key_Backtab)) {
            const bool backwards = keyEvent->key() == Qt::Key_Backtab
                || keyEvent->modifiers().testFlag(Qt::ShiftModifier);
            cycleFocus(backwards);
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

int ZzDrawerPrivate::panelWidth() const
{
    const int hostWidth = q_ptr->width();
    if (hostWidth <= 0) {
        return 0;
    }
    const auto snapshot = theme.snapshot();
    const int requested = widthHint > 0
        ? widthHint
        : qCeil(snapshot->metric(ZzMetricToken::DrawerDefaultWidth));
    return std::clamp(requested, 1, hostWidth);
}

int ZzDrawerPrivate::transitionDuration() const
{
    const auto snapshot = theme.snapshot();
    return ZzAnimationPolicy::adjustedDuration(
        snapshot->duration(ZzMotionToken::Normal),
        snapshot->reducedMotion(),
        false);
}

void ZzDrawerPrivate::setProgress(qreal value)
{
    const QRectF previousPanel = panelRect();
    progress = std::clamp(value, 0.0, 1.0);
    updateGeometryAndMask();
    if (modal) {
        q_ptr->update();
    } else {
        q_ptr->update(
            previousPanel.united(panelRect()).toAlignedRect());
    }
}

void ZzDrawerPrivate::startTransition(qreal target)
{
    const qreal normalizedTarget = std::clamp(target, 0.0, 1.0);
    const qreal remaining = std::abs(normalizedTarget - progress);
    progressAnimation->stop();
    const int fullDuration = transitionDuration();
    if (fullDuration <= 0 || qFuzzyIsNull(remaining)) {
        setProgress(normalizedTarget);
        finishTransition();
        return;
    }
    progressAnimation->setStartValue(progress);
    progressAnimation->setEndValue(normalizedTarget);
    progressAnimation->setDuration(std::max(
        1,
        qRound(static_cast<qreal>(fullDuration) * remaining)));
    progressAnimation->start();
}

void ZzDrawerPrivate::finishTransition()
{
    progressAnimation->stop();
    setProgress(open ? 1.0 : 0.0);
    if (open) {
        syncApplicationEventFilter();
        return;
    }

    hidingInternally = true;
    q_ptr->hide();
    hidingInternally = false;
    syncApplicationEventFilter();
    restorePreviousFocus();
}

void ZzDrawerPrivate::updateGeometryAndMask()
{
    QWidget *const host = observedHost.data();
    if (host != nullptr && q_ptr->parentWidget() == host
        && q_ptr->geometry() != host->rect()) {
        q_ptr->setGeometry(host->rect());
    }
    const QRectF logicalPanel = panelRect();
    const QRect panelGeometry(
        qRound(logicalPanel.x()),
        0,
        qRound(logicalPanel.width()),
        q_ptr->height());
    panelHost->setGeometry(panelGeometry);

    if (modal) {
        q_ptr->clearMask();
    } else {
        q_ptr->setMask(QRegion(panelGeometry.intersected(q_ptr->rect())));
    }
}

void ZzDrawerPrivate::syncApplicationEventFilter()
{
    QCoreApplication *const application = QCoreApplication::instance();
    const bool shouldInstall = application != nullptr && q_ptr->isVisible()
        && (open || progress > 0.0);
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

QList<QWidget *> ZzDrawerPrivate::focusableWidgets() const
{
    QList<QWidget *> result;
    QWidget *candidate = panelHost->nextInFocusChain();
    while (candidate != nullptr && candidate != panelHost) {
        const int policy = static_cast<int>(candidate->focusPolicy());
        const int tabFocus = static_cast<int>(Qt::TabFocus);
        if (candidate != q_ptr && q_ptr->isAncestorOf(candidate)
            && candidate->isVisibleTo(q_ptr) && candidate->isEnabled()
            && (policy & tabFocus) != 0) {
            result.append(candidate);
        }
        candidate = candidate->nextInFocusChain();
    }
    return result;
}

void ZzDrawerPrivate::focusFirstContent()
{
    const QList<QWidget *> candidates = focusableWidgets();
    if (!candidates.isEmpty()) {
        candidates.constFirst()->setFocus(Qt::OtherFocusReason);
        return;
    }
    q_ptr->setFocus(Qt::OtherFocusReason);
}

void ZzDrawerPrivate::cycleFocus(bool backwards)
{
    const QList<QWidget *> candidates = focusableWidgets();
    if (candidates.isEmpty()) {
        q_ptr->setFocus(Qt::TabFocusReason);
        return;
    }
    QWidget *const current = QApplication::focusWidget();
    const qsizetype currentIndex = candidates.indexOf(current);
    qsizetype nextIndex = backwards
        ? currentIndex - 1
        : currentIndex + 1;
    if (currentIndex < 0) {
        nextIndex = backwards ? candidates.size() - 1 : 0;
    } else if (nextIndex < 0) {
        nextIndex = candidates.size() - 1;
    } else if (nextIndex >= candidates.size()) {
        nextIndex = 0;
    }
    candidates.at(nextIndex)->setFocus(Qt::TabFocusReason);
}

void ZzDrawerPrivate::restorePreviousFocus()
{
    QWidget *const focus = previousFocus.data();
    previousFocus = nullptr;
    if (focus != nullptr && focus->isVisible() && focus->isEnabled()) {
        focus->setFocus(Qt::OtherFocusReason);
    }
}

} // namespace ZzFluentUI
