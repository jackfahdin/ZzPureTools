#include "ZzAnimatedStackedWidgetPrivate.h"
#include <algorithm>
#include <QtCore/QEvent>
#include <QtCore/QThread>
#include <QtCore/QVariantAnimation>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLayout>
#include <ZzFluentUI/ZzAnimationPolicy.h>
#include <ZzFluentUI/ZzMotionToken.h>
#include <ZzFluentUI/ZzThemeSnapshot.h>
#include "ZzStackTransitionOverlay.h"

namespace ZzFluentUI {

ZzAnimatedStackedWidgetPrivate::ZzAnimatedStackedWidgetPrivate(ZzAnimatedStackedWidget *owner)
    : q(owner), theme(owner), host(new QWidget(owner)),
      overlay(new ZzStackTransitionOverlay(owner)), animation(new QVariantAnimation(owner))
{
    host->setGeometry(q->contentsRect());
    host->installEventFilter(this);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    QObject::connect(animation, &QVariantAnimation::valueChanged, this,
        [this](const QVariant &value) { overlay->setProgress(value.toReal()); });
    QObject::connect(animation, &QVariantAnimation::finished, this, [this] { finish(false); });
}

ZzAnimatedStackedWidgetPrivate::~ZzAnimatedStackedWidgetPrivate()
{
    animation->stop();
    for (const auto &page : pages) {
        QObject::disconnect(page.destroyed);
        if (page.view) page.view->removeEventFilter(this);
    }
}

int ZzAnimatedStackedWidgetPrivate::indexOf(QWidget *page) const noexcept
{
    if (!page) return -1;
    for (qsizetype i = 0; i < pages.size(); ++i) {
        if (pages.at(i).identity == page) return static_cast<int>(i);
    }
    return -1;
}

void ZzAnimatedStackedWidgetPrivate::notifyCurrent()
{
    const int index = indexOf(current);
    if (notifiedCurrent == current && notifiedIndex == index) return;
    notifiedCurrent = current;
    notifiedIndex = index;
    Q_EMIT q->currentChanged(index);
}

void ZzAnimatedStackedWidgetPrivate::endMutation()
{
    --mutationDepth;
    if (mutationDepth || draining || running) return;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    draining = true;
    while (pending) {
        const QPointer<QWidget> next = pending;
        const auto direction = pendingDirection;
        pending.clear();
        request(next, direction);
        if (!guard) return;
        if (running) break;
    }
    draining = false;
}

void ZzAnimatedStackedWidgetPrivate::synchronizePages()
{
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    ++mutationDepth;
    host->setGeometry(q->contentsRect());
    if (!guard) return;
    // 显隐事件可能移除任意页面，遍历独立观察快照而不是活动容器。
    const auto snapshot = pages;
    for (const auto &entry : snapshot) {
        if (entry.view && entry.identity != current && indexOf(entry.identity) >= 0) entry.view->hide();
        if (!guard) return;
    }
    const QPointer<QWidget> target = q->currentWidget();
    if (target) {
        target->setGeometry(host->rect());
        if (!guard) return;
        if (target && target == q->currentWidget()) target->show();
        if (!guard) return;
    }
    q->updateGeometry();
    endMutation();
}

int ZzAnimatedStackedWidgetPrivate::insert(int index, QWidget *page)
{
    if (!page || QThread::currentThread() != q->thread() || page->thread() != q->thread()
        || page == q || page->isAncestorOf(q) || index < 0 || index > pages.size()) return -1;
    const int existing = indexOf(page);
    if (existing >= 0) return existing;
    if (page->parent() && page->parent() != q) return -1;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    const QPointer<QWidget> view(page);
    ++mutationDepth;
    ++generation;
    page->setParent(host);
    if (!guard) return -1;
    if (!view || view->parentWidget() != host) { endMutation(); return -1; }
    ZzAnimatedStackPage entry;
    entry.view = page;
    entry.identity = page;
    entry.destroyed = QObject::connect(page, &QObject::destroyed, this, [this, page] { remove(page, false); });
    pages.insert(std::min(index, static_cast<int>(pages.size())), entry);
    if (!current) current = page;
    page->installEventFilter(this);
    synchronizePages();
    if (!guard) return -1;
    Q_EMIT q->countChanged(static_cast<int>(pages.size()));
    if (!guard) return -1;
    notifyCurrent();
    if (!guard) return -1;
    endMutation();
    return guard && view ? indexOf(view) : -1;
}

QWidget *ZzAnimatedStackedWidgetPrivate::remove(QWidget *page, bool detach)
{
    const int index = indexOf(page);
    if (index < 0) return nullptr;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    const auto entry = pages.at(index);
    const QPointer<QWidget> view = entry.view;
    const bool removedCurrent = current == page;
    const bool involved = (running || preparing) && (removedCurrent || source == page);
    ++mutationDepth;
    ++generation;
    pages.removeAt(index);
    QObject::disconnect(entry.destroyed);
    if (view) view->removeEventFilter(this);
    if (pending == page) pending.clear();
    if (removedCurrent) current = pages.isEmpty() ? nullptr : pages.at(std::min(index, static_cast<int>(pages.size()) - 1)).identity;
    if (involved) {
        pending.clear();
        finish(false);
        if (!guard) return view;
    }
    if (detach && view) {
        view->hide();
        if (!guard) return nullptr;
        if (view) view->setParent(nullptr);
        if (!guard) return view;
    }
    synchronizePages();
    if (!guard) return view;
    Q_EMIT q->countChanged(static_cast<int>(pages.size()));
    if (!guard) return view;
    notifyCurrent();
    if (!guard) return view;
    endMutation();
    return view;
}

bool ZzAnimatedStackedWidgetPrivate::request(QWidget *page, Direction direction)
{
    if (indexOf(page) < 0 || direction < Direction::Automatic || direction > Direction::Backward) return false;
    if (mutationDepth || running) {
        pending = current == page ? nullptr : page;
        pendingDirection = direction;
        return true;
    }
    if (current == page) return true;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    const QPointer<QWidget> destination(page);
    const QPointer<QWidget> outgoing(q->currentWidget());
    const int fromIndex = indexOf(current);
    const int toIndex = indexOf(page);
    const auto snapshot = theme.snapshot();
    const int duration = ZzAnimationPolicy::adjustedDuration(milliseconds < 0
        ? snapshot->duration(ZzMotionToken::PageTransition) : milliseconds,
        snapshot->reducedMotion(), false);
    const QSize pixels = ZzStackTransitionOverlay::boundedPixelSize(q->contentsRect().size(), q->devicePixelRatioF());
    bool animate = mode != Mode::None && duration > 0 && q->isVisible() && q->isEnabled()
        && !pixels.isEmpty() && canCapture(outgoing) && canCapture(destination);
    ++mutationDepth;
    const quint64 operation = ++generation;
    preparing = true;
    source = outgoing;
    QWidget *focused = QApplication::focusWidget();
    restoreFocus = outgoing && focused && (focused == outgoing || outgoing->isAncestorOf(focused));
    if (restoreFocus && fromIndex >= 0) pages[fromIndex].focus = focused;
    QImage oldImage;
    if (animate) oldImage = capture(outgoing, pixels);
    if (!guard) return true;
    if (!destination || indexOf(destination) < 0) {
        preparing = false;
        source = nullptr;
        endMutation();
        return true;
    }
    animate = animate && operation == generation && !oldImage.isNull();
    current = page;
    synchronizePages();
    if (!guard) return true;
    QImage newImage;
    if (animate && destination && destination == q->currentWidget() && operation == generation) newImage = capture(destination, pixels);
    if (!guard) return true;
    preparing = false;
    animate = animate && operation == generation && !newImage.isNull();
    if (animate) {
        bool reverse = direction == Direction::Backward || (direction == Direction::Automatic && toIndex < fromIndex);
        if (mode == Mode::SlideHorizontal && q->isRightToLeft()) reverse = !reverse;
        overlay->setGeometry(q->contentsRect());
        overlay->setFrames(std::move(oldImage), std::move(newImage), mode, reverse);
        running = true;
        if (restoreFocus) q->setFocus(Qt::OtherFocusReason);
        if (!guard) return true;
        if (running) host->hide();
        if (!guard) return true;
        if (running) overlay->show();
        if (!guard) return true;
        if (running) overlay->raise();
        if (!guard) return true;
    } else {
        source = nullptr;
        restoreFocus = false;
    }
    notifyCurrent();
    if (!guard) return true;
    if (running) {
        announced = true;
        Q_EMIT q->transitionStarted();
        if (!guard) return true;
        if (running) {
            animation->setDuration(duration);
            animation->start();
        }
    }
    if (guard) endMutation();
    return true;
}

bool ZzAnimatedStackedWidgetPrivate::canCapture(QWidget *page) const
{
    const int index = indexOf(page);
    if (index < 0 || !pages.at(index).animationEnabled) return false;
    auto widgets = page->findChildren<QWidget *>();
    widgets.prepend(page);
    for (auto *child : widgets) {
        if (child->testAttribute(Qt::WA_NativeWindow) || child->inherits("QOpenGLWidget")
            || child->inherits("QQuickWidget") || child->inherits("QVideoWidget")
            || child->inherits("QWindowContainer")) return false;
    }
    return true;
}

QImage ZzAnimatedStackedWidgetPrivate::capture(QWidget *page, QSize pixels)
{
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    const QPointer<QWidget> view(page);
    page->ensurePolished();
    if (!guard || !view) return {};
    view->setGeometry(host->rect());
    if (!guard || !view) return {};
    if (view->layout()) view->layout()->activate();
    if (!guard || !view) return {};
    QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) return {};
    image.setDevicePixelRatio(q->devicePixelRatioF());
    image.fill(q->palette().color(QPalette::Window));
    QPainter painter(&image);
    view->render(&painter, QPoint(), QRegion(), QWidget::DrawWindowBackground | QWidget::DrawChildren);
    return image;
}

void ZzAnimatedStackedWidgetPrivate::settle()
{
    if (running || preparing) finish(true);
}

void ZzAnimatedStackedWidgetPrivate::finish(bool latest)
{
    if (settling) return;
    const QPointer<ZzAnimatedStackedWidget> guard(q);
    ++mutationDepth;
    ++generation;
    settling = true;
    preparing = false;
    const bool notify = announced;
    const bool focusOwned = restoreFocus && q->hasFocus();
    announced = false;
    running = false;
    restoreFocus = false;
    source = nullptr;
    animation->stop();
    overlay->hide();
    if (!guard) return;
    overlay->clear();
    if (latest && pending && indexOf(pending) >= 0) { current = pending; pending.clear(); }
    host->show();
    if (!guard) return;
    synchronizePages();
    if (!guard) return;
    if (focusOwned && q->isVisible() && q->isEnabled() && q->isActiveWindow()) {
        const int index = indexOf(current);
        QPointer<QWidget> target = q->currentWidget();
        QPointer<QWidget> focus = index >= 0 ? pages.at(index).focus : nullptr;
        if (!focus || !target || !target->isAncestorOf(focus) || !focus->isVisible() || !focus->isEnabled()) {
            focus.clear();
            if (target) {
                QWidget *candidate = target;
                do {
                    if (candidate->isVisible() && candidate->isEnabled()
                        && (candidate->focusPolicy() & Qt::TabFocus)
                        && (candidate == target || target->isAncestorOf(candidate))) { focus = candidate; break; }
                    candidate = candidate->nextInFocusChain();
                } while (candidate != target);
            }
        }
        if (focus) focus->setFocus(Qt::OtherFocusReason);
        if (!guard) return;
    }
    settling = false;
    notifyCurrent();
    if (!guard) return;
    if (notify) Q_EMIT q->transitionFinished();
    if (guard) endMutation();
}

bool ZzAnimatedStackedWidgetPrivate::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == host && event->type() == QEvent::LayoutRequest) q->updateGeometry();
    if (event->type() == QEvent::ParentChange) {
        auto *page = qobject_cast<QWidget *>(watched);
        if (page && indexOf(page) >= 0 && page->parentWidget() != host) {
            const QPointer<QObject> receiver(watched);
            remove(page, false);
            return receiver.isNull();
        }
    }
    return false;
}

} // namespace ZzFluentUI
